#ifndef MPLAYER_SWS_UTILS_H
#define MPLAYER_SWS_UTILS_H

#include <stdbool.h>

#include "mp_image.h"

struct mp_image;
struct mpv_global;

// libswscale currently requires 16 bytes alignment for row pointers and
// strides. Otherwise, it will print warnings and use slow codepaths.
// Guaranteed to be a power of 2 and > 1.
#define SWS_MIN_BYTE_ALIGN MP_IMAGE_BYTE_ALIGN

extern const int mp_sws_fast_flags;

bool mp_sws_supported_format(int imgfmt);

int mp_image_swscale(struct mp_image *dst, struct mp_image *src,
                     int my_sws_flags);

int mp_image_sw_blur_scale(struct mp_image *dst, struct mp_image *src,
                           float gblur);

enum mp_sws_scaler {
    MP_SWS_AUTO = 0, // use command line
    MP_SWS_SWS,
    MP_SWS_ZIMG,
};

struct mp_sws_context {
    // Can be set for verbose error printing.
    struct mp_log *log;
    // User configuration. These can be changed freely, at any time.
    // mp_sws_scale() will handle the changes transparently.
    int flags;
    bool allow_zimg; // use zimg if available (ignores filters and all)
    bool force_reload;
    // These are also implicitly set by mp_sws_scale(), and thus optional.
    // Setting them before that call makes sense when using mp_sws_reinit().
    struct mp_image_params src, dst;

    // This is unfortunately a hack: bypass command line choice
    enum mp_sws_scaler force_scaler;

    // If zimg is used. Need to manually invalidate cache (set force_reload).
    // Conflicts with enabling command line opts.
    struct zimg_opts *zimg_opts;

    // Changing these requires setting force_reload=true.
    // By default, they are NULL.
    // Freeing the mp_sws_context will deallocate these if set.
    struct SwsFilter *src_filter, *dst_filter;
    double params[2];

    // Cached context (if any)
    struct SwsContext *sws;
    bool supports_csp;

    // Private.
    struct m_config_cache *opts_cache;
    struct mp_sws_context *cached; // contains parameters for which sws is valid
    struct mp_zimg_context *zimg;
    bool zimg_ok;
    struct mp_image *aligned_src, *aligned_dst;
};

struct mp_sws_context *mp_sws_alloc(void *talloc_ctx);
void mp_sws_enable_cmdline_opts(struct mp_sws_context *ctx, struct mpv_global *g);
int mp_sws_reinit(struct mp_sws_context *ctx);
int mp_sws_scale(struct mp_sws_context *ctx, struct mp_image *dst,
                 struct mp_image *src);

bool mp_sws_supports_formats(struct mp_sws_context *ctx,
                             int imgfmt_out, int imgfmt_in);

struct mp_image *mp_img_swap_to_native(struct mp_image *img);

#endif /* MP_SWS_UTILS_H */

// vim: ts=4 sw=4 et tw=80
