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

#include <libavutil/opt.h>

#include "sws_utils.h"

#include "video/mp_image.h"
#include "video/img_format.h"
#include "fmt-conversion.h"
#include "csputils.h"
#include "core/mp_msg.h"

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
struct SwsContext *sws_getContextFromCmdLine(int srcW, int srcH,
                                             int srcFormat, int dstW,
                                             int dstH, int dstFormat)
{
    int flags;
    SwsFilter *dstFilterParam, *srcFilterParam;
    enum PixelFormat dfmt, sfmt;

    dfmt = imgfmt2pixfmt(dstFormat);
    sfmt = imgfmt2pixfmt(srcFormat);
    if (srcFormat == IMGFMT_RGB8 || srcFormat == IMGFMT_BGR8)
        sfmt = PIX_FMT_PAL8;
    sws_getFlagsAndFilterFromCmdLine(&flags, &srcFilterParam, &dstFilterParam);

    return sws_getContext(srcW, srcH, sfmt, dstW, dstH, dfmt, flags,
                          srcFilterParam, dstFilterParam, NULL);
}

bool mp_sws_supported_format(int imgfmt)
{
    enum PixelFormat av_format = imgfmt2pixfmt(imgfmt);

    return av_format != PIX_FMT_NONE && sws_isSupportedInput(av_format)
        && sws_isSupportedOutput(av_format);
}

static int mp_csp_to_sws_colorspace(enum mp_csp csp)
{
    switch (csp) {
    case MP_CSP_BT_601:     return SWS_CS_ITU601;
    case MP_CSP_BT_709:     return SWS_CS_ITU709;
    case MP_CSP_SMPTE_240M: return SWS_CS_SMPTE240M;
    default:                return SWS_CS_DEFAULT;
    }
}

// component_offset[]: byte index of each r (0), g (1), b (2), a (3) component
static void planarize32(struct mp_image *dst, struct mp_image *src,
                        int component_offset[4])
{
    for (int y = 0; y < dst->h; y++) {
        for (int p = 0; p < 3; p++) {
            uint8_t *d_line = dst->planes[p] + y * dst->stride[p];
            uint8_t *s_line = src->planes[0] + y * src->stride[0];
            s_line += component_offset[(p + 1) % 3]; // GBR => RGB
            for (int x = 0; x < dst->w; x++) {
                d_line[x] = s_line[x * 4];
            }
        }
    }
}

#define SET_COMPS(comp, r, g, b, a) \
    { (comp)[0] = (r); (comp)[1] = (g); (comp)[2] = (b); (comp)[3] = (a); }

static void to_gbrp(struct mp_image *dst, struct mp_image *src,
                    int my_sws_flags)
{
    struct mp_image *temp = NULL;
    int comp[4];

    switch (src->imgfmt) {
    case IMGFMT_ABGR: SET_COMPS(comp, 3, 2, 1, 0); break;
    case IMGFMT_BGRA: SET_COMPS(comp, 2, 1, 0, 3); break;
    case IMGFMT_ARGB: SET_COMPS(comp, 1, 2, 3, 0); break;
    case IMGFMT_RGBA: SET_COMPS(comp, 0, 1, 2, 3); break;
    default:
        temp = alloc_mpi(dst->w, dst->h, IMGFMT_RGBA);
        mp_image_swscale(temp, src, my_sws_flags);
        src = temp;
        SET_COMPS(comp, 0, 1, 2, 3);
    }

    planarize32(dst, src, comp);

    talloc_free(temp);
}


static void mp_sws_set_conv(struct SwsContext *sws, struct mp_image *dst,
                            struct mp_image *src, int my_sws_flags)
{
    enum PixelFormat s_fmt = imgfmt2pixfmt(src->imgfmt);
    if (src->imgfmt == IMGFMT_RGB8 || src->imgfmt == IMGFMT_BGR8)
        s_fmt = PIX_FMT_PAL8;
    int s_csp = mp_csp_to_sws_colorspace(mp_image_csp(src));
    int s_range = mp_image_levels(src) == MP_CSP_LEVELS_PC;

    enum PixelFormat d_fmt = imgfmt2pixfmt(dst->imgfmt);
    int d_csp = mp_csp_to_sws_colorspace(mp_image_csp(dst));
    int d_range = mp_image_levels(dst) == MP_CSP_LEVELS_PC;

    // Work around libswscale bug #1852 (fixed in ffmpeg commit 8edf9b1fa):
    // setting range flags for RGB gives random bogus results.
    // Newer libswscale always ignores range flags for RGB.
    bool s_yuv = src->flags & MP_IMGFLAG_YUV;
    bool d_yuv = dst->flags & MP_IMGFLAG_YUV;
    s_range = s_range && s_yuv;
    d_range = d_range && d_yuv;

    av_opt_set_int(sws, "sws_flags", my_sws_flags, 0);

    av_opt_set_int(sws, "srcw", src->w, 0);
    av_opt_set_int(sws, "srch", src->h, 0);
    av_opt_set_int(sws, "src_format", s_fmt, 0);

    av_opt_set_int(sws, "dstw", dst->w, 0);
    av_opt_set_int(sws, "dsth", dst->h, 0);
    av_opt_set_int(sws, "dst_format", d_fmt, 0);

    sws_setColorspaceDetails(sws, sws_getCoefficients(s_csp), s_range,
                             sws_getCoefficients(d_csp), d_range,
                             0, 1 << 16, 1 << 16);
}

void mp_image_swscale(struct mp_image *dst, struct mp_image *src,
                      int my_sws_flags)
{
    if (dst->imgfmt == IMGFMT_GBRP)
        return to_gbrp(dst, src, my_sws_flags);

    struct SwsContext *sws = sws_alloc_context();
    mp_sws_set_conv(sws, dst, src, my_sws_flags);

    int res = sws_init_context(sws, NULL, NULL);
    assert(res >= 0);

    sws_scale(sws, (const uint8_t *const *) src->planes, src->stride,
              0, src->h, dst->planes, dst->stride);
    sws_freeContext(sws);
}

void mp_image_sw_blur_scale(struct mp_image *dst, struct mp_image *src,
                            float gblur)
{
    struct SwsContext *sws = sws_alloc_context();

    int flags = SWS_LANCZOS | SWS_FULL_CHR_H_INT | SWS_FULL_CHR_H_INP |
                SWS_ACCURATE_RND | SWS_BITEXACT;

    mp_sws_set_conv(sws, dst, src, flags);

    SwsFilter *src_filter = sws_getDefaultFilter(gblur, gblur, 0, 0, 0, 0, 0);

    int res = sws_init_context(sws, src_filter, NULL);
    assert(res >= 0);

    sws_scale(sws, (const uint8_t *const *) src->planes, src->stride,
              0, src->h, dst->planes, dst->stride);
    sws_freeContext(sws);

    sws_freeFilter(src_filter);
}

// vim: ts=4 sw=4 et tw=80
