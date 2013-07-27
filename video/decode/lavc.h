#ifndef MPV_LAVC_H
#define MPV_LAVC_H

#include <stdbool.h>

#include <libavcodec/avcodec.h>

#include "config.h"

#include "demux/stheader.h"
#include "video/mp_image.h"

typedef struct lavc_ctx {
    AVCodecContext *avctx;
    AVFrame *pic;
    struct hwdec *hwdec;
    enum PixelFormat pix_fmt;
    int do_hw_dr1;
    int vo_initialized;
    int best_csp;
    struct mp_image_params image_params;
    AVRational last_sample_aspect_ratio;
    enum AVDiscard skip_frame;
    const char *software_fallback_decoder;

    // From VO
    struct mp_hwdec_info *hwdec_info;

    // For free use by hwdec implementation
    void *hwdec_priv;

    // Legacy
    bool do_dr1;
    struct FramePool *dr1_buffer_pool;
    struct mp_image_pool *non_dr1_pool;
} vd_ffmpeg_ctx;

struct vd_lavc_hwdec_functions {
    // If not-NULL, a 0 terminated list of IMGFMT_ formats. Only one of these
    // formats is accepted when handling the libavcodec get_format callback.
    const int *image_formats;
    int (*init)(struct lavc_ctx *ctx);
    void (*uninit)(struct lavc_ctx *ctx);
    struct mp_image *(*allocate_image)(struct lavc_ctx *ctx, AVFrame *frame);
    void (*fix_image)(struct lavc_ctx *ctx, struct mp_image *img);
};

// lavc_dr1.c
int mp_codec_get_buffer(AVCodecContext *s, AVFrame *frame);
void mp_codec_release_buffer(AVCodecContext *s, AVFrame *frame);
struct FrameBuffer;
void mp_buffer_ref(struct FrameBuffer *buffer);
void mp_buffer_unref(struct FrameBuffer *buffer);
bool mp_buffer_is_unique(struct FrameBuffer *buffer);
void mp_buffer_pool_free(struct FramePool **pool);

#endif
