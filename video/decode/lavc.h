#ifndef MPV_LAVC_H
#define MPV_LAVC_H

#include <stdbool.h>

#include <libavcodec/avcodec.h>

#include "config.h"

#include "demux/stheader.h"
#include "video/mp_image.h"
#include "video/hwdec.h"

// keep in sync with --hwdec option
enum hwdec_type {
    HWDEC_AUTO = -1,
    HWDEC_NONE = 0,
    HWDEC_VDPAU = 1,
    HWDEC_VDA = 2,
    HWDEC_CRYSTALHD = 3,
    HWDEC_VAAPI = 4,
    HWDEC_VAAPI_COPY = 5,
};

typedef struct lavc_ctx {
    struct MPOpts *opts;
    AVCodecContext *avctx;
    AVFrame *pic;
    struct vd_lavc_hwdec *hwdec;
    enum PixelFormat pix_fmt;
    int do_hw_dr1;
    int best_csp;
    struct mp_image_params image_params;
    struct mp_image_params vo_image_params;
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

struct vd_lavc_hwdec {
    enum hwdec_type type;
    // If non-NULL: lists pairs software and hardware decoders. If the current
    // codec is not one of the listed software decoders, probing fails.
    // Otherwise, the AVCodecContext is initialized with the associated
    // hardware decoder.
    // Useful only if hw decoding requires a special codec, instead  of using
    // the libavcodec hwaccel infrastructure.
    const char **codec_pairs;
    // If not-NULL: a 0 terminated list of IMGFMT_ formats, and only one of
    // these formats is accepted in the libavcodec get_format callback.
    const int *image_formats;
    int (*probe)(struct vd_lavc_hwdec *hwdec, struct mp_hwdec_info *info,
                 const char *decoder);
    int (*init)(struct lavc_ctx *ctx);
    void (*uninit)(struct lavc_ctx *ctx);
    struct mp_image *(*allocate_image)(struct lavc_ctx *ctx, int fmt,
                                       int w, int h);
    // Process the image returned by the libavcodec decoder.
    struct mp_image *(*process_image)(struct lavc_ctx *ctx, struct mp_image *img);
};

enum {
    HWDEC_ERR_NO_CTX = -2,
    HWDEC_ERR_NO_CODEC = -3,
};

struct hwdec_profile_entry {
    enum AVCodecID av_codec;
    int ff_profile;
    uint64_t hw_profile;
};

const struct hwdec_profile_entry *hwdec_find_profile(
    struct lavc_ctx *ctx, const struct hwdec_profile_entry *table);
bool hwdec_check_codec_support(const char *decoder,
                               const struct hwdec_profile_entry *table);
int hwdec_get_max_refs(struct lavc_ctx *ctx);

// lavc_dr1.c
int mp_codec_get_buffer(AVCodecContext *s, AVFrame *frame);
void mp_codec_release_buffer(AVCodecContext *s, AVFrame *frame);
struct FrameBuffer;
void mp_buffer_ref(struct FrameBuffer *buffer);
void mp_buffer_unref(struct FrameBuffer *buffer);
bool mp_buffer_is_unique(struct FrameBuffer *buffer);
void mp_buffer_pool_free(struct FramePool **pool);

#endif
