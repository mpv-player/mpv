#ifndef MPV_VT_H
#define MPV_VT_H

#include <stdint.h>

int mp_imgfmt_from_cvpixelformat(uint32_t cvpixfmt);

struct mp_image;
struct mp_image_pool;
struct mp_hwdec_ctx;
struct mp_image *mp_vt_download_image(struct mp_hwdec_ctx *ctx,
                                      struct mp_image *hw_image,
                                      struct mp_image_pool *swpool);

#endif
