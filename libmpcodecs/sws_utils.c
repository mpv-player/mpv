/*
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <assert.h>

#include "libmpcodecs/sws_utils.h"

#include "libmpcodecs/mp_image.h"
#include "libmpcodecs/img_format.h"
#include "fmt-conversion.h"
#include "libvo/csputils.h"
#include "mp_msg.h"

//global sws_flags from the command line
int sws_flags = 2;

float sws_lum_gblur = 0.0;
float sws_chr_gblur = 0.0;
int sws_chr_vshift = 0;
int sws_chr_hshift = 0;
float sws_chr_sharpen = 0.0;
float sws_lum_sharpen = 0.0;

//global srcFilter
static SwsFilter *src_filter = NULL;

void sws_getFlagsAndFilterFromCmdLine(int *flags, SwsFilter **srcFilterParam,
                                      SwsFilter **dstFilterParam)
{
    static int firstTime = 1;
    *flags = 0;

    if (firstTime) {
        firstTime = 0;
        *flags = SWS_PRINT_INFO;
    } else if (mp_msg_test(MSGT_VFILTER, MSGL_DBG2))
        *flags = SWS_PRINT_INFO;

    if (src_filter)
        sws_freeFilter(src_filter);

    src_filter = sws_getDefaultFilter(
        sws_lum_gblur, sws_chr_gblur,
        sws_lum_sharpen, sws_chr_sharpen,
        sws_chr_hshift, sws_chr_vshift, verbose > 1);

    switch (sws_flags) {
    case 0: *flags |= SWS_FAST_BILINEAR;
        break;
    case 1: *flags |= SWS_BILINEAR;
        break;
    case 2: *flags |= SWS_BICUBIC;
        break;
    case 3: *flags |= SWS_X;
        break;
    case 4: *flags |= SWS_POINT;
        break;
    case 5: *flags |= SWS_AREA;
        break;
    case 6: *flags |= SWS_BICUBLIN;
        break;
    case 7: *flags |= SWS_GAUSS;
        break;
    case 8: *flags |= SWS_SINC;
        break;
    case 9: *flags |= SWS_LANCZOS;
        break;
    case 10: *flags |= SWS_SPLINE;
        break;
    default: *flags |= SWS_BILINEAR;
        break;
    }

    *srcFilterParam = src_filter;
    *dstFilterParam = NULL;
}

// will use sws_flags & src_filter (from cmd line)
static struct SwsContext *sws_getContextFromCmdLine2(int srcW, int srcH,
                                                     int srcFormat, int dstW,
                                                     int dstH, int dstFormat,
                                                     int extraflags)
{
    int flags;
    SwsFilter *dstFilterParam, *srcFilterParam;
    enum PixelFormat dfmt, sfmt;

    dfmt = imgfmt2pixfmt(dstFormat);
    sfmt = imgfmt2pixfmt(srcFormat);
    if (srcFormat == IMGFMT_RGB8 || srcFormat == IMGFMT_BGR8)
        sfmt = PIX_FMT_PAL8;
    sws_getFlagsAndFilterFromCmdLine(&flags, &srcFilterParam, &dstFilterParam);

    return sws_getContext(srcW, srcH, sfmt, dstW, dstH, dfmt, flags |
                          extraflags, srcFilterParam, dstFilterParam,
                          NULL);
}

struct SwsContext *sws_getContextFromCmdLine(int srcW, int srcH, int srcFormat,
                                             int dstW, int dstH,
                                             int dstFormat)
{
    return sws_getContextFromCmdLine2(srcW, srcH, srcFormat, dstW, dstH,
                                      dstFormat,
                                      0);
}

struct SwsContext *sws_getContextFromCmdLine_hq(int srcW, int srcH,
                                                int srcFormat, int dstW,
                                                int dstH,
                                                int dstFormat)
{
    return sws_getContextFromCmdLine2(
               srcW, srcH, srcFormat, dstW, dstH, dstFormat,
               SWS_FULL_CHR_H_INT | SWS_FULL_CHR_H_INP |
               SWS_ACCURATE_RND | SWS_BITEXACT);
}

bool mp_sws_supported_format(int imgfmt)
{
    enum PixelFormat av_format = imgfmt2pixfmt(imgfmt);

    return av_format != PIX_FMT_NONE && sws_isSupportedInput(av_format)
        && sws_isSupportedOutput(av_format);
}

void mp_image_swscale(struct mp_image *dst,
                      const struct mp_image *src,
                      struct mp_csp_details *csp,
                      int my_sws_flags)
{
    enum PixelFormat dfmt, sfmt;
    dfmt = imgfmt2pixfmt(dst->imgfmt);
    sfmt = imgfmt2pixfmt(src->imgfmt);
    if (src->imgfmt == IMGFMT_RGB8 || src->imgfmt == IMGFMT_BGR8)
        sfmt = PIX_FMT_PAL8;

    struct SwsContext *sws =
        sws_getContext(src->w, src->h, sfmt, dst->w, dst->h, dfmt,
                       my_sws_flags, NULL, NULL, NULL);
    struct mp_csp_details mycsp = MP_CSP_DETAILS_DEFAULTS;
    if (csp)
        mycsp = *csp;
    mp_sws_set_colorspace(sws, &mycsp);
    sws_scale(sws, (const unsigned char *const *) src->planes, src->stride,
              0, src->h,
              dst->planes, dst->stride);
    sws_freeContext(sws);
}

// vim: ts=4 sw=4 et tw=80
