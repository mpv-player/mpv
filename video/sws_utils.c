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

#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>

#include "sws_utils.h"

#include "video/mp_image.h"
#include "video/img_format.h"
#include "fmt-conversion.h"
#include "csputils.h"
#include "mpvcore/mp_msg.h"

//global sws_flags from the command line
int sws_flags = 2;

float sws_lum_gblur = 0.0;
float sws_chr_gblur = 0.0;
int sws_chr_vshift = 0;
int sws_chr_hshift = 0;
float sws_chr_sharpen = 0.0;
float sws_lum_sharpen = 0.0;

// Highest quality, but also slowest.
const int mp_sws_hq_flags = SWS_LANCZOS | SWS_FULL_CHR_H_INT |
                            SWS_FULL_CHR_H_INP | SWS_ACCURATE_RND |
                            SWS_BITEXACT;

// Set ctx parameters to global command line flags.
void mp_sws_set_from_cmdline(struct mp_sws_context *ctx)
{
    sws_freeFilter(ctx->src_filter);
    ctx->src_filter = sws_getDefaultFilter(sws_lum_gblur, sws_chr_gblur,
                                           sws_lum_sharpen, sws_chr_sharpen,
                                           sws_chr_hshift, sws_chr_vshift, 0);
    ctx->force_reload = true;

    ctx->flags = SWS_PRINT_INFO;

    switch (sws_flags) {
    case 0:  ctx->flags |= SWS_FAST_BILINEAR;   break;
    case 1:  ctx->flags |= SWS_BILINEAR;        break;
    case 2:  ctx->flags |= SWS_BICUBIC;         break;
    case 3:  ctx->flags |= SWS_X;               break;
    case 4:  ctx->flags |= SWS_POINT;           break;
    case 5:  ctx->flags |= SWS_AREA;            break;
    case 6:  ctx->flags |= SWS_BICUBLIN;        break;
    case 7:  ctx->flags |= SWS_GAUSS;           break;
    case 8:  ctx->flags |= SWS_SINC;            break;
    case 9:  ctx->flags |= SWS_LANCZOS;         break;
    case 10: ctx->flags |= SWS_SPLINE;          break;
    default: ctx->flags |= SWS_BILINEAR;        break;
    }
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

static int to_gbrp(struct mp_image *dst, struct mp_image *src,
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
        temp = mp_image_alloc(IMGFMT_RGBA, dst->w, dst->h);
        mp_image_swscale(temp, src, my_sws_flags);
        src = temp;
        SET_COMPS(comp, 0, 1, 2, 3);
    }

    planarize32(dst, src, comp);

    talloc_free(temp);
    return 0;
}

static bool cache_valid(struct mp_sws_context *ctx)
{
    struct mp_sws_context *old = ctx->cached;
    if (ctx->force_reload)
        return false;
    return mp_image_params_equals(&ctx->src, &old->src) &&
           mp_image_params_equals(&ctx->dst, &old->dst) &&
           ctx->flags == old->flags &&
           ctx->brightness == old->brightness &&
           ctx->contrast == old->contrast &&
           ctx->saturation == old->saturation;
}

static int free_mp_sws(void *p)
{
    struct mp_sws_context *ctx = p;
    sws_freeContext(ctx->sws);
    sws_freeFilter(ctx->src_filter);
    sws_freeFilter(ctx->dst_filter);
    return 0;
}

// You're supposed to set your scaling parameters on the returned context.
// Free the context with talloc_free().
struct mp_sws_context *mp_sws_alloc(void *talloc_parent)
{
    struct mp_sws_context *ctx = talloc_ptrtype(talloc_parent, ctx);
    *ctx = (struct mp_sws_context) {
        .flags = SWS_BILINEAR,
        .contrast = 1 << 16,    // 1.0 in 16.16 fixed point
        .saturation = 1 << 16,
        .force_reload = true,
        .params = {SWS_PARAM_DEFAULT, SWS_PARAM_DEFAULT},
        .cached = talloc_zero(ctx, struct mp_sws_context),
    };
    talloc_set_destructor(ctx, free_mp_sws);
    return ctx;
}

// Reinitialize (if needed) - return error code.
// Optional, but possibly useful to avoid having to handle mp_sws_scale errors.
int mp_sws_reinit(struct mp_sws_context *ctx)
{
    if (cache_valid(ctx))
        return 0;

    sws_freeContext(ctx->sws);
    ctx->sws = sws_alloc_context();
    if (!ctx->sws)
        return -1;

    struct mp_image_params *src = &ctx->src;
    struct mp_image_params *dst = &ctx->dst;

    mp_image_params_guess_csp(src); // sanitize colorspace/colorlevels
    mp_image_params_guess_csp(dst);

    struct mp_imgfmt_desc src_fmt = mp_imgfmt_get_desc(src->imgfmt);
    struct mp_imgfmt_desc dst_fmt = mp_imgfmt_get_desc(dst->imgfmt);
    if (!src_fmt.id || !dst_fmt.id)
        return -1;

    enum PixelFormat s_fmt = imgfmt2pixfmt(src->imgfmt);
    if (s_fmt == PIX_FMT_NONE || sws_isSupportedInput(s_fmt) < 1)
        return -1;

    enum PixelFormat d_fmt = imgfmt2pixfmt(dst->imgfmt);
    if (d_fmt == PIX_FMT_NONE || sws_isSupportedOutput(d_fmt) < 1)
        return -1;

    int s_csp = mp_csp_to_sws_colorspace(src->colorspace);
    int s_range = src->colorlevels == MP_CSP_LEVELS_PC;

    int d_csp = mp_csp_to_sws_colorspace(dst->colorspace);
    int d_range = dst->colorlevels == MP_CSP_LEVELS_PC;

    // Work around libswscale bug #1852 (fixed in ffmpeg commit 8edf9b1fa):
    // setting range flags for RGB gives random bogus results.
    // Newer libswscale always ignores range flags for RGB.
    s_range = s_range && (src_fmt.flags & MP_IMGFLAG_YUV);
    d_range = d_range && (dst_fmt.flags & MP_IMGFLAG_YUV);

    av_opt_set_int(ctx->sws, "sws_flags", ctx->flags, 0);

    av_opt_set_int(ctx->sws, "srcw", src->w, 0);
    av_opt_set_int(ctx->sws, "srch", src->h, 0);
    av_opt_set_int(ctx->sws, "src_format", s_fmt, 0);

    av_opt_set_int(ctx->sws, "dstw", dst->w, 0);
    av_opt_set_int(ctx->sws, "dsth", dst->h, 0);
    av_opt_set_int(ctx->sws, "dst_format", d_fmt, 0);

    av_opt_set_double(ctx->sws, "param0", ctx->params[0], 0);
    av_opt_set_double(ctx->sws, "param1", ctx->params[1], 0);

#if HAVE_AVCODEC_CHROMA_POS_API
    int cr_src = mp_chroma_location_to_av(src->chroma_location);
    int cr_dst = mp_chroma_location_to_av(dst->chroma_location);
    int cr_xpos, cr_ypos;
    if (avcodec_enum_to_chroma_pos(&cr_xpos, &cr_ypos, cr_src) >= 0) {
        av_opt_set_int(ctx->sws, "src_h_chr_pos", cr_xpos, 0);
        av_opt_set_int(ctx->sws, "src_v_chr_pos", cr_ypos, 0);
    }
    if (avcodec_enum_to_chroma_pos(&cr_xpos, &cr_ypos, cr_dst) >= 0) {
        av_opt_set_int(ctx->sws, "dst_h_chr_pos", cr_xpos, 0);
        av_opt_set_int(ctx->sws, "dst_v_chr_pos", cr_ypos, 0);
    }
#endif

    // This can fail even with normal operation, e.g. if a conversion path
    // simply does not support these settings.
    sws_setColorspaceDetails(ctx->sws, sws_getCoefficients(s_csp), s_range,
                             sws_getCoefficients(d_csp), d_range,
                             ctx->brightness, ctx->contrast, ctx->saturation);

    if (sws_init_context(ctx->sws, ctx->src_filter, ctx->dst_filter) < 0)
        return -1;

    ctx->force_reload = false;
    *ctx->cached = *ctx;
    return 1;
}

// Scale from src to dst - if src/dst have different parameters from previous
// calls, the context is reinitialized. Return error code. (It can fail if
// reinitialization was necessary, and swscale returned an error.)
int mp_sws_scale(struct mp_sws_context *ctx, struct mp_image *dst,
                 struct mp_image *src)
{
    // Hack for older swscale versions which don't support this.
    // We absolutely need this in the OSD rendering path.
    if (dst->imgfmt == IMGFMT_GBRP && !sws_isSupportedOutput(PIX_FMT_GBRP))
        return to_gbrp(dst, src, ctx->flags);

    mp_image_params_from_image(&ctx->src, src);
    mp_image_params_from_image(&ctx->dst, dst);

    int r = mp_sws_reinit(ctx);
    if (r < 0) {
        mp_msg(MSGT_VFILTER, MSGL_ERR, "libswscale initialization failed.\n");
        return r;
    }

    sws_scale(ctx->sws, (const uint8_t *const *) src->planes, src->stride,
              0, src->h, dst->planes, dst->stride);
    return 0;
}

void mp_image_swscale(struct mp_image *dst, struct mp_image *src,
                      int my_sws_flags)
{
    struct mp_sws_context *ctx = mp_sws_alloc(NULL);
    ctx->flags = my_sws_flags;
    mp_sws_scale(ctx, dst, src);
    talloc_free(ctx);
}

void mp_image_sw_blur_scale(struct mp_image *dst, struct mp_image *src,
                            float gblur)
{
    struct mp_sws_context *ctx = mp_sws_alloc(NULL);
    ctx->flags = mp_sws_hq_flags;
    ctx->src_filter = sws_getDefaultFilter(gblur, gblur, 0, 0, 0, 0, 0);
    ctx->force_reload = true;
    mp_sws_scale(ctx, dst, src);
    talloc_free(ctx);
}

// vim: ts=4 sw=4 et tw=80
