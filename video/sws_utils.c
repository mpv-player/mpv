/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <assert.h>

#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
#include <libavutil/bswap.h>
#include <libavutil/opt.h>

#include "config.h"

#include "sws_utils.h"

#include "common/common.h"
#include "options/m_config.h"
#include "options/m_option.h"
#include "video/mp_image.h"
#include "video/img_format.h"
#include "fmt-conversion.h"
#include "csputils.h"
#include "common/msg.h"
#include "osdep/endian.h"

#if HAVE_ZIMG
#include "zimg.h"
#endif

//global sws_flags from the command line
struct sws_opts {
    int scaler;
    float lum_gblur;
    float chr_gblur;
    int chr_vshift;
    int chr_hshift;
    float chr_sharpen;
    float lum_sharpen;
    int fast;
    int bitexact;
    int zimg;
};

#define OPT_BASE_STRUCT struct sws_opts
const struct m_sub_options sws_conf = {
    .opts = (const m_option_t[]) {
        {"scaler", OPT_CHOICE(scaler,
            {"fast-bilinear",   SWS_FAST_BILINEAR},
            {"bilinear",        SWS_BILINEAR},
            {"bicubic",         SWS_BICUBIC},
            {"x",               SWS_X},
            {"point",           SWS_POINT},
            {"area",            SWS_AREA},
            {"bicublin",        SWS_BICUBLIN},
            {"gauss",           SWS_GAUSS},
            {"sinc",            SWS_SINC},
            {"lanczos",         SWS_LANCZOS},
            {"spline",          SWS_SPLINE})},
        {"lgb", OPT_FLOAT(lum_gblur), M_RANGE(0, 100.0)},
        {"cgb", OPT_FLOAT(chr_gblur), M_RANGE(0, 100.0)},
        {"cvs", OPT_INT(chr_vshift)},
        {"chs", OPT_INT(chr_hshift)},
        {"ls", OPT_FLOAT(lum_sharpen), M_RANGE(-100.0, 100.0)},
        {"cs", OPT_FLOAT(chr_sharpen), M_RANGE(-100.0, 100.0)},
        {"fast", OPT_FLAG(fast)},
        {"bitexact", OPT_FLAG(bitexact)},
        {"allow-zimg", OPT_FLAG(zimg)},
        {0}
    },
    .size = sizeof(struct sws_opts),
    .defaults = &(const struct sws_opts){
        .scaler = SWS_LANCZOS,
        .zimg = 1,
    },
};

// Highest quality, but also slowest.
static const int mp_sws_hq_flags = SWS_FULL_CHR_H_INT | SWS_FULL_CHR_H_INP |
                                   SWS_ACCURATE_RND;

// Fast, lossy.
const int mp_sws_fast_flags = SWS_BILINEAR;

// Set ctx parameters to global command line flags.
static void mp_sws_update_from_cmdline(struct mp_sws_context *ctx)
{
    m_config_cache_update(ctx->opts_cache);
    struct sws_opts *opts = ctx->opts_cache->opts;

    sws_freeFilter(ctx->src_filter);
    ctx->src_filter = sws_getDefaultFilter(opts->lum_gblur, opts->chr_gblur,
                                           opts->lum_sharpen, opts->chr_sharpen,
                                           opts->chr_hshift, opts->chr_vshift, 0);
    ctx->force_reload = true;

    ctx->flags = SWS_PRINT_INFO;
    ctx->flags |= opts->scaler;
    if (!opts->fast)
        ctx->flags |= mp_sws_hq_flags;
    if (opts->bitexact)
        ctx->flags |= SWS_BITEXACT;

    ctx->allow_zimg = opts->zimg;
}

bool mp_sws_supported_format(int imgfmt)
{
    enum AVPixelFormat av_format = imgfmt2pixfmt(imgfmt);

    return av_format != AV_PIX_FMT_NONE && sws_isSupportedInput(av_format)
        && sws_isSupportedOutput(av_format);
}

static bool allow_zimg(struct mp_sws_context *ctx)
{
    return ctx->force_scaler == MP_SWS_ZIMG ||
           (ctx->force_scaler == MP_SWS_AUTO && ctx->allow_zimg);
}

static bool allow_sws(struct mp_sws_context *ctx)
{
    return ctx->force_scaler == MP_SWS_SWS || ctx->force_scaler == MP_SWS_AUTO;
}

bool mp_sws_supports_formats(struct mp_sws_context *ctx,
                             int imgfmt_out, int imgfmt_in)
{
#if HAVE_ZIMG
    if (allow_zimg(ctx)) {
        if (mp_zimg_supports_in_format(imgfmt_in) &&
            mp_zimg_supports_out_format(imgfmt_out))
            return true;
    }
#endif

    return allow_sws(ctx) &&
           sws_isSupportedInput(imgfmt2pixfmt(imgfmt_in)) &&
           sws_isSupportedOutput(imgfmt2pixfmt(imgfmt_out));
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
           ctx->allow_zimg == old->allow_zimg &&
           ctx->force_scaler == old->force_scaler &&
           (!ctx->opts_cache || !m_config_cache_update(ctx->opts_cache));
}

static void free_mp_sws(void *p)
{
    struct mp_sws_context *ctx = p;
    sws_freeContext(ctx->sws);
    sws_freeFilter(ctx->src_filter);
    sws_freeFilter(ctx->dst_filter);
    TA_FREEP(&ctx->aligned_src);
    TA_FREEP(&ctx->aligned_dst);
}

// You're supposed to set your scaling parameters on the returned context.
// Free the context with talloc_free().
struct mp_sws_context *mp_sws_alloc(void *talloc_ctx)
{
    struct mp_sws_context *ctx = talloc_ptrtype(talloc_ctx, ctx);
    *ctx = (struct mp_sws_context) {
        .log = mp_null_log,
        .flags = SWS_BILINEAR,
        .force_reload = true,
        .params = {SWS_PARAM_DEFAULT, SWS_PARAM_DEFAULT},
        .cached = talloc_zero(ctx, struct mp_sws_context),
    };
    talloc_set_destructor(ctx, free_mp_sws);

#if HAVE_ZIMG
    ctx->zimg = mp_zimg_alloc();
    talloc_steal(ctx, ctx->zimg);
#endif

    return ctx;
}

// Enable auto-update of parameters from command line. Don't try to set custom
// options (other than possibly .src/.dst), because they might be overwritten
// if the user changes any options.
void mp_sws_enable_cmdline_opts(struct mp_sws_context *ctx, struct mpv_global *g)
{
    if (ctx->opts_cache)
        return;

    ctx->opts_cache = m_config_cache_alloc(ctx, g, &sws_conf);
    ctx->force_reload = true;
    mp_sws_update_from_cmdline(ctx);

#if HAVE_ZIMG
    mp_zimg_enable_cmdline_opts(ctx->zimg, g);
#endif
}

// Reinitialize (if needed) - return error code.
// Optional, but possibly useful to avoid having to handle mp_sws_scale errors.
int mp_sws_reinit(struct mp_sws_context *ctx)
{
    struct mp_image_params src = ctx->src;
    struct mp_image_params dst = ctx->dst;

    if (cache_valid(ctx))
        return 0;

    if (ctx->opts_cache)
        mp_sws_update_from_cmdline(ctx);

    sws_freeContext(ctx->sws);
    ctx->sws = NULL;
    ctx->zimg_ok = false;
    TA_FREEP(&ctx->aligned_src);
    TA_FREEP(&ctx->aligned_dst);

#if HAVE_ZIMG
    if (allow_zimg(ctx)) {
        ctx->zimg->log = ctx->log;
        ctx->zimg->src = src;
        ctx->zimg->dst = dst;
        if (ctx->zimg_opts)
            ctx->zimg->opts = *ctx->zimg_opts;
        if (mp_zimg_config(ctx->zimg)) {
            ctx->zimg_ok = true;
            MP_VERBOSE(ctx, "Using zimg.\n");
            goto success;
        }
        MP_WARN(ctx, "Not using zimg, falling back to swscale.\n");
    }
#endif

    if (!allow_sws(ctx)) {
        MP_ERR(ctx, "No scaler.\n");
        return -1;
    }

    ctx->sws = sws_alloc_context();
    if (!ctx->sws)
        return -1;

    mp_image_params_guess_csp(&src); // sanitize colorspace/colorlevels
    mp_image_params_guess_csp(&dst);

    enum AVPixelFormat s_fmt = imgfmt2pixfmt(src.imgfmt);
    if (s_fmt == AV_PIX_FMT_NONE || sws_isSupportedInput(s_fmt) < 1) {
        MP_ERR(ctx, "Input image format %s not supported by libswscale.\n",
               mp_imgfmt_to_name(src.imgfmt));
        return -1;
    }

    enum AVPixelFormat d_fmt = imgfmt2pixfmt(dst.imgfmt);
    if (d_fmt == AV_PIX_FMT_NONE || sws_isSupportedOutput(d_fmt) < 1) {
        MP_ERR(ctx, "Output image format %s not supported by libswscale.\n",
               mp_imgfmt_to_name(dst.imgfmt));
        return -1;
    }

    int s_csp = mp_csp_to_sws_colorspace(src.color.space);
    int s_range = src.color.levels == MP_CSP_LEVELS_PC;

    int d_csp = mp_csp_to_sws_colorspace(dst.color.space);
    int d_range = dst.color.levels == MP_CSP_LEVELS_PC;

    av_opt_set_int(ctx->sws, "sws_flags", ctx->flags, 0);

    av_opt_set_int(ctx->sws, "srcw", src.w, 0);
    av_opt_set_int(ctx->sws, "srch", src.h, 0);
    av_opt_set_int(ctx->sws, "src_format", s_fmt, 0);

    av_opt_set_int(ctx->sws, "dstw", dst.w, 0);
    av_opt_set_int(ctx->sws, "dsth", dst.h, 0);
    av_opt_set_int(ctx->sws, "dst_format", d_fmt, 0);

    av_opt_set_double(ctx->sws, "param0", ctx->params[0], 0);
    av_opt_set_double(ctx->sws, "param1", ctx->params[1], 0);

    int cr_src = mp_chroma_location_to_av(src.chroma_location);
    int cr_dst = mp_chroma_location_to_av(dst.chroma_location);
    int cr_xpos, cr_ypos;
    if (avcodec_enum_to_chroma_pos(&cr_xpos, &cr_ypos, cr_src) >= 0) {
        av_opt_set_int(ctx->sws, "src_h_chr_pos", cr_xpos, 0);
        av_opt_set_int(ctx->sws, "src_v_chr_pos", cr_ypos, 0);
    }
    if (avcodec_enum_to_chroma_pos(&cr_xpos, &cr_ypos, cr_dst) >= 0) {
        av_opt_set_int(ctx->sws, "dst_h_chr_pos", cr_xpos, 0);
        av_opt_set_int(ctx->sws, "dst_v_chr_pos", cr_ypos, 0);
    }

    // This can fail even with normal operation, e.g. if a conversion path
    // simply does not support these settings.
    int r =
        sws_setColorspaceDetails(ctx->sws, sws_getCoefficients(s_csp), s_range,
                                 sws_getCoefficients(d_csp), d_range,
                                 0, 1 << 16, 1 << 16);
    ctx->supports_csp = r >= 0;

    if (sws_init_context(ctx->sws, ctx->src_filter, ctx->dst_filter) < 0)
        return -1;

success:
    ctx->force_reload = false;
    *ctx->cached = *ctx;
    return 1;
}

static struct mp_image *check_alignment(struct mp_log *log,
                                        struct mp_image **alloc,
                                        struct mp_image *img)
{
    // It's completely unclear which alignment libswscale wants (for performance)
    // or requires (for avoiding crashes and memory corruption).
    // Is it av_cpu_max_align()? Is it the hardcoded AVFrame "default" of 32
    // in get_video_buffer()? Is it whatever avcodec_align_dimensions2()
    // determines? It's like you can't win if you try to prevent libswscale from
    // corrupting memory...
    // So use 32, a value that has been experimentally determined to be safe,
    // and which in most cases is not larger than decoder output. It is smaller
    // or equal to what most image allocators in mpv/ffmpeg use.
    size_t align = 32;
    assert(align <= MP_IMAGE_BYTE_ALIGN); // or mp_image_alloc will not cut it

    bool is_aligned = true;
    for (int p = 0; p < img->num_planes; p++) {
        is_aligned &= MP_IS_ALIGNED((uintptr_t)img->planes[p], align);
        is_aligned &= MP_IS_ALIGNED(labs(img->stride[p]), align);
    }

    if (is_aligned)
        return img;

    if (!*alloc) {
        mp_verbose(log, "unaligned libswscale parameter; using slow copy.\n");
        *alloc = mp_image_alloc(img->imgfmt, img->w, img->h);
        if (!*alloc)
            return NULL;
    }

    mp_image_copy_attributes(*alloc, img);
    return *alloc;
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

#if HAVE_ZIMG
    if (ctx->zimg_ok)
        return mp_zimg_convert(ctx->zimg, dst, src) ? 0 : -1;
#endif

    struct mp_image *a_src = check_alignment(ctx->log, &ctx->aligned_src, src);
    struct mp_image *a_dst = check_alignment(ctx->log, &ctx->aligned_dst, dst);
    if (!a_src || !a_dst) {
        MP_ERR(ctx, "image allocation failed.\n");
        return -1;
    }

    if (a_src != src)
        mp_image_copy(a_src, src);

    sws_scale(ctx->sws, (const uint8_t *const *) a_src->planes, a_src->stride,
              0, a_src->h, a_dst->planes, a_dst->stride);

    if (a_dst != dst)
        mp_image_copy(dst, a_dst);

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
    ctx->flags = SWS_LANCZOS | mp_sws_hq_flags;
    ctx->src_filter = sws_getDefaultFilter(gblur, gblur, 0, 0, 0, 0, 0);
    ctx->force_reload = true;
    int res = mp_sws_scale(ctx, dst, src);
    talloc_free(ctx);
    return res;
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
    int avfmt = imgfmt2pixfmt(img->imgfmt);
    int to = AV_PIX_FMT_NONE;
    for (int n = 0; endian_swaps[n][0] != AV_PIX_FMT_NONE; n++) {
        if (endian_swaps[n][0] == avfmt)
            to = endian_swaps[n][1];
    }
    if (to == AV_PIX_FMT_NONE || !mp_image_make_writeable(img))
        return img;
    int elems = img->fmt.bpp[0] / 8 / 2 * img->w;
    for (int y = 0; y < img->h; y++) {
        uint16_t *p = (uint16_t *)(img->planes[0] + y * img->stride[0]);
        for (int i = 0; i < elems; i++)
            p[i] = av_be2ne16(p[i]);
    }
    mp_image_setfmt(img, pixfmt2imgfmt(to));
    return img;
}

// vim: ts=4 sw=4 et tw=80
