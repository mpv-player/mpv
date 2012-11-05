#ifndef MPV_LAVC_H
#define MPV_LAVC_H

#include <stdbool.h>

#include <libavcodec/avcodec.h>

#include "demux/stheader.h"
#include "video/mp_image.h"

typedef struct ffmpeg_ctx {
    AVCodecContext *avctx;
    AVFrame *pic;
    struct hwdec *hwdec;
    enum PixelFormat pix_fmt;
    int do_hw_dr1, do_dr1;
    int vo_initialized;
    int best_csp;
    int qp_stat[32];
    double qp_sum;
    double inv_qp_sum;
    AVRational last_sample_aspect_ratio;
    enum AVDiscard skip_frame;
    int rawvideo_fmt;
    AVCodec *software_fallback;
    struct FramePool *dr1_buffer_pool;
    struct mp_image_pool *non_dr1_pool;
} vd_ffmpeg_ctx;

int mp_codec_get_buffer(AVCodecContext *s, AVFrame *frame);
void mp_codec_release_buffer(AVCodecContext *s, AVFrame *frame);

struct FrameBuffer;

void mp_buffer_ref(struct FrameBuffer *buffer);
void mp_buffer_unref(struct FrameBuffer *buffer);
bool mp_buffer_is_unique(struct FrameBuffer *buffer);

void mp_buffer_pool_free(struct FramePool **pool);

#endif
