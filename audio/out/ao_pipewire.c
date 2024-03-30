/*
 * PipeWire audio output driver.
 * Copyright (C) 2021 Thomas Weißschuh <thomas@t-8ch.de>
 * Copyright (C) 2021 Oschowa <oschowa@web.de>
 * Copyright (C) 2020 Andreas Kempf <aakempf@gmail.com>
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

#include <pipewire/pipewire.h>
#include <pipewire/global.h>
#include <spa/param/audio/format-utils.h>
#include <spa/param/props.h>
#include <spa/utils/result.h>
#include <math.h>

#include "common/common.h"
#include "common/msg.h"
#include "options/m_config.h"
#include "options/m_option.h"
#include "ao.h"
#include "audio/format.h"
#include "internal.h"
#include "osdep/timer.h"

#if !PW_CHECK_VERSION(0, 3, 50)
static inline int pw_stream_get_time_n(struct pw_stream *stream, struct pw_time *time, size_t size) {
	return pw_stream_get_time(stream, time);
}
#endif

#if !PW_CHECK_VERSION(0, 3, 57)
// Earlier versions segfault on zeroed hooks
#define spa_hook_remove(hook) if ((hook)->link.prev) spa_hook_remove(hook)
#endif

enum init_state {
    INIT_STATE_NONE,
    INIT_STATE_SUCCESS,
    INIT_STATE_ERROR,
};

enum {
    VOLUME_MODE_CHANNEL,
    VOLUME_MODE_GLOBAL,
};

struct priv {
    struct pw_thread_loop *loop;
    struct pw_stream *stream;
    struct pw_core *core;
    struct spa_hook stream_listener;
    struct spa_hook core_listener;
    enum init_state init_state;

    bool muted;
    float volume;

    struct {
        int buffer_msec;
        char *remote;
        int volume_mode;
    } options;

    struct {
        struct pw_registry *registry;
        struct spa_hook registry_listener;
        struct spa_list sinks;
    } hotplug;
};

struct id_list {
    uint32_t id;
    struct spa_list node;
};

static enum spa_audio_format af_fmt_to_pw(enum af_format format)
{
    switch (format) {
    case AF_FORMAT_U8:          return SPA_AUDIO_FORMAT_U8;
    case AF_FORMAT_S16:         return SPA_AUDIO_FORMAT_S16;
    case AF_FORMAT_S32:         return SPA_AUDIO_FORMAT_S32;
    case AF_FORMAT_FLOAT:       return SPA_AUDIO_FORMAT_F32;
    case AF_FORMAT_DOUBLE:      return SPA_AUDIO_FORMAT_F64;
    case AF_FORMAT_U8P:         return SPA_AUDIO_FORMAT_U8P;
    case AF_FORMAT_S16P:        return SPA_AUDIO_FORMAT_S16P;
    case AF_FORMAT_S32P:        return SPA_AUDIO_FORMAT_S32P;
    case AF_FORMAT_FLOATP:      return SPA_AUDIO_FORMAT_F32P;
    case AF_FORMAT_DOUBLEP:     return SPA_AUDIO_FORMAT_F64P;
    default:                    return SPA_AUDIO_FORMAT_UNKNOWN;
    }
}

static enum spa_audio_iec958_codec af_fmt_to_codec(enum af_format format)
{
    switch (format) {
    case AF_FORMAT_S_AAC:    return SPA_AUDIO_IEC958_CODEC_MPEG2_AAC;
    case AF_FORMAT_S_AC3:    return SPA_AUDIO_IEC958_CODEC_AC3;
    case AF_FORMAT_S_DTS:    return SPA_AUDIO_IEC958_CODEC_DTS;
    case AF_FORMAT_S_DTSHD:  return SPA_AUDIO_IEC958_CODEC_DTSHD;
    case AF_FORMAT_S_EAC3:   return SPA_AUDIO_IEC958_CODEC_EAC3;
    case AF_FORMAT_S_MP3:    return SPA_AUDIO_IEC958_CODEC_MPEG;
    case AF_FORMAT_S_TRUEHD: return SPA_AUDIO_IEC958_CODEC_TRUEHD;
    default:                 return SPA_AUDIO_IEC958_CODEC_UNKNOWN;
    }
}

static enum spa_audio_channel mp_speaker_id_to_spa(struct ao *ao, enum mp_speaker_id mp_speaker_id)
{
    switch (mp_speaker_id) {
    case MP_SPEAKER_ID_FL:   return SPA_AUDIO_CHANNEL_FL;
    case MP_SPEAKER_ID_FR:   return SPA_AUDIO_CHANNEL_FR;
    case MP_SPEAKER_ID_FC:   return SPA_AUDIO_CHANNEL_FC;
    case MP_SPEAKER_ID_LFE:  return SPA_AUDIO_CHANNEL_LFE;
    case MP_SPEAKER_ID_BL:   return SPA_AUDIO_CHANNEL_RL;
    case MP_SPEAKER_ID_BR:   return SPA_AUDIO_CHANNEL_RR;
    case MP_SPEAKER_ID_FLC:  return SPA_AUDIO_CHANNEL_FLC;
    case MP_SPEAKER_ID_FRC:  return SPA_AUDIO_CHANNEL_FRC;
    case MP_SPEAKER_ID_BC:   return SPA_AUDIO_CHANNEL_RC;
    case MP_SPEAKER_ID_SL:   return SPA_AUDIO_CHANNEL_SL;
    case MP_SPEAKER_ID_SR:   return SPA_AUDIO_CHANNEL_SR;
    case MP_SPEAKER_ID_TC:   return SPA_AUDIO_CHANNEL_TC;
    case MP_SPEAKER_ID_TFL:  return SPA_AUDIO_CHANNEL_TFL;
    case MP_SPEAKER_ID_TFC:  return SPA_AUDIO_CHANNEL_TFC;
    case MP_SPEAKER_ID_TFR:  return SPA_AUDIO_CHANNEL_TFR;
    case MP_SPEAKER_ID_TBL:  return SPA_AUDIO_CHANNEL_TRL;
    case MP_SPEAKER_ID_TBC:  return SPA_AUDIO_CHANNEL_TRC;
    case MP_SPEAKER_ID_TBR:  return SPA_AUDIO_CHANNEL_TRR;
    case MP_SPEAKER_ID_DL:   return SPA_AUDIO_CHANNEL_FL;
    case MP_SPEAKER_ID_DR:   return SPA_AUDIO_CHANNEL_FR;
    case MP_SPEAKER_ID_WL:   return SPA_AUDIO_CHANNEL_FL;
    case MP_SPEAKER_ID_WR:   return SPA_AUDIO_CHANNEL_FR;
    case MP_SPEAKER_ID_SDL:  return SPA_AUDIO_CHANNEL_SL;
    case MP_SPEAKER_ID_SDR:  return SPA_AUDIO_CHANNEL_SL;
    case MP_SPEAKER_ID_LFE2: return SPA_AUDIO_CHANNEL_LFE2;
    case MP_SPEAKER_ID_TSL:  return SPA_AUDIO_CHANNEL_TSL;
    case MP_SPEAKER_ID_TSR:  return SPA_AUDIO_CHANNEL_TSR;
    case MP_SPEAKER_ID_BFC:  return SPA_AUDIO_CHANNEL_BC;
    case MP_SPEAKER_ID_BFL:  return SPA_AUDIO_CHANNEL_BLC;
    case MP_SPEAKER_ID_BFR:  return SPA_AUDIO_CHANNEL_BRC;
    case MP_SPEAKER_ID_NA:   return SPA_AUDIO_CHANNEL_NA;
    default:
                             MP_WARN(ao, "Unhandled channel %d\n", mp_speaker_id);
                             return SPA_AUDIO_CHANNEL_UNKNOWN;
    };
}

static void on_process(void *userdata)
{
    struct ao *ao = userdata;
    struct priv *p = ao->priv;
    struct pw_time time;
    struct pw_buffer *b;
    void *data[MP_NUM_CHANNELS];

    if ((b = pw_stream_dequeue_buffer(p->stream)) == NULL) {
        MP_WARN(ao, "out of buffers: %s\n", mp_strerror(errno));
        return;
    }

    struct spa_buffer *buf = b->buffer;

    int nframes = buf->datas[0].maxsize / ao->sstride;
#if PW_CHECK_VERSION(0, 3, 49)
    if (b->requested != 0)
        nframes = MPMIN(b->requested, nframes);
#endif

    for (int i = 0; i < buf->n_datas; i++)
        data[i] = buf->datas[i].data;

    pw_stream_get_time_n(p->stream, &time, sizeof(time));
    if (time.rate.denom == 0)
        time.rate.denom = ao->samplerate;
    if (time.rate.num == 0)
        time.rate.num = 1;

    int64_t end_time = mp_time_ns();
    /* time.queued is always going to be 0, so we don't need to care */
    end_time += (nframes * 1e9 / ao->samplerate) +
                ((double) time.delay * SPA_NSEC_PER_SEC * time.rate.num / time.rate.denom);

    int samples = ao_read_data_nonblocking(ao, data, nframes, end_time);
    b->size = samples;

    for (int i = 0; i < buf->n_datas; i++) {
        buf->datas[i].chunk->size = samples * ao->sstride;
        buf->datas[i].chunk->offset = 0;
        buf->datas[i].chunk->stride = ao->sstride;
    }

    pw_stream_queue_buffer(p->stream, b);

    MP_TRACE(ao, "queued %d of %d samples\n", samples, nframes);
}

static void on_param_changed(void *userdata, uint32_t id, const struct spa_pod *param)
{
    struct ao *ao = userdata;
    struct priv *p = ao->priv;
    const struct spa_pod *params[1];
    uint8_t buffer[1024];
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));

    /* We want to know when our node is linked.
     * As there is no proper callback for this we use the Latency param for this
     */
    if (id == SPA_PARAM_Latency) {
        p->init_state = INIT_STATE_SUCCESS;
        pw_thread_loop_signal(p->loop, false);
    }

    if (param == NULL || id != SPA_PARAM_Format)
        return;

    int buffer_size = ao->device_buffer * af_fmt_to_bytes(ao->format) * ao->channels.num;

    params[0] = spa_pod_builder_add_object(&b,
                    SPA_TYPE_OBJECT_ParamBuffers, SPA_PARAM_Buffers,
                    SPA_PARAM_BUFFERS_blocks,     SPA_POD_Int(ao->num_planes),
                    SPA_PARAM_BUFFERS_size,       SPA_POD_CHOICE_RANGE_Int(
                                                    buffer_size, 0, INT32_MAX),
                    SPA_PARAM_BUFFERS_stride,     SPA_POD_Int(ao->sstride));
    if (!params[0]) {
        MP_ERR(ao, "Could not build parameter pod\n");
        return;
    }

    if (pw_stream_update_params(p->stream, params, 1) < 0) {
        MP_ERR(ao, "Could not update stream parameters\n");
        return;
    }
}

static void on_state_changed(void *userdata, enum pw_stream_state old, enum pw_stream_state state, const char *error)
{
    struct ao *ao = userdata;
    struct priv *p = ao->priv;
    MP_DBG(ao, "Stream state changed: old_state=%s state=%s error=%s\n",
           pw_stream_state_as_string(old), pw_stream_state_as_string(state), error);

    if (state == PW_STREAM_STATE_ERROR) {
        MP_WARN(ao, "Stream in error state, trying to reload...\n");
        p->init_state = INIT_STATE_ERROR;
        pw_thread_loop_signal(p->loop, false);
        ao_request_reload(ao);
    }

    if (state == PW_STREAM_STATE_UNCONNECTED && old != PW_STREAM_STATE_UNCONNECTED) {
        MP_WARN(ao, "Stream disconnected, trying to reload...\n");
        ao_request_reload(ao);
    }
}

static float spa_volume_to_mp_volume(float vol)
{
        return vol * 100;
}

static float mp_volume_to_spa_volume(float vol)
{
        return vol / 100;
}

static float volume_avg(float* vols, uint32_t n)
{
    float sum = 0.0;
    for (int i = 0; i < n; i++)
        sum += vols[i];
    return sum / n;
}

static void on_control_info(void *userdata, uint32_t id,
        const struct pw_stream_control *control)
{
    struct ao *ao = userdata;
    struct priv *p = ao->priv;

    switch (id) {
        case SPA_PROP_mute:
            if (control->n_values == 1)
                p->muted = control->values[0] >= 0.5;
            break;
        case SPA_PROP_channelVolumes:
            if (p->options.volume_mode != VOLUME_MODE_CHANNEL)
                break;
            if (control->n_values > 0)
                p->volume = volume_avg(control->values, control->n_values);
            break;
        case SPA_PROP_volume:
            if (p->options.volume_mode != VOLUME_MODE_GLOBAL)
                break;
            if (control->n_values > 0)
                p->volume = control->values[0];
            break;
    }
}

static const struct pw_stream_events stream_events = {
    .version = PW_VERSION_STREAM_EVENTS,
    .param_changed = on_param_changed,
    .process = on_process,
    .state_changed = on_state_changed,
    .control_info = on_control_info,
};

static void uninit(struct ao *ao)
{
    struct priv *p = ao->priv;
    if (p->loop)
        pw_thread_loop_stop(p->loop);
    spa_hook_remove(&p->stream_listener);
    spa_zero(p->stream_listener);
    if (p->stream)
        pw_stream_destroy(p->stream);
    p->stream = NULL;
    if (p->core)
        pw_context_destroy(pw_core_get_context(p->core));
    p->core = NULL;
    if (p->loop)
        pw_thread_loop_destroy(p->loop);
    p->loop = NULL;
    pw_deinit();
}

struct registry_event_global_ctx {
    struct ao *ao;
    void (*sink_cb) (struct ao *ao, uint32_t id, const struct spa_dict *props, void *sink_cb_ctx);
    void *sink_cb_ctx;
};

static bool is_sink_node(const char *type, const struct spa_dict *props)
{
    if (strcmp(type, PW_TYPE_INTERFACE_Node) != 0)
        return false;

    if (!props)
        return false;

    const char *class = spa_dict_lookup(props, PW_KEY_MEDIA_CLASS);
    if (!class || strcmp(class, "Audio/Sink") != 0)
        return false;

    return true;
}

static void for_each_sink_registry_event_global(void *data, uint32_t id,
                                                uint32_t permissions, const
                                                char *type, uint32_t version,
                                                const struct spa_dict *props)
{
    struct registry_event_global_ctx *ctx = data;

    if (!is_sink_node(type, props))
        return;

    ctx->sink_cb(ctx->ao, id, props, ctx->sink_cb_ctx);
}


struct for_each_done_ctx {
    struct pw_thread_loop *loop;
    bool done;
};

static const struct pw_registry_events for_each_sink_registry_events = {
    .version = PW_VERSION_REGISTRY_EVENTS,
    .global = for_each_sink_registry_event_global,
};

static void for_each_sink_done(void *data, uint32_t it, int seq)
{
    struct for_each_done_ctx *ctx = data;
    ctx->done = true;
    pw_thread_loop_signal(ctx->loop, false);
}

static const struct pw_core_events for_each_sink_core_events = {
    .version = PW_VERSION_CORE_EVENTS,
    .done = for_each_sink_done,
};

static int for_each_sink(struct ao *ao, void (cb) (struct ao *ao, uint32_t id,
                         const struct spa_dict *props, void *ctx), void *cb_ctx)
{
    struct priv *priv = ao->priv;
    struct pw_registry *registry;
    struct spa_hook core_listener;
    struct for_each_done_ctx done_ctx = {
        .loop = priv->loop,
        .done = false,
    };
    int ret = -1;

    pw_thread_loop_lock(priv->loop);

    spa_zero(core_listener);
    if (pw_core_add_listener(priv->core, &core_listener, &for_each_sink_core_events, &done_ctx) < 0)
        goto unlock_loop;

    registry = pw_core_get_registry(priv->core, PW_VERSION_REGISTRY, 0);
    if (!registry)
        goto remove_core_listener;

    pw_core_sync(priv->core, 0, 0);

    struct spa_hook registry_listener;
    struct registry_event_global_ctx revents_ctx = {
            .ao = ao,
            .sink_cb = cb,
            .sink_cb_ctx = cb_ctx,
    };
    spa_zero(registry_listener);
    if (pw_registry_add_listener(registry, &registry_listener, &for_each_sink_registry_events, &revents_ctx) < 0)
        goto destroy_registry;

    while (!done_ctx.done)
        pw_thread_loop_wait(priv->loop);

    spa_hook_remove(&registry_listener);

    ret = 0;

destroy_registry:
    pw_proxy_destroy((struct pw_proxy *)registry);

remove_core_listener:
    spa_hook_remove(&core_listener);

unlock_loop:
    pw_thread_loop_unlock(priv->loop);

    return ret;
}

static void have_sink(struct ao *ao, uint32_t id, const struct spa_dict *props, void *ctx)
{
    bool *b = ctx;
    *b = true;
}

static bool session_has_sinks(struct ao *ao)
{
    bool b = false;

    if (for_each_sink(ao, have_sink, &b) < 0)
        MP_WARN(ao, "Could not list devices, sink detection may be wrong\n");

    return b;
}

static void on_error(void *data, uint32_t id, int seq, int res, const char *message)
{
    struct ao *ao = data;

    MP_WARN(ao, "Error during playback: %s, %s\n", spa_strerror(res), message);
}

static void on_core_info(void *data, const struct pw_core_info *info)
{
    struct ao *ao = data;

    MP_VERBOSE(ao, "Core user: %s\n", info->user_name);
    MP_VERBOSE(ao, "Core host: %s\n", info->host_name);
    MP_VERBOSE(ao, "Core version: %s\n", info->version);
    MP_VERBOSE(ao, "Core name: %s\n", info->name);
}

static const struct pw_core_events core_events = {
    .version = PW_VERSION_CORE_EVENTS,
    .error = on_error,
    .info = on_core_info,
};

static int pipewire_init_boilerplate(struct ao *ao)
{
    struct priv *p = ao->priv;
    struct pw_context *context;

    pw_init(NULL, NULL);

    MP_VERBOSE(ao, "Headers version: %s\n", pw_get_headers_version());
    MP_VERBOSE(ao, "Library version: %s\n", pw_get_library_version());

    p->loop = pw_thread_loop_new("mpv/ao/pipewire", NULL);
    if (p->loop == NULL)
        return -1;

    pw_thread_loop_lock(p->loop);

    if (pw_thread_loop_start(p->loop) < 0)
        goto error;

    context = pw_context_new(
            pw_thread_loop_get_loop(p->loop),
            pw_properties_new(PW_KEY_CONFIG_NAME, "client-rt.conf", NULL),
            0);
    if (!context)
        goto error;

    p->core = pw_context_connect(
            context,
            pw_properties_new(PW_KEY_REMOTE_NAME, p->options.remote, NULL),
            0);
    if (!p->core) {
        MP_MSG(ao, ao->probing ? MSGL_V : MSGL_ERR,
               "Could not connect to context '%s': %s\n",
               p->options.remote, mp_strerror(errno));
        pw_context_destroy(context);
        goto error;
    }

    if (pw_core_add_listener(p->core, &p->core_listener, &core_events, ao) < 0)
        goto error;

    pw_thread_loop_unlock(p->loop);

    if (!session_has_sinks(ao)) {
        MP_VERBOSE(ao, "PipeWire does not have any audio sinks, skipping\n");
        return -1;
    }

    return 0;

error:
    pw_thread_loop_unlock(p->loop);
    return -1;
}

static void wait_for_init_done(struct ao *ao)
{
    struct priv *p = ao->priv;
    struct timespec abstime;
    int r;

    r = pw_thread_loop_get_time(p->loop, &abstime, 50 * SPA_NSEC_PER_MSEC);
    if (r < 0) {
        MP_WARN(ao, "Could not get timeout for initialization: %s\n", spa_strerror(r));
        return;
    }

    while (p->init_state == INIT_STATE_NONE) {
        r = pw_thread_loop_timed_wait_full(p->loop, &abstime);
        if (r < 0) {
            MP_WARN(ao, "Could not wait for initialization: %s\n", spa_strerror(r));
            return;
        }
    }
}

static int init(struct ao *ao)
{
    struct priv *p = ao->priv;
    uint8_t buffer[1024];
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
    const struct spa_pod *params[1];
    struct pw_properties *props = pw_properties_new(
        PW_KEY_MEDIA_TYPE, "Audio",
        PW_KEY_MEDIA_CATEGORY, "Playback",
        PW_KEY_MEDIA_ROLE, ao->init_flags & AO_INIT_MEDIA_ROLE_MUSIC ?  "Music" : "Movie",
        PW_KEY_NODE_NAME, ao->client_name,
        PW_KEY_NODE_DESCRIPTION, ao->client_name,
        PW_KEY_APP_NAME, ao->client_name,
        PW_KEY_APP_ID, ao->client_name,
        PW_KEY_APP_ICON_NAME, ao->client_name,
        PW_KEY_NODE_ALWAYS_PROCESS, "true",
        PW_KEY_TARGET_OBJECT, ao->device,
        NULL
    );

    if (pipewire_init_boilerplate(ao) < 0)
        goto error_props;

    if (p->options.buffer_msec) {
        ao->device_buffer = p->options.buffer_msec * ao->samplerate / 1000;

        pw_properties_setf(props, PW_KEY_NODE_LATENCY, "%d/%d", ao->device_buffer, ao->samplerate);
    }

    pw_properties_setf(props, PW_KEY_NODE_RATE, "1/%d", ao->samplerate);

    if (af_fmt_is_spdif(ao->format)) {
        enum spa_audio_iec958_codec spa_codec = af_fmt_to_codec(ao->format);
        if (spa_codec == SPA_AUDIO_IEC958_CODEC_UNKNOWN) {
            MP_ERR(ao, "Unhandled codec %d\n", ao->format);
            goto error_props;
        }

        struct spa_audio_info_iec958 audio_info = {
            .codec = spa_codec,
            .rate = ao->samplerate,
        };

        params[0] = spa_format_audio_iec958_build(&b, SPA_PARAM_EnumFormat, &audio_info);
        if (!params[0])
            goto error_props;
    } else {
        enum spa_audio_format spa_format = af_fmt_to_pw(ao->format);
        if (spa_format == SPA_AUDIO_FORMAT_UNKNOWN) {
            MP_ERR(ao, "Unhandled format %d\n", ao->format);
            goto error_props;
        }

        struct spa_audio_info_raw audio_info = {
            .format = spa_format,
            .rate = ao->samplerate,
            .channels = ao->channels.num,
        };

        for (int i = 0; i < ao->channels.num; i++)
            audio_info.position[i] = mp_speaker_id_to_spa(ao, ao->channels.speaker[i]);

        params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &audio_info);
        if (!params[0])
            goto error_props;
    }

    if (af_fmt_is_planar(ao->format)) {
        ao->num_planes = ao->channels.num;
        ao->sstride = af_fmt_to_bytes(ao->format);
    } else {
        ao->num_planes = 1;
        ao->sstride = ao->channels.num * af_fmt_to_bytes(ao->format);
    }

    pw_thread_loop_lock(p->loop);

    p->stream = pw_stream_new(p->core, "audio-src", props);
    if (p->stream == NULL) {
        pw_thread_loop_unlock(p->loop);
        goto error;
    }

    pw_stream_add_listener(p->stream, &p->stream_listener, &stream_events, ao);

    enum pw_stream_flags flags = PW_STREAM_FLAG_AUTOCONNECT |
                                 PW_STREAM_FLAG_INACTIVE |
                                 PW_STREAM_FLAG_MAP_BUFFERS |
                                 PW_STREAM_FLAG_RT_PROCESS;

    if (ao->init_flags & AO_INIT_EXCLUSIVE)
        flags |= PW_STREAM_FLAG_EXCLUSIVE;

    if (pw_stream_connect(p->stream,
                    PW_DIRECTION_OUTPUT, PW_ID_ANY, flags, params, 1) < 0) {
        pw_thread_loop_unlock(p->loop);
        goto error;
    }

    wait_for_init_done(ao);

    pw_thread_loop_unlock(p->loop);

    if (p->init_state == INIT_STATE_ERROR)
        goto error;

    return 0;

error_props:
    pw_properties_free(props);
error:
    uninit(ao);
    return -1;
}

static void reset(struct ao *ao)
{
    struct priv *p = ao->priv;
    pw_thread_loop_lock(p->loop);
    pw_stream_set_active(p->stream, false);
    pw_stream_flush(p->stream, false);
    pw_thread_loop_unlock(p->loop);
}

static void start(struct ao *ao)
{
    struct priv *p = ao->priv;
    pw_thread_loop_lock(p->loop);
    pw_stream_set_active(p->stream, true);
    pw_thread_loop_unlock(p->loop);
}

#define CONTROL_RET(r) (!r ? CONTROL_OK : CONTROL_ERROR)

static int control(struct ao *ao, enum aocontrol cmd, void *arg)
{
    struct priv *p = ao->priv;

    switch (cmd) {
        case AOCONTROL_GET_VOLUME: {
            float *vol = arg;
            *vol = spa_volume_to_mp_volume(p->volume);
            return CONTROL_OK;
        }
        case AOCONTROL_GET_MUTE: {
            bool *muted = arg;
            *muted = p->muted;
            return CONTROL_OK;
        }
        case AOCONTROL_SET_VOLUME:
        case AOCONTROL_SET_MUTE:
        case AOCONTROL_UPDATE_STREAM_TITLE: {
            int ret;

            pw_thread_loop_lock(p->loop);
            switch (cmd) {
                case AOCONTROL_SET_VOLUME: {
                    float *vol = arg;
                    uint8_t n = ao->channels.num;
                    if (p->options.volume_mode == VOLUME_MODE_CHANNEL) {
                        float values[MP_NUM_CHANNELS] = {0};
                        for (int i = 0; i < n; i++)
                            values[i] = mp_volume_to_spa_volume(*vol);
                        ret = CONTROL_RET(pw_stream_set_control(
                                    p->stream, SPA_PROP_channelVolumes, n, values, 0));
                    } else {
                        float value = mp_volume_to_spa_volume(*vol);
                        ret = CONTROL_RET(pw_stream_set_control(
                                    p->stream, SPA_PROP_volume, 1, &value, 0));
                    }
                    break;
                }
                case AOCONTROL_SET_MUTE: {
                    bool *muted = arg;
                    float value = *muted ? 1.f : 0.f;
                    ret = CONTROL_RET(pw_stream_set_control(p->stream, SPA_PROP_mute, 1, &value, 0));
                    break;
                }
                case AOCONTROL_UPDATE_STREAM_TITLE: {
                    char *title = arg;
                    struct spa_dict_item items[1];
                    items[0] = SPA_DICT_ITEM_INIT(PW_KEY_MEDIA_NAME, title);
                    ret = CONTROL_RET(pw_stream_update_properties(p->stream, &SPA_DICT_INIT(items, MP_ARRAY_SIZE(items))));
                    break;
                }
                default:
                    ret = CONTROL_NA;
            }
            pw_thread_loop_unlock(p->loop);
            return ret;
        }
        default:
            return CONTROL_UNKNOWN;
    }
}

static void add_device_to_list(struct ao *ao, uint32_t id, const struct spa_dict *props, void *ctx)
{
    struct ao_device_list *list = ctx;
    const char *name = spa_dict_lookup(props, PW_KEY_NODE_NAME);

    if (!name)
        return;

    const char *description = spa_dict_lookup(props, PW_KEY_NODE_DESCRIPTION);

    ao_device_list_add(list, ao, &(struct ao_device_desc){name, description});
}

static void hotplug_registry_global_cb(void *data, uint32_t id,
                                       uint32_t permissions, const char *type,
                                       uint32_t version, const struct spa_dict *props)
{
    struct ao *ao = data;
    struct priv *priv = ao->priv;

    if (!is_sink_node(type, props))
        return;

    pw_thread_loop_lock(priv->loop);
    struct id_list *item = talloc(ao, struct id_list);
    item->id = id;
    spa_list_init(&item->node);
    spa_list_append(&priv->hotplug.sinks, &item->node);
    pw_thread_loop_unlock(priv->loop);

    ao_hotplug_event(ao);
}

static void hotplug_registry_global_remove_cb(void *data, uint32_t id)
{
    struct ao *ao = data;
    struct priv *priv = ao->priv;
    bool removed_sink = false;

    struct id_list *e;

    pw_thread_loop_lock(priv->loop);
    spa_list_for_each(e, &priv->hotplug.sinks, node) {
        if (e->id == id) {
            removed_sink = true;
            spa_list_remove(&e->node);
            talloc_free(e);
            break;
        }
    }

    pw_thread_loop_unlock(priv->loop);

    if (removed_sink)
        ao_hotplug_event(ao);
}

static const struct pw_registry_events hotplug_registry_events = {
    .version = PW_VERSION_REGISTRY_EVENTS,
    .global = hotplug_registry_global_cb,
    .global_remove = hotplug_registry_global_remove_cb,
};

static int hotplug_init(struct ao *ao)
{
    struct priv *priv = ao->priv;

    int res = pipewire_init_boilerplate(ao);
    if (res)
        goto error_no_unlock;

    pw_thread_loop_lock(priv->loop);

    spa_zero(priv->hotplug);
    spa_list_init(&priv->hotplug.sinks);

    priv->hotplug.registry = pw_core_get_registry(priv->core, PW_VERSION_REGISTRY, 0);
    if (!priv->hotplug.registry)
        goto error;

    if (pw_registry_add_listener(priv->hotplug.registry, &priv->hotplug.registry_listener, &hotplug_registry_events, ao) < 0) {
        pw_proxy_destroy((struct pw_proxy *)priv->hotplug.registry);
        goto error;
    }

    pw_thread_loop_unlock(priv->loop);

    return res;

error:
    pw_thread_loop_unlock(priv->loop);
error_no_unlock:
    uninit(ao);
    return -1;
}

static void hotplug_uninit(struct ao *ao)
{
    struct priv *priv = ao->priv;

    pw_thread_loop_lock(priv->loop);

    spa_hook_remove(&priv->hotplug.registry_listener);
    pw_proxy_destroy((struct pw_proxy *)priv->hotplug.registry);

    pw_thread_loop_unlock(priv->loop);
    uninit(ao);
}

static void list_devs(struct ao *ao, struct ao_device_list *list)
{
    ao_device_list_add(list, ao, &(struct ao_device_desc){});

    if (for_each_sink(ao, add_device_to_list, list) < 0)
        MP_WARN(ao, "Could not list devices, list may be incomplete\n");
}

#define OPT_BASE_STRUCT struct priv

const struct ao_driver audio_out_pipewire = {
    .description = "PipeWire audio output",
    .name        = "pipewire",

    .init        = init,
    .uninit      = uninit,
    .reset       = reset,
    .start       = start,

    .control     = control,

    .hotplug_init   = hotplug_init,
    .hotplug_uninit = hotplug_uninit,
    .list_devs      = list_devs,

    .priv_size = sizeof(struct priv),
    .priv_defaults = &(const struct priv)
    {
        .loop = NULL,
        .stream = NULL,
        .init_state = INIT_STATE_NONE,
        .options.buffer_msec = 0,
        .options.volume_mode = VOLUME_MODE_CHANNEL,
    },
    .options_prefix = "pipewire",
    .options = (const struct m_option[]) {
        {"buffer", OPT_CHOICE(options.buffer_msec, {"native", 0}),
            M_RANGE(1, 2000)},
        {"remote", OPT_STRING(options.remote) },
        {"volume-mode", OPT_CHOICE(options.volume_mode,
            {"channel", VOLUME_MODE_CHANNEL}, {"global", VOLUME_MODE_GLOBAL})},
        {0}
    },
};
