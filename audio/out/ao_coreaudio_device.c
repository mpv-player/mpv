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

static bool ca_format_is_digital(AudioStreamBasicDescription asbd)
{
    switch (asbd.mFormatID)
    case 'IAC3':
    case 'iac3':
    case  kAudioFormat60958AC3:
    case  kAudioFormatAC3:
        return true;
    return false;
}

static bool ca_stream_supports_digital(struct ao *ao, AudioStreamID stream)
{
    AudioStreamRangedDescription *formats = NULL;
    size_t n_formats;

    OSStatus err =
        CA_GET_ARY(stream, kAudioStreamPropertyAvailablePhysicalFormats,
                   &formats, &n_formats);

    CHECK_CA_ERROR("Could not get number of stream formats.");

    for (int i = 0; i < n_formats; i++) {
        AudioStreamBasicDescription asbd = formats[i].mFormat;
        ca_print_asbd(ao, "supported format:", &(asbd));
        if (ca_format_is_digital(asbd)) {
            talloc_free(formats);
            return true;
        }
    }

    talloc_free(formats);
coreaudio_error:
    return false;
}

static bool ca_device_supports_digital(struct ao *ao, AudioDeviceID device)
{
    AudioStreamID *streams = NULL;
    size_t n_streams;

    /* Retrieve all the output streams. */
    OSStatus err =
        CA_GET_ARY_O(device, kAudioDevicePropertyStreams, &streams, &n_streams);

    CHECK_CA_ERROR("could not get number of streams.");

    for (int i = 0; i < n_streams; i++) {
        if (ca_stream_supports_digital(ao, streams[i])) {
            talloc_free(streams);
            return true;
        }
    }

    talloc_free(streams);

coreaudio_error:
    return false;
}

static OSStatus ca_property_listener(
    AudioObjectPropertySelector selector,
    AudioObjectID object, uint32_t n_addresses,
    const AudioObjectPropertyAddress addresses[],
    void *data)
{
    void *talloc_ctx = talloc_new(NULL);

    for (int i = 0; i < n_addresses; i++) {
        if (addresses[i].mSelector == selector) {
            if (data) *(volatile int *)data = 1;
            break;
        }
    }
    talloc_free(talloc_ctx);
    return noErr;
}

static OSStatus ca_stream_listener(
    AudioObjectID object, uint32_t n_addresses,
    const AudioObjectPropertyAddress addresses[],
    void *data)
{
    return ca_property_listener(kAudioStreamPropertyPhysicalFormat,
                                object, n_addresses, addresses, data);
}

static OSStatus ca_device_listener(
    AudioObjectID object, uint32_t n_addresses,
    const AudioObjectPropertyAddress addresses[],
    void *data)
{
    return ca_property_listener(kAudioDevicePropertyDeviceHasChanged,
                                object, n_addresses, addresses, data);
}

static OSStatus ca_lock_device(AudioDeviceID device, pid_t *pid) {
    *pid = getpid();
    OSStatus err = CA_SET(device, kAudioDevicePropertyHogMode, pid);
    if (err != noErr)
        *pid = -1;

    return err;
}

static OSStatus ca_unlock_device(AudioDeviceID device, pid_t *pid) {
    if (*pid == getpid()) {
        *pid = -1;
        return CA_SET(device, kAudioDevicePropertyHogMode, &pid);
    }
    return noErr;
}

static OSStatus ca_change_mixing(struct ao *ao, AudioDeviceID device,
                                 uint32_t val, bool *changed) {
    *changed = false;

    AudioObjectPropertyAddress p_addr = (AudioObjectPropertyAddress) {
        .mSelector = kAudioDevicePropertySupportsMixing,
        .mScope    = kAudioObjectPropertyScopeGlobal,
        .mElement  = kAudioObjectPropertyElementMaster,
    };

    if (AudioObjectHasProperty(device, &p_addr)) {
        OSStatus err;
        Boolean writeable = 0;
        err = CA_SETTABLE(device, kAudioDevicePropertySupportsMixing,
                          &writeable);

        if (!CHECK_CA_WARN("can't tell if mixing property is settable")) {
            return err;
        }

        if (!writeable)
            return noErr;

        err = CA_SET(device, kAudioDevicePropertySupportsMixing, &val);
        if (err != noErr)
            return err;

        if (!CHECK_CA_WARN("can't set mix mode")) {
            return err;
        }

        *changed = true;
    }

    return noErr;
}

static OSStatus ca_disable_mixing(struct ao *ao,
                                  AudioDeviceID device, bool *changed) {
    return ca_change_mixing(ao, device, 0, changed);
}

static OSStatus ca_enable_mixing(struct ao *ao,
                                 AudioDeviceID device, bool changed) {
    if (changed) {
        bool dont_care = false;
        return ca_change_mixing(ao, device, 1, &dont_care);
    }

    return noErr;
}

static OSStatus ca_change_device_listening(AudioDeviceID device,
                                           void *flag, bool enabled)
{
    AudioObjectPropertyAddress p_addr = (AudioObjectPropertyAddress) {
        .mSelector = kAudioDevicePropertyDeviceHasChanged,
        .mScope    = kAudioObjectPropertyScopeGlobal,
        .mElement  = kAudioObjectPropertyElementMaster,
    };

    if (enabled) {
        return AudioObjectAddPropertyListener(
            device, &p_addr, ca_device_listener, flag);
    } else {
        return AudioObjectRemovePropertyListener(
            device, &p_addr, ca_device_listener, flag);
    }
}

static OSStatus ca_enable_device_listener(AudioDeviceID device, void *flag) {
    return ca_change_device_listening(device, flag, true);
}

static OSStatus ca_disable_device_listener(AudioDeviceID device, void *flag) {
    return ca_change_device_listening(device, flag, false);
}

static bool ca_change_format(struct ao *ao, AudioStreamID stream,
                             AudioStreamBasicDescription change_format)
{
    OSStatus err = noErr;
    AudioObjectPropertyAddress p_addr;
    volatile int stream_format_changed = 0;

    ca_print_asbd(ao, "setting stream format:", &change_format);

    /* Install the callback. */
    p_addr = (AudioObjectPropertyAddress) {
        .mSelector = kAudioStreamPropertyPhysicalFormat,
        .mScope    = kAudioObjectPropertyScopeGlobal,
        .mElement  = kAudioObjectPropertyElementMaster,
    };

    err = AudioObjectAddPropertyListener(stream, &p_addr, ca_stream_listener,
                                         (void *)&stream_format_changed);
    if (!CHECK_CA_WARN("can't add property listener during format change")) {
        return false;
    }

    /* Change the format. */
    err = CA_SET(stream, kAudioStreamPropertyPhysicalFormat, &change_format);
    if (!CHECK_CA_WARN("error changing physical format")) {
        return false;
    }

    /* The AudioStreamSetProperty is not only asynchronious,
     * it is also not Atomic, in its behaviour.
     * Therefore we check 5 times before we really give up. */
    bool format_set = false;
    for (int i = 0; !format_set && i < 5; i++) {
        for (int j = 0; !stream_format_changed && j < 50; j++)
            mp_sleep_us(10000);

        if (stream_format_changed) {
            stream_format_changed = 0;
        } else {
            MP_VERBOSE(ao, "reached timeout\n");
        }

        AudioStreamBasicDescription actual_format;
        err = CA_GET(stream, kAudioStreamPropertyPhysicalFormat, &actual_format);

        ca_print_asbd(ao, "actual format in use:", &actual_format);
        if (actual_format.mSampleRate == change_format.mSampleRate &&
            actual_format.mFormatID == change_format.mFormatID &&
            actual_format.mFramesPerPacket == change_format.mFramesPerPacket) {
            format_set = true;
        }
    }

    err = AudioObjectRemovePropertyListener(stream, &p_addr, ca_stream_listener,
                                            (void *)&stream_format_changed);

    if (!CHECK_CA_WARN("can't remove property listener")) {
        return false;
    }

    return format_set;
}


struct priv {
    AudioDeviceID device;   // selected device

    bool paused;

    struct mp_ring *buffer;

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

    // options
    int opt_device_id;
    int opt_list;
};

static int get_ring_size(struct ao *ao)
{
    return af_fmt_seconds_to_bytes(
            ao->format, 0.5, ao->channels.num, ao->samplerate);
}

static OSStatus render_cb_digital(
        AudioDeviceID device, const AudioTimeStamp *ts,
        const void *in_data, const AudioTimeStamp *in_ts,
        AudioBufferList *out_data, const AudioTimeStamp *out_ts, void *ctx)
{
    struct ao *ao    = ctx;
    struct priv *p   = ao->priv;
    AudioBuffer buf  = out_data->mBuffers[p->stream_idx];
    int requested    = buf.mDataByteSize;

    if (p->muted)
        mp_ring_drain(p->buffer, requested);
    else
        mp_ring_read(p->buffer, buf.mData, requested);

    return noErr;
}

static int control(struct ao *ao, enum aocontrol cmd, void *arg)
{
    struct priv *p = ao->priv;
    ao_control_vol_t *control_vol;
    switch (cmd) {
    case AOCONTROL_GET_VOLUME:
        control_vol = (ao_control_vol_t *)arg;
        // Digital output has no volume adjust.
        int digitalvol = p->muted ? 0 : 100;
        *control_vol = (ao_control_vol_t) {
            .left = digitalvol, .right = digitalvol,
        };
        return CONTROL_TRUE;

    case AOCONTROL_SET_VOLUME:
        control_vol = (ao_control_vol_t *)arg;
        // Digital output can not set volume. Here we have to return true
        // to make mixer forget it. Else mixer will add a soft filter,
        // that's not we expected and the filter not support ac3 stream
        // will cause mplayer die.

        // Although not support set volume, but at least we support mute.
        // MPlayer set mute by set volume to zero, we handle it.
        if (control_vol->left == 0 && control_vol->right == 0)
            p->muted = true;
        else
            p->muted = false;
        return CONTROL_TRUE;

    } // end switch
    return CONTROL_UNKNOWN;
}

static int init_digital(struct ao *ao, AudioStreamBasicDescription asbd);

static int init(struct ao *ao)
{
    struct priv *p = ao->priv;

    if (p->opt_list) ca_print_device_list(ao);

    *p = (struct priv) {
        .muted = false,
        .stream_asbd_changed = 0,
        .hog_pid = -1,
        .stream = 0,
        .stream_idx = -1,
        .changed_mixing = false,
    };

    OSStatus err = ca_select_device(ao, p->opt_device_id, &p->device);
    CHECK_CA_ERROR("failed to select device");

    ao->format = af_fmt_from_planar(ao->format);

    bool supports_digital = false;
    /* Probe whether device support S/PDIF stream output if input is AC3 stream. */
    if (AF_FORMAT_IS_AC3(ao->format)) {
        if (ca_device_supports_digital(ao, p->device))
            supports_digital = true;
    }

    if (!supports_digital) {
        MP_ERR(ao, "selected device doesn't support digital formats\n");
        goto coreaudio_error;
    } // closes if (!supports_digital)

    // Build ASBD for the input format
    AudioStreamBasicDescription asbd;
    ca_fill_asbd(ao, &asbd);

    return init_digital(ao, asbd);

coreaudio_error:
    return CONTROL_ERROR;
}

static int init_digital(struct ao *ao, AudioStreamBasicDescription asbd)
{
    struct priv *p = ao->priv;
    OSStatus err = noErr;

    uint32_t is_alive = 1;
    err = CA_GET(p->device, kAudioDevicePropertyDeviceIsAlive, &is_alive);
    CHECK_CA_WARN("could not check whether device is alive");

    if (!is_alive)
        MP_WARN(ao , "device is not alive\n");

    err = ca_lock_device(p->device, &p->hog_pid);
    CHECK_CA_WARN("failed to set hogmode");

    err = ca_disable_mixing(ao, p->device, &p->changed_mixing);
    CHECK_CA_WARN("failed to disable mixing");

    AudioStreamID *streams;
    size_t n_streams;

    /* Get a list of all the streams on this device. */
    err = CA_GET_ARY_O(p->device, kAudioDevicePropertyStreams,
                       &streams, &n_streams);

    CHECK_CA_ERROR("could not get number of streams");

    for (int i = 0; i < n_streams && p->stream_idx < 0; i++) {
        bool digital = ca_stream_supports_digital(ao, streams[i]);

        if (digital) {
            err = CA_GET(streams[i], kAudioStreamPropertyPhysicalFormat,
                         &p->original_asbd);
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

            p->stream = streams[i];
            p->stream_idx = i;

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
                p->stream_asbd = formats[req_rate_format].mFormat;
            else
                p->stream_asbd = formats[max_rate_format].mFormat;

            talloc_free(formats);
        }
    }

    talloc_free(streams);

    if (p->stream_idx < 0) {
        MP_WARN(ao , "can't find any digital output stream format\n");
        goto coreaudio_error;
    }

    if (!ca_change_format(ao, p->stream, p->stream_asbd))
        goto coreaudio_error;

    void *changed = (void *) &(p->stream_asbd_changed);
    err = ca_enable_device_listener(p->device, changed);
    CHECK_CA_ERROR("cannot install format change listener during init");

#if BYTE_ORDER == BIG_ENDIAN
    if (!(p->stream_asdb.mFormatFlags & kAudioFormatFlagIsBigEndian))
#else
    /* tell mplayer that we need a byteswap on AC3 streams, */
    if (p->stream_asbd.mFormatID & kAudioFormat60958AC3)
        ao->format = AF_FORMAT_AC3_LE;
    else if (p->stream_asbd.mFormatFlags & kAudioFormatFlagIsBigEndian)
#endif
        MP_WARN(ao, "stream has non-native byte order, output may fail\n");

    ao->samplerate = p->stream_asbd.mSampleRate;
    ao->bps = ao->samplerate *
                  (p->stream_asbd.mBytesPerPacket /
                   p->stream_asbd.mFramesPerPacket);

    p->buffer = mp_ring_new(p, get_ring_size(ao));

    err = AudioDeviceCreateIOProcID(p->device,
                                    (AudioDeviceIOProc)render_cb_digital,
                                    (void *)ao,
                                    &p->render_cb);

    CHECK_CA_ERROR("failed to register digital render callback");

    reset(ao);

    return CONTROL_TRUE;

coreaudio_error:
    err = ca_unlock_device(p->device, &p->hog_pid);
    CHECK_CA_WARN("can't release hog mode");
    return CONTROL_ERROR;
}

static int play(struct ao *ao, void **data, int samples, int flags)
{
    struct priv *p   = ao->priv;
    void *output_samples = data[0];
    int num_bytes = samples * ao->sstride;

    // Check whether we need to reset the digital output stream.
    if (p->stream_asbd_changed) {
        p->stream_asbd_changed = 0;
        if (ca_stream_supports_digital(ao, p->stream)) {
            if (!ca_change_format(ao, p->stream, p->stream_asbd)) {
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

    void *changed = (void *) &(p->stream_asbd_changed);
    err = ca_disable_device_listener(p->device, changed);
    CHECK_CA_WARN("can't remove device listener, this may cause a crash");

    err = AudioDeviceStop(p->device, p->render_cb);
    CHECK_CA_WARN("failed to stop audio device");

    err = AudioDeviceDestroyIOProcID(p->device, p->render_cb);
    CHECK_CA_WARN("failed to remove device render callback");

    if (!ca_change_format(ao, p->stream, p->original_asbd))
        MP_WARN(ao, "can't revert to original device format");

    err = ca_enable_mixing(ao, p->device, p->changed_mixing);
    CHECK_CA_WARN("can't re-enable mixing");

    err = ca_unlock_device(p->device, &p->hog_pid);
    CHECK_CA_WARN("can't release hog mode");
}

static void audio_pause(struct ao *ao)
{
    struct priv *p = ao->priv;

    if (p->paused)
        return;

    OSStatus err = AudioDeviceStop(p->device, p->render_cb);
    CHECK_CA_WARN("can't stop digital device");

    p->paused = true;
}

static void audio_resume(struct ao *ao)
{
    struct priv *p = ao->priv;

    if (!p->paused)
        return;

    OSStatus err = AudioDeviceStart(p->device, p->render_cb);
    CHECK_CA_WARN("can't start digital device");

    p->paused = false;
}

#define OPT_BASE_STRUCT struct priv

const struct ao_driver audio_out_coreaudio_exclusive = {
    .description = "CoreAudio Exclusive Mode",
    .name      = "coreaudio_exclusive",
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
