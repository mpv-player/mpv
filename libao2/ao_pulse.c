/*
 * PulseAudio audio output driver.
 * Copyright (C) 2006 Lennart Poettering
 * Copyright (C) 2007 Reimar Doeffinger
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
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <string.h>

#include <pulse/pulseaudio.h>

#include "config.h"
#include "libaf/af_format.h"
#include "mp_msg.h"
#include "audio_out.h"
#include "audio_out_internal.h"

#define PULSE_CLIENT_NAME "MPlayer"

/** General driver info */
static ao_info_t info = {
    "PulseAudio audio output",
    "pulse",
    "Lennart Poettering",
    ""
};

/** PulseAudio playback stream object */
static struct pa_stream *stream;

/** PulseAudio connection context */
static struct pa_context *context;

/** Main event loop object */
static struct pa_threaded_mainloop *mainloop;

/** A temporary variable to store the current volume */
static pa_cvolume volume;

LIBAO_EXTERN(pulse)

#define GENERIC_ERR_MSG(ctx, str) \
    mp_msg(MSGT_AO, MSGL_ERR, "AO: [pulse] "str": %s\n", \
    pa_strerror(pa_context_errno(ctx)))

static void context_state_cb(pa_context *c, void *userdata) {
    switch (pa_context_get_state(c)) {
        case PA_CONTEXT_READY:
        case PA_CONTEXT_TERMINATED:
        case PA_CONTEXT_FAILED:
            pa_threaded_mainloop_signal(mainloop, 0);
            break;
    }
}

static void stream_state_cb(pa_stream *s, void *userdata) {
    switch (pa_stream_get_state(s)) {
        case PA_STREAM_READY:
        case PA_STREAM_FAILED:
        case PA_STREAM_TERMINATED:
            pa_threaded_mainloop_signal(mainloop, 0);
            break;
    }
}

static void stream_request_cb(pa_stream *s, size_t length, void *userdata) {
    pa_threaded_mainloop_signal(mainloop, 0);
}

static void stream_latency_update_cb(pa_stream *s, void *userdata) {
    pa_threaded_mainloop_signal(mainloop, 0);
}

static void success_cb(pa_stream *s, int success, void *userdata) {
    if (userdata)
        *(int *)userdata = success;
    pa_threaded_mainloop_signal(mainloop, 0);
}

/**
 * \brief waits for a pulseaudio operation to finish, frees it and
 *        unlocks the mainloop
 * \param op operation to wait for
 * \return 1 if operation has finished normally (DONE state), 0 otherwise
 */
static int waitop(pa_operation *op) {
    pa_operation_state_t state;
    if (!op) return 0;
    state = pa_operation_get_state(op);
    while (state == PA_OPERATION_RUNNING) {
        pa_threaded_mainloop_wait(mainloop);
        state = pa_operation_get_state(op);
    }
    pa_operation_unref(op);
    pa_threaded_mainloop_unlock(mainloop);
    return state == PA_OPERATION_DONE;
}

static const struct format_map_s {
    int mp_format;
    pa_sample_format_t pa_format;
} format_maps[] = {
    {AF_FORMAT_S16_LE, PA_SAMPLE_S16LE},
    {AF_FORMAT_S16_BE, PA_SAMPLE_S16BE},
#ifdef PA_SAMPLE_S32NE
    {AF_FORMAT_S32_LE, PA_SAMPLE_S32LE},
    {AF_FORMAT_S32_BE, PA_SAMPLE_S32BE},
#endif
#ifdef PA_SAMPLE_FLOAT32NE
    {AF_FORMAT_FLOAT_LE, PA_SAMPLE_FLOAT32LE},
    {AF_FORMAT_FLOAT_BE, PA_SAMPLE_FLOAT32BE},
#endif
    {AF_FORMAT_U8, PA_SAMPLE_U8},
    {AF_FORMAT_MU_LAW, PA_SAMPLE_ULAW},
    {AF_FORMAT_A_LAW, PA_SAMPLE_ALAW},
    {AF_FORMAT_UNKNOWN, 0}
};

static int init(int rate_hz, int channels, int format, int flags) {
    struct pa_sample_spec ss;
    struct pa_channel_map map;
    const struct format_map_s *fmt_map;
    char *devarg = NULL;
    char *host = NULL;
    char *sink = NULL;

    if (ao_subdevice) {
        devarg = strdup(ao_subdevice);
        sink = strchr(devarg, ':');
        if (sink) *sink++ = 0;
        if (devarg[0]) host = devarg;
    }

    ss.channels = channels;
    ss.rate = rate_hz;

    ao_data.samplerate = rate_hz;
    ao_data.channels = channels;

    fmt_map = format_maps;
    while (fmt_map->mp_format != format) {
        if (fmt_map->mp_format == AF_FORMAT_UNKNOWN) {
            mp_msg(MSGT_AO, MSGL_V, "AO: [pulse] Unsupported format, using default\n");
            fmt_map = format_maps;
            break;
        }
        fmt_map++;
    }
    ao_data.format = fmt_map->mp_format;
    ss.format = fmt_map->pa_format;

    if (!pa_sample_spec_valid(&ss)) {
        mp_msg(MSGT_AO, MSGL_ERR, "AO: [pulse] Invalid sample spec\n");
        goto fail;
    }

    pa_channel_map_init_auto(&map, ss.channels, PA_CHANNEL_MAP_ALSA);
    ao_data.bps = pa_bytes_per_second(&ss);

    pa_cvolume_reset(&volume, ss.channels);

    if (!(mainloop = pa_threaded_mainloop_new())) {
        mp_msg(MSGT_AO, MSGL_ERR, "AO: [pulse] Failed to allocate main loop\n");
        goto fail;
    }

    if (!(context = pa_context_new(pa_threaded_mainloop_get_api(mainloop), PULSE_CLIENT_NAME))) {
        mp_msg(MSGT_AO, MSGL_ERR, "AO: [pulse] Failed to allocate context\n");
        goto fail;
    }

    pa_context_set_state_callback(context, context_state_cb, NULL);

    if (pa_context_connect(context, host, 0, NULL) < 0)
        goto fail;

    pa_threaded_mainloop_lock(mainloop);

    if (pa_threaded_mainloop_start(mainloop) < 0)
        goto unlock_and_fail;

    /* Wait until the context is ready */
    pa_threaded_mainloop_wait(mainloop);

    if (pa_context_get_state(context) != PA_CONTEXT_READY)
        goto unlock_and_fail;

    if (!(stream = pa_stream_new(context, "audio stream", &ss, &map)))
        goto unlock_and_fail;

    pa_stream_set_state_callback(stream, stream_state_cb, NULL);
    pa_stream_set_write_callback(stream, stream_request_cb, NULL);
    pa_stream_set_latency_update_callback(stream, stream_latency_update_cb, NULL);

    if (pa_stream_connect_playback(stream, sink, NULL, PA_STREAM_INTERPOLATE_TIMING|PA_STREAM_AUTO_TIMING_UPDATE, &volume, NULL) < 0)
        goto unlock_and_fail;

    /* Wait until the stream is ready */
    pa_threaded_mainloop_wait(mainloop);

    if (pa_stream_get_state(stream) != PA_STREAM_READY)
        goto unlock_and_fail;

    pa_threaded_mainloop_unlock(mainloop);

    free(devarg);
    return 1;

unlock_and_fail:

    if (mainloop)
        pa_threaded_mainloop_unlock(mainloop);

fail:
    if (context)
        GENERIC_ERR_MSG(context, "Init failed");
    free(devarg);
    uninit(1);
    return 0;
}

/** Destroy libao driver */
static void uninit(int immed) {
    if (stream && !immed) {
            pa_threaded_mainloop_lock(mainloop);
            waitop(pa_stream_drain(stream, success_cb, NULL));
    }

    if (mainloop)
        pa_threaded_mainloop_stop(mainloop);

    if (stream) {
        pa_stream_disconnect(stream);
        pa_stream_unref(stream);
        stream = NULL;
    }

    if (context) {
        pa_context_disconnect(context);
        pa_context_unref(context);
        context = NULL;
    }

    if (mainloop) {
        pa_threaded_mainloop_free(mainloop);
        mainloop = NULL;
    }
}

/** Play the specified data to the pulseaudio server */
static int play(void* data, int len, int flags) {
    pa_threaded_mainloop_lock(mainloop);
    if (pa_stream_write(stream, data, len, NULL, 0, PA_SEEK_RELATIVE) < 0) {
        GENERIC_ERR_MSG(context, "pa_stream_write() failed");
        len = -1;
    }
    pa_threaded_mainloop_unlock(mainloop);
    return len;
}

static void cork(int b) {
    int success = 0;
    pa_threaded_mainloop_lock(mainloop);
    if (!waitop(pa_stream_cork(stream, b, success_cb, &success)) ||
        !success)
        GENERIC_ERR_MSG(context, "pa_stream_cork() failed");
}

/** Pause the audio stream by corking it on the server */
static void audio_pause(void) {
    cork(1);
}

/** Resume the audio stream by uncorking it on the server */
static void audio_resume(void) {
    cork(0);
}

/** Reset the audio stream, i.e. flush the playback buffer on the server side */
static void reset(void) {
    int success = 0;
    pa_threaded_mainloop_lock(mainloop);
    if (!waitop(pa_stream_flush(stream, success_cb, &success)) ||
        !success)
        GENERIC_ERR_MSG(context, "pa_stream_flush() failed");
}

/** Return number of bytes that may be written to the server without blocking */
static int get_space(void) {
    size_t l;
    pa_threaded_mainloop_lock(mainloop);
    l = pa_stream_writable_size(stream);
    pa_threaded_mainloop_unlock(mainloop);
    return l;
}

/** Return the current latency in seconds */
static float get_delay(void) {
    pa_usec_t latency = (pa_usec_t) -1;
    pa_threaded_mainloop_lock(mainloop);
    while (pa_stream_get_latency(stream, &latency, NULL) < 0) {
        if (pa_context_errno(context) != PA_ERR_NODATA) {
            GENERIC_ERR_MSG(context, "pa_stream_get_latency() failed");
            break;
        }
        /* Wait until latency data is available again */
        pa_threaded_mainloop_wait(mainloop);
    }
    pa_threaded_mainloop_unlock(mainloop);
    return latency == (pa_usec_t) -1 ? 0 : latency / 1000000.0;
}

/** A callback function that is called when the
 * pa_context_get_sink_input_info() operation completes. Saves the
 * volume field of the specified structure to the global variable volume. */
static void info_func(struct pa_context *c, const struct pa_sink_input_info *i, int is_last, void *userdata) {
    if (is_last < 0) {
        GENERIC_ERR_MSG(context, "Failed to get sink input info");
        return;
    }
    if (!i)
        return;
    volume = i->volume;
    pa_threaded_mainloop_signal(mainloop, 0);
}

static int control(int cmd, void *arg) {
    switch (cmd) {
        case AOCONTROL_GET_VOLUME: {
            ao_control_vol_t *vol = arg;
            uint32_t devidx = pa_stream_get_index(stream);
            pa_threaded_mainloop_lock(mainloop);
            if (!waitop(pa_context_get_sink_input_info(context, devidx, info_func, NULL))) {
                GENERIC_ERR_MSG(context, "pa_stream_get_sink_input_info() failed");
                return CONTROL_ERROR;
            }

            if (volume.channels != 2)
                vol->left = vol->right = pa_cvolume_avg(&volume)*100/PA_VOLUME_NORM;
            else {
                vol->left = volume.values[0]*100/PA_VOLUME_NORM;
                vol->right = volume.values[1]*100/PA_VOLUME_NORM;
            }

            return CONTROL_OK;
        }

        case AOCONTROL_SET_VOLUME: {
            const ao_control_vol_t *vol = arg;
            pa_operation *o;

            if (volume.channels != 2)
                pa_cvolume_set(&volume, volume.channels, (pa_volume_t)vol->left*PA_VOLUME_NORM/100);
            else {
                volume.values[0] = (pa_volume_t)vol->left*PA_VOLUME_NORM/100;
                volume.values[1] = (pa_volume_t)vol->right*PA_VOLUME_NORM/100;
            }

            if (!(o = pa_context_set_sink_input_volume(context, pa_stream_get_index(stream), &volume, NULL, NULL))) {
                GENERIC_ERR_MSG(context, "pa_context_set_sink_input_volume() failed");
                return CONTROL_ERROR;
            }
            /* We don't wait for completion here */
            pa_operation_unref(o);
            return CONTROL_OK;
        }

        default:
            return CONTROL_UNKNOWN;
    }
}
