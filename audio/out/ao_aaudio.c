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

    // clang-format off
    /* Stream builder functions */
    // API 26
    aaudio_result_t (*AAudio_createStreamBuilder)(AAudioStreamBuilder **builder);
    void (*AAudioStreamBuilder_setDeviceId)(AAudioStreamBuilder *builder,
                                            int32_t deviceId);
    void (*AAudioStreamBuilder_setSampleRate)(AAudioStreamBuilder *builder,
                                              int32_t sampleRate);
    void (*AAudioStreamBuilder_setChannelCount)(AAudioStreamBuilder *builder,
                                                int32_t channelCount);
    void (*AAudioStreamBuilder_setSamplesPerFrame)(AAudioStreamBuilder *builder,
                                                   int32_t samplesPerFrame);
    void (*AAudioStreamBuilder_setFormat)(AAudioStreamBuilder *builder, aaudio_format_t format);
    void (*AAudioStreamBuilder_setSharingMode)(AAudioStreamBuilder *builder,
                                               aaudio_sharing_mode_t sharingMode);
    void (*AAudioStreamBuilder_setDirection)(AAudioStreamBuilder *builder,
                                             aaudio_direction_t direction);
    void (*AAudioStreamBuilder_setBufferCapacityInFrames)(AAudioStreamBuilder *builder,
                                                          int32_t numFrames);
    void (*AAudioStreamBuilder_setPerformanceMode)(AAudioStreamBuilder *builder,
                                                   aaudio_performance_mode_t mode);
    void (*AAudioStreamBuilder_setDataCallback)(AAudioStreamBuilder *builder,
                                                AAudioStream_dataCallback callback,
                                                void *userData);
    void (*AAudioStreamBuilder_setFramesPerDataCallback)(AAudioStreamBuilder *builder, int32_t numFrames);
    void (*AAudioStreamBuilder_setErrorCallback)(AAudioStreamBuilder *builder,
                                                 AAudioStream_errorCallback callback,
                                                 void *userData);
    aaudio_result_t (*AAudioStreamBuilder_openStream)(AAudioStreamBuilder *builder,
                                                      AAudioStream **stream);
    aaudio_result_t (*AAudioStreamBuilder_delete)(AAudioStreamBuilder *builder);

    // API 28
    void (*AAudioStreamBuilder_setUsage)(AAudioStreamBuilder* builder, aaudio_usage_t usage);
    void (*AAudioStreamBuilder_setContentType)(AAudioStreamBuilder* builder,
                                               aaudio_content_type_t contentType);

    /* Stream control functions */
    // API 26
    aaudio_result_t (*AAudioStream_close)(AAudioStream *stream);
    aaudio_result_t (*AAudioStream_requestStart)(AAudioStream *stream);
    aaudio_result_t (*AAudioStream_requestPause)(AAudioStream *stream);
    aaudio_result_t (*AAudioStream_requestFlush)(AAudioStream *stream);
    aaudio_result_t (*AAudioStream_requestStop)(AAudioStream *stream);

    /* Stream query functions */
    // API 26
    aaudio_stream_state_t (*AAudioStream_getState)(AAudioStream *stream);
    aaudio_result_t (*AAudioStream_waitForStateChange)(AAudioStream *stream,
                                                       aaudio_stream_state_t inputState,
                                                       aaudio_stream_state_t *nextState,
                                                       int64_t timeoutNanoseconds);
    aaudio_result_t (*AAudioStream_read)(AAudioStream *stream,
                                         void *buffer,
                                         int32_t numFrames,
                                         int64_t timeoutNanoseconds);
    aaudio_result_t (*AAudioStream_write)(AAudioStream *stream, const void *buffer,
                                          int32_t numFrames,
                                          int64_t timeoutNanoseconds);
    aaudio_result_t (*AAudioStream_setBufferSizeInFrames)(AAudioStream *stream,
                                                          int32_t numFrames);
    int32_t (*AAudioStream_getBufferSizeInFrames)(AAudioStream *stream);
    int32_t (*AAudioStream_getFramesPerBurst)(AAudioStream *stream);
    int32_t (*AAudioStream_getBufferCapacityInFrames)(AAudioStream *stream);
    int32_t (*AAudioStream_getFramesPerDataCallback)(AAudioStream *stream);
    int32_t (*AAudioStream_getXRunCount)(AAudioStream *stream);
    int32_t (*AAudioStream_getSampleRate)(AAudioStream *stream);
    int32_t (*AAudioStream_getChannelCount)(AAudioStream *stream);
    int32_t (*AAudioStream_getSamplesPerFrame)(AAudioStream *stream);
    int32_t (*AAudioStream_getDeviceId)(AAudioStream *stream);
    aaudio_format_t (*AAudioStream_getFormat)(AAudioStream *stream);
    aaudio_sharing_mode_t (*AAudioStream_getSharingMode)(AAudioStream *stream);
    aaudio_performance_mode_t (*AAudioStream_getPerformanceMode)(AAudioStream *stream);
    aaudio_direction_t (*AAudioStream_getDirection)(AAudioStream *stream);
    int64_t (*AAudioStream_getFramesWritten)(AAudioStream *stream);
    int64_t (*AAudioStream_getFramesRead)(AAudioStream *stream);
    int64_t (*AAudioStream_getTimestamp)(AAudioStream *stream, clockid_t clockid,
                                         int64_t* framePosition,
                                         int64_t* timeNanoseconds);

    /* Utility functions */
    // API 26
    const char * (*AAudio_convertResultToText)(aaudio_result_t returnCode);
    const char* (*AAudio_convertStreamStateToText)(aaudio_stream_state_t state);
};
struct function_map {
    const char *symbol;
    int offset;
};
static const struct function_map lib_functions26[] = {
    {"AAudio_convertResultToText", offsetof(struct priv, AAudio_convertResultToText)},
    {"AAudio_convertStreamStateToText", offsetof(struct priv, AAudio_convertStreamStateToText)},
    {"AAudio_createStreamBuilder", offsetof(struct priv, AAudio_createStreamBuilder)},
    {"AAudioStreamBuilder_setDeviceId", offsetof(struct priv, AAudioStreamBuilder_setDeviceId)},
    {"AAudioStreamBuilder_setSampleRate", offsetof(struct priv, AAudioStreamBuilder_setSampleRate)},
    {"AAudioStreamBuilder_setChannelCount", offsetof(struct priv, AAudioStreamBuilder_setChannelCount)},
    {"AAudioStreamBuilder_setSamplesPerFrame", offsetof(struct priv, AAudioStreamBuilder_setSamplesPerFrame)},
    {"AAudioStreamBuilder_setFormat", offsetof(struct priv, AAudioStreamBuilder_setFormat)},
    {"AAudioStreamBuilder_setSharingMode", offsetof(struct priv, AAudioStreamBuilder_setSharingMode)},
    {"AAudioStreamBuilder_setDirection", offsetof(struct priv, AAudioStreamBuilder_setDirection)},
    {"AAudioStreamBuilder_setBufferCapacityInFrames", offsetof(struct priv, AAudioStreamBuilder_setBufferCapacityInFrames)},
    {"AAudioStreamBuilder_setPerformanceMode", offsetof(struct priv, AAudioStreamBuilder_setPerformanceMode)},
    {"AAudioStreamBuilder_setDataCallback", offsetof(struct priv, AAudioStreamBuilder_setDataCallback)},
    {"AAudioStreamBuilder_setFramesPerDataCallback", offsetof(struct priv, AAudioStreamBuilder_setFramesPerDataCallback)},
    {"AAudioStreamBuilder_setErrorCallback", offsetof(struct priv, AAudioStreamBuilder_setErrorCallback)},
    {"AAudioStreamBuilder_openStream", offsetof(struct priv, AAudioStreamBuilder_openStream)},
    {"AAudioStreamBuilder_delete", offsetof(struct priv, AAudioStreamBuilder_delete)},
    {"AAudioStream_close", offsetof(struct priv, AAudioStream_close)},
    {"AAudioStream_requestStart", offsetof(struct priv, AAudioStream_requestStart)},
    {"AAudioStream_requestPause", offsetof(struct priv, AAudioStream_requestPause)},
    {"AAudioStream_requestFlush", offsetof(struct priv, AAudioStream_requestFlush)},
    {"AAudioStream_requestStop", offsetof(struct priv, AAudioStream_requestStop)},
    {"AAudioStream_getState", offsetof(struct priv, AAudioStream_getState)},
    {"AAudioStream_waitForStateChange", offsetof(struct priv, AAudioStream_waitForStateChange)},
    {"AAudioStream_read", offsetof(struct priv, AAudioStream_read)},
    {"AAudioStream_write", offsetof(struct priv, AAudioStream_write)},
    {"AAudioStream_setBufferSizeInFrames", offsetof(struct priv, AAudioStream_setBufferSizeInFrames)},
    {"AAudioStream_getBufferSizeInFrames", offsetof(struct priv, AAudioStream_getBufferSizeInFrames)},
    {"AAudioStream_getFramesPerBurst", offsetof(struct priv, AAudioStream_getFramesPerBurst)},
    {"AAudioStream_getBufferCapacityInFrames", offsetof(struct priv, AAudioStream_getBufferCapacityInFrames)},
    {"AAudioStream_getFramesPerDataCallback", offsetof(struct priv, AAudioStream_getFramesPerDataCallback)},
    {"AAudioStream_getXRunCount", offsetof(struct priv, AAudioStream_getXRunCount)},
    {"AAudioStream_getSampleRate", offsetof(struct priv, AAudioStream_getSampleRate)},
    {"AAudioStream_getChannelCount", offsetof(struct priv, AAudioStream_getChannelCount)},
    {"AAudioStream_getSamplesPerFrame", offsetof(struct priv, AAudioStream_getSamplesPerFrame)},
    {"AAudioStream_getDeviceId", offsetof(struct priv, AAudioStream_getDeviceId)},
    {"AAudioStream_getFormat", offsetof(struct priv, AAudioStream_getFormat)},
    {"AAudioStream_getSharingMode", offsetof(struct priv, AAudioStream_getSharingMode)},
    {"AAudioStream_getPerformanceMode", offsetof(struct priv, AAudioStream_getPerformanceMode)},
    {"AAudioStream_getDirection", offsetof(struct priv, AAudioStream_getDirection)},
    {"AAudioStream_getFramesWritten", offsetof(struct priv, AAudioStream_getFramesWritten)},
    {"AAudioStream_getFramesRead", offsetof(struct priv, AAudioStream_getFramesRead)},
    {"AAudioStream_getTimestamp", offsetof(struct priv, AAudioStream_getTimestamp)},
};

static const struct function_map lib_functions28[] = {
    { "AAudioStreamBuilder_setUsage",      offsetof(struct priv, AAudioStreamBuilder_setUsage) },
    { "AAudioStreamBuilder_setContentType",  offsetof(struct priv, AAudioStreamBuilder_setContentType) },
};
static const struct {
    int api_level;
    int length;
    const struct function_map* functions;
} lib_functions[] = {
    {26, MP_ARRAY_SIZE(lib_functions26), lib_functions26},
    {28, MP_ARRAY_SIZE(lib_functions28), lib_functions28}
};

// clang-format on
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

    if(p->builder) {
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
