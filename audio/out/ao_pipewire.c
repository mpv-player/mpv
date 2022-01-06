
#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <spa/param/props.h>
#include <math.h>

#include "common/msg.h"
#include "options/m_config.h"
#include "options/m_option.h"
#include "ao.h"
#include "audio/format.h"
#include "config.h"
#include "generated/version.h"
#include "internal.h"
#include "osdep/timer.h"

// Added in Pipewire 0.3.33
// remove the fallback when we require a newer version
#ifndef PW_KEY_NODE_RATE
#define PW_KEY_NODE_RATE "node.rate"
#endif

struct priv {
    struct pw_thread_loop *loop;
    struct pw_stream *stream;

    int buffer_msec;
    bool muted;
    float volume[2];
};

static enum spa_audio_format af_fmt_to_pw(struct ao *ao, enum af_format format)
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
    default:
                                MP_WARN(ao, "Unhandled format %d\n", format);
                                return SPA_AUDIO_FORMAT_UNKNOWN;
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
        pw_log_warn("out of buffers: %m");
        return;
    }

    struct spa_buffer *buf = b->buffer;

    int bytes_per_channel = buf->datas[0].maxsize / ao->channels.num;
    int nframes = bytes_per_channel / ao->sstride;

    for (int i = 0; i < buf->n_datas; i++) {
        data[i] = buf->datas[i].data;
        buf->datas[i].chunk->size = bytes_per_channel;
        buf->datas[i].chunk->offset = 0;
    }

    pw_stream_get_time(p->stream, &time);
    if (time.rate.denom == 0)
        time.rate.denom = ao->samplerate;
    if (time.rate.num == 0)
        time.rate.num = 1;

    int64_t end_time = mp_time_us();
    /* time.queued is always going to be 0, so we don't need to care */
    end_time += (nframes * 1e6 / ao->samplerate) +
                ((float) time.delay * SPA_USEC_PER_SEC * time.rate.num / time.rate.denom);

    ao_read_data(ao, data, nframes, end_time);

    pw_stream_queue_buffer(p->stream, b);
}

static void on_param_changed(void *userdata, uint32_t id, const struct spa_pod *param)
{
    struct ao *ao = userdata;
    struct priv *p = ao->priv;
    const struct spa_pod *params[1];
    uint8_t buffer[1024];
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));

    if (param == NULL || id != SPA_PARAM_Format)
        return;

    int buffer_size = ao->device_buffer * af_fmt_to_bytes(ao->format) * ao->channels.num;

    params[0] = spa_pod_builder_add_object(&b,
                    SPA_TYPE_OBJECT_ParamBuffers, SPA_PARAM_Buffers,
                    SPA_PARAM_BUFFERS_blocks,     SPA_POD_Int(ao->num_planes),
                    SPA_PARAM_BUFFERS_size,       SPA_POD_Int(buffer_size),
                    SPA_PARAM_BUFFERS_stride,     SPA_POD_Int(ao->sstride));

    pw_stream_update_params(p->stream, params, 1);
}

static void on_state_changed(void *userdata, enum pw_stream_state old, enum pw_stream_state state, const char *error)
{
    struct ao *ao = userdata;
    MP_DBG(ao, "Stream state changed: old_state=%d state=%d error=%s\n", old, state, error);

    if (state == PW_STREAM_STATE_ERROR) {
        MP_WARN(ao, "Stream in error state, trying to reload...\n");
        ao_request_reload(ao);
    }
}

static float spa_volume_to_mp_volume(float vol)
{
        return cbrt(vol) * 100;
}

static float mp_volume_to_spa_volume(float vol)
{
        vol /= 100;
        return vol * vol * vol;
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
            if (control->n_values == 1) {
                p->volume[0] = control->values[0];
                p->volume[1] = control->values[0];
            } else if (control->n_values == 2) {
                p->volume[0] = control->values[0];
                p->volume[1] = control->values[1];
            }
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
    if (p->stream)
        pw_stream_destroy(p->stream);
    p->stream = NULL;
    if (p->loop)
        pw_thread_loop_destroy(p->loop);
    p->loop = NULL;
    pw_deinit();
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
        PW_KEY_MEDIA_ROLE, "Movie",
        PW_KEY_NODE_NAME, ao->client_name,
        PW_KEY_NODE_DESCRIPTION, ao->client_name,
        PW_KEY_APP_NAME, ao->client_name,
        PW_KEY_APP_ID, ao->client_name,
        PW_KEY_APP_ICON_NAME, ao->client_name,
        PW_KEY_NODE_ALWAYS_PROCESS, "true",
        NULL
    );

    ao->device_buffer = p->buffer_msec * ao->samplerate / 1000;

    pw_properties_setf(props, PW_KEY_NODE_LATENCY, "%d/%d", ao->device_buffer, ao->samplerate);
    pw_properties_setf(props, PW_KEY_NODE_RATE, "1/%d", ao->samplerate);

    enum spa_audio_format spa_format = af_fmt_to_pw(ao, ao->format);
    if (spa_format == SPA_AUDIO_FORMAT_UNKNOWN) {
        ao->format = AF_FORMAT_FLOATP;
        spa_format = SPA_AUDIO_FORMAT_F32P;
    }

    struct spa_audio_info_raw audio_info = {
        .format = spa_format,
        .rate = ao->samplerate,
        .channels = ao->channels.num,
    };

    for (int i = 0; i < ao->channels.num; i++)
        audio_info.position[i] = mp_speaker_id_to_spa(ao, ao->channels.speaker[i]);

    params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &audio_info);

    if (af_fmt_is_planar(ao->format)) {
        ao->num_planes = ao->channels.num;
        ao->sstride = af_fmt_to_bytes(ao->format);
    } else {
        ao->num_planes = 1;
        ao->sstride = ao->channels.num * af_fmt_to_bytes(ao->format);
    }

    pw_init(NULL, NULL);

    p->loop = pw_thread_loop_new("ao-pipewire", NULL);
    if (p->loop == NULL)
        goto error;

    p->stream = pw_stream_new_simple(
                    pw_thread_loop_get_loop(p->loop),
                    "audio-src",
                    props,
                    &stream_events,
                    ao);
    if (p->stream == NULL)
        goto error;

    if (pw_stream_connect(p->stream,
                    PW_DIRECTION_OUTPUT,
                    PW_ID_ANY,
                    PW_STREAM_FLAG_AUTOCONNECT |
                    PW_STREAM_FLAG_INACTIVE |
                    PW_STREAM_FLAG_MAP_BUFFERS |
                    PW_STREAM_FLAG_RT_PROCESS,
                    params, 1) < 0)
        goto error;

    if (pw_thread_loop_start(p->loop) < 0)
        goto error;

    return 0;

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
                struct ao_control_vol *vol = arg;
                vol->left = spa_volume_to_mp_volume(p->volume[0]);
                vol->right = spa_volume_to_mp_volume(p->volume[1]);
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
                    struct ao_control_vol *vol = arg;
                    float left = mp_volume_to_spa_volume(vol->left), right = mp_volume_to_spa_volume(vol->right);
                    ret = CONTROL_RET(pw_stream_set_control(p->stream, SPA_PROP_channelVolumes, 2, &left, &right));
                    break;
               }
                case AOCONTROL_SET_MUTE: {
                    bool *muted = arg;
                    float value = *muted ? 1.f : 0.f;
                    ret = CONTROL_RET(pw_stream_set_control(p->stream, SPA_PROP_mute, 1, &value));
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

#define OPT_BASE_STRUCT struct priv

const struct ao_driver audio_out_pipewire = {
    .description = "PipeWire audio output",
    .name        = "pipewire",

    .init        = init,
    .uninit      = uninit,
    .reset       = reset,
    .start       = start,

    .control     = control,

    .priv_size = sizeof(struct priv),
    .priv_defaults = &(const struct priv)
    {
        .loop = NULL,
        .stream = NULL,
        .buffer_msec = 20,
    },
    .options_prefix = "pipewire",
    .options = (const struct m_option[]) {
        {"buffer", OPT_INT(buffer_msec), M_RANGE(1, 2000)},
        {0}
    },
};
