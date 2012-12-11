#ifndef MPV_LAVC_H
#define MPV_LAVC_H

#include <libavcodec/avcodec.h>

#include "demux/stheader.h"
#include "video/mp_image.h"

#define MAX_NUM_MPI 50

typedef struct ffmpeg_ctx {
    AVCodecContext *avctx;
    AVFrame *pic;
    struct mp_image export_mpi;
    struct mp_image hwdec_mpi[MAX_NUM_MPI];
    struct hwdec *hwdec;
    enum PixelFormat pix_fmt;
    int do_dr1;
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
} vd_ffmpeg_ctx;

int mp_codec_get_buffer(AVCodecContext *s, AVFrame *frame);
void mp_codec_release_buffer(AVCodecContext *s, AVFrame *frame);

struct FrameBuffer;

void mp_buffer_ref(struct FrameBuffer *buffer);
void mp_buffer_unref(struct FrameBuffer *buffer);

void mp_buffer_pool_free(struct FramePool **pool);

#endif
