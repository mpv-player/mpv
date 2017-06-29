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
 * mpv is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * The MacOS X CoreAudio framework doesn't mesh as simply as some
 * simpler frameworks do.  This is due to the fact that CoreAudio pulls
 * audio samples rather than having them pushed at it (which is nice
 * when you are wanting to do good buffering of audio).
 */

#include <CoreAudio/HostTime.h>

#include <libavutil/intreadwrite.h>
#include <libavutil/intfloat.h>

#include "config.h"
#include "ao.h"
#include "internal.h"
#include "audio/format.h"
#include "osdep/timer.h"
#include "osdep/atomic.h"
#include "options/m_option.h"
#include "common/msg.h"
#include "audio/out/ao_coreaudio_chmap.h"
#include "audio/out/ao_coreaudio_properties.h"
#include "audio/out/ao_coreaudio_utils.h"

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

    // Output s16 physical format, float32 virtual format, ac3/dts mpv format
    int spdif_hack;

    bool changed_mixing;

    atomic_bool reload_requested;

    uint32_t hw_latency_us;
};

static OSStatus property_listener_cb(
    AudioObjectID object, uint32_t n_addresses,
    const AudioObjectPropertyAddress addresses[],
    void *data)
{
    struct ao *ao = data;
    struct priv *p = ao->priv;

    // Check whether we need to reset the compressed output stream.
    AudioStreamBasicDescription f;
    OSErr err = CA_GET(p->stream, kAudioStreamPropertyVirtualFormat, &f);
    CHECK_CA_WARN("could not get stream format");
    if (err != noErr || !ca_asbd_equals(&p->stream_asbd, &f)) {
        if (atomic_compare_exchange_strong(&p->reload_requested,
                                           &(bool){false}, true))
        {
            ao_request_reload(ao);
            MP_INFO(ao, "Stream format changed! Reloading.\n");
        }
    }

    return noErr;
}

static OSStatus enable_property_listener(struct ao *ao, bool enabled)
{
    struct priv *p = ao->priv;

    uint32_t selectors[] = {kAudioDevicePropertyDeviceHasChanged,
                            kAudioHardwarePropertyDevices};
    AudioDeviceID devs[] = {p->device,
                            kAudioObjectSystemObject};
    assert(MP_ARRAY_SIZE(selectors) == MP_ARRAY_SIZE(devs));

    OSStatus status = noErr;
    for (int n = 0; n < MP_ARRAY_SIZE(devs); n++) {
        AudioObjectPropertyAddress addr = {
            .mScope    = kAudioObjectPropertyScopeGlobal,
            .mElement  = kAudioObjectPropertyElementMaster,
            .mSelector = selectors[n],
        };
        AudioDeviceID device = devs[n];

        OSStatus status2;
        if (enabled) {
            status2 = AudioObjectAddPropertyListener(
                device, &addr, property_listener_cb, ao);
        } else {
            status2 = AudioObjectRemovePropertyListener(
                device, &addr, property_listener_cb, ao);
        }
        if (status == noErr)
            status = status2;
    }

    return status;
}

// This is a hack for passing through AC3/DTS on drivers which don't support it.
// The goal is to have the driver output the AC3 data bitexact, so basically we
// feed it float data by converting the AC3 data to float in the reverse way we
// assume the driver outputs it.
// Input: data_as_int16[0..samples]
// Output: data_as_float[0..samples]
// The conversion is done in-place.
static void bad_hack_mygodwhy(char *data, int samples)
{
    // In reverse, so we can do it in-place.
    for (int n = samples - 1; n >= 0; n--) {
        int16_t val = AV_RN16(data + n * 2);
        float fval = val / (float)(1 << 15);
        uint32_t ival = av_float2int(fval);
        AV_WN32(data + n * 4, ival);
    }
}

static OSStatus render_cb_compressed(
        AudioDeviceID device, const AudioTimeStamp *ts,
        const void *in_data, const AudioTimeStamp *in_ts,
        AudioBufferList *out_data, const AudioTimeStamp *out_ts, void *ctx)
{
    struct ao *ao    = ctx;
    struct priv *p   = ao->priv;
    AudioBuffer buf  = out_data->mBuffers[p->stream_idx];
    int requested    = buf.mDataByteSize;
    int sstride      = p->spdif_hack ? 4 * ao->channels.num : ao->sstride;

    int pseudo_frames = requested / sstride;

    // we expect the callback to read full frames, which are aligned accordingly
    if (pseudo_frames * sstride != requested) {
        MP_ERR(ao, "Unsupported unaligned read of %d bytes.\n", requested);
        return kAudioHardwareUnspecifiedError;
    }

    int64_t end = mp_time_us();
    end += p->hw_latency_us + ca_get_latency(ts)
        + ca_frames_to_us(ao, pseudo_frames);

    ao_read_data(ao, &buf.mData, pseudo_frames, end);

    if (p->spdif_hack)
        bad_hack_mygodwhy(buf.mData, pseudo_frames * ao->channels.num);

    return noErr;
}

// Apparently, audio devices can have multiple sub-streams. It's not clear to
// me what devices with multiple streams actually do. So only select the first
// one that fulfills some minimum requirements.
// If this is not sufficient, we could duplicate the device list entries for
// each sub-stream, and make it explicit.
static int select_stream(struct ao *ao)
{
    struct priv *p = ao->priv;

    AudioStreamID *streams;
    size_t n_streams;
    OSStatus err;

    /* Get a list of all the streams on this device. */
    err = CA_GET_ARY_O(p->device, kAudioDevicePropertyStreams,
                       &streams, &n_streams);
    CHECK_CA_ERROR("could not get number of streams");
    for (int i = 0; i < n_streams; i++) {
        uint32_t direction;
        err = CA_GET(streams[i], kAudioStreamPropertyDirection, &direction);
        CHECK_CA_WARN("could not get stream direction");
        if (err == noErr && direction != 0) {
            MP_VERBOSE(ao, "Substream %d is not an output stream.\n", i);
            continue;
        }

        if (af_fmt_is_pcm(ao->format) || p->spdif_hack ||
            ca_stream_supports_compressed(ao, streams[i]))
        {
            MP_VERBOSE(ao, "Using substream %d/%zd.\n", i, n_streams);
            p->stream = streams[i];
            p->stream_idx = i;
            break;
        }
    }

    talloc_free(streams);

    if (p->stream_idx < 0) {
        MP_ERR(ao, "No useable substream found.\n");
        goto coreaudio_error;
    }

    return 0;

coreaudio_error:
    return -1;
}

static int find_best_format(struct ao *ao, AudioStreamBasicDescription *out_fmt)
{
    struct priv *p = ao->priv;

    // Build ASBD for the input format
    AudioStreamBasicDescription asbd;
    ca_fill_asbd(ao, &asbd);
    ca_print_asbd(ao, "our format:", &asbd);

    *out_fmt = (AudioStreamBasicDescription){0};

    AudioStreamRangedDescription *formats;
    size_t n_formats;
    OSStatus err;

    err = CA_GET_ARY(p->stream, kAudioStreamPropertyAvailablePhysicalFormats,
                     &formats, &n_formats);
    CHECK_CA_ERROR("could not get number of stream formats");

    for (int j = 0; j < n_formats; j++) {
        AudioStreamBasicDescription *stream_asbd = &formats[j].mFormat;

        ca_print_asbd(ao, "- ", stream_asbd);

        if (!out_fmt->mFormatID || ca_asbd_is_better(&asbd, out_fmt, stream_asbd))
            *out_fmt = *stream_asbd;
    }

    talloc_free(formats);

    if (!out_fmt->mFormatID) {
        MP_ERR(ao, "no format found\n");
        return -1;
    }

    return 0;
coreaudio_error:
    return -1;
}

static int init(struct ao *ao)
{
    struct priv *p = ao->priv;
    int original_format = ao->format;

    OSStatus err = ca_select_device(ao, ao->device, &p->device);
    CHECK_CA_ERROR_L(coreaudio_error_nounlock, "failed to select device");

    ao->format = af_fmt_from_planar(ao->format);

    if (!af_fmt_is_pcm(ao->format) && !af_fmt_is_spdif(ao->format)) {
        MP_ERR(ao, "Unsupported format.\n");
        goto coreaudio_error_nounlock;
    }

    if (af_fmt_is_pcm(ao->format))
        p->spdif_hack = false;

    if (p->spdif_hack) {
        if (af_fmt_to_bytes(ao->format) != 2) {
            MP_ERR(ao, "HD formats not supported with spdif hack.\n");
            goto coreaudio_error_nounlock;
        }
        // Let the pure evil begin!
        ao->format = AF_FORMAT_S16;
    }

    uint32_t is_alive = 1;
    err = CA_GET(p->device, kAudioDevicePropertyDeviceIsAlive, &is_alive);
    CHECK_CA_WARN("could not check whether device is alive");

    if (!is_alive)
        MP_WARN(ao, "device is not alive\n");

    err = ca_lock_device(p->device, &p->hog_pid);
    CHECK_CA_WARN("failed to set hogmode");

    err = ca_disable_mixing(ao, p->device, &p->changed_mixing);
    CHECK_CA_WARN("failed to disable mixing");

    if (select_stream(ao) < 0)
        goto coreaudio_error;

    AudioStreamBasicDescription hwfmt;
    if (find_best_format(ao, &hwfmt) < 0)
        goto coreaudio_error;

    err = CA_GET(p->stream, kAudioStreamPropertyPhysicalFormat,
                 &p->original_asbd);
    CHECK_CA_ERROR("could not get stream's original physical format");

    // Even if changing the physical format fails, we can try using the current
    // virtual format.
    ca_change_physical_format_sync(ao, p->stream, hwfmt);

    if (!ca_init_chmap(ao, p->device))
        goto coreaudio_error;

    err = CA_GET(p->stream, kAudioStreamPropertyVirtualFormat, &p->stream_asbd);
    CHECK_CA_ERROR("could not get stream's virtual format");

    ca_print_asbd(ao, "virtual format", &p->stream_asbd);

    if (p->stream_asbd.mChannelsPerFrame > MP_NUM_CHANNELS) {
        MP_ERR(ao, "unsupported number of channels: %d > %d.\n",
               p->stream_asbd.mChannelsPerFrame, MP_NUM_CHANNELS);
        goto coreaudio_error;
    }

    int new_format = ca_asbd_to_mp_format(&p->stream_asbd);

    // If both old and new formats are spdif, avoid changing it due to the
    // imperfect mapping between mp and CA formats.
    if (!(af_fmt_is_spdif(ao->format) && af_fmt_is_spdif(new_format)))
        ao->format = new_format;

    if (!ao->format || af_fmt_is_planar(ao->format)) {
        MP_ERR(ao, "hardware format not supported\n");
        goto coreaudio_error;
    }

    ao->samplerate = p->stream_asbd.mSampleRate;

    if (ao->channels.num != p->stream_asbd.mChannelsPerFrame) {
        ca_get_active_chmap(ao, p->device, p->stream_asbd.mChannelsPerFrame,
                            &ao->channels);
    }
    if (!ao->channels.num) {
        MP_ERR(ao, "number of channels changed, and unknown channel layout!\n");
        goto coreaudio_error;
    }

    if (p->spdif_hack) {
        AudioStreamBasicDescription physical_format = {0};
        err = CA_GET(p->stream, kAudioStreamPropertyPhysicalFormat,
                     &physical_format);
        CHECK_CA_ERROR("could not get stream's physical format");
        int ph_format = ca_asbd_to_mp_format(&physical_format);
        if (ao->format != AF_FORMAT_FLOAT || ph_format != AF_FORMAT_S16) {
            MP_ERR(ao, "Wrong parameters for spdif hack (%d / %d)\n",
                   ao->format, ph_format);
        }
        ao->format = original_format; // pretend AC3 or DTS *evil laughter*
        MP_WARN(ao, "Using spdif passthrough hack. This could produce noise.\n");
    }

    p->hw_latency_us = ca_get_device_latency_us(ao, p->device);
    MP_VERBOSE(ao, "base latency: %d microseconds\n", (int)p->hw_latency_us);

    err = enable_property_listener(ao, true);
    CHECK_CA_ERROR("cannot install format change listener during init");

    err = AudioDeviceCreateIOProcID(p->device,
                                    (AudioDeviceIOProc)render_cb_compressed,
                                    (void *)ao,
                                    &p->render_cb);
    CHECK_CA_ERROR("failed to register audio render callback");

    return CONTROL_TRUE;

coreaudio_error:
    err = enable_property_listener(ao, false);
    CHECK_CA_WARN("can't remove format change listener");
    err = ca_unlock_device(p->device, &p->hog_pid);
    CHECK_CA_WARN("can't release hog mode");
coreaudio_error_nounlock:
    return CONTROL_ERROR;
}

static void uninit(struct ao *ao)
{
    struct priv *p = ao->priv;
    OSStatus err = noErr;

    err = enable_property_listener(ao, false);
    CHECK_CA_WARN("can't remove device listener, this may cause a crash");

    err = AudioDeviceStop(p->device, p->render_cb);
    CHECK_CA_WARN("failed to stop audio device");

    err = AudioDeviceDestroyIOProcID(p->device, p->render_cb);
    CHECK_CA_WARN("failed to remove device render callback");

    if (!ca_change_physical_format_sync(ao, p->stream, p->original_asbd))
        MP_WARN(ao, "can't revert to original device format\n");

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
    .reset     = audio_pause,
    .resume    = audio_resume,
    .list_devs = ca_get_device_list,
    .priv_size = sizeof(struct priv),
    .priv_defaults = &(const struct priv){
        .hog_pid = -1,
        .stream = 0,
        .stream_idx = -1,
        .changed_mixing = false,
    },
    .options = (const struct m_option[]){
        OPT_FLAG("spdif-hack", spdif_hack, 0),
        {0}
    },
    .options_prefix = "coreaudio",
};
