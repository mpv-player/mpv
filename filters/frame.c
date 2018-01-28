#include <libavutil/frame.h>

#include "audio/aframe.h"
#include "common/av_common.h"
#include "demux/packet.h"
#include "video/mp_image.h"

#include "frame.h"

struct frame_handler {
    const char *name;
    bool is_data;
    bool is_signaling;
    void *(*new_ref)(void *data);
    double (*get_pts)(void *data);
    void (*set_pts)(void *data, double pts);
    AVFrame *(*new_av_ref)(void *data);
    void *(*from_av_ref)(AVFrame *data);
    void (*free)(void *data);
};

static void *video_ref(void *data)
{
    return mp_image_new_ref(data);
}

static double video_get_pts(void *data)
{
    return ((struct mp_image *)data)->pts;
}

static void video_set_pts(void *data, double pts)
{
    ((struct mp_image *)data)->pts = pts;
}

static AVFrame *video_new_av_ref(void *data)
{
    return mp_image_to_av_frame(data);
}

static void *video_from_av_ref(AVFrame *data)
{
    return mp_image_from_av_frame(data);
}

static void *audio_ref(void *data)
{
    return mp_aframe_new_ref(data);
}

static double audio_get_pts(void *data)
{
    return mp_aframe_get_pts(data);
}

static void audio_set_pts(void *data, double pts)
{
    mp_aframe_set_pts(data, pts);
}

static AVFrame *audio_new_av_ref(void *data)
{
    return mp_aframe_to_avframe(data);
}

static void *audio_from_av_ref(AVFrame *data)
{
    return mp_aframe_from_avframe(data);
}

static void *packet_ref(void *data)
{
    return demux_copy_packet(data);
}

static const struct frame_handler frame_handlers[] = {
    [MP_FRAME_NONE] = {
        .name = "none",
    },
    [MP_FRAME_EOF] = {
        .name = "eof",
        .is_signaling = true,
    },
    [MP_FRAME_VIDEO] = {
        .name = "video",
        .is_data = true,
        .new_ref = video_ref,
        .get_pts = video_get_pts,
        .set_pts = video_set_pts,
        .new_av_ref = video_new_av_ref,
        .from_av_ref = video_from_av_ref,
        .free = talloc_free,
    },
    [MP_FRAME_AUDIO] = {
        .name = "audio",
        .is_data = true,
        .new_ref = audio_ref,
        .get_pts = audio_get_pts,
        .set_pts = audio_set_pts,
        .new_av_ref = audio_new_av_ref,
        .from_av_ref = audio_from_av_ref,
        .free = talloc_free,
    },
    [MP_FRAME_PACKET] = {
        .name = "packet",
        .is_data = true,
        .new_ref = packet_ref,
        .free = talloc_free,
    },
};

const char *mp_frame_type_str(enum mp_frame_type t)
{
    return frame_handlers[t].name;
}

bool mp_frame_is_data(struct mp_frame frame)
{
    return frame_handlers[frame.type].is_data;
}

bool mp_frame_is_signaling(struct mp_frame frame)
{
    return frame_handlers[frame.type].is_signaling;
}

void mp_frame_unref(struct mp_frame *frame)
{
    if (!frame)
        return;

    if (frame_handlers[frame->type].free)
        frame_handlers[frame->type].free(frame->data);

    *frame = (struct mp_frame){0};
}

struct mp_frame mp_frame_ref(struct mp_frame frame)
{
    if (frame_handlers[frame.type].new_ref) {
        assert(frame.data);
        frame.data = frame_handlers[frame.type].new_ref(frame.data);
        if (!frame.data)
            frame.type = MP_FRAME_NONE;
    }
    return frame;
}

double mp_frame_get_pts(struct mp_frame frame)
{
    if (frame_handlers[frame.type].get_pts)
        return frame_handlers[frame.type].get_pts(frame.data);
    return MP_NOPTS_VALUE;
}

void mp_frame_set_pts(struct mp_frame frame, double pts)
{
    if (frame_handlers[frame.type].get_pts)
        frame_handlers[frame.type].set_pts(frame.data, pts);
}

AVFrame *mp_frame_to_av(struct mp_frame frame, struct AVRational *tb)
{
    if (!frame_handlers[frame.type].new_av_ref)
        return NULL;

    AVFrame *res = frame_handlers[frame.type].new_av_ref(frame.data);
    if (!res)
        return NULL;

    res->pts = mp_pts_to_av(mp_frame_get_pts(frame), tb);
    return res;
}

struct mp_frame mp_frame_from_av(enum mp_frame_type type, struct AVFrame *frame,
                                 struct AVRational *tb)
{
    struct mp_frame res = {type};

    if (!frame_handlers[res.type].from_av_ref)
        return MP_NO_FRAME;

    res.data = frame_handlers[res.type].from_av_ref(frame);
    if (!res.data)
        return MP_NO_FRAME;

    mp_frame_set_pts(res, mp_pts_from_av(frame->pts, tb));
    return res;
}
