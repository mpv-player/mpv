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

#include <libavutil/common.h>

#include "common/common.h"
#include "common/msg.h"
#include "csputils.h"
#include "options/m_config.h"
#include "options/m_option.h"
#include "video/img_format.h"
#include "zimg.h"

static_assert(MP_IMAGE_BYTE_ALIGN >= ZIMG_ALIGN, "");

struct zimg_opts {
    int scaler;
    int fast;
};

#define OPT_BASE_STRUCT struct zimg_opts
const struct m_sub_options zimg_conf = {
    .opts = (struct m_option[]) {
        OPT_CHOICE("scaler", scaler, 0,
                   ({"point",           ZIMG_RESIZE_POINT},
                    {"bilinear",        ZIMG_RESIZE_BILINEAR},
                    {"bicubic",         ZIMG_RESIZE_BICUBIC},
                    {"spline16",        ZIMG_RESIZE_SPLINE16},
                    {"lanczos",         ZIMG_RESIZE_LANCZOS})),
        OPT_FLAG("fast", fast, 0),
        {0}
    },
    .size = sizeof(struct zimg_opts),
    .defaults = &(const struct zimg_opts){
        .scaler = ZIMG_RESIZE_BILINEAR,
        .fast = 1,
    },
};

// Component ID (see struct mp_regular_imgfmt_plane.components) to plane index.
static const int corder_gbrp[4] = {0, 2, 0, 1};
static const int corder_yuv[4] = {0, 0, 1, 2};

struct mp_zimg_repack {
    bool pack;                  // if false, this is for unpacking
    struct mp_image_params fmt; // original mp format (possibly packed format)
    int zimgfmt;                // zimg equivalent unpacked format
    int zplanes;                // number of planes (zimgfmt)
    unsigned zmask;             // zimg_image_buffer.mask
    int z_planes[4];            // z_planes[zimg_index] = mp_index

    // If set, the pack/unpack callback to pass to zimg.
    // Called with user==mp_zimg_repack.
    zimg_filter_graph_callback repack;

    // For packed_repack.
    int components[4];          // p2[n] = mp_image.planes[components[n]]
    //  pack:   p1 is dst, p2 is src
    //  unpack: p1 is src, p2 is dst
    void (*packed_repack_scanline)(void *p1, void *p2[], int x0, int x1);

    // Temporary memory for slice-wise repacking. This may be set even if repack
    // is not set (then it may be used to avoid alignment issues). This has
    // about one slice worth of data.
    struct mp_image *tmp;

    // Temporary, per-call source/target frame. (Regrettably a mutable field,
    // but it's not the only one, and makes the callbacks much less of a mess
    // by avoiding another "closure" indirection.)
    // To be used by the repack callback.
    struct mp_image *mpi;
};

void mp_zimg_set_from_cmdline(struct mp_zimg_context *ctx, struct mpv_global *g)
{
    struct zimg_opts *opts = mp_get_config_group(NULL, g, &zimg_conf);

    ctx->scaler = opts->scaler;
    ctx->fast = opts->fast;

    talloc_free(opts);
}

static zimg_chroma_location_e mp_to_z_chroma(enum mp_chroma_location cl)
{
    switch (cl) {
    case MP_CHROMA_LEFT:        return ZIMG_CHROMA_LEFT;
    case MP_CHROMA_CENTER:      return ZIMG_CHROMA_CENTER;
    default:                    return ZIMG_CHROMA_LEFT;
    }
}

static zimg_matrix_coefficients_e mp_to_z_matrix(enum mp_csp csp)
{
    switch (csp) {
    case MP_CSP_BT_601:         return ZIMG_MATRIX_BT470_BG;
    case MP_CSP_BT_709:         return ZIMG_MATRIX_BT709;
    case MP_CSP_SMPTE_240M:     return ZIMG_MATRIX_ST240_M;
    case MP_CSP_BT_2020_NC:     return ZIMG_MATRIX_BT2020_NCL;
    case MP_CSP_BT_2020_C:      return ZIMG_MATRIX_BT2020_CL;
    case MP_CSP_RGB:            return ZIMG_MATRIX_RGB;
    case MP_CSP_XYZ:            return ZIMG_MATRIX_RGB;
    case MP_CSP_YCGCO:          return ZIMG_MATRIX_YCGCO;
    default:                    return ZIMG_MATRIX_BT709;
    }
}

static zimg_transfer_characteristics_e mp_to_z_trc(enum mp_csp_trc trc)
{
    switch (trc) {
    case MP_CSP_TRC_BT_1886:    return ZIMG_TRANSFER_BT709;
    case MP_CSP_TRC_SRGB:       return ZIMG_TRANSFER_IEC_61966_2_1;
    case MP_CSP_TRC_LINEAR:     return ZIMG_TRANSFER_LINEAR;
    case MP_CSP_TRC_GAMMA22:    return ZIMG_TRANSFER_BT470_M;
    case MP_CSP_TRC_GAMMA28:    return ZIMG_TRANSFER_BT470_BG;
    case MP_CSP_TRC_PQ:         return ZIMG_TRANSFER_ST2084;
    case MP_CSP_TRC_HLG:        return ZIMG_TRANSFER_ARIB_B67;
    case MP_CSP_TRC_GAMMA18:    // ?
    case MP_CSP_TRC_GAMMA20:
    case MP_CSP_TRC_GAMMA24:
    case MP_CSP_TRC_GAMMA26:
    case MP_CSP_TRC_PRO_PHOTO:
    case MP_CSP_TRC_V_LOG:
    case MP_CSP_TRC_S_LOG1:
    case MP_CSP_TRC_S_LOG2:     // ?
    default:                    return ZIMG_TRANSFER_BT709;
    }
}

static zimg_color_primaries_e mp_to_z_prim(enum mp_csp_prim prim)
{
    switch (prim) {
    case MP_CSP_PRIM_BT_601_525:return ZIMG_PRIMARIES_ST170_M;
    case MP_CSP_PRIM_BT_601_625:return ZIMG_PRIMARIES_BT470_BG;
    case MP_CSP_PRIM_BT_709:    return ZIMG_PRIMARIES_BT709;
    case MP_CSP_PRIM_BT_2020:   return ZIMG_PRIMARIES_BT2020;
    case MP_CSP_PRIM_BT_470M:   return ZIMG_PRIMARIES_BT470_M;
    case MP_CSP_PRIM_CIE_1931:  return ZIMG_PRIMARIES_ST428;
    case MP_CSP_PRIM_DCI_P3:    return ZIMG_PRIMARIES_ST431_2;
    case MP_CSP_PRIM_DISPLAY_P3:return ZIMG_PRIMARIES_ST432_1;
    case MP_CSP_PRIM_APPLE:     // ?
    case MP_CSP_PRIM_ADOBE:
    case MP_CSP_PRIM_PRO_PHOTO:
    case MP_CSP_PRIM_V_GAMUT:
    case MP_CSP_PRIM_S_GAMUT:   // ?
    default:                    return ZIMG_PRIMARIES_BT709;
    }
}

static void destroy_zimg(struct mp_zimg_context *ctx)
{
    free(ctx->zimg_tmp);
    ctx->zimg_tmp = NULL;
    zimg_filter_graph_free(ctx->zimg_graph);
    ctx->zimg_graph = NULL;
    TA_FREEP(&ctx->zimg_src);
    TA_FREEP(&ctx->zimg_dst);
}

static void free_mp_zimg(void *p)
{
    struct mp_zimg_context *ctx = p;

    destroy_zimg(ctx);
}

struct mp_zimg_context *mp_zimg_alloc(void)
{
    struct mp_zimg_context *ctx = talloc_ptrtype(NULL, ctx);
    *ctx = (struct mp_zimg_context) {
        .log = mp_null_log,
        .scaler = ZIMG_RESIZE_BILINEAR,
        .scaler_params = {NAN, NAN},
        .scaler_chroma = ZIMG_RESIZE_BILINEAR,
        .scaler_chroma_params = {NAN, NAN},
        .dither = ZIMG_DITHER_NONE,
        .fast = true,
    };
    talloc_set_destructor(ctx, free_mp_zimg);
    return ctx;
}

static void copy_rect(struct mp_image *dst, unsigned dst_mask,
                      struct mp_image *src, unsigned src_mask,
                      unsigned i, unsigned x0, unsigned x1)
{
    for (int p = 0; p < dst->fmt.num_planes; p++) {
        int bpp = dst->fmt.bytes[p];
        int xs = dst->fmt.xs[p];
        int ys = dst->fmt.ys[p];
        // Number of lines on this plane.
        int h = (1 << dst->fmt.chroma_ys) - (1 << ys) + 1;

        for (int y = i; y < i + h; y++) {
            void *psrc = src->planes[p] +
                         src->stride[p] * (ptrdiff_t)((y >> ys) & src_mask) +
                         bpp * (x0 >> xs);
            void *pdst = dst->planes[p] +
                         dst->stride[p] * (ptrdiff_t)((y >> ys) & dst_mask) +
                         bpp * (x0 >> xs);
            memcpy(pdst, psrc, ((x1 - x0) >> xs) * bpp);
        }
    }
}

static int align_pack(void *user, unsigned i, unsigned x0, unsigned x1)
{
    struct mp_zimg_repack *r = user;

    copy_rect(r->mpi, ZIMG_BUFFER_MAX, r->tmp, r->zmask, i, x0, x1);

    return 0;
}

static int align_unpack(void *user, unsigned i, unsigned x0, unsigned x1)
{
    struct mp_zimg_repack *r = user;

    copy_rect(r->tmp, r->zmask, r->mpi, ZIMG_BUFFER_MAX, i, x0, x1);

    return 0;
}

// 3 8 bit color components sourced from 3 planes, plus 8 MSB padding bits.
static void x8ccc8_pack(void *dst, void *src[], int x0, int x1)
{
    for (int x = x0; x < x1; x++) {
        ((uint32_t *)dst)[x] =
            ((uint8_t *)src[0])[x] |
            ((uint32_t)((uint8_t *)src[1])[x] << 8) |
            ((uint32_t)((uint8_t *)src[2])[x] << 16);
    }
}

// 3 8 bit color components sourced from 3 planes, plus 8 LSB padding bits.
static void ccc8x8_pack(void *dst, void *src[], int x0, int x1)
{
    for (int x = x0; x < x1; x++) {
        ((uint32_t *)dst)[x] =
            ((uint32_t)((uint8_t *)src[0])[x] << 8) |
            ((uint32_t)((uint8_t *)src[1])[x] << 16) |
            ((uint32_t)((uint8_t *)src[2])[x] << 24);
    }
}

// 3 16 bit color components written to 3 planes.
static void ccc16_unpack(void *src, void *dst[], int x0, int x1)
{
    uint16_t *r = src;
    for (int x = x0; x < x1; x++) {
        ((uint16_t *)dst[0])[x] = *r++;
        ((uint16_t *)dst[1])[x] = *r++;
        ((uint16_t *)dst[2])[x] = *r++;
    }
}

// 3 10 bit color components source from 3 planes, plus 2 MSB padding bits.
static void x2ccc10_pack(void *dst, void *src[], int x0, int x1)
{
    for (int x = x0; x < x1; x++) {
        ((uint32_t *)dst)[x] =
            ((uint16_t *)src[0])[x] |
            ((uint32_t)((uint16_t *)src[1])[x] << 10) |
            ((uint32_t)((uint16_t *)src[2])[x] << 20);
    }
}

static int packed_repack(void *user, unsigned i, unsigned x0, unsigned x1)
{
    struct mp_zimg_repack *r = user;

    uint32_t *p1 =
        (void *)(r->mpi->planes[0] + r->mpi->stride[0] * (ptrdiff_t)i);

    void *p2[3];
    for (int p = 0; p < 3; p++) {
        int s = r->components[p];
        p2[p] = r->tmp->planes[s] + r->tmp->stride[s] * (ptrdiff_t)(i & r->zmask);
    }

    r->packed_repack_scanline(p1, p2, x0, x1);

    return 0;
}

static void wrap_buffer(struct mp_zimg_repack *r,
                        zimg_image_buffer *buf,
                        zimg_filter_graph_callback *cb,
                        struct mp_image *mpi)
{
    *buf = (zimg_image_buffer){ZIMG_API_VERSION};
    *cb = r->repack;

    struct mp_image *wrap_mpi = r->tmp;

    if (!*cb) {
        bool aligned = true;
        for (int n = 0; n < r->zplanes; n++) {
            if (((uintptr_t)mpi->planes[n] % ZIMG_ALIGN) ||
                (mpi->stride[n] % ZIMG_ALIGN))
                aligned = false;
        }
        if (aligned) {
            wrap_mpi = mpi;
        } else {
            *cb = r->pack ? align_pack : align_unpack;
        }
    }

    for (int n = 0; n < r->zplanes; n++) {
        int mplane = r->z_planes[n];
        buf->plane[n].data = wrap_mpi->planes[mplane];
        buf->plane[n].stride = wrap_mpi->stride[mplane];
        buf->plane[n].mask = wrap_mpi == mpi ? ZIMG_BUFFER_MAX : r->zmask;
    }

    r->mpi = mpi;
}

static void setup_misc_packer(struct mp_zimg_repack *r)
{
    if (r->zimgfmt == IMGFMT_RGB30) {
        int planar_fmt = mp_imgfmt_find(0, 0, 3, 10, MP_IMGFLAG_RGB_P);
        if (!planar_fmt || !r->pack)
            return;
        r->zimgfmt = planar_fmt;
        r->repack = packed_repack;
        r->packed_repack_scanline = x2ccc10_pack;
        static int c_order[] = {3, 2, 1};
        for (int n = 0; n < 3; n++)
            r->components[n] = corder_gbrp[c_order[n]];
    }
}

// Tries to set a packer/unpacker for component-wise byte aligned RGB formats.
static void setup_regular_rgb_packer(struct mp_zimg_repack *r)
{
    struct mp_regular_imgfmt desc;
    if (!mp_get_regular_imgfmt(&desc, r->zimgfmt))
        return;

    if (desc.num_planes != 1 || desc.planes[0].num_components < 2)
        return;
    struct mp_regular_imgfmt_plane *p = &desc.planes[0];

    for (int n = 0; n < p->num_components; n++) {
        if (p->components[n] >= 4) // no alpha
            return;
    }

    // Component ID to plane, with 0 (padding) just mapping to plane 0.
    const int *corder = NULL;

    int typeflag = 0;
    enum mp_csp forced_csp = mp_imgfmt_get_forced_csp(r->zimgfmt);
    if (forced_csp == MP_CSP_RGB || forced_csp == MP_CSP_XYZ) {
        typeflag = MP_IMGFLAG_RGB_P;
        corder = corder_gbrp;
    } else {
        typeflag = MP_IMGFLAG_YUV_P;
        corder = corder_yuv;
    }

    // Find a compatible planar format (typically AV_PIX_FMT_GBRP).
    int depth = desc.component_size * 8 + MPMIN(0, desc.component_pad);
    int planar_fmt = mp_imgfmt_find(0, 0, 3, depth, typeflag);
    if (!planar_fmt)
        return;

    if (desc.component_size == 1 && p->num_components == 4) {
        if (!r->pack) // no unpacker yet
            return;
        if (p->components[0] && p->components[3]) // padding must be in MSB or LSB
            return;
        // The following assumes little endian (because the repack backends use
        // word access, while the metadata here uses byte access).
        int first = p->components[0] ? 0 : 1;
        r->repack = packed_repack;
        r->packed_repack_scanline = p->components[0] ? x8ccc8_pack : ccc8x8_pack;
        r->zimgfmt = planar_fmt;
        for (int n = 0; n < 3; n++)
            r->components[n] = corder[p->components[first + n]];
        return;
    }

    if (desc.component_size == 2 && p->num_components == 3) {
        if (r->pack) // no packer yet
            return;
        r->repack = packed_repack;
        r->packed_repack_scanline = ccc16_unpack;
        r->zimgfmt = planar_fmt;
        for (int n = 0; n < 3; n++)
            r->components[n] = corder[p->components[n]];
        return;
    }
}

// (ctx can be NULL for the sake of probing.)
static bool setup_format(zimg_image_format *zfmt, struct mp_zimg_repack *r,
                         struct mp_zimg_context *ctx)
{
    zimg_image_format_default(zfmt, ZIMG_API_VERSION);

    struct mp_image_params fmt = r->fmt;
    mp_image_params_guess_csp(&fmt);

    r->zimgfmt = fmt.imgfmt;

    if (!r->repack)
        setup_misc_packer(r);
    if (!r->repack)
        setup_regular_rgb_packer(r);

    struct mp_regular_imgfmt desc;
    if (!mp_get_regular_imgfmt(&desc, r->zimgfmt))
        return false;
    enum mp_csp csp = mp_imgfmt_get_forced_csp(r->zimgfmt);

    // no alpha plane, no odd chroma subsampling
    if (desc.num_planes > 3 || !MP_IS_POWER_OF_2(desc.chroma_w) ||
        !MP_IS_POWER_OF_2(desc.chroma_h))
        return false;

    // Accept only true planar formats.
    for (int n = 0; n < desc.num_planes; n++) {
        if (desc.planes[n].num_components != 1)
            return false;
        int c = desc.planes[n].components[0];
        if (c < 1 || c > 3)
            return false;
        // Unfortunately, ffmpeg prefers GBR order for planar RGB, while zimg
        // is sane. This makes it necessary to determine and fix the order.
        r->z_planes[c - 1] = n;
    }

    r->zplanes = desc.num_planes;

    zfmt->width = fmt.w;
    zfmt->height = fmt.h;

    zfmt->subsample_w = av_log2(desc.chroma_w);
    zfmt->subsample_h = av_log2(desc.chroma_h);

    zfmt->color_family = ZIMG_COLOR_YUV;
    if (desc.num_planes == 1) {
        zfmt->color_family = ZIMG_COLOR_GREY;
    } else if (csp == MP_CSP_RGB || csp == MP_CSP_XYZ) {
        zfmt->color_family = ZIMG_COLOR_RGB;
    }

    if (desc.component_type == MP_COMPONENT_TYPE_UINT &&
        desc.component_size == 1)
    {
        zfmt->pixel_type = ZIMG_PIXEL_BYTE;
    } else if (desc.component_type == MP_COMPONENT_TYPE_UINT &&
               desc.component_size == 2)
    {
        zfmt->pixel_type = ZIMG_PIXEL_WORD;
    } else if (desc.component_type == MP_COMPONENT_TYPE_FLOAT &&
               desc.component_size == 2)
    {
        zfmt->pixel_type = ZIMG_PIXEL_HALF;
    } else if (desc.component_type == MP_COMPONENT_TYPE_FLOAT &&
               desc.component_size == 4)
    {
        zfmt->pixel_type = ZIMG_PIXEL_FLOAT;
    } else {
        return false;
    }

    // (Formats like P010 are basically reported as P016.)
    zfmt->depth = desc.component_size * 8 + MPMIN(0, desc.component_pad);

    zfmt->pixel_range = fmt.color.levels == MP_CSP_LEVELS_PC ?
                        ZIMG_RANGE_FULL : ZIMG_RANGE_LIMITED;

    zfmt->matrix_coefficients = mp_to_z_matrix(fmt.color.space);
    zfmt->transfer_characteristics = mp_to_z_trc(fmt.color.gamma);
    zfmt->color_primaries = mp_to_z_prim(fmt.color.primaries);
    zfmt->chroma_location = mp_to_z_chroma(fmt.chroma_location);

    if (ctx && ctx->fast) {
        // mpv's default for RGB output slows down zimg significantly.
        if (zfmt->transfer_characteristics == ZIMG_TRANSFER_IEC_61966_2_1 &&
            zfmt->color_family == ZIMG_COLOR_RGB)
            zfmt->transfer_characteristics = ZIMG_TRANSFER_BT709;
    }

    return true;
}

static bool allocate_buffer(struct mp_zimg_context *ctx,
                            struct mp_zimg_repack *r)
{
    unsigned lines = 0;
    int err;
    if (r->pack) {
        err = zimg_filter_graph_get_output_buffering(ctx->zimg_graph, &lines);
    } else {
        err = zimg_filter_graph_get_input_buffering(ctx->zimg_graph, &lines);
    }

    if (err)
        return false;

    r->zmask = zimg_select_buffer_mask(lines);

    // Either ZIMG_BUFFER_MAX, or a power-of-2 slice buffer.
    assert(r->zmask == ZIMG_BUFFER_MAX || MP_IS_POWER_OF_2(r->zmask + 1));

    int h = r->zmask == ZIMG_BUFFER_MAX ? r->fmt.h : r->zmask + 1;
    if (h >= r->fmt.h) {
        h = r->fmt.h;
        r->zmask = ZIMG_BUFFER_MAX;
    }

    r->tmp = mp_image_alloc(r->zimgfmt, r->fmt.w, h);
    talloc_steal(r, r->tmp);

    return !!r->tmp;
}

bool mp_zimg_config(struct mp_zimg_context *ctx)
{
    destroy_zimg(ctx);

    ctx->zimg_src = talloc_zero(NULL, struct mp_zimg_repack);
    ctx->zimg_src->pack = false;
    ctx->zimg_src->fmt = ctx->src;

    ctx->zimg_dst = talloc_zero(NULL, struct mp_zimg_repack);
    ctx->zimg_dst->pack = true;
    ctx->zimg_dst->fmt = ctx->dst;

    zimg_image_format src_fmt, dst_fmt;

    if (!setup_format(&src_fmt, ctx->zimg_src, ctx) ||
        !setup_format(&dst_fmt, ctx->zimg_dst, ctx))
        goto fail;

    zimg_graph_builder_params params;
    zimg_graph_builder_params_default(&params, ZIMG_API_VERSION);

    params.resample_filter = ctx->scaler;
    params.filter_param_a = ctx->scaler_params[0];
    params.filter_param_b = ctx->scaler_params[1];

    params.resample_filter_uv = ctx->scaler_chroma;
    params.filter_param_a_uv = ctx->scaler_chroma_params[0];
    params.filter_param_b_uv = ctx->scaler_chroma_params[1];

    params.dither_type = ctx->dither;

    params.cpu_type = ZIMG_CPU_AUTO_64B;

    if (ctx->fast)
        params.allow_approximate_gamma = 1;

    if (ctx->src.color.sig_peak > 0)
        params.nominal_peak_luminance = ctx->src.color.sig_peak;

    ctx->zimg_graph = zimg_filter_graph_build(&src_fmt, &dst_fmt, &params);
    if (!ctx->zimg_graph) {
        char err[128] = {0};
        zimg_get_last_error(err, sizeof(err) - 1);
        MP_ERR(ctx, "zimg_filter_graph_build: %s \n", err);
        goto fail;
    }

    size_t tmp_size;
    if (!zimg_filter_graph_get_tmp_size(ctx->zimg_graph, &tmp_size)) {
        tmp_size = MP_ALIGN_UP(tmp_size, ZIMG_ALIGN);
        ctx->zimg_tmp = aligned_alloc(ZIMG_ALIGN, tmp_size);
    }

    if (!ctx->zimg_tmp)
        goto fail;

    if (!allocate_buffer(ctx, ctx->zimg_src) ||
        !allocate_buffer(ctx, ctx->zimg_dst))
        goto fail;

    return true;

fail:
    destroy_zimg(ctx);
    return false;
}

bool mp_zimg_config_image_params(struct mp_zimg_context *ctx)
{
    if (ctx->zimg_src && mp_image_params_equal(&ctx->src, &ctx->zimg_src->fmt) &&
        ctx->zimg_dst && mp_image_params_equal(&ctx->dst, &ctx->zimg_dst->fmt) &&
        ctx->zimg_graph)
        return true;
    return mp_zimg_config(ctx);
}

bool mp_zimg_convert(struct mp_zimg_context *ctx, struct mp_image *dst,
                     struct mp_image *src)
{
    ctx->src = src->params;
    ctx->dst = dst->params;

    if (!mp_zimg_config_image_params(ctx)) {
        MP_ERR(ctx, "zimg initialization failed.\n");
        return false;
    }

    assert(ctx->zimg_graph);

    zimg_image_buffer zsrc, zdst;
    zimg_filter_graph_callback cbsrc, cbdst;

    wrap_buffer(ctx->zimg_src, &zsrc, &cbsrc, src);
    wrap_buffer(ctx->zimg_dst, &zdst, &cbdst, dst);

    // An annoyance.
    zimg_image_buffer_const zsrc_c = {ZIMG_API_VERSION};
    for (int n = 0; n < 3; n++) {
        zsrc_c.plane[n].data = zsrc.plane[n].data;
        zsrc_c.plane[n].stride = zsrc.plane[n].stride;
        zsrc_c.plane[n].mask = zsrc.plane[n].mask;
    }

    // (The API promises to succeed if no user callbacks fail, so no need
    // to check the return value.)
    zimg_filter_graph_process(ctx->zimg_graph, &zsrc_c, &zdst,
                              ctx->zimg_tmp,
                              cbsrc, ctx->zimg_src,
                              cbdst, ctx->zimg_dst);

    ctx->zimg_src->mpi = NULL;
    ctx->zimg_dst->mpi = NULL;

    return true;
}

static bool supports_format(int imgfmt, bool out)
{
    struct mp_zimg_repack t = {
        .pack = out,
        .fmt = {
            .imgfmt = imgfmt,
        },
    };
    zimg_image_format fmt;
    return setup_format(&fmt, &t, NULL);
}

bool mp_zimg_supports_in_format(int imgfmt)
{
    return supports_format(imgfmt, false);
}

bool mp_zimg_supports_out_format(int imgfmt)
{
    return supports_format(imgfmt, true);
}
