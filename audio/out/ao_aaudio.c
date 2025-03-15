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

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <dlfcn.h>

#include <android/api-level.h>
#include <aaudio/AAudio.h>

#include "ao.h"
#include "audio/format.h"
#include "osdep/timer.h"
#include "common/msg.h"
#include "internal.h"
#include "options/m_option.h"

struct priv {
    AAudioStreamBuilder *builder;
    AAudioStream *stream;

    int32_t device_id;
    int32_t buffer_capacity;
    aaudio_performance_mode_t performance_mode;

    int device_api;
    void *lib_handle;

#define AAUDIO_FUNCTION(name, ret, ...) ret (*name)(__VA_ARGS__);
#include "aaudio_functions26.inc"
#include "aaudio_functions28.inc"
#undef AAUDIO_FUNCTION
};
struct function_map {
    const char *symbol;
    int offset;
};

#define AAUDIO_FUNCTION(name, ret, ...) {#name, offsetof(struct priv, name)},
static const struct function_map lib_functions26[] = {
#include "aaudio_functions26.inc"
};

static const struct function_map lib_functions28[] = {
#include "aaudio_functions28.inc"
};
#undef AAUDIO_FUNCTION

static const struct {
    int api_level;
    int length;
    const struct function_map* functions;
} lib_functions[] = {
    {26, MP_ARRAY_SIZE(lib_functions26), lib_functions26},
    {28, MP_ARRAY_SIZE(lib_functions28), lib_functions28}
};

static bool load_lib_functions(struct ao *ao)
{
    struct priv *p = ao->priv;

    p->device_api = android_get_device_api_level();
    p->lib_handle = dlopen("libaaudio.so", RTLD_NOW | RTLD_GLOBAL);
    if (!p->lib_handle)
        return false;

    for (int i = 0; i < MP_ARRAY_SIZE(lib_functions); i++) {
        if (p->device_api < lib_functions[i].api_level)
            break;

        for (int j = 0; j < lib_functions[i].length; j++) {
            const char *sym = lib_functions[i].functions[j].symbol;
            void *fun = dlsym(p->lib_handle, sym);
            if (!fun)
                fun = dlsym(RTLD_DEFAULT, sym);
            if (!fun) {
                MP_WARN(ao, "Could not resolve symbol %s\n", sym);
                return false;
            }
            *(void **)((uint8_t *)p + lib_functions[i].functions[j].offset) = fun;
        }
    }
    return true;
}

static void error_callback(AAudioStream *stream, void *context, aaudio_result_t error)
{
    struct ao *ao = context;
    struct priv *p = ao->priv;

    MP_ERR(ao, "%s, trying to reload...\n", p->AAudio_convertResultToText(error));
    ao_request_reload(ao);
}

static aaudio_data_callback_result_t data_callback(AAudioStream *stream, void *context,
                                                   void *data, int32_t nframes)
{
    struct ao *ao = context;
    struct priv *p = ao->priv;

    int64_t written = p->AAudioStream_getFramesWritten(stream);

    int64_t presented;
    int64_t present_time;
    p->AAudioStream_getTimestamp(stream, CLOCK_MONOTONIC, &presented, &present_time);

    int64_t end_time = mp_time_ns();
    end_time += MP_TIME_S_TO_NS(nframes) / ao->samplerate;
    end_time += MP_TIME_S_TO_NS(written - presented) / ao->samplerate;

    bool eof;
    ao_read_data(ao, &data, nframes, end_time, &eof, true, true);

    return eof ? AAUDIO_CALLBACK_RESULT_STOP : AAUDIO_CALLBACK_RESULT_CONTINUE;
}

static void uninit(struct ao *ao)
{
    struct priv *p = ao->priv;

    if (p->builder) {
        p->AAudioStreamBuilder_delete(p->builder);
        p->builder = NULL;
    }

    if (p->stream) {
        p->AAudioStream_close(p->stream);
        p->stream = NULL;
    }

    if (p->lib_handle) {
        dlclose(p->lib_handle);
        p->lib_handle = NULL;
    }
}

static int init(struct ao *ao)
{
    if (load_lib_functions(ao)) {
        struct priv *p = ao->priv;

        aaudio_result_t result;
        AAudioStreamBuilder *builder;

        if ((result = p->AAudio_createStreamBuilder(&builder)) >= 0) {
            aaudio_format_t format;

            if (p->device_api >= 34 && af_fmt_is_spdif(ao->format)) {
                format = AAUDIO_FORMAT_IEC61937;
            } else if (af_fmt_is_float(ao->format)) {
                ao->format = AF_FORMAT_FLOAT;
                format = AAUDIO_FORMAT_PCM_FLOAT;
            } else if (af_fmt_is_int(ao->format)) {
                if (af_fmt_to_bytes(ao->format) > 2 && p->device_api >= 31) {
                    ao->format = AF_FORMAT_S32;
                    format = AAUDIO_FORMAT_PCM_I32;
                } else {
                    ao->format = AF_FORMAT_S16;
                    format = AAUDIO_FORMAT_PCM_I16;
                }
            } else {
                ao->format = AF_FORMAT_S16;
                format = AAUDIO_FORMAT_PCM_I16;
            }

            p->AAudioStreamBuilder_setDeviceId(builder, p->device_id);
            p->AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_OUTPUT);
            p->AAudioStreamBuilder_setSharingMode(builder,
                                                  (ao->init_flags & AO_INIT_EXCLUSIVE) ?
                                                      AAUDIO_SHARING_MODE_EXCLUSIVE :
                                                      AAUDIO_SHARING_MODE_SHARED);
            p->AAudioStreamBuilder_setFormat(builder, format);
            p->AAudioStreamBuilder_setChannelCount(builder, ao->channels.num);
            p->AAudioStreamBuilder_setSampleRate(builder, ao->samplerate);
            p->AAudioStreamBuilder_setErrorCallback(builder, error_callback, ao);
            p->AAudioStreamBuilder_setBufferCapacityInFrames(builder, p->buffer_capacity);
            p->AAudioStreamBuilder_setPerformanceMode(builder, p->performance_mode);
            p->AAudioStreamBuilder_setDataCallback(builder, data_callback, ao);

            if (p->device_api >= 28) {
                p->AAudioStreamBuilder_setContentType(builder, AAUDIO_CONTENT_TYPE_MOVIE);
                p->AAudioStreamBuilder_setUsage(builder, AAUDIO_USAGE_MEDIA);
            }

            p->builder = builder;

            if ((result = p->AAudioStreamBuilder_openStream(p->builder, &p->stream)) >= 0)
                ao->device_buffer = p->AAudioStream_getBufferCapacityInFrames(p->stream);
        }
        if (result >= 0)
            return 1;

        MP_ERR(ao, "Failed to open stream: %s\n", p->AAudio_convertResultToText(result));
    }

    return -1;
}

static void start(struct ao *ao)
{
    struct priv *p = ao->priv;

    aaudio_result_t result = AAUDIO_OK;
    if (!p->stream) {
        if ((result = p->AAudioStreamBuilder_openStream(p->builder, &p->stream)) >= 0)
            ao->device_buffer = p->AAudioStream_getBufferCapacityInFrames(p->stream);
    }

    if (result >= 0) {
        aaudio_stream_state_t next;

        if ((result = p->AAudioStream_requestStart(p->stream)) >= 0)
            result = p->AAudioStream_waitForStateChange(
                p->stream, AAUDIO_STREAM_STATE_STARTING, &next, INT64_MAX);
    }

    if (result < 0)
        MP_ERR(ao, "Failed to start stream: %s\n", p->AAudio_convertResultToText(result));
}

static bool set_pause(struct ao *ao, bool paused)
{
    struct priv *p = ao->priv;

    aaudio_result_t result;
    aaudio_stream_state_t state, next;

    if (paused) {
        result = p->AAudioStream_requestPause(p->stream);
        state = AAUDIO_STREAM_STATE_PAUSING;
    } else {
        result = p->AAudioStream_requestStart(p->stream);
        state = AAUDIO_STREAM_STATE_STARTING;
    }

    if (result >= 0) {
        result = p->AAudioStream_waitForStateChange(p->stream, state, &next, INT64_MAX);
        return true;
    }

    MP_ERR(ao, "Failed to pause stream: %s\n", p->AAudio_convertResultToText(result));
    return false;
}

static void reset(struct ao *ao)
{
    struct priv *p = ao->priv;

    if (p->stream) {
        p->AAudioStream_close(p->stream);
        p->stream = NULL;
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

    .priv_size = sizeof(struct priv),
    .priv_defaults = &(const struct priv){.device_id = AAUDIO_UNSPECIFIED,
                                          .buffer_capacity = AAUDIO_UNSPECIFIED,
                                          .performance_mode = AAUDIO_PERFORMANCE_MODE_NONE,
                                          .stream = NULL,
                                          .lib_handle = NULL},
    .options_prefix = "aaudio",
    .options =
        (const struct m_option[]){
            {"device-id", OPT_CHOICE(device_id, {"auto", AAUDIO_UNSPECIFIED}),
             M_RANGE(1, 96000)},
            {"buffer-capacity", OPT_CHOICE(buffer_capacity, {"auto", AAUDIO_UNSPECIFIED}),
             M_RANGE(1, 96000)},
            {"performance-mode",
             OPT_CHOICE(performance_mode, {"none", AAUDIO_PERFORMANCE_MODE_NONE},
                        {"low-latency", AAUDIO_PERFORMANCE_MODE_LOW_LATENCY},
                        {"power-saving", AAUDIO_PERFORMANCE_MODE_POWER_SAVING})},
            {0}},
};
