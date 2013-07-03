/*
 * CoreAudio audio output driver for Mac OS X
 *
 * original copyright (C) Timothy J. Wood - Aug 2000
 * ported to MPlayer libao2 by Dan Christiansen
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

#include "audio/out/ao_coreaudio_common.c"

#include "ao.h"
#include "audio/format.h"
#include "osdep/timer.h"
#include "core/subopt-helper.h"
#include "core/mp_ring.h"

static void audio_pause(struct ao *ao);
static void audio_resume(struct ao *ao);
static void reset(struct ao *ao);

static void print_buffer(struct mp_ring *buffer)
{
    void *tctx = talloc_new(NULL);
    ca_msg(MSGL_V, "%s\n", mp_ring_repr(buffer, tctx));
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

    buf.mDataByteSize = mp_ring_read(p->buffer, buf.mData, requested);

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
            int vol = d->muted ? 0 : 100;
            *control_vol = (ao_control_vol_t) {
                .left = vol, .right = vol,
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

static int AudioStreamChangeFormat(AudioStreamID stream,
                                   AudioStreamBasicDescription change_format);

static void print_help(void)
{
    char *help = talloc_strdup(NULL,
           " -ao coreaudio commandline help:\n"
           "Example: mpv -ao coreaudio:device_id=266\n"
           "    open Core Audio with output device ID 266.\n"
           "\nOptions:\n"
           "    device_id\n"
           "        ID of output device to use (0 = default device)\n"
           "    help\n"
           "        This help including list of available devices.\n"
           "\n"
           "Available output devices:\n");

    AudioDeviceID *devs;
    uint32_t devs_size =
        GetGlobalAudioPropertyArray(kAudioObjectSystemObject,
                                    kAudioHardwarePropertyDevices,
                                    (void **)&devs);
    if (!devs_size) {
        ca_msg(MSGL_FATAL, "Failed to get list of output devices.\n");
        goto coreaudio_out;
    }

    int devs_n = devs_size / sizeof(AudioDeviceID);

    for (int i = 0; i < devs_n; i++) {
        char *name;
        OSStatus err =
            GetAudioPropertyString(devs[i], kAudioObjectPropertyName, &name);

        if (err == noErr) {
            help = talloc_asprintf_append(help,
                    "%s (id: %" PRIu32 ")\n", name, devs[i]);
            free(name);
        } else
            help = talloc_asprintf_append(help,
                    "Unknown (id: %" PRIu32 ")\n", devs[i]);
    }

    free(devs);

coreaudio_out:
    ca_msg(MSGL_FATAL, "%s", help);
    talloc_free(help);
}

static int init_lpcm(struct ao *ao, AudioStreamBasicDescription asbd);
static int init_digital(struct ao *ao, AudioStreamBasicDescription asbd);

static int init(struct ao *ao, char *params)
{
    OSStatus err;
    int device_opt = -1, help_opt = 0;

    const opt_t subopts[] = {
        {"device_id", OPT_ARG_INT, &device_opt, NULL},
        {"help", OPT_ARG_BOOL, &help_opt, NULL},
        {NULL}
    };

    if (subopt_parse(params, subopts) != 0) {
        print_help();
        return 0;
    }

    if (help_opt)
        print_help();

    struct priv *p = talloc_zero(ao, struct priv);
    *p = (struct priv) {
        .device = 0,
        .is_digital = 0,
    };

    struct priv_d *d= talloc_zero(p, struct priv_d);
    *d = (struct priv_d) {
        .muted = false,
        .stream_asbd_changed = 0,
        .hog_pid = -1,
        .stream = 0,
        .stream_idx = -1,
        .changed_mixing = false,
    };

    p->digital = d;
    ao->priv   = p;

    ao->per_application_mixer = true;
    ao->no_persistent_volume  = true;

    AudioDeviceID selected_device = 0;
    if (device_opt < 0) {
        // device not set by user, get the default one
        err = GetAudioProperty(kAudioObjectSystemObject,
                               kAudioHardwarePropertyDefaultOutputDevice,
                               sizeof(uint32_t), &selected_device);
        CHECK_CA_ERROR("could not get default audio device");
    } else {
        selected_device = device_opt;
    }

    char *device_name;
    err = GetAudioPropertyString(selected_device,
                                 kAudioObjectPropertyName,
                                 &device_name);

    CHECK_CA_ERROR("could not get selected audio device name");

    ca_msg(MSGL_V,
           "selected audio output device: %s (%" PRIu32 ")\n",
           device_name, selected_device);

    free(device_name);

    // Save selected device id
    p->device = selected_device;

    struct mp_chmap_sel chmap_sel = {0};
    mp_chmap_sel_add_waveext(&chmap_sel);
    if (!ao_chmap_sel_adjust(ao, &chmap_sel, &ao->channels))
        goto coreaudio_error;

    bool supports_digital = false;
    /* Probe whether device support S/PDIF stream output if input is AC3 stream. */
    if (AF_FORMAT_IS_AC3(ao->format)) {
        if (AudioDeviceSupportsDigital(selected_device))
            supports_digital = true;
    }

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

    ca_print_asbd("source format:", &asbd);

    if (supports_digital)
        return init_digital(ao, asbd);
    else
        return init_lpcm(ao, asbd);

coreaudio_error:
    return CONTROL_FALSE;
}

static int init_lpcm(struct ao *ao, AudioStreamBasicDescription asbd)
{
    OSStatus err;
    uint32_t size;
    struct priv *p = ao->priv;

    AudioComponentDescription desc = (AudioComponentDescription) {
        .componentType         = kAudioUnitType_Output,
        .componentSubType      = kAudioUnitSubType_HALOutput,
        .componentManufacturer = kAudioUnitManufacturer_Apple,
        .componentFlags        = 0,
        .componentFlagsMask    = 0,
    };

    AudioComponent comp = AudioComponentFindNext(NULL, &desc);
    if (comp == NULL) {
        ca_msg(MSGL_ERR, "unable to find audio component\n");
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

    if (ao->channels.num > 2) {
        // No need to set a channel layout for mono and stereo inputs
        AudioChannelLayout acl = (AudioChannelLayout) {
            .mChannelLayoutTag = kAudioChannelLayoutTag_UseChannelBitmap,
            .mChannelBitmap    = mp_chmap_to_waveext(&ao->channels)
        };

        err = AudioUnitSetProperty(p->audio_unit,
                                   kAudioUnitProperty_AudioChannelLayout,
                                   kAudioUnitScope_Input, 0, &acl,
                                   sizeof(AudioChannelLayout));
        CHECK_CA_ERROR_L(coreaudio_error_audiounit,
                         "can't set channel layout bitmap into audio unit");
    }

    p->buffer = mp_ring_new(p, get_ring_size(ao));
    print_buffer(p->buffer);

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
    return CONTROL_FALSE;
}

static int init_digital(struct ao *ao, AudioStreamBasicDescription asbd)
{
    struct priv *p = ao->priv;
    struct priv_d *d = p->digital;
    OSStatus err = noErr;
    uint32_t size;

    uint32_t is_alive = 1;
    err = GetAudioProperty(p->device,
                               kAudioDevicePropertyDeviceIsAlive,
                               sizeof(uint32_t), &is_alive);

    CHECK_CA_WARN( "could not check whether device is alive");

    if (!is_alive)
        ca_msg(MSGL_WARN, "device is not alive\n");

    d->stream_asbd = asbd;

    p->is_digital = 1;

    err = ca_lock_device(p->device, &d->hog_pid);
    CHECK_CA_WARN("failed to set hogmode");

    err = ca_disable_mixing(p->device, &d->changed_mixing);
    CHECK_CA_WARN("failed to disable mixing");

    AudioStreamID *streams = NULL;
    /* Get a list of all the streams on this device. */
    size = GetAudioPropertyArray(p->device,
                                 kAudioDevicePropertyStreams,
                                 kAudioDevicePropertyScopeOutput,
                                 (void **)&streams);

    if (!size) {
        ca_msg(MSGL_WARN, "could not get number of streams\n");
        goto coreaudio_error;
    }

    int streams_n = size / sizeof(AudioStreamID);

    for (int i = 0; i < streams_n && d->stream_idx < 0; i++) {
        bool digital = AudioStreamSupportsDigital(streams[i]);

        if (digital) {
            /* Find a stream with a cac3 stream. */
            AudioStreamRangedDescription *formats = NULL;
            size = GetGlobalAudioPropertyArray(streams[i],
                                               kAudioStreamPropertyAvailablePhysicalFormats,
                                               (void **)&formats);

            if (!size) {
                ca_msg(MSGL_WARN, "could not get number of stream formats\n");
                continue; // try next one
            }

            int formats_n = size / sizeof(AudioStreamRangedDescription);
            /* If this stream supports a digital (cac3) format, then set it. */
            int req_rate_format = -1;
            int max_rate_format = -1;

            d->stream = streams[i];
            d->stream_idx = i;

            for (int j = 0; j < formats_n; j++)
                if (AudioFormatIsDigital(asbd)) {
                    // select the digital format that has exactly the same
                    // samplerate. If an exact match cannot be found, select
                    // the format with highest samplerate as backup.
                    if (formats[j].mFormat.mSampleRate ==
                        d->stream_asbd.mSampleRate) {
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

            free(formats);
        }
    }

    free(streams);

    if (d->stream_idx < 0) {
        ca_msg(MSGL_WARN, "can't find any digital output stream format\n");
        goto coreaudio_error;
    }

    if (!AudioStreamChangeFormat(d->stream, d->stream_asbd))
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
        ca_msg(MSGL_WARN,
               "stream has non-native byte order, digital output may fail\n");
#endif

    ao->samplerate = d->stream_asbd.mSampleRate;
    ao->bps = ao->samplerate *
                  (d->stream_asbd.mBytesPerPacket /
                   d->stream_asbd.mFramesPerPacket);

    p->buffer = mp_ring_new(p, get_ring_size(ao));
    print_buffer(p->buffer);

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
    return CONTROL_FALSE;
}

static int play(struct ao *ao, void *output_samples, int num_bytes, int flags)
{
    struct priv *p   = ao->priv;
    struct priv_d *d = p->digital;

    // Check whether we need to reset the digital output stream.
    if (p->is_digital && d->stream_asbd_changed) {
        d->stream_asbd_changed = 0;
        if (AudioStreamSupportsDigital(d->stream)) {
            if (!AudioStreamChangeFormat(d->stream, d->stream_asbd)) {
                ca_msg(MSGL_WARN, "can't restore digital output\n");
            } else {
                ca_msg(MSGL_WARN, "restoring digital output succeeded.\n");
                reset(ao);
            }
        }
    }

    int wrote = mp_ring_write(p->buffer, output_samples, num_bytes);
    audio_resume(ao);

    return wrote;
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
    return mp_ring_available(p->buffer);
}

static float get_delay(struct ao *ao)
{
    // FIXME: should also report the delay of coreaudio itself (hardware +
    // internal buffers)
    struct priv *p = ao->priv;
    return mp_ring_buffered(p->buffer) / (float)ao->bps;
}

static void uninit(struct ao *ao, bool immed)
{
    struct priv *p = ao->priv;
    OSStatus err = noErr;

    if (!immed)
        mp_sleep_us(get_delay(ao) * 1000000);

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

        err = ca_enable_mixing(p->device, d->changed_mixing);
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

const struct ao_driver audio_out_coreaudio = {
    .info = &(const struct ao_info) {
        "CoreAudio (OS X Audio Output)",
        "coreaudio",
        "Timothy J. Wood, Dan Christiansen, Chris Roccati & Stefano Pigozzi",
        "",
    },
    .uninit    = uninit,
    .init      = init,
    .play      = play,
    .control   = control,
    .get_space = get_space,
    .get_delay = get_delay,
    .reset     = reset,
    .pause     = audio_pause,
    .resume    = audio_resume,
};
