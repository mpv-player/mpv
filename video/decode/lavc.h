#ifndef MPV_LAVC_H
#define MPV_LAVC_H

#include <stdbool.h>

#include <libavcodec/avcodec.h>

#include "demux/stheader.h"
#include "video/mp_image.h"
#include "video/hwdec.h"

typedef struct lavc_ctx {
    struct mp_log *log;
    struct MPOpts *opts;
    AVCodecContext *avctx;
    AVFrame *pic;
    struct vd_lavc_hwdec *hwdec;
    int selected_hwdec;
    enum AVPixelFormat pix_fmt;
    int best_csp;
    enum AVDiscard skip_frame;
    const char *software_fallback_decoder;

    // From VO
    struct mp_hwdec_info *hwdec_info;

    // For free use by hwdec implementation
    void *hwdec_priv;

    int hwdec_fmt;
    int hwdec_w;
    int hwdec_h;
    int hwdec_profile;
} vd_ffmpeg_ctx;

struct vd_lavc_hwdec {
    enum hwdec_type type;
    // If not-0: the IMGFMT_ format that should be accepted in the libavcodec
    // get_format callback.
    int image_format;
    int (*probe)(struct vd_lavc_hwdec *hwdec, struct mp_hwdec_info *info,
                 const char *decoder);
    int (*init)(struct lavc_ctx *ctx);
    int (*init_decoder)(struct lavc_ctx *ctx, int fmt, int w, int h);
    void (*uninit)(struct lavc_ctx *ctx);
    // Note: if init_decoder is set, this will always use the values from the
    //       last successful init_decoder call. Otherwise, it's up to you.
    struct mp_image *(*allocate_image)(struct lavc_ctx *ctx, int fmt,
                                       int w, int h);
    // Process the image returned by the libavcodec decoder.
    struct mp_image *(*process_image)(struct lavc_ctx *ctx, struct mp_image *img);
    // For horrible Intel shit-drivers only
    void (*lock)(struct lavc_ctx *ctx);
    void (*unlock)(struct lavc_ctx *ctx);
    // Optional; if a special hardware decoder is needed (instead of "hwaccel").
    const char *(*get_codec)(struct lavc_ctx *ctx);
};

enum {
    HWDEC_ERR_NO_CTX = -2,
    HWDEC_ERR_NO_CODEC = -3,
    HWDEC_ERR_EMULATED = -4,    // probing successful, but emulated API detected
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

#endif
