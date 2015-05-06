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
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * The MacOS X CoreAudio framework doesn't mesh as simply as some
 * simpler frameworks do.  This is due to the fact that CoreAudio pulls
 * audio samples rather than having them pushed at it (which is nice
 * when you are wanting to do good buffering of audio).
 */

#include <CoreAudio/HostTime.h>

#include "config.h"
#include "ao.h"
#include "internal.h"
#include "audio/format.h"
#include "osdep/timer.h"
#include "osdep/atomics.h"
#include "options/m_option.h"
#include "common/msg.h"
#include "audio/out/ao_coreaudio_properties.h"
#include "audio/out/ao_coreaudio_utils.h"

static bool ca_format_is_compressed(AudioStreamBasicDescription asbd)
{
    switch (asbd.mFormatID)
    case 'IAC3':
    case 'iac3':
    case  kAudioFormat60958AC3:
    case  kAudioFormatAC3:
        return true;
    return false;
}

static bool ca_stream_supports_compressed(struct ao *ao, AudioStreamID stream)
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
        if (ca_format_is_compressed(asbd)) {
            talloc_free(formats);
            return true;
        }
    }

    talloc_free(formats);
coreaudio_error:
    return false;
}

static bool ca_device_supports_compressed(struct ao *ao, AudioDeviceID device)
{
    AudioStreamID *streams = NULL;
    size_t n_streams;

    /* Retrieve all the output streams. */
    OSStatus err =
        CA_GET_ARY_O(device, kAudioDevicePropertyStreams, &streams, &n_streams);

    CHECK_CA_ERROR("could not get number of streams.");

    for (int i = 0; i < n_streams; i++) {
        if (ca_stream_supports_compressed(ao, streams[i])) {
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
    for (int i = 0; i < n_addresses; i++) {
        if (addresses[i].mSelector == selector) {
            if (data)
                atomic_store((atomic_bool *)data, true);
            break;
        }
    }
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
    atomic_bool stream_format_changed = ATOMIC_VAR_INIT(false);

    ca_print_asbd(ao, "setting stream format:", &change_format);

    /* Install the callback. */
    p_addr = (AudioObjectPropertyAddress) {
        .mSelector = kAudioStreamPropertyPhysicalFormat,
        .mScope    = kAudioObjectPropertyScopeGlobal,
        .mElement  = kAudioObjectPropertyElementMaster,
    };

    err = AudioObjectAddPropertyListener(stream, &p_addr, ca_stream_listener,
                                         &stream_format_changed);
    if (!CHECK_CA_WARN("can't add property listener during format change")) {
        return false;
    }

    /* Change the format. */
    err = CA_SET(stream, kAudioStreamPropertyPhysicalFormat, &change_format);
    if (!CHECK_CA_WARN("error changing physical format")) {
        return false;
    }

    /* The AudioStreamSetProperty is not only asynchronous,
     * it is also not Atomic, in its behaviour.
     * Therefore we check 5 times before we really give up. */
    bool format_set = false;
    AudioStreamBasicDescription actual_format = {0};
    for (int i = 0; i < 5; i++) {
        err = CA_GET(stream, kAudioStreamPropertyPhysicalFormat, &actual_format);
        if (!CHECK_CA_WARN("could not retrieve physical format"))
            break;

        format_set = ca_asbd_equals(&change_format, &actual_format);
        if (format_set)
            break;

        for (int j = 0; !atomic_load(&stream_format_changed) && j < 50; j++)
            mp_sleep_us(10000);

        bool old = true;
        if (!atomic_compare_exchange_strong(&stream_format_changed, &old, false))
            MP_VERBOSE(ao, "reached timeout\n");
    }

    ca_print_asbd(ao, "actual format in use:", &actual_format);

    err = AudioObjectRemovePropertyListener(stream, &p_addr, ca_stream_listener,
                                            &stream_format_changed);

    if (!CHECK_CA_WARN("can't remove property listener")) {
        return false;
    }

    return format_set;
}


struct priv {
    AudioDeviceID device;   // selected device

    bool paused;

    // audio render callback
    AudioDeviceIOProcID render_cb;

    // pid set for hog mode, (-1) means that hog mode on the device was
    // released. hog mode is exclusive access to a device
    pid_t hog_pid;

    AudioStreamID stream;

    // stream index in an AudioBufferList
    int stream_idx;

    // format we changed the stream to, and the original format to restore
    AudioStreamBasicDescription stream_asbd;
    AudioStreamBasicDescription original_asbd;

    bool changed_mixing;
    atomic_bool stream_asbd_changed;
    bool reload_requested;

    uint32_t hw_latency_us;
};

static OSStatus render_cb_compressed(
        AudioDeviceID device, const AudioTimeStamp *ts,
        const void *in_data, const AudioTimeStamp *in_ts,
        AudioBufferList *out_data, const AudioTimeStamp *out_ts, void *ctx)
{
    struct ao *ao    = ctx;
    struct priv *p   = ao->priv;
    AudioBuffer buf  = out_data->mBuffers[p->stream_idx];
    int requested    = buf.mDataByteSize;

    int pseudo_frames = requested / ao->sstride;

    // we expect the callback to read full frames, which are aligned accordingly
    if (pseudo_frames * ao->sstride != requested) {
        MP_ERR(ao, "Unsupported unaligned read of %d bytes.\n", requested);
        return kAudioHardwareUnspecifiedError;
    }

    int64_t end = mp_time_us();
    end += p->hw_latency_us + ca_get_latency(ts)
        + ca_frames_to_us(ao, pseudo_frames);

    ao_read_data(ao, &buf.mData, pseudo_frames, end);

    // Check whether we need to reset the compressed output stream.
    if (atomic_load(&p->stream_asbd_changed)) {
        AudioStreamBasicDescription f;
        OSErr err = CA_GET(p->stream, kAudioStreamPropertyPhysicalFormat, &f);
        CHECK_CA_WARN("could not get stream format");
        if (err == noErr && ca_asbd_equals(&p->stream_asbd, &f))
            atomic_store(&p->stream_asbd_changed, false);
    }

    if (atomic_load(&p->stream_asbd_changed) && !p->reload_requested) {
        p->reload_requested = true;
        ao_request_reload(ao);
        MP_INFO(ao, "Stream format changed! Reloading.\n");
    }

    return noErr;
}

static int init(struct ao *ao)
{
    struct priv *p = ao->priv;

    OSStatus err = ca_select_device(ao, ao->device, &p->device);
    CHECK_CA_ERROR_L(coreaudio_error_nounlock, "failed to select device");

    ao->format = af_fmt_from_planar(ao->format);

    if (!AF_FORMAT_IS_IEC61937(ao->format)) {
        MP_ERR(ao, "Only compressed formats are supported.\n");
        goto coreaudio_error_nounlock;
    }

    if (!ca_device_supports_compressed(ao, p->device)) {
        MP_ERR(ao, "selected device doesn't support compressed formats\n");
        goto coreaudio_error_nounlock;
    }

    // Build ASBD for the input format
    AudioStreamBasicDescription asbd;
    ca_fill_asbd(ao, &asbd);

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
        bool compressed = ca_stream_supports_compressed(ao, streams[i]);

        if (compressed) {
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
                if (ca_format_is_compressed(formats[j].mFormat)) {
                    // select the compressed format that has exactly the same
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
        MP_WARN(ao , "can't find any compressed output stream format\n");
        goto coreaudio_error;
    }

    err = CA_GET(p->stream, kAudioStreamPropertyPhysicalFormat,
                 &p->original_asbd);
    CHECK_CA_ERROR("could not get stream's original physical format");

    if (!ca_change_format(ao, p->stream, p->stream_asbd))
        goto coreaudio_error;

    void *changed = &p->stream_asbd_changed;
    err = ca_enable_device_listener(p->device, changed);
    CHECK_CA_ERROR("cannot install format change listener during init");

    if (p->stream_asbd.mFormatFlags & kAudioFormatFlagIsBigEndian)
        MP_WARN(ao, "stream has non-native byte order, output may fail\n");

    ao->samplerate = p->stream_asbd.mSampleRate;
    ao->bps = ao->samplerate *
                  (p->stream_asbd.mBytesPerPacket /
                   p->stream_asbd.mFramesPerPacket);

    uint32_t latency_frames = 0;
    uint32_t latency_properties[] = {
        kAudioDevicePropertyLatency,
        kAudioDevicePropertyBufferFrameSize,
        kAudioDevicePropertySafetyOffset,
    };
    for (int n = 0; n < MP_ARRAY_SIZE(latency_properties); n++) {
        uint32_t temp;
        err = CA_GET_O(p->device, latency_properties[n], &temp);
        CHECK_CA_WARN("cannot get device latency");
        if (err == noErr)
            latency_frames += temp;
    }

    p->hw_latency_us = ca_frames_to_us(ao, latency_frames);
    MP_VERBOSE(ao, "base latency: %d microseconds\n", (int)p->hw_latency_us);

    err = AudioDeviceCreateIOProcID(p->device,
                                    (AudioDeviceIOProc)render_cb_compressed,
                                    (void *)ao,
                                    &p->render_cb);
    CHECK_CA_ERROR("failed to register audio render callback");

    return CONTROL_TRUE;

coreaudio_error:
    err = ca_unlock_device(p->device, &p->hog_pid);
    CHECK_CA_WARN("can't release hog mode");
coreaudio_error_nounlock:
    return CONTROL_ERROR;
}

static void uninit(struct ao *ao)
{
    struct priv *p = ao->priv;
    OSStatus err = noErr;

    void *changed = &p->stream_asbd_changed;
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

    OSStatus err = AudioDeviceStop(p->device, p->render_cb);
    CHECK_CA_WARN("can't stop audio device");
}

static void audio_resume(struct ao *ao)
{
    struct priv *p = ao->priv;

    OSStatus err = AudioDeviceStart(p->device, p->render_cb);
    CHECK_CA_WARN("can't start audio device");
}

#define OPT_BASE_STRUCT struct priv

const struct ao_driver audio_out_coreaudio_exclusive = {
    .description = "CoreAudio Exclusive Mode",
    .name      = "coreaudio_exclusive",
    .uninit    = uninit,
    .init      = init,
    .pause     = audio_pause,
    .resume    = audio_resume,
    .list_devs = ca_get_device_list,
    .priv_size = sizeof(struct priv),
    .priv_defaults = &(const struct priv){
        .stream_asbd_changed = ATOMIC_VAR_INIT(false),
        .hog_pid = -1,
        .stream = 0,
        .stream_idx = -1,
        .changed_mixing = false,
    },
};
