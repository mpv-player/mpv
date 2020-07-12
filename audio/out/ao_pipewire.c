#include <limits.h>

#include <pipewire-0.3/pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <spa/param/props.h>

#include "ao.h"
#include "audio/format.h"
#include "config.h"
#include "internal.h"
#include "osdep/timer.h"

static void on_process(void *userdata);

struct priv {
    struct pw_thread_loop *loop;
    struct pw_stream *stream;
};

static const struct pw_stream_events stream_events = {
    PW_VERSION_STREAM_EVENTS,
    .process = on_process,
};

static void on_process(void *userdata)
{
    struct ao *ao = userdata;
    struct priv *p = ao->priv;

    struct pw_buffer *b;
    struct spa_buffer *buf;
    void * data_pointer[16];
    int nframes = 0;

    pw_thread_loop_lock(p->loop);
    if ((b = pw_stream_dequeue_buffer(p->stream)) == NULL) {
        pw_log_warn("out of buffers: %m");
        return;
    }
    buf = b->buffer;

    unsigned int maxbuf = UINT_MAX;
    for (int i = 0; i < ao->channels.num; ++i) {
        if ((data_pointer[i] = buf->datas[i].data) == NULL)
            return;
        maxbuf = buf->datas[i].maxsize < maxbuf ? buf->datas[i].maxsize : maxbuf;
    }

    nframes = maxbuf / ao->sstride / 2;
    int64_t end_time = mp_time_us();

    struct pw_time time = {0};
    pw_stream_get_time(p->stream, &time);
    if (time.rate.denom == 0)
        time.rate.denom = 1;

    nframes = ao_read_data(ao, data_pointer, nframes,
                           end_time + (nframes + time.queued / ao->channels.num / ao->sstride + time.delay)
                           * 1e6 / time.rate.denom);
    b->size = 0;
    for (int i = 0; i < ao->channels.num; ++i) {
        buf->datas[i].chunk->offset = 0;
        buf->datas[i].chunk->stride = ao->sstride;
        buf->datas[i].chunk->size = nframes * ao->sstride;
        b->size += buf->datas[i].chunk->size;
    }

    pw_stream_queue_buffer(p->stream, b);
    pw_thread_loop_unlock(p->loop);
}

static int init(struct ao *ao)
{
    struct priv *p = ao->priv;

    uint8_t buffer[1024];
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
    const struct spa_pod *params[1];
    int argc = 1;
    char arg[] = "mpv";
    char * argv[1];
    argv[0] = arg;

    pw_init(&argc, (char***)&argv);

    p->loop = pw_thread_loop_new("ao-pipewire", NULL);
    p->stream = pw_stream_new_simple(
                    pw_thread_loop_get_loop(p->loop),
                    "audio-src",
                    pw_properties_new(
                        PW_KEY_MEDIA_TYPE, "Audio",
                        PW_KEY_MEDIA_CATEGORY, "Playback",
                        PW_KEY_MEDIA_ROLE, "Music",
                        NULL),
                    &stream_events,
                    ao);

    struct mp_chmap_sel sel = {0};
    mp_chmap_sel_add_waveext_def(&sel);
    ao_chmap_sel_adjust(ao, &sel, &ao->channels);
    ao_chmap_sel_get_def(ao, &sel, &ao->channels, ao->channels.num);

    ao->format = AF_FORMAT_FLOATP;
    ao->sstride = ao->channels.num * sizeof(float);
    ao->samplerate = 48000;
    params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat,
                                           &SPA_AUDIO_INFO_RAW_INIT(
                                                   .format = SPA_AUDIO_FORMAT_F32P,
                                                   .channels = ao->channels.num,
                                                   .rate = ao->samplerate));

    pw_stream_connect(p->stream,
                      PW_DIRECTION_OUTPUT,
                      PW_ID_ANY,
                      PW_STREAM_FLAG_AUTOCONNECT |
                      PW_STREAM_FLAG_MAP_BUFFERS |
                      PW_STREAM_FLAG_RT_PROCESS,
                      params, 1);

    pw_stream_set_active(p->stream, true);

    return 0;
}

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

static void reset(struct ao *ao)
{
    struct priv *p = ao->priv;
    pw_thread_loop_stop(p->loop);
}

static void start(struct ao *ao)
{
    struct priv *p = ao->priv;
    pw_thread_loop_start(p->loop);
}

const struct ao_driver audio_out_pipewire = {
    .description = "PipeWire audio output",
    .name      = "pipewire",

    .init      = init,
    .uninit    = uninit,
    .reset     = reset,
    .start     = start,

    .priv_size = sizeof(struct priv),
    .priv_defaults = &(const struct priv)
    {
        .loop = NULL,
        .stream = NULL,
    },
};
