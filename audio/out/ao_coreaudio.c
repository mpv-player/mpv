/*
 * CoreAudio audio output driver for Mac OS X
 *
 * original copyright (C) Timothy J. Wood - Aug 2000
 * ported to MPlayer libao2 by Dan Christiansen
 *
 * Chris Roccati
 * Stefano Pigozzi
 *
 * The S/PDIF part of the code is based on the auhal audio output
 * module from VideoLAN:
 * Copyright (c) 2006 Derk-Jan Hartman <hartman at videolan dot org>
 *
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * along with MPlayer; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/*
 * The MacOS X CoreAudio framework doesn't mesh as simply as some
 * simpler frameworks do.  This is due to the fact that CoreAudio pulls
 * audio samples rather than having them pushed at it (which is nice
 * when you are wanting to do good buffering of audio).
 */

#include "config.h"
#include "ao.h"
#include "internal.h"
#include "audio/format.h"
#include "osdep/timer.h"
#include "options/m_option.h"
#include "misc/ring.h"
#include "common/msg.h"
#include "audio/out/ao_coreaudio_properties.h"
#include "audio/out/ao_coreaudio_utils.h"

static void audio_pause(struct ao *ao);
static void audio_resume(struct ao *ao);
static void reset(struct ao *ao);

static void print_buffer(struct ao *ao, struct mp_ring *buffer)
{
    void *tctx = talloc_new(NULL);
    MP_VERBOSE(ao, "%s\n", mp_ring_repr(buffer, tctx));
    talloc_free(tctx);
}

struct priv_d {
    // digital render callback
    AudioDeviceIOProcID render_cb;

    // pid set for hog mode, (-1) means that hog mode on the device was
    // released. hog mode is exclusive access to a device
    pid_t hog_pid;

    // stream selected for digital playback by the detection in init
    AudioStreamID stream;

    // stream index in an AudioBufferList
    int stream_idx;

    // format we changed the stream to: for the digital case each application
    // sets the stream format for a device to what it needs
    AudioStreamBasicDescription stream_asbd;
    AudioStreamBasicDescription original_asbd;

    bool changed_mixing;
    int stream_asbd_changed;
    bool muted;
};

struct priv {
    AudioDeviceID device;   // selected device
    bool is_digital;        // running in digital mode?

    AudioUnit audio_unit;   // AudioUnit for lpcm output

    bool paused;

    struct mp_ring *buffer;
    struct priv_d *digital;

    // options
    int opt_device_id;
    int opt_list;
};

static int get_ring_size(struct ao *ao)
{
    return af_fmt_seconds_to_bytes(
            ao->format, 0.5, ao->channels.num, ao->samplerate);
}

static OSStatus render_cb_lpcm(void *ctx, AudioUnitRenderActionFlags *aflags,
                              const AudioTimeStamp *ts, UInt32 bus,
                              UInt32 frames, AudioBufferList *buffer_list)
{
    struct ao *ao   = ctx;
    struct priv *p  = ao->priv;

    AudioBuffer buf = buffer_list->mBuffers[0];
    int requested   = buf.mDataByteSize;

    if (mp_ring_buffered(p->buffer) < requested) {
        MP_VERBOSE(ao, "buffer underrun\n");
        audio_pause(ao);
        memset(buf.mData, 0, requested);
    } else {
        mp_ring_read(p->buffer, buf.mData, requested);
    }

    return noErr;
}

static OSStatus render_cb_digital(
        AudioDeviceID device, const AudioTimeStamp *ts,
        const void *in_data, const AudioTimeStamp *in_ts,
        AudioBufferList *out_data, const AudioTimeStamp *out_ts, void *ctx)
{
    struct ao *ao    = ctx;
    struct priv *p   = ao->priv;
    struct priv_d *d = p->digital;
    AudioBuffer buf  = out_data->mBuffers[d->stream_idx];
    int requested    = buf.mDataByteSize;

    if (d->muted)
        mp_ring_drain(p->buffer, requested);
    else
        mp_ring_read(p->buffer, buf.mData, requested);

    return noErr;
}

static int control(struct ao *ao, enum aocontrol cmd, void *arg)
{
    struct priv *p = ao->priv;
    ao_control_vol_t *control_vol;
    OSStatus err;
    Float32 vol;
    switch (cmd) {
    case AOCONTROL_GET_VOLUME:
        control_vol = (ao_control_vol_t *)arg;
        if (p->is_digital) {
            struct priv_d *d = p->digital;
            // Digital output has no volume adjust.
            int digitalvol = d->muted ? 0 : 100;
            *control_vol = (ao_control_vol_t) {
                .left = digitalvol, .right = digitalvol,
            };
            return CONTROL_TRUE;
        }

        err = AudioUnitGetParameter(p->audio_unit, kHALOutputParam_Volume,
                                    kAudioUnitScope_Global, 0, &vol);

        CHECK_CA_ERROR("could not get HAL output volume");
        control_vol->left = control_vol->right = vol * 100.0;
        return CONTROL_TRUE;

    case AOCONTROL_SET_VOLUME:
        control_vol = (ao_control_vol_t *)arg;

        if (p->is_digital) {
            struct priv_d *d = p->digital;
            // Digital output can not set volume. Here we have to return true
            // to make mixer forget it. Else mixer will add a soft filter,
            // that's not we expected and the filter not support ac3 stream
            // will cause mplayer die.

            // Although not support set volume, but at least we support mute.
            // MPlayer set mute by set volume to zero, we handle it.
            if (control_vol->left == 0 && control_vol->right == 0)
                d->muted = true;
            else
                d->muted = false;
            return CONTROL_TRUE;
        }

        vol = (control_vol->left + control_vol->right) / 200.0;
        err = AudioUnitSetParameter(p->audio_unit, kHALOutputParam_Volume,
                                    kAudioUnitScope_Global, 0, vol, 0);

        CHECK_CA_ERROR("could not set HAL output volume");
        return CONTROL_TRUE;

    } // end switch
    return CONTROL_UNKNOWN;

coreaudio_error:
    return CONTROL_ERROR;
}

static void print_list(struct ao *ao)
{
    char *help = talloc_strdup(NULL, "Available output devices:\n");

    AudioDeviceID *devs;
    size_t n_devs;

    OSStatus err =
        CA_GET_ARY(kAudioObjectSystemObject, kAudioHardwarePropertyDevices,
                   &devs, &n_devs);

    CHECK_CA_ERROR("Failed to get list of output devices.");

    for (int i = 0; i < n_devs; i++) {
        char *name;
        err = CA_GET_STR(devs[i], kAudioObjectPropertyName, &name);

        if (err == noErr)
            talloc_steal(devs, name);
        else
            name = "Unknown";

        help = talloc_asprintf_append(
                help, "  * %s (id: %" PRIu32 ")\n", name, devs[i]);
    }

    talloc_free(devs);

coreaudio_error:
    MP_INFO(ao, "%s", help);
    talloc_free(help);
}

static int init_lpcm(struct ao *ao, AudioStreamBasicDescription asbd);
static int init_digital(struct ao *ao, AudioStreamBasicDescription asbd);

static int init(struct ao *ao)
{
    OSStatus err;
    struct priv *p   = ao->priv;

    if (p->opt_list) print_list(ao);

    struct priv_d *d = talloc_zero(p, struct priv_d);

    *d = (struct priv_d) {
        .muted = false,
        .stream_asbd_changed = 0,
        .hog_pid = -1,
        .stream = 0,
        .stream_idx = -1,
        .changed_mixing = false,
    };

    p->digital = d;

    ao->per_application_mixer = true;
    ao->no_persistent_volume  = true;

    AudioDeviceID selected_device = 0;
    if (p->opt_device_id < 0) {
        // device not set by user, get the default one
        err = CA_GET(kAudioObjectSystemObject,
                     kAudioHardwarePropertyDefaultOutputDevice,
                     &selected_device);
        CHECK_CA_ERROR("could not get default audio device");
    } else {
        selected_device = p->opt_device_id;
    }

    if (mp_msg_test(ao->log, MSGL_V)) {
        char *name;
        err = CA_GET_STR(selected_device, kAudioObjectPropertyName, &name);
        CHECK_CA_ERROR("could not get selected audio device name");

        MP_VERBOSE(ao, "selected audio output device: %s (%" PRIu32 ")\n",
                       name, selected_device);

        talloc_free(name);
    }

    // Save selected device id
    p->device = selected_device;

    ao->format = af_fmt_from_planar(ao->format);

    bool supports_digital = false;
    /* Probe whether device support S/PDIF stream output if input is AC3 stream. */
    if (AF_FORMAT_IS_AC3(ao->format)) {
        if (ca_device_supports_digital(ao, selected_device))
            supports_digital = true;
    }

    if (!supports_digital) {
        AudioChannelLayout *layouts;
        size_t n_layouts;
        err = CA_GET_ARY_O(selected_device,
                           kAudioDevicePropertyPreferredChannelLayout,
                           &layouts, &n_layouts);
        CHECK_CA_ERROR("could not get audio device prefered layouts");

        struct mp_chmap_sel chmap_sel = {0};
        for (int i = 0; i < n_layouts; i++) {
            struct mp_chmap chmap = {0};
            if (ca_layout_to_mp_chmap(ao, &layouts[i], &chmap))
                mp_chmap_sel_add_map(&chmap_sel, &chmap);
        }

        talloc_free(layouts);

        if (ao->channels.num < 3) {
            struct mp_chmap chmap;
            mp_chmap_from_channels(&chmap, ao->channels.num);
            mp_chmap_sel_add_map(&chmap_sel, &chmap);
        }

        if (!ao_chmap_sel_adjust(ao, &chmap_sel, &ao->channels)) {
            MP_ERR(ao, "could not select a suitable channel map among the "
                       "hardware supported ones. Make sure to configure your "
                       "output device correctly in 'Audio MIDI Setup.app'\n");
            goto coreaudio_error;
        }

    } // closes if (!supports_digital)

    // Build ASBD for the input format
    AudioStreamBasicDescription asbd;
    asbd.mSampleRate       = ao->samplerate;
    asbd.mFormatID         = supports_digital ?
                             kAudioFormat60958AC3 : kAudioFormatLinearPCM;
    asbd.mChannelsPerFrame = ao->channels.num;
    asbd.mBitsPerChannel   = af_fmt2bits(ao->format);
    asbd.mFormatFlags      = kAudioFormatFlagIsPacked;

    if ((ao->format & AF_FORMAT_POINT_MASK) == AF_FORMAT_F)
        asbd.mFormatFlags |= kAudioFormatFlagIsFloat;

    if ((ao->format & AF_FORMAT_SIGN_MASK) == AF_FORMAT_SI)
        asbd.mFormatFlags |= kAudioFormatFlagIsSignedInteger;

    if ((ao->format & AF_FORMAT_END_MASK) == AF_FORMAT_BE)
        asbd.mFormatFlags |= kAudioFormatFlagIsBigEndian;

    asbd.mFramesPerPacket = 1;
    asbd.mBytesPerPacket = asbd.mBytesPerFrame =
        asbd.mFramesPerPacket * asbd.mChannelsPerFrame *
        (asbd.mBitsPerChannel / 8);

    ca_print_asbd(ao, "source format:", &asbd);

    if (supports_digital)
        return init_digital(ao, asbd);
    else
        return init_lpcm(ao, asbd);

coreaudio_error:
    return CONTROL_ERROR;
}

static int init_lpcm(struct ao *ao, AudioStreamBasicDescription asbd)
{
    OSStatus err;
    uint32_t size;
    struct priv *p = ao->priv;

    AudioComponentDescription desc = (AudioComponentDescription) {
        .componentType         = kAudioUnitType_Output,
        .componentSubType      = (p->opt_device_id < 0) ?
                                    kAudioUnitSubType_DefaultOutput :
                                    kAudioUnitSubType_HALOutput,
        .componentManufacturer = kAudioUnitManufacturer_Apple,
        .componentFlags        = 0,
        .componentFlagsMask    = 0,
    };

    AudioComponent comp = AudioComponentFindNext(NULL, &desc);
    if (comp == NULL) {
        MP_ERR(ao, "unable to find audio component\n");
        goto coreaudio_error;
    }

    err = AudioComponentInstanceNew(comp, &(p->audio_unit));
    CHECK_CA_ERROR("unable to open audio component");

    // Initialize AudioUnit
    err = AudioUnitInitialize(p->audio_unit);
    CHECK_CA_ERROR_L(coreaudio_error_component,
                     "unable to initialize audio unit");

    size = sizeof(AudioStreamBasicDescription);
    err = AudioUnitSetProperty(p->audio_unit,
                               kAudioUnitProperty_StreamFormat,
                               kAudioUnitScope_Input, 0, &asbd, size);

    CHECK_CA_ERROR_L(coreaudio_error_audiounit,
                     "unable to set the input format on the audio unit");

    //Set the Current Device to the Default Output Unit.
    err = AudioUnitSetProperty(p->audio_unit,
                               kAudioOutputUnitProperty_CurrentDevice,
                               kAudioUnitScope_Global, 0, &p->device,
                               sizeof(p->device));
    CHECK_CA_ERROR_L(coreaudio_error_audiounit,
                     "can't link audio unit to selected device");

    p->buffer = mp_ring_new(p, get_ring_size(ao));
    print_buffer(ao, p->buffer);

    AURenderCallbackStruct render_cb = (AURenderCallbackStruct) {
        .inputProc       = render_cb_lpcm,
        .inputProcRefCon = ao,
    };

    err = AudioUnitSetProperty(p->audio_unit,
                               kAudioUnitProperty_SetRenderCallback,
                               kAudioUnitScope_Input, 0, &render_cb,
                               sizeof(AURenderCallbackStruct));

    CHECK_CA_ERROR_L(coreaudio_error_audiounit,
                     "unable to set render callback on audio unit");

    reset(ao);
    return CONTROL_OK;

coreaudio_error_audiounit:
    AudioUnitUninitialize(p->audio_unit);
coreaudio_error_component:
    AudioComponentInstanceDispose(p->audio_unit);
coreaudio_error:
    return CONTROL_ERROR;
}

static int init_digital(struct ao *ao, AudioStreamBasicDescription asbd)
{
    struct priv *p = ao->priv;
    struct priv_d *d = p->digital;
    OSStatus err = noErr;

    uint32_t is_alive = 1;
    err = CA_GET(p->device, kAudioDevicePropertyDeviceIsAlive, &is_alive);
    CHECK_CA_WARN("could not check whether device is alive");

    if (!is_alive)
        MP_WARN(ao , "device is not alive\n");

    p->is_digital = 1;

    err = ca_lock_device(p->device, &d->hog_pid);
    CHECK_CA_WARN("failed to set hogmode");

    err = ca_disable_mixing(ao, p->device, &d->changed_mixing);
    CHECK_CA_WARN("failed to disable mixing");

    AudioStreamID *streams;
    size_t n_streams;

    /* Get a list of all the streams on this device. */
    err = CA_GET_ARY_O(p->device, kAudioDevicePropertyStreams,
                       &streams, &n_streams);

    CHECK_CA_ERROR("could not get number of streams");

    for (int i = 0; i < n_streams && d->stream_idx < 0; i++) {
        bool digital = ca_stream_supports_digital(ao, streams[i]);

        if (digital) {
            err = CA_GET(streams[i], kAudioStreamPropertyPhysicalFormat,
                         &d->original_asbd);
            if (!CHECK_CA_WARN("could not get stream's physical format to "
                               "revert to, getting the next one"))
                continue;

            AudioStreamRangedDescription *formats;
            size_t n_formats;

            err = CA_GET_ARY(streams[i],
                             kAudioStreamPropertyAvailablePhysicalFormats,
                             &formats, &n_formats);

            if (!CHECK_CA_WARN("could not get number of stream formats"))
                continue; // try next one

            int req_rate_format = -1;
            int max_rate_format = -1;

            d->stream = streams[i];
            d->stream_idx = i;

            for (int j = 0; j < n_formats; j++)
                if (ca_format_is_digital(formats[j].mFormat)) {
                    // select the digital format that has exactly the same
                    // samplerate. If an exact match cannot be found, select
                    // the format with highest samplerate as backup.
                    if (formats[j].mFormat.mSampleRate == asbd.mSampleRate) {
                        req_rate_format = j;
                        break;
                    } else if (max_rate_format < 0 ||
                        formats[j].mFormat.mSampleRate >
                        formats[max_rate_format].mFormat.mSampleRate)
                        max_rate_format = j;
                }

            if (req_rate_format >= 0)
                d->stream_asbd = formats[req_rate_format].mFormat;
            else
                d->stream_asbd = formats[max_rate_format].mFormat;

            talloc_free(formats);
        }
    }

    talloc_free(streams);

    if (d->stream_idx < 0) {
        MP_WARN(ao , "can't find any digital output stream format\n");
        goto coreaudio_error;
    }

    if (!ca_change_format(ao, d->stream, d->stream_asbd))
        goto coreaudio_error;

    void *changed = (void *) &(d->stream_asbd_changed);
    err = ca_enable_device_listener(p->device, changed);
    CHECK_CA_ERROR("cannot install format change listener during init");

#if BYTE_ORDER == BIG_ENDIAN
    if (!(p->stream_asdb.mFormatFlags & kAudioFormatFlagIsBigEndian))
#else
    /* tell mplayer that we need a byteswap on AC3 streams, */
    if (d->stream_asbd.mFormatID & kAudioFormat60958AC3)
        ao->format = AF_FORMAT_AC3_LE;
    else if (d->stream_asbd.mFormatFlags & kAudioFormatFlagIsBigEndian)
#endif
        MP_WARN(ao, "stream has non-native byte order, output may fail\n");

    ao->samplerate = d->stream_asbd.mSampleRate;
    ao->bps = ao->samplerate *
                  (d->stream_asbd.mBytesPerPacket /
                   d->stream_asbd.mFramesPerPacket);

    p->buffer = mp_ring_new(p, get_ring_size(ao));
    print_buffer(ao, p->buffer);

    err = AudioDeviceCreateIOProcID(p->device,
                                    (AudioDeviceIOProc)render_cb_digital,
                                    (void *)ao,
                                    &d->render_cb);

    CHECK_CA_ERROR("failed to register digital render callback");

    reset(ao);

    return CONTROL_TRUE;

coreaudio_error:
    err = ca_unlock_device(p->device, &d->hog_pid);
    CHECK_CA_WARN("can't release hog mode");
    return CONTROL_ERROR;
}

static int play(struct ao *ao, void **data, int samples, int flags)
{
    struct priv *p   = ao->priv;
    struct priv_d *d = p->digital;
    void *output_samples = data[0];
    int num_bytes = samples * ao->sstride;

    // Check whether we need to reset the digital output stream.
    if (p->is_digital && d->stream_asbd_changed) {
        d->stream_asbd_changed = 0;
        if (ca_stream_supports_digital(ao, d->stream)) {
            if (!ca_change_format(ao, d->stream, d->stream_asbd)) {
                MP_WARN(ao , "can't restore digital output\n");
            } else {
                MP_WARN(ao, "restoring digital output succeeded.\n");
                reset(ao);
            }
        }
    }

    int wrote = mp_ring_write(p->buffer, output_samples, num_bytes);
    audio_resume(ao);

    return wrote / ao->sstride;
}

static void reset(struct ao *ao)
{
    struct priv *p = ao->priv;
    audio_pause(ao);
    mp_ring_reset(p->buffer);
}

static int get_space(struct ao *ao)
{
    struct priv *p = ao->priv;
    return mp_ring_available(p->buffer) / ao->sstride;
}

static float get_delay(struct ao *ao)
{
    // FIXME: should also report the delay of coreaudio itself (hardware +
    // internal buffers)
    struct priv *p = ao->priv;
    return mp_ring_buffered(p->buffer) / (float)ao->bps;
}

static void uninit(struct ao *ao)
{
    struct priv *p = ao->priv;
    OSStatus err = noErr;

    if (!p->is_digital) {
        AudioOutputUnitStop(p->audio_unit);
        AudioUnitUninitialize(p->audio_unit);
        AudioComponentInstanceDispose(p->audio_unit);
    } else {
        struct priv_d *d = p->digital;

        void *changed = (void *) &(d->stream_asbd_changed);
        err = ca_disable_device_listener(p->device, changed);
        CHECK_CA_WARN("can't remove device listener, this may cause a crash");

        err = AudioDeviceStop(p->device, d->render_cb);
        CHECK_CA_WARN("failed to stop audio device");

        err = AudioDeviceDestroyIOProcID(p->device, d->render_cb);
        CHECK_CA_WARN("failed to remove device render callback");

        if (!ca_change_format(ao, d->stream, d->original_asbd))
            MP_WARN(ao, "can't revert to original device format");

        err = ca_enable_mixing(ao, p->device, d->changed_mixing);
        CHECK_CA_WARN("can't re-enable mixing");

        err = ca_unlock_device(p->device, &d->hog_pid);
        CHECK_CA_WARN("can't release hog mode");
    }
}

static void audio_pause(struct ao *ao)
{
    struct priv *p = ao->priv;
    OSErr err = noErr;

    if (p->paused)
        return;

    if (!p->is_digital) {
        err = AudioOutputUnitStop(p->audio_unit);
        CHECK_CA_WARN("can't stop audio unit");
    } else {
        struct priv_d *d = p->digital;
        err = AudioDeviceStop(p->device, d->render_cb);
        CHECK_CA_WARN("can't stop digital device");
    }

    p->paused = true;
}

static void audio_resume(struct ao *ao)
{
    struct priv *p = ao->priv;
    OSErr err = noErr;

    if (!p->paused)
        return;

    if (!p->is_digital) {
        err = AudioOutputUnitStart(p->audio_unit);
        CHECK_CA_WARN("can't start audio unit");
    } else {
        struct priv_d *d = p->digital;
        err = AudioDeviceStart(p->device, d->render_cb);
        CHECK_CA_WARN("can't start digital device");
    }

    p->paused = false;
}

#define OPT_BASE_STRUCT struct priv

const struct ao_driver audio_out_coreaudio = {
    .description = "CoreAudio (OS X Audio Output)",
    .name      = "coreaudio",
    .uninit    = uninit,
    .init      = init,
    .play      = play,
    .control   = control,
    .get_space = get_space,
    .get_delay = get_delay,
    .reset     = reset,
    .pause     = audio_pause,
    .resume    = audio_resume,
    .priv_size = sizeof(struct priv),
    .options = (const struct m_option[]) {
        OPT_INT("device_id", opt_device_id, 0, OPTDEF_INT(-1)),
        OPT_FLAG("list", opt_list, 0),
        {0}
    },
};
