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

#include <math.h>

#include "common/common.h"
#include "common/msg.h"
#include "csputils.h"
#include "options/m_config.h"
#include "options/m_option.h"
#include "video/img_format.h"
#include "zimg.h"

static_assert(MP_IMAGE_BYTE_ALIGN >= ZIMG_ALIGN, "");

static const struct m_opt_choice_alternatives mp_zimg_scalers[] = {
    {"point",           ZIMG_RESIZE_POINT},
    {"bilinear",        ZIMG_RESIZE_BILINEAR},
    {"bicubic",         ZIMG_RESIZE_BICUBIC},
    {"spline16",        ZIMG_RESIZE_SPLINE16},
    {"spline36",        ZIMG_RESIZE_SPLINE36},
    {"lanczos",         ZIMG_RESIZE_LANCZOS},
    {0}
};

#define OPT_PARAM(name, var, flags) \
    OPT_DOUBLE(name, var, (flags) | M_OPT_DEFAULT_NAN)

#define OPT_BASE_STRUCT struct zimg_opts
const struct m_sub_options zimg_conf = {
    .opts = (struct m_option[]) {
        OPT_CHOICE_C("scaler", scaler, 0, mp_zimg_scalers),
        OPT_PARAM("scaler-param-a", scaler_params[0], 0),
        OPT_PARAM("scaler-param-b", scaler_params[1], 0),
        OPT_CHOICE_C("scaler-chroma", scaler_chroma, 0, mp_zimg_scalers),
        OPT_PARAM("scaler-chroma-param-a", scaler_chroma_params[0], 0),
        OPT_PARAM("scaler-chroma-param-b", scaler_chroma_params[1], 0),
        OPT_CHOICE("dither", dither, 0,
                   ({"no",              ZIMG_DITHER_NONE},
                    {"ordered",         ZIMG_DITHER_ORDERED},
                    {"random",          ZIMG_DITHER_RANDOM},
                    {"error-diffusion", ZIMG_DITHER_ERROR_DIFFUSION})),
        OPT_FLAG("fast", fast, 0),
        {0}
    },
    .size = sizeof(struct zimg_opts),
    .defaults = &(const struct zimg_opts){
        .scaler = ZIMG_RESIZE_LANCZOS,
        .scaler_params = {NAN, NAN},
        .scaler_chroma_params = {NAN, NAN},
        .scaler_chroma = ZIMG_RESIZE_BILINEAR,
        .dither = ZIMG_DITHER_RANDOM,
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
    unsigned zmask[4];          // zmask[n] = zimg_image_buffer.plane[n].mask
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

static void mp_zimg_update_from_cmdline(struct mp_zimg_context *ctx)
{
    m_config_cache_update(ctx->opts_cache);

    struct zimg_opts *opts = ctx->opts_cache->opts;
    ctx->opts = *opts;
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
    };
    ctx->opts = *(struct zimg_opts *)zimg_conf.defaults;
    talloc_set_destructor(ctx, free_mp_zimg);
    return ctx;
}

void mp_zimg_enable_cmdline_opts(struct mp_zimg_context *ctx,
                                 struct mpv_global *g)
{
    if (ctx->opts_cache)
        return;

    ctx->opts_cache = m_config_cache_alloc(ctx, g, &zimg_conf);
    destroy_zimg(ctx); // force update
    mp_zimg_update_from_cmdline(ctx); // first update
}

static int repack_align(void *user, unsigned i, unsigned x0, unsigned x1)
{
    struct mp_zimg_repack *r = user;

    for (int p = 0; p < r->mpi->fmt.num_planes; p++) {
        int bpp = r->mpi->fmt.bytes[p];
        int xs = r->mpi->fmt.xs[p];
        int ys = r->mpi->fmt.ys[p];
        // Number of lines on this plane.
        int h = (1 << r->mpi->fmt.chroma_ys) - (1 << ys) + 1;

        for (int y = i; y < i + h; y++) {
            void *a = r->mpi->planes[p] +
                      r->mpi->stride[p] * (ptrdiff_t)(y >> ys) +
                      bpp * (x0 >> xs);
            void *b = r->tmp->planes[p] +
                      r->tmp->stride[p] * (ptrdiff_t)((y >> ys) & r->zmask[p]) +
                      bpp * (x0 >> xs);
            size_t size = ((x1 - x0) >> xs) * bpp;
            if (r->pack) {
                memcpy(a, b, size);
            } else {
                memcpy(b, a, size);
            }
        }
    }

    return 0;
}

// PA = PAck, copy planar input to single packed array
// UN = UNpack, copy packed input to planar output
// Naming convention:
//  pa_/un_ prefix to identify conversion direction.
//  Left (LSB, lowest byte address) -> Right (MSB, highest byte address).
//      (This is unusual; MSG to LSB is more commonly used to describe formats,
//       but our convention makes more sense for byte access in little endian.)
//  "c" identifies a color component.
//  "z" identifies known zero padding.
//  "o" identifies opaque alpha (unused/unsupported yet).
//  "x" identifies uninitialized padding.
//  A component is followed by its size in bits.
//  Size can be omitted for multiple uniform components (c8c8c8 == ccc8).
// Unpackers will often use "x" for padding, because they ignore it, while
// packets will use "z" because they write zero.

#define PA_WORD_3(name, packed_t, plane_t, sh_c0, sh_c1, sh_c2, pad)        \
    static void name(void *dst, void *src[], int x0, int x1) {              \
        for (int x = x0; x < x1; x++) {                                     \
            ((packed_t *)dst)[x] = (pad) |                                  \
                ((packed_t)((plane_t *)src[0])[x] << (sh_c0)) |             \
                ((packed_t)((plane_t *)src[1])[x] << (sh_c1)) |             \
                ((packed_t)((plane_t *)src[2])[x] << (sh_c2));              \
        }                                                                   \
    }

#define UN_WORD_3(name, packed_t, plane_t, sh_c0, sh_c1, sh_c2, mask)       \
    static void name(void *src, void *dst[], int x0, int x1) {              \
        for (int x = x0; x < x1; x++) {                                     \
            packed_t c = ((packed_t *)src)[x];                              \
            ((plane_t *)dst[0])[x] = (c >> (sh_c0)) & (mask);               \
            ((plane_t *)dst[1])[x] = (c >> (sh_c1)) & (mask);               \
            ((plane_t *)dst[2])[x] = (c >> (sh_c2)) & (mask);               \
        }                                                                   \
    }

UN_WORD_3(un_ccc8x8,  uint32_t, uint8_t,  0, 8,  16, 0xFFu)
PA_WORD_3(pa_ccc8z8,  uint32_t, uint8_t,  0, 8,  16, 0)
UN_WORD_3(un_x8ccc8,  uint32_t, uint8_t,  8, 16, 24, 0xFFu)
PA_WORD_3(pa_z8ccc8,  uint32_t, uint8_t,  8, 16, 24, 0)
UN_WORD_3(un_ccc10x2, uint32_t, uint16_t, 0, 10, 20, 0x3FFu)
PA_WORD_3(pa_ccc10z2, uint32_t, uint16_t, 0, 10, 20, 0)

#define PA_WORD_2(name, packed_t, plane_t, sh_c0, sh_c1, pad)               \
    static void name(void *dst, void *src[], int x0, int x1) {              \
        for (int x = x0; x < x1; x++) {                                     \
            ((packed_t *)dst)[x] = (pad) |                                  \
                ((packed_t)((plane_t *)src[0])[x] << (sh_c0)) |             \
                ((packed_t)((plane_t *)src[1])[x] << (sh_c1));              \
        }                                                                   \
    }

#define UN_WORD_2(name, packed_t, plane_t, sh_c0, sh_c1, mask)              \
    static void name(void *src, void *dst[], int x0, int x1) {              \
        for (int x = x0; x < x1; x++) {                                     \
            packed_t c = ((packed_t *)src)[x];                              \
            ((plane_t *)dst[0])[x] = (c >> (sh_c0)) & (mask);               \
            ((plane_t *)dst[1])[x] = (c >> (sh_c1)) & (mask);               \
        }                                                                   \
    }

UN_WORD_2(un_cc8,  uint16_t, uint8_t,  0, 8,  0xFFu)
PA_WORD_2(pa_cc8,  uint16_t, uint8_t,  0, 8,  0)
UN_WORD_2(un_cc16, uint32_t, uint16_t, 0, 16, 0xFFFFu)
PA_WORD_2(pa_cc16, uint32_t, uint16_t, 0, 16, 0)

#define PA_SEQ_3(name, comp_t)                                              \
    static void name(void *dst, void *src[], int x0, int x1) {              \
        comp_t *r = dst;                                                    \
        for (int x = x0; x < x1; x++) {                                     \
            *r++ = ((comp_t *)src[0])[x];                                   \
            *r++ = ((comp_t *)src[1])[x];                                   \
            *r++ = ((comp_t *)src[2])[x];                                   \
        }                                                                   \
    }

#define UN_SEQ_3(name, comp_t)                                              \
    static void name(void *src, void *dst[], int x0, int x1) {              \
        comp_t *r = src;                                                    \
        for (int x = x0; x < x1; x++) {                                     \
            ((comp_t *)dst[0])[x] = *r++;                                   \
            ((comp_t *)dst[1])[x] = *r++;                                   \
            ((comp_t *)dst[2])[x] = *r++;                                   \
        }                                                                   \
    }

UN_SEQ_3(un_ccc8,  uint8_t)
PA_SEQ_3(pa_ccc8,  uint8_t)
UN_SEQ_3(un_ccc16, uint16_t)
PA_SEQ_3(pa_ccc16, uint16_t)

// "regular": single packed plane, all components have same width (except padding)
struct regular_repacker {
    int packed_width;       // number of bits of the packed pixel
    int component_width;    // number of bits for a single component
    int prepadding;         // number of bits of LSB padding
    int num_components;     // number of components that can be accessed
    void (*pa_scanline)(void *p1, void *p2[], int x0, int x1);
    void (*un_scanline)(void *p1, void *p2[], int x0, int x1);
};

static const struct regular_repacker regular_repackers[] = {
    {32, 8,  0, 3, pa_ccc8z8,  un_ccc8x8},
    {32, 8,  8, 3, pa_z8ccc8,  un_x8ccc8},
    {24, 8,  0, 3, pa_ccc8,    un_ccc8},
    {48, 16, 0, 3, pa_ccc16,   un_ccc16},
    {16, 8,  0, 2, pa_cc8,     un_cc8},
    {32, 16, 0, 2, pa_cc16,    un_cc16},
    {32, 10, 0, 3, pa_ccc10z2, un_ccc10x2},
};

static int packed_repack(void *user, unsigned i, unsigned x0, unsigned x1)
{
    struct mp_zimg_repack *r = user;

    uint32_t *p1 =
        (void *)(r->mpi->planes[0] + r->mpi->stride[0] * (ptrdiff_t)i);

    void *p2[3];
    for (int p = 0; p < 3; p++) {
        int s = r->components[p];
        p2[p] = r->tmp->planes[s] +
                r->tmp->stride[s] * (ptrdiff_t)(i & r->zmask[s]);
    }

    r->packed_repack_scanline(p1, p2, x0, x1);

    return 0;
}

static int repack_nv(void *user, unsigned i, unsigned x0, unsigned x1)
{
    struct mp_zimg_repack *r = user;

    int xs = r->mpi->fmt.chroma_xs;
    int ys = r->mpi->fmt.chroma_ys;

    // Copy Y.
    int l_h = 1 << ys;
    for (int y = i; y < i + l_h; y++) {
        ptrdiff_t bpp = r->mpi->fmt.bytes[0];
        void *a = r->mpi->planes[0] +
                  r->mpi->stride[0] * (ptrdiff_t)y + bpp * x0;
        void *b = r->tmp->planes[0] +
                  r->tmp->stride[0] * (ptrdiff_t)(y & r->zmask[0]) + bpp * x0;
        size_t size = (x1 - x0) * bpp;
        if (r->pack) {
            memcpy(a, b, size);
        } else {
            memcpy(b, a, size);
        }
    }

    uint32_t *p1 =
        (void *)(r->mpi->planes[1] + r->mpi->stride[1] * (ptrdiff_t)(i >> ys));

    void *p2[2];
    for (int p = 0; p < 2; p++) {
        int s = r->components[p];
        p2[p] = r->tmp->planes[s] +
                r->tmp->stride[s] * (ptrdiff_t)((i >> ys) & r->zmask[s]);
    }

    r->packed_repack_scanline(p1, p2, x0 >> xs, x1 >> xs);

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
            *cb = repack_align;
        }
    }

    for (int n = 0; n < r->zplanes; n++) {
        int mplane = r->z_planes[n];
        buf->plane[n].data = wrap_mpi->planes[mplane];
        buf->plane[n].stride = wrap_mpi->stride[mplane];
        buf->plane[n].mask = wrap_mpi == mpi ? ZIMG_BUFFER_MAX : r->zmask[n];
    }

    r->mpi = mpi;
}

static void setup_nv_packer(struct mp_zimg_repack *r)
{
    struct mp_regular_imgfmt desc;
    if (!mp_get_regular_imgfmt(&desc, r->zimgfmt))
        return;

    // Check for NV.
    if (desc.num_planes != 2)
        return;
    if (desc.planes[0].num_components != 1 || desc.planes[0].components[0] != 1)
        return;
    if (desc.planes[1].num_components != 2)
        return;
    int cr0 = desc.planes[1].components[0];
    int cr1 = desc.planes[1].components[1];
    if (cr0 > cr1)
        MPSWAP(int, cr0, cr1);
    if (cr0 != 2 || cr1 != 3)
        return;

    // Construct equivalent planar format.
    struct mp_regular_imgfmt desc2 = desc;
    desc2.num_planes = 3;
    desc2.planes[1].num_components = 1;
    desc2.planes[1].components[0] = 2;
    desc2.planes[2].num_components = 1;
    desc2.planes[2].components[0] = 3;
    // For P010. Strangely this concept exists only for the NV format.
    if (desc2.component_pad > 0)
        desc2.component_pad = 0;

    int planar_fmt = mp_find_regular_imgfmt(&desc2);
    if (!planar_fmt)
        return;

    for (int i = 0; i < MP_ARRAY_SIZE(regular_repackers); i++) {
        const struct regular_repacker *pa = &regular_repackers[i];

        void (*repack_cb)(void *p1, void *p2[], int x0, int x1) =
            r->pack ? pa->pa_scanline : pa->un_scanline;

        if (pa->packed_width != desc.component_size * 2 * 8 ||
            pa->component_width != desc.component_size * 8 ||
            pa->num_components != 2 ||
            pa->prepadding != 0 ||
            !repack_cb)
            continue;

        r->repack = repack_nv;
        r->packed_repack_scanline = repack_cb;
        r->zimgfmt = planar_fmt;
        r->components[0] = desc.planes[1].components[0] - 1;
        r->components[1] = desc.planes[1].components[1] - 1;
        return;
    }
}

static void setup_misc_packer(struct mp_zimg_repack *r)
{
    // Although it's in regular_repackers[], the generic mpv imgfmt metadata
    // can't handle it yet.
    if (r->zimgfmt == IMGFMT_RGB30) {
        int planar_fmt = mp_imgfmt_find(0, 0, 3, 10, MP_IMGFLAG_RGB_P);
        if (!planar_fmt)
            return;
        r->zimgfmt = planar_fmt;
        r->repack = packed_repack;
        r->packed_repack_scanline = r->pack ? pa_ccc10z2 : un_ccc10x2;
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

    if (desc.num_planes != 1 || desc.planes[0].num_components < 3)
        return;
    struct mp_regular_imgfmt_plane *p = &desc.planes[0];

    for (int n = 0; n < p->num_components; n++) {
        if (p->components[n] >= 4) // no alpha
            return;
    }

    // padding must be in MSB or LSB
    if (p->components[0] && p->components[3])
        return;

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

    for (int i = 0; i < MP_ARRAY_SIZE(regular_repackers); i++) {
        const struct regular_repacker *pa = &regular_repackers[i];

        // The following may assumes little endian (because some repack backends
        // use word access, while the metadata here uses byte access).

        int prepad = p->components[0] ? 0 : 8;
        int first_comp = p->components[0] ? 0 : 1;
        void (*repack_cb)(void *p1, void *p2[], int x0, int x1) =
            r->pack ? pa->pa_scanline : pa->un_scanline;

        if (pa->packed_width != desc.component_size * p->num_components * 8 ||
            pa->component_width != depth ||
            pa->num_components != 3 ||
            pa->prepadding != prepad ||
            !repack_cb)
            continue;

        r->repack = packed_repack;
        r->packed_repack_scanline = repack_cb;
        r->zimgfmt = planar_fmt;
        for (int n = 0; n < 3; n++)
            r->components[n] = corder[p->components[first_comp + n]];
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
        setup_nv_packer(r);
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

    zfmt->subsample_w = mp_log2(desc.chroma_w);
    zfmt->subsample_h = mp_log2(desc.chroma_h);

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

    if (ctx && ctx->opts.fast) {
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

    r->zmask[0] = zimg_select_buffer_mask(lines);

    // Either ZIMG_BUFFER_MAX, or a power-of-2 slice buffer.
    assert(r->zmask[0] == ZIMG_BUFFER_MAX || MP_IS_POWER_OF_2(r->zmask[0] + 1));

    int h = r->zmask[0] == ZIMG_BUFFER_MAX ? r->fmt.h : r->zmask[0] + 1;
    if (h >= r->fmt.h) {
        h = r->fmt.h;
        r->zmask[0] = ZIMG_BUFFER_MAX;
    }

    r->tmp = mp_image_alloc(r->zimgfmt, r->fmt.w, h);
    talloc_steal(r, r->tmp);

    if (r->tmp) {
        for (int n = 1; n < r->tmp->fmt.num_planes; n++) {
            r->zmask[n] = r->zmask[0];
            if (r->zmask[0] != ZIMG_BUFFER_MAX)
                r->zmask[n] = r->zmask[n] >> r->tmp->fmt.ys[n];
        }
    }

    return !!r->tmp;
}

bool mp_zimg_config(struct mp_zimg_context *ctx)
{
    struct zimg_opts *opts = &ctx->opts;

    destroy_zimg(ctx);

    if (ctx->opts_cache)
        mp_zimg_update_from_cmdline(ctx);

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

    params.resample_filter = opts->scaler;
    params.filter_param_a = opts->scaler_params[0];
    params.filter_param_b = opts->scaler_params[1];

    params.resample_filter_uv = opts->scaler_chroma;
    params.filter_param_a_uv = opts->scaler_chroma_params[0];
    params.filter_param_b_uv = opts->scaler_chroma_params[1];

    params.dither_type = opts->dither;

    params.cpu_type = ZIMG_CPU_AUTO_64B;

    if (opts->fast)
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
        (!ctx->opts_cache || !m_config_cache_update(ctx->opts_cache)) &&
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
