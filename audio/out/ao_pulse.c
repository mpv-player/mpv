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

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <pulse/pulseaudio.h>

#include "config.h"
#include "audio/format.h"
#include "core/mp_msg.h"
#include "ao.h"
#include "core/input/input.h"

#define PULSE_CLIENT_NAME "mpv"

#define VOL_PA2MP(v) ((v) * 100 / PA_VOLUME_NORM)
#define VOL_MP2PA(v) ((v) * PA_VOLUME_NORM / 100)

struct priv {
    // PulseAudio playback stream object
    struct pa_stream *stream;

    // PulseAudio connection context
    struct pa_context *context;

    // Main event loop object
    struct pa_threaded_mainloop *mainloop;

    // temporary during control()
    struct pa_sink_input_info pi;

    bool broken_pause;
    int retval;
};

#define GENERIC_ERR_MSG(ctx, str) \
    mp_msg(MSGT_AO, MSGL_ERR, "AO: [pulse] "str": %s\n", \
           pa_strerror(pa_context_errno(ctx)))

static void context_state_cb(pa_context *c, void *userdata)
{
    struct ao *ao = userdata;
    struct priv *priv = ao->priv;
    switch (pa_context_get_state(c)) {
    case PA_CONTEXT_READY:
    case PA_CONTEXT_TERMINATED:
    case PA_CONTEXT_FAILED:
        pa_threaded_mainloop_signal(priv->mainloop, 0);
        break;
    }
}

static void stream_state_cb(pa_stream *s, void *userdata)
{
    struct ao *ao = userdata;
    struct priv *priv = ao->priv;
    switch (pa_stream_get_state(s)) {
    case PA_STREAM_READY:
    case PA_STREAM_FAILED:
    case PA_STREAM_TERMINATED:
        pa_threaded_mainloop_signal(priv->mainloop, 0);
        break;
    }
}

static void stream_request_cb(pa_stream *s, size_t length, void *userdata)
{
    struct ao *ao = userdata;
    struct priv *priv = ao->priv;
    mp_input_wakeup(ao->input_ctx);
    pa_threaded_mainloop_signal(priv->mainloop, 0);
}

static void stream_latency_update_cb(pa_stream *s, void *userdata)
{
    struct ao *ao = userdata;
    struct priv *priv = ao->priv;
    pa_threaded_mainloop_signal(priv->mainloop, 0);
}

static void success_cb(pa_stream *s, int success, void *userdata)
{
    struct ao *ao = userdata;
    struct priv *priv = ao->priv;
    priv->retval = success;
    pa_threaded_mainloop_signal(priv->mainloop, 0);
}

/**
 * \brief waits for a pulseaudio operation to finish, frees it and
 *        unlocks the mainloop
 * \param op operation to wait for
 * \return 1 if operation has finished normally (DONE state), 0 otherwise
 */
static int waitop(struct priv *priv, pa_operation *op)
{
    if (!op) {
        pa_threaded_mainloop_unlock(priv->mainloop);
        return 0;
    }
    pa_operation_state_t state = pa_operation_get_state(op);
    while (state == PA_OPERATION_RUNNING) {
        pa_threaded_mainloop_wait(priv->mainloop);
        state = pa_operation_get_state(op);
    }
    pa_operation_unref(op);
    pa_threaded_mainloop_unlock(priv->mainloop);
    return state == PA_OPERATION_DONE;
}

static const struct format_map {
    int mp_format;
    pa_sample_format_t pa_format;
} format_maps[] = {
    {AF_FORMAT_S16_LE, PA_SAMPLE_S16LE},
    {AF_FORMAT_S16_BE, PA_SAMPLE_S16BE},
    {AF_FORMAT_S32_LE, PA_SAMPLE_S32LE},
    {AF_FORMAT_S32_BE, PA_SAMPLE_S32BE},
    {AF_FORMAT_FLOAT_LE, PA_SAMPLE_FLOAT32LE},
    {AF_FORMAT_FLOAT_BE, PA_SAMPLE_FLOAT32BE},
    {AF_FORMAT_U8, PA_SAMPLE_U8},
    {AF_FORMAT_UNKNOWN, 0}
};

static void uninit(struct ao *ao, bool cut_audio)
{
    struct priv *priv = ao->priv;
    if (priv->stream && !cut_audio) {
        pa_threaded_mainloop_lock(priv->mainloop);
        waitop(priv, pa_stream_drain(priv->stream, success_cb, ao));
    }

    if (priv->mainloop)
        pa_threaded_mainloop_stop(priv->mainloop);

    if (priv->stream) {
        pa_stream_disconnect(priv->stream);
        pa_stream_unref(priv->stream);
        priv->stream = NULL;
    }

    if (priv->context) {
        pa_context_disconnect(priv->context);
        pa_context_unref(priv->context);
        priv->context = NULL;
    }

    if (priv->mainloop) {
        pa_threaded_mainloop_free(priv->mainloop);
        priv->mainloop = NULL;
    }
}

static int init(struct ao *ao, char *params)
{
    struct pa_sample_spec ss;
    struct pa_channel_map map;
    char *devarg = NULL;
    char *host = NULL;
    char *sink = NULL;
    const char *version = pa_get_library_version();

    struct priv *priv = talloc_zero(ao, struct priv);
    ao->priv = priv;

    ao->per_application_mixer = true;

    if (params) {
        devarg = strdup(params);
        sink = strchr(devarg, ':');
        if (sink)
            *sink++ = 0;
        if (devarg[0])
            host = devarg;
    }

    priv->broken_pause = false;
    /* not sure which versions are affected, assume 0.9.11* to 0.9.14*
     * known bad: 0.9.14, 0.9.13
     * known good: 0.9.9, 0.9.10, 0.9.15
     * To test: pause, wait ca. 5 seconds, framestep and see if MPlayer
     * hangs somewhen. */
    if (strncmp(version, "0.9.1", 5) == 0 && version[5] >= '1'
        && version[5] <= '4') {
        mp_msg(MSGT_AO, MSGL_WARN,
               "[pulse] working around probably broken pause functionality,\n"
               "        see http://www.pulseaudio.org/ticket/440\n");
        priv->broken_pause = true;
    }

    ss.channels = ao->channels.num;
    ss.rate = ao->samplerate;

    const struct format_map *fmt_map = format_maps;
    while (fmt_map->mp_format != ao->format) {
        if (fmt_map->mp_format == AF_FORMAT_UNKNOWN) {
            mp_msg(MSGT_AO, MSGL_V,
                   "AO: [pulse] Unsupported format, using default\n");
            fmt_map = format_maps;
            break;
        }
        fmt_map++;
    }
    ao->format = fmt_map->mp_format;
    ss.format = fmt_map->pa_format;

    if (!pa_sample_spec_valid(&ss)) {
        mp_msg(MSGT_AO, MSGL_ERR, "AO: [pulse] Invalid sample spec\n");
        goto fail;
    }

    mp_chmap_reorder_to_waveext(&ao->channels);
    pa_channel_map_init_auto(&map, ss.channels, PA_CHANNEL_MAP_WAVEEX);

    ao->bps = pa_bytes_per_second(&ss);

    if (!(priv->mainloop = pa_threaded_mainloop_new())) {
        mp_msg(MSGT_AO, MSGL_ERR, "AO: [pulse] Failed to allocate main loop\n");
        goto fail;
    }

    if (!(priv->context = pa_context_new(pa_threaded_mainloop_get_api(
                                 priv->mainloop), PULSE_CLIENT_NAME))) {
        mp_msg(MSGT_AO, MSGL_ERR, "AO: [pulse] Failed to allocate context\n");
        goto fail;
    }

    pa_context_set_state_callback(priv->context, context_state_cb, ao);

    if (pa_context_connect(priv->context, host, 0, NULL) < 0)
        goto fail;

    pa_threaded_mainloop_lock(priv->mainloop);

    if (pa_threaded_mainloop_start(priv->mainloop) < 0)
        goto unlock_and_fail;

    /* Wait until the context is ready */
    pa_threaded_mainloop_wait(priv->mainloop);

    if (pa_context_get_state(priv->context) != PA_CONTEXT_READY)
        goto unlock_and_fail;

    if (!(priv->stream = pa_stream_new(priv->context, "audio stream", &ss,
                                       &map)))
        goto unlock_and_fail;

    pa_stream_set_state_callback(priv->stream, stream_state_cb, ao);
    pa_stream_set_write_callback(priv->stream, stream_request_cb, ao);
    pa_stream_set_latency_update_callback(priv->stream,
                                          stream_latency_update_cb, ao);
    pa_buffer_attr bufattr = {
        .maxlength = -1,
        .tlength = pa_usec_to_bytes(1000000, &ss),
        .prebuf = -1,
        .minreq = -1,
        .fragsize = -1,
    };
    if (pa_stream_connect_playback(priv->stream, sink, &bufattr,
                                   PA_STREAM_NOT_MONOTONIC, NULL, NULL) < 0)
        goto unlock_and_fail;

    /* Wait until the stream is ready */
    pa_threaded_mainloop_wait(priv->mainloop);

    if (pa_stream_get_state(priv->stream) != PA_STREAM_READY)
        goto unlock_and_fail;

    pa_threaded_mainloop_unlock(priv->mainloop);

    free(devarg);
    return 0;

unlock_and_fail:

    if (priv->mainloop)
        pa_threaded_mainloop_unlock(priv->mainloop);

fail:
    if (priv->context) {
        if (!(pa_context_errno(priv->context) == PA_ERR_CONNECTIONREFUSED
              && ao->probing))
            GENERIC_ERR_MSG(priv->context, "Init failed");
    }
    free(devarg);
    uninit(ao, true);
    return -1;
}

static void cork(struct ao *ao, bool pause)
{
    struct priv *priv = ao->priv;
    pa_threaded_mainloop_lock(priv->mainloop);
    priv->retval = 0;
    if (!waitop(priv, pa_stream_cork(priv->stream, pause, success_cb, ao)) ||
        !priv->retval)
        GENERIC_ERR_MSG(priv->context, "pa_stream_cork() failed");
}

// Play the specified data to the pulseaudio server
static int play(struct ao *ao, void *data, int len, int flags)
{
    struct priv *priv = ao->priv;
    pa_threaded_mainloop_lock(priv->mainloop);
    if (pa_stream_write(priv->stream, data, len, NULL, 0,
                        PA_SEEK_RELATIVE) < 0) {
        GENERIC_ERR_MSG(priv->context, "pa_stream_write() failed");
        len = -1;
    }
    if (flags & AOPLAY_FINAL_CHUNK) {
        // Force start in case the stream was too short for prebuf
        pa_operation *op = pa_stream_trigger(priv->stream, NULL, NULL);
        pa_operation_unref(op);
    }
    pa_threaded_mainloop_unlock(priv->mainloop);
    return len;
}

// Reset the audio stream, i.e. flush the playback buffer on the server side
static void reset(struct ao *ao)
{
    // pa_stream_flush() works badly if not corked
    cork(ao, true);
    struct priv *priv = ao->priv;
    pa_threaded_mainloop_lock(priv->mainloop);
    priv->retval = 0;
    if (!waitop(priv, pa_stream_flush(priv->stream, success_cb, ao)) ||
        !priv->retval)
        GENERIC_ERR_MSG(priv->context, "pa_stream_flush() failed");
    cork(ao, false);
}

// Pause the audio stream by corking it on the server
static void pause(struct ao *ao)
{
    cork(ao, true);
}

// Resume the audio stream by uncorking it on the server
static void resume(struct ao *ao)
{
    struct priv *priv = ao->priv;
    /* Without this, certain versions will cause an infinite hang because
     * pa_stream_writable_size returns 0 always.
     * Note that this workaround causes A-V desync after pause. */
    if (priv->broken_pause)
        reset(ao);
    cork(ao, false);
}

// Return number of bytes that may be written to the server without blocking
static int get_space(struct ao *ao)
{
    struct priv *priv = ao->priv;
    pa_threaded_mainloop_lock(priv->mainloop);
    size_t space = pa_stream_writable_size(priv->stream);
    pa_threaded_mainloop_unlock(priv->mainloop);
    return space;
}

// Return the current latency in seconds
static float get_delay(struct ao *ao)
{
    /* This code basically does what pa_stream_get_latency() _should_
     * do, but doesn't due to multiple known bugs in PulseAudio (at
     * PulseAudio version 2.1). In particular, the timing interpolation
     * mode (PA_STREAM_INTERPOLATE_TIMING) can return completely bogus
     * values, and the non-interpolating code has a bug causing too
     * large results at end of stream (so a stream never seems to finish).
     * This code can still return wrong values in some cases due to known
     * PulseAudio bugs that can not be worked around on the client side.
     *
     * We always query the server for latest timing info. This may take
     * too long to work well with remote audio servers, but at least
     * this should be enough to fix the normal local playback case.
     */
    struct priv *priv = ao->priv;
    pa_threaded_mainloop_lock(priv->mainloop);
    if (!waitop(priv, pa_stream_update_timing_info(priv->stream, NULL, NULL))) {
        GENERIC_ERR_MSG(priv->context, "pa_stream_update_timing_info() failed");
        return 0;
    }
    pa_threaded_mainloop_lock(priv->mainloop);
    const pa_timing_info *ti = pa_stream_get_timing_info(priv->stream);
    if (!ti) {
        pa_threaded_mainloop_unlock(priv->mainloop);
        GENERIC_ERR_MSG(priv->context, "pa_stream_get_timing_info() failed");
        return 0;
    }
    const struct pa_sample_spec *ss = pa_stream_get_sample_spec(priv->stream);
    if (!ss) {
        pa_threaded_mainloop_unlock(priv->mainloop);
        GENERIC_ERR_MSG(priv->context, "pa_stream_get_sample_spec() failed");
        return 0;
    }
    // data left in PulseAudio's main buffers (not written to sink yet)
    int64_t latency = pa_bytes_to_usec(ti->write_index - ti->read_index, ss);
    // since this info may be from a while ago, playback has progressed since
    latency -= ti->transport_usec;
    // data already moved from buffers to sink, but not played yet
    int64_t sink_latency = ti->sink_usec;
    if (!ti->playing)
        /* At the end of a stream, part of the data "left" in the sink may
         * be padding silence after the end; that should be subtracted to
         * get the amount of real audio from our stream. This adjustment
         * is missing from Pulseaudio's own get_latency calculations
         * (as of PulseAudio 2.1). */
        sink_latency -= pa_bytes_to_usec(ti->since_underrun, ss);
    if (sink_latency > 0)
        latency += sink_latency;
    if (latency < 0)
        latency = 0;
    pa_threaded_mainloop_unlock(priv->mainloop);
    return latency / 1e6;
}

/* A callback function that is called when the
 * pa_context_get_sink_input_info() operation completes. Saves the
 * volume field of the specified structure to the global variable volume.
 */
static void info_func(struct pa_context *c, const struct pa_sink_input_info *i,
                      int is_last, void *userdata)
{
    struct ao *ao = userdata;
    struct priv *priv = ao->priv;
    if (is_last < 0) {
        GENERIC_ERR_MSG(priv->context, "Failed to get sink input info");
        return;
    }
    if (!i)
        return;
    priv->pi = *i;
    pa_threaded_mainloop_signal(priv->mainloop, 0);
}

static int control(struct ao *ao, enum aocontrol cmd, void *arg)
{
    struct priv *priv = ao->priv;
    switch (cmd) {
    case AOCONTROL_GET_MUTE:
    case AOCONTROL_GET_VOLUME: {
        uint32_t devidx = pa_stream_get_index(priv->stream);
        pa_threaded_mainloop_lock(priv->mainloop);
        if (!waitop(priv, pa_context_get_sink_input_info(priv->context, devidx,
                                                         info_func, ao))) {
            GENERIC_ERR_MSG(priv->context,
                            "pa_stream_get_sink_input_info() failed");
            return CONTROL_ERROR;
        }
        // Warning: some information in pi might be unaccessible, because
        // we naively copied the struct, without updating pointers etc.
        // Pointers might point to invalid data, accessors might fail.
        if (cmd == AOCONTROL_GET_VOLUME) {
            ao_control_vol_t *vol = arg;
            if (priv->pi.volume.channels != 2)
                vol->left = vol->right =
                    VOL_PA2MP(pa_cvolume_avg(&priv->pi.volume));
            else {
                vol->left = VOL_PA2MP(priv->pi.volume.values[0]);
                vol->right = VOL_PA2MP(priv->pi.volume.values[1]);
            }
        } else if (cmd == AOCONTROL_GET_MUTE) {
            bool *mute = arg;
            *mute = priv->pi.mute;
        }
        return CONTROL_OK;
    }

    case AOCONTROL_SET_MUTE:
    case AOCONTROL_SET_VOLUME: {
        pa_operation *o;

        pa_threaded_mainloop_lock(priv->mainloop);
        uint32_t stream_index = pa_stream_get_index(priv->stream);
        if (cmd == AOCONTROL_SET_VOLUME) {
            const ao_control_vol_t *vol = arg;
            struct pa_cvolume volume;

            pa_cvolume_reset(&volume, ao->channels.num);
            if (volume.channels != 2)
                pa_cvolume_set(&volume, volume.channels, VOL_MP2PA(vol->left));
            else {
                volume.values[0] = VOL_MP2PA(vol->left);
                volume.values[1] = VOL_MP2PA(vol->right);
            }
            o = pa_context_set_sink_input_volume(priv->context, stream_index,
                                                 &volume, NULL, NULL);
            if (!o) {
                pa_threaded_mainloop_unlock(priv->mainloop);
                GENERIC_ERR_MSG(priv->context,
                                "pa_context_set_sink_input_volume() failed");
                return CONTROL_ERROR;
            }
        } else if (cmd == AOCONTROL_SET_MUTE) {
            const bool *mute = arg;
            o = pa_context_set_sink_input_mute(priv->context, stream_index,
                                               *mute, NULL, NULL);
            if (!o) {
                pa_threaded_mainloop_unlock(priv->mainloop);
                GENERIC_ERR_MSG(priv->context,
                                "pa_context_set_sink_input_mute() failed");
                return CONTROL_ERROR;
            }
        } else
            abort();
        /* We don't wait for completion here */
        pa_operation_unref(o);
        pa_threaded_mainloop_unlock(priv->mainloop);
        return CONTROL_OK;
    }
    default:
        return CONTROL_UNKNOWN;
    }
}

const struct ao_driver audio_out_pulse = {
    .is_new = true,
    .info = &(const struct ao_info) {
        "PulseAudio audio output",
        "pulse",
        "Lennart Poettering",
        "",
    },
    .control   = control,
    .init      = init,
    .uninit    = uninit,
    .reset     = reset,
    .get_space = get_space,
    .play      = play,
    .get_delay = get_delay,
    .pause     = pause,
    .resume    = resume,
};
