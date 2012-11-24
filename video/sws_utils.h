#ifndef MPLAYER_SWS_UTILS_H
#define MPLAYER_SWS_UTILS_H

#include <stdbool.h>
#include <libswscale/swscale.h>

struct mp_image;
struct mp_csp_details;

// libswscale currently requires 16 bytes alignment for row pointers and
// strides. Otherwise, it will print warnings and use slow codepaths.
// Guaranteed to be a power of 2 and > 1.
#define SWS_MIN_BYTE_ALIGN 16

void sws_getFlagsAndFilterFromCmdLine(int *flags, SwsFilter **srcFilterParam,
                                      SwsFilter **dstFilterParam);
struct SwsContext *sws_getContextFromCmdLine(int srcW, int srcH, int srcFormat,
                                             int dstW, int dstH,
                                             int dstFormat);

bool mp_sws_supported_format(int imgfmt);

void mp_image_swscale(struct mp_image *dst, struct mp_image *src,
                      int my_sws_flags);

void mp_image_sw_blur_scale(struct mp_image *dst, struct mp_image *src,
                            float gblur);

#endif /* MP_SWS_UTILS_H */

// vim: ts=4 sw=4 et tw=80
