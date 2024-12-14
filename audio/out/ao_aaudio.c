/*
 * AAudio audio output driver
 *
 * Copyright (C) 2024 Jun Bo Bi <jambonmcyeah@gmail.com>
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

#include <stdbool.h>
#include <stdint.h>

#include <aaudio/AAudio.h>
#include <android/api-level.h>

#include "ao.h"
#include "audio/format.h"
#include "common/common.h"
#include "common/msg.h"
#include "internal.h"
#include "options/m_option.h"

struct priv {
    AAudioStream *stream;
    int32_t x_run_count;

    int32_t device_id;
    int32_t buffer_capacity;
    aaudio_performance_mode_t performance_mode;
};

static void error_callback(AAudioStream *stream, void *context, aaudio_result_t error)
{
    struct ao *ao = context;
    switch (error) {
    case AAUDIO_ERROR_DISCONNECTED:
        MP_ERR(ao, "Device disconnected, trying to reload...\n");
        break;
    default:
        MP_ERR(ao, "Unkown error %" PRId32 ", trying to reload...\n", error);
    }

    ao_request_reload(ao);
}

static void uninit(struct ao *ao)
{
    if (__builtin_available(android 26, *)) {
        struct priv *p = ao->priv;

        if (!p->stream)
            AAudioStream_close(p->stream);
    }
}

static int init(struct ao *ao)
{
    if (__builtin_available(android 26, *)) {
        const int api_level = android_get_device_api_level();
        aaudio_result_t result;

        struct priv *p = ao->priv;

        AAudioStreamBuilder *builder;
        if ((result = AAudio_createStreamBuilder(&builder)) < 0) {
            MP_ERR(ao, "Failed to create stream builder: %" PRId32 "\n", result);
            return -1;
        }

        aaudio_format_t format = AAUDIO_UNSPECIFIED;
        if (api_level >= 34 && af_fmt_is_spdif(ao->format)) {
            format = AAUDIO_FORMAT_IEC61937;
        } else if (af_fmt_is_float(ao->format)) {
            ao->format = AF_FORMAT_FLOAT;
            format = AAUDIO_FORMAT_PCM_FLOAT;
        } else if (af_fmt_is_int(ao->format)) {
            if (af_fmt_to_bytes(ao->format) > 2 && api_level >= 31) {
                ao->format = AF_FORMAT_S32;
                format = AAUDIO_FORMAT_PCM_I32;
            } else {
                ao->format = AF_FORMAT_S16;
                format = AAUDIO_FORMAT_PCM_I16;
            }
        }

        AAudioStreamBuilder_setDeviceId(builder, p->device_id);
        AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_OUTPUT);
        AAudioStreamBuilder_setSharingMode(builder, (ao->init_flags & AO_INIT_EXCLUSIVE) ?
                                                        AAUDIO_SHARING_MODE_EXCLUSIVE :
                                                        AAUDIO_SHARING_MODE_SHARED);
        AAudioStreamBuilder_setFormat(builder, format);
        AAudioStreamBuilder_setChannelCount(builder, ao->channels.num);
        AAudioStreamBuilder_setSampleRate(builder, ao->samplerate);
        AAudioStreamBuilder_setErrorCallback(builder, error_callback, ao);
        AAudioStreamBuilder_setPerformanceMode(builder, p->performance_mode);
        AAudioStreamBuilder_setBufferCapacityInFrames(builder, p->buffer_capacity);

        if ((result = AAudioStreamBuilder_openStream(builder, &p->stream)) < 0) {
            MP_ERR(ao, "Failed to open stream: %" PRId32 "\n", result);
            AAudioStreamBuilder_delete(builder);
            return -1;
        }

        ao->device_buffer = AAudioStream_getBufferCapacityInFrames(p->stream);
        return 1;
    }
    return -1;
}

static void start(struct ao *ao)
{
    if (__builtin_available(android 26, *)) {
        struct priv *p = ao->priv;
        aaudio_result_t result = AAudioStream_requestStart(p->stream);

        if (result < 0)
            MP_ERR(ao, "Failed to start stream: %" PRId32 "\n", result);
    }
}

static bool set_pause(struct ao *ao, bool paused)
{
    if (__builtin_available(android 26, *)) {
        struct priv *p = ao->priv;
        aaudio_result_t result;
        if (paused)
            result = AAudioStream_requestPause(p->stream);
        else
            result = AAudioStream_requestStart(p->stream);

        if (result < 0) {
            MP_ERR(ao, "Failed to pause stream: %" PRId32 "\n", result);
            return false;
        }

        return true;
    }
    return false;
}

static void reset(struct ao *ao)
{
    if (__builtin_available(android 26, *)) {
        struct priv *p = ao->priv;
        aaudio_result_t result;

        if ((result = AAudioStream_requestStop(p->stream)) < 0)
            MP_ERR(ao, "Failed to stop stream: %" PRId32 "\n", result);
    }
}

static bool audio_write(struct ao *ao, void **data, int samples)
{
    if (__builtin_available(android 26, *)) {
        struct priv *p = ao->priv;
        aaudio_result_t result = AAudioStream_write(p->stream, data[0], samples, INT64_MAX);

        if (result < 0) {
            MP_ERR(ao, "Failed to write data: %" PRId32 "\n", result);
            return false;
        }

        return true;
    }
    return false;
}
static void get_state(struct ao *ao, struct mp_pcm_state *state)
{
    if (__builtin_available(android 26, *)) {
        struct priv *p = ao->priv;

        state->free_samples =
            MPCLAMP(AAudioStream_getBufferSizeInFrames(p->stream), 0, ao->device_buffer);
        state->queued_samples = ao->device_buffer - state->free_samples;

        int64_t frame_pos;
        int64_t time_ns;
        aaudio_result_t result =
            AAudioStream_getTimestamp(p->stream, CLOCK_MONOTONIC, &frame_pos, &time_ns);

        if (result >= 0)
            state->delay =
                (double)(AAudioStream_getFramesWritten(p->stream) - frame_pos) / ao->samplerate;

        int32_t x_run_count = AAudioStream_getXRunCount(p->stream);
        if (x_run_count > p->x_run_count) {
            state->playing = false;
        } else {
            aaudio_stream_state_t stream_state = AAudioStream_getState(p->stream);
            switch (stream_state) {
            case AAUDIO_STREAM_STATE_STARTING:
            case AAUDIO_STREAM_STATE_STARTED:
            case AAUDIO_STREAM_STATE_PAUSED:
            case AAUDIO_STREAM_STATE_PAUSING:
            case AAUDIO_STREAM_STATE_OPEN:
                state->playing = true;
                break;
            }
        }

        p->x_run_count = x_run_count;
    }
}

#define OPT_BASE_STRUCT struct priv

const struct ao_driver audio_out_aaudio = {
    .description = "AAudio audio output",
    .name = "aaudio",
    .init = init,
    .uninit = uninit,
    .start = start,
    .reset = reset,
    .set_pause = set_pause,
    .write = audio_write,
    .get_state = get_state,
    .priv_size = sizeof(struct priv),
    .priv_defaults = &(const struct priv) {
      .device_id = AAUDIO_UNSPECIFIED,
      .buffer_capacity = AAUDIO_UNSPECIFIED,
      .performance_mode = AAUDIO_PERFORMANCE_MODE_NONE
    },
    .options_prefix = "aaudio",
    .options = (const struct m_option[]) {
        {"device-id",
            OPT_CHOICE(device_id, {"auto", AAUDIO_UNSPECIFIED}),
            M_RANGE(1, 96000)},
        {"buffer-capacity",
            OPT_CHOICE(buffer_capacity, {"auto", AAUDIO_UNSPECIFIED}),
            M_RANGE(1, 96000)},
        {"performance-mode", OPT_CHOICE(performance_mode,
            {"none", AAUDIO_PERFORMANCE_MODE_NONE},
            {"low-latency", AAUDIO_PERFORMANCE_MODE_LOW_LATENCY},
            {"power-saving", AAUDIO_PERFORMANCE_MODE_POWER_SAVING}
        )},
        {0}
    },
};
