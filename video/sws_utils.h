#ifndef MPLAYER_SWS_UTILS_H
#define MPLAYER_SWS_UTILS_H

#include <stdbool.h>

#include "mp_image.h"

struct mp_image;
struct sws_opts;

// libswscale currently requires 16 bytes alignment for row pointers and
// strides. Otherwise, it will print warnings and use slow codepaths.
// Guaranteed to be a power of 2 and > 1.
#define SWS_MIN_BYTE_ALIGN 16

extern const int mp_sws_hq_flags;
extern const int mp_sws_fast_flags;

bool mp_sws_supported_format(int imgfmt);

int mp_image_swscale(struct mp_image *dst, struct mp_image *src,
                     int my_sws_flags);

int mp_image_sw_blur_scale(struct mp_image *dst, struct mp_image *src,
                           float gblur);

struct mp_sws_context {
    // Can be set for verbose error printing.
    struct mp_log *log;
    // User configuration. These can be changed freely, at any time.
    // mp_sws_scale() will handle the changes transparently.
    int flags;
    int brightness, contrast, saturation;
    bool force_reload;
    // These are also implicitly set by mp_sws_scale(), and thus optional.
    // Setting them before that call makes sense when using mp_sws_reinit().
    struct mp_image_params src, dst;

    // Changing these requires setting force_reload=true.
    // By default, they are NULL.
    // Freeing the mp_sws_context will deallocate these if set.
    struct SwsFilter *src_filter, *dst_filter;
    double params[2];

    // Cached context (if any)
    struct SwsContext *sws;
    bool supports_csp;

    // Contains parameters for which sws is valid
    struct mp_sws_context *cached;
};

struct mp_sws_context *mp_sws_alloc(void *talloc_ctx);
int mp_sws_reinit(struct mp_sws_context *ctx);
void mp_sws_set_from_cmdline(struct mp_sws_context *ctx, struct sws_opts *opts);
int mp_sws_scale(struct mp_sws_context *ctx, struct mp_image *dst,
                 struct mp_image *src);

struct vf_seteq;
int mp_sws_set_vf_equalizer(struct mp_sws_context *sws, struct vf_seteq *eq);
int mp_sws_get_vf_equalizer(struct mp_sws_context *sws, struct vf_seteq *eq);

struct mp_image *mp_img_swap_to_native(struct mp_image *img);

#endif /* MP_SWS_UTILS_H */

// vim: ts=4 sw=4 et tw=80
