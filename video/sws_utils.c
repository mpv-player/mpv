/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <assert.h>

#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
#include <libavutil/bswap.h>
#include <libavutil/opt.h>

#include "config.h"

#include "sws_utils.h"

#include "common/common.h"
#include "options/m_option.h"
#include "video/mp_image.h"
#include "video/img_format.h"
#include "fmt-conversion.h"
#include "csputils.h"
#include "common/msg.h"
#include "video/filter/vf.h"
#include "osdep/endian.h"

//global sws_flags from the command line
struct sws_opts {
    int scaler;
    float lum_gblur;
    float chr_gblur;
    int chr_vshift;
    int chr_hshift;
    float chr_sharpen;
    float lum_sharpen;
};

#define OPT_BASE_STRUCT struct sws_opts
const struct m_sub_options sws_conf = {
    .opts = (const m_option_t[]) {
        OPT_CHOICE("scaler", scaler, 0,
                   ({"fast-bilinear",   SWS_FAST_BILINEAR},
                    {"bilinear",        SWS_BILINEAR},
                    {"bicubic",         SWS_BICUBIC},
                    {"x",               SWS_X},
                    {"point",           SWS_POINT},
                    {"area",            SWS_AREA},
                    {"bicublin",        SWS_BICUBLIN},
                    {"gauss",           SWS_GAUSS},
                    {"sinc",            SWS_SINC},
                    {"lanczos",         SWS_LANCZOS},
                    {"spline",          SWS_SPLINE})),
        OPT_FLOATRANGE("lgb", lum_gblur, 0, 0, 100.0),
        OPT_FLOATRANGE("cgb", chr_gblur, 0, 0, 100.0),
        OPT_INT("cvs", chr_vshift, 0),
        OPT_INT("chs", chr_hshift, 0),
        OPT_FLOATRANGE("ls", lum_sharpen, 0, -100.0, 100.0),
        OPT_FLOATRANGE("cs", chr_sharpen, 0, -100.0, 100.0),
        {0}
    },
    .size = sizeof(struct sws_opts),
    .defaults = &(const struct sws_opts){
        .scaler = SWS_BICUBIC,
    },
};

// Highest quality, but also slowest.
const int mp_sws_hq_flags = SWS_LANCZOS | SWS_FULL_CHR_H_INT |
                            SWS_FULL_CHR_H_INP | SWS_ACCURATE_RND |
                            SWS_BITEXACT;

// Fast, lossy.
const int mp_sws_fast_flags = SWS_BILINEAR;

// Set ctx parameters to global command line flags.
void mp_sws_set_from_cmdline(struct mp_sws_context *ctx, struct sws_opts *opts)
{
    sws_freeFilter(ctx->src_filter);
    ctx->src_filter = sws_getDefaultFilter(opts->lum_gblur, opts->chr_gblur,
                                           opts->lum_sharpen, opts->chr_sharpen,
                                           opts->chr_hshift, opts->chr_vshift, 0);
    ctx->force_reload = true;

    ctx->flags = SWS_PRINT_INFO;
    ctx->flags |= opts->scaler;
}

bool mp_sws_supported_format(int imgfmt)
{
    enum AVPixelFormat av_format = imgfmt2pixfmt(imgfmt);

    return av_format != AV_PIX_FMT_NONE && sws_isSupportedInput(av_format)
        && sws_isSupportedOutput(av_format);
}

static int mp_csp_to_sws_colorspace(enum mp_csp csp)
{
    // The SWS_CS_* macros are just convenience redefinitions of the
    // AVCOL_SPC_* macros, inside swscale.h.
    return mp_csp_to_avcol_spc(csp);
}

static bool cache_valid(struct mp_sws_context *ctx)
{
    struct mp_sws_context *old = ctx->cached;
    if (ctx->force_reload)
        return false;
    return mp_image_params_equal(&ctx->src, &old->src) &&
           mp_image_params_equal(&ctx->dst, &old->dst) &&
           ctx->flags == old->flags &&
           ctx->brightness == old->brightness &&
           ctx->contrast == old->contrast &&
           ctx->saturation == old->saturation;
}

static void free_mp_sws(void *p)
{
    struct mp_sws_context *ctx = p;
    sws_freeContext(ctx->sws);
    sws_freeFilter(ctx->src_filter);
    sws_freeFilter(ctx->dst_filter);
}

// You're supposed to set your scaling parameters on the returned context.
// Free the context with talloc_free().
struct mp_sws_context *mp_sws_alloc(void *talloc_ctx)
{
    struct mp_sws_context *ctx = talloc_ptrtype(talloc_ctx, ctx);
    *ctx = (struct mp_sws_context) {
        .log = mp_null_log,
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
    struct mp_image_params *src = &ctx->src;
    struct mp_image_params *dst = &ctx->dst;

    // Neutralize unsupported or ignored parameters.
    src->d_w = dst->d_w = 0;
    src->d_h = dst->d_h = 0;
    src->outputlevels = dst->outputlevels = MP_CSP_LEVELS_AUTO;

    if (cache_valid(ctx))
        return 0;

    sws_freeContext(ctx->sws);
    ctx->sws = sws_alloc_context();
    if (!ctx->sws)
        return -1;

    mp_image_params_guess_csp(src); // sanitize colorspace/colorlevels
    mp_image_params_guess_csp(dst);

    struct mp_imgfmt_desc src_fmt = mp_imgfmt_get_desc(src->imgfmt);
    struct mp_imgfmt_desc dst_fmt = mp_imgfmt_get_desc(dst->imgfmt);
    if (!src_fmt.id || !dst_fmt.id)
        return -1;

    enum AVPixelFormat s_fmt = imgfmt2pixfmt(src->imgfmt);
    if (s_fmt == AV_PIX_FMT_NONE || sws_isSupportedInput(s_fmt) < 1) {
        MP_ERR(ctx, "Input image format %s not supported by libswscale.\n",
               mp_imgfmt_to_name(src->imgfmt));
        return -1;
    }

    enum AVPixelFormat d_fmt = imgfmt2pixfmt(dst->imgfmt);
    if (d_fmt == AV_PIX_FMT_NONE || sws_isSupportedOutput(d_fmt) < 1) {
        MP_ERR(ctx, "Output image format %s not supported by libswscale.\n",
               mp_imgfmt_to_name(dst->imgfmt));
        return -1;
    }

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
    int r =
        sws_setColorspaceDetails(ctx->sws, sws_getCoefficients(s_csp), s_range,
                                 sws_getCoefficients(d_csp), d_range,
                                 ctx->brightness, ctx->contrast, ctx->saturation);
    ctx->supports_csp = r >= 0;

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
    ctx->src = src->params;
    ctx->dst = dst->params;

    int r = mp_sws_reinit(ctx);
    if (r < 0) {
        MP_ERR(ctx, "libswscale initialization failed.\n");
        return r;
    }

    sws_scale(ctx->sws, (const uint8_t *const *) src->planes, src->stride,
              0, src->h, dst->planes, dst->stride);
    return 0;
}

int mp_image_swscale(struct mp_image *dst, struct mp_image *src,
                     int my_sws_flags)
{
    struct mp_sws_context *ctx = mp_sws_alloc(NULL);
    ctx->flags = my_sws_flags;
    int res = mp_sws_scale(ctx, dst, src);
    talloc_free(ctx);
    return res;
}

int mp_image_sw_blur_scale(struct mp_image *dst, struct mp_image *src,
                           float gblur)
{
    struct mp_sws_context *ctx = mp_sws_alloc(NULL);
    ctx->flags = mp_sws_hq_flags;
    ctx->src_filter = sws_getDefaultFilter(gblur, gblur, 0, 0, 0, 0, 0);
    ctx->force_reload = true;
    int res = mp_sws_scale(ctx, dst, src);
    talloc_free(ctx);
    return res;
}

int mp_sws_get_vf_equalizer(struct mp_sws_context *sws, struct vf_seteq *eq)
{
    if (!sws->supports_csp)
        return 0;
    if (!strcmp(eq->item, "brightness"))
        eq->value =  ((sws->brightness * 100) + (1 << 15)) >> 16;
    else if (!strcmp(eq->item, "contrast"))
        eq->value = (((sws->contrast  * 100) + (1 << 15)) >> 16) - 100;
    else if (!strcmp(eq->item, "saturation"))
        eq->value = (((sws->saturation * 100) + (1 << 15)) >> 16) - 100;
    else
        return 0;
    return 1;
}

int mp_sws_set_vf_equalizer(struct mp_sws_context *sws, struct vf_seteq *eq)
{
    if (!sws->supports_csp)
        return 0;
    if (!strcmp(eq->item, "brightness"))
        sws->brightness = ((eq->value << 16) + 50) / 100;
    else if (!strcmp(eq->item, "contrast"))
        sws->contrast   = MPMAX(1, (((eq->value + 100) << 16) + 50) / 100);
    else if (!strcmp(eq->item, "saturation"))
        sws->saturation = (((eq->value + 100) << 16) + 50) / 100;
    else
        return 0;

    return mp_sws_reinit(sws) >= 0 ? 1 : -1;
}

static const int endian_swaps[][2] = {
#if BYTE_ORDER == LITTLE_ENDIAN
#if defined(AV_PIX_FMT_YA16) && defined(AV_PIX_FMT_RGBA64)
    {AV_PIX_FMT_YA16BE,     AV_PIX_FMT_YA16LE},
    {AV_PIX_FMT_RGBA64BE,   AV_PIX_FMT_RGBA64LE},
    {AV_PIX_FMT_GRAY16BE,   AV_PIX_FMT_GRAY16LE},
    {AV_PIX_FMT_RGB48BE,    AV_PIX_FMT_RGB48LE},
#endif
#endif
    {AV_PIX_FMT_NONE,       AV_PIX_FMT_NONE}
};

// Swap _some_ non-native endian formats to native. We do this specifically
// for pixel formats used by PNG, to avoid going through libswscale, which
// might reduce the effective bit depth in some cases.
struct mp_image *mp_img_swap_to_native(struct mp_image *img)
{
    int to = AV_PIX_FMT_NONE;
    for (int n = 0; endian_swaps[n][0] != AV_PIX_FMT_NONE; n++) {
        if (endian_swaps[n][0] == img->fmt.avformat)
            to = endian_swaps[n][1];
    }
    if (to == AV_PIX_FMT_NONE || !mp_image_make_writeable(img))
        return img;
    int elems = img->fmt.bytes[0] / 2 * img->w;
    for (int y = 0; y < img->h; y++) {
        uint16_t *p = (uint16_t *)(img->planes[0] + y * img->stride[0]);
        for (int i = 0; i < elems; i++)
            p[i] = av_be2ne16(p[i]);
    }
    mp_image_setfmt(img, pixfmt2imgfmt(to));
    return img;
}

// vim: ts=4 sw=4 et tw=80
