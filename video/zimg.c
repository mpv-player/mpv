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

#include <libavutil/bswap.h>
#include <libavutil/pixfmt.h>

#include "common/common.h"
#include "common/msg.h"
#include "csputils.h"
#include "options/m_config.h"
#include "options/m_option.h"
#include "video/fmt-conversion.h"
#include "video/img_format.h"
#include "zimg.h"

static_assert(MP_IMAGE_BYTE_ALIGN >= ZIMG_ALIGN, "");

#define HAVE_ZIMG_ALPHA (ZIMG_API_VERSION >= ZIMG_MAKE_API_VERSION(2, 4))

static const struct m_opt_choice_alternatives mp_zimg_scalers[] = {
    {"point",           ZIMG_RESIZE_POINT},
    {"bilinear",        ZIMG_RESIZE_BILINEAR},
    {"bicubic",         ZIMG_RESIZE_BICUBIC},
    {"spline16",        ZIMG_RESIZE_SPLINE16},
    {"spline36",        ZIMG_RESIZE_SPLINE36},
    {"lanczos",         ZIMG_RESIZE_LANCZOS},
    {0}
};

#define OPT_PARAM(var) OPT_DOUBLE(var), .flags = M_OPT_DEFAULT_NAN

#define OPT_BASE_STRUCT struct zimg_opts
const struct m_sub_options zimg_conf = {
    .opts = (struct m_option[]) {
        {"scaler", OPT_CHOICE_C(scaler, mp_zimg_scalers)},
        {"scaler-param-a", OPT_PARAM(scaler_params[0])},
        {"scaler-param-b", OPT_PARAM(scaler_params[1])},
        {"scaler-chroma", OPT_CHOICE_C(scaler_chroma, mp_zimg_scalers)},
        {"scaler-chroma-param-a", OPT_PARAM(scaler_chroma_params[0])},
        {"scaler-chroma-param-b", OPT_PARAM(scaler_chroma_params[1])},
        {"dither", OPT_CHOICE(dither,
            {"no",              ZIMG_DITHER_NONE},
            {"ordered",         ZIMG_DITHER_ORDERED},
            {"random",          ZIMG_DITHER_RANDOM},
            {"error-diffusion", ZIMG_DITHER_ERROR_DIFFUSION})},
        {"fast", OPT_FLAG(fast)},
        {0}
    },
    .size = sizeof(struct zimg_opts),
    .defaults = &(const struct zimg_opts){
        .scaler = ZIMG_RESIZE_LANCZOS,
        .scaler_params = {NAN, NAN},
        .scaler_chroma_params = {NAN, NAN},
        .scaler_chroma = ZIMG_RESIZE_BILINEAR,
        .dither = ZIMG_DITHER_RANDOM,
        .fast = 1,
    },
};

struct mp_zimg_repack {
    bool pack;                  // if false, this is for unpacking
    struct mp_image_params fmt; // original mp format (possibly packed format,
                                // swapped endian)
    int zimgfmt;                // zimg equivalent unpacked format
    int num_planes;             // number of planes involved
    unsigned zmask[4];          // zmask[mp_index] = zimg mask (using mp index!)
    int z_planes[4];            // z_planes[zimg_index] = mp_index (or -1)
    bool pass_through_y;        // luma plane optimization for e.g. nv12

    // If set, the pack/unpack callback to pass to zimg.
    // Called with user==mp_zimg_repack.
    zimg_filter_graph_callback repack;

    // Endian-swap (done before/after actual repacker).
    int endian_size;            // 0=no swapping, 2/4=word byte size to swap
    int endian_items[4];        // number of words per pixel/plane

    // For packed_repack.
    int components[4];          // p2[n] = mp_image.planes[components[n]]
    //  pack:   p1 is dst, p2 is src
    //  unpack: p1 is src, p2 is dst
    void (*packed_repack_scanline)(void *p1, void *p2[], int x0, int x1);

    // Fringe RGB/YUV.
    uint8_t comp_size;
    uint8_t *comp_map;
    uint8_t comp_shifts[3];
    uint8_t *comp_lut; // 256 * 3

    // Temporary memory for slice-wise repacking. This may be set even if repack
    // is not set (then it may be used to avoid alignment issues). This has
    // about one slice worth of data.
    struct mp_image *tmp;

    // Temporary memory for endian swapping. This has about one slice worth
    // of data; set and used only if endian swapping is used (endian_size>0).
    // It's also used only for pack==false; packers do this in-place.
    struct mp_image *tmp_endian;

    // Temporary, per-call source/target frame.
    struct mp_image *mpi;
    // Y coordinate of first line in mpi; usually 0 if mpi==user_mpi, or the
    // start of the current slice (in the current repack cb).
    // repackers should use: mpi->data[p] + mpi->stride[p] * (i - mpi_y0)
    int mpi_y0;

    struct mp_image *user_mpi;

    // Also temporary, per-call. use_buf[n] == plane n uses tmp (and not mpi).
    bool use_buf[4];

    int real_w, real_h;         // aligned size
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
    talloc_free(ctx->zimg_tmp_alloc);
    ctx->zimg_tmp = ctx->zimg_tmp_alloc = NULL;
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
        if (!r->use_buf[p])
            continue;

        int bpp = r->mpi->fmt.bytes[p];
        int xs = r->mpi->fmt.xs[p];
        int ys = r->mpi->fmt.ys[p];
        // Number of lines on this plane.
        int h = (1 << r->mpi->fmt.chroma_ys) - (1 << ys) + 1;

        for (int y = i; y < i + h; y++) {
            void *a = r->mpi->planes[p] +
                      r->mpi->stride[p] * (ptrdiff_t)((y - r->mpi_y0) >> ys) +
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

// Swap endian for one line.
static void swap_endian(struct mp_zimg_repack *r, struct mp_image *dst, int dst_y,
                        struct mp_image *src, int src_y, int x0, int x1)
{
    for (int p = 0; p < dst->fmt.num_planes; p++) {
        int xs = dst->fmt.xs[p];
        int ys = dst->fmt.ys[p];
        int words_per_pixel = r->endian_items[p];
        int bpp = words_per_pixel * r->endian_size;
        // Number of lines on this plane.
        int h = (1 << dst->fmt.chroma_ys) - (1 << ys) + 1;
        int num_words = ((x1 - x0) >> xs) * words_per_pixel;

        for (int y = 0; y < h; y++) {
            void *s = src->planes[p] +
                      src->stride[p] * (ptrdiff_t)((y + src_y) >> ys) +
                      bpp * (x0 >> xs);
            void *d = dst->planes[p] +
                      dst->stride[p] * (ptrdiff_t)((y + dst_y) >> ys) +
                      bpp * (x0 >> xs);
            switch (r->endian_size) {
            case 2:
                for (int w = 0; w < num_words; w++)
                    ((uint16_t *)d)[w] = av_bswap16(((uint16_t *)s)[w]);
                break;
            case 4:
                for (int w = 0; w < num_words; w++)
                    ((uint32_t *)d)[w] = av_bswap32(((uint32_t *)s)[w]);
                break;
            default:
                assert(0);
            }
        }
    }
}

// PA = PAck, copy planar input to single packed array
// UN = UNpack, copy packed input to planar output
// Naming convention:
//  pa_/un_ prefix to identify conversion direction.
//  Left (LSB, lowest byte address) -> Right (MSB, highest byte address).
//      (This is unusual; MSB to LSB is more commonly used to describe formats,
//       but our convention makes more sense for byte access in little endian.)
//  "c" identifies a color component.
//  "z" identifies known zero padding.
//  "x" identifies uninitialized padding.
//  A component is followed by its size in bits.
//  Size can be omitted for multiple uniform components (c8c8c8 == ccc8).
// Unpackers will often use "x" for padding, because they ignore it, while
// packers will use "z" because they write zero.

#define PA_WORD_4(name, packed_t, plane_t, sh_c0, sh_c1, sh_c2, sh_c3)      \
    static void name(void *dst, void *src[], int x0, int x1) {              \
        for (int x = x0; x < x1; x++) {                                     \
            ((packed_t *)dst)[x] =                                          \
                ((packed_t)((plane_t *)src[0])[x] << (sh_c0)) |             \
                ((packed_t)((plane_t *)src[1])[x] << (sh_c1)) |             \
                ((packed_t)((plane_t *)src[2])[x] << (sh_c2)) |             \
                ((packed_t)((plane_t *)src[3])[x] << (sh_c3));              \
        }                                                                   \
    }

#define UN_WORD_4(name, packed_t, plane_t, sh_c0, sh_c1, sh_c2, sh_c3, mask)\
    static void name(void *src, void *dst[], int x0, int x1) {              \
        for (int x = x0; x < x1; x++) {                                     \
            packed_t c = ((packed_t *)src)[x];                              \
            ((plane_t *)dst[0])[x] = (c >> (sh_c0)) & (mask);               \
            ((plane_t *)dst[1])[x] = (c >> (sh_c1)) & (mask);               \
            ((plane_t *)dst[2])[x] = (c >> (sh_c2)) & (mask);               \
            ((plane_t *)dst[3])[x] = (c >> (sh_c3)) & (mask);               \
        }                                                                   \
    }


#define PA_WORD_3(name, packed_t, plane_t, sh_c0, sh_c1, sh_c2, pad)        \
    static void name(void *dst, void *src[], int x0, int x1) {              \
        for (int x = x0; x < x1; x++) {                                     \
            ((packed_t *)dst)[x] = (pad) |                                  \
                ((packed_t)((plane_t *)src[0])[x] << (sh_c0)) |             \
                ((packed_t)((plane_t *)src[1])[x] << (sh_c1)) |             \
                ((packed_t)((plane_t *)src[2])[x] << (sh_c2));              \
        }                                                                   \
    }

UN_WORD_4(un_cccc8,  uint32_t, uint8_t,  0, 8,  16, 24, 0xFFu)
PA_WORD_4(pa_cccc8,  uint32_t, uint8_t,  0, 8,  16, 24)
// Not sure if this is a good idea; there may be no alignment guarantee.
UN_WORD_4(un_cccc16,  uint64_t, uint16_t,  0, 16,  32, 48, 0xFFFFu)
PA_WORD_4(pa_cccc16,  uint64_t, uint16_t,  0, 16,  32, 48)

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
PA_WORD_3(pa_ccc10z2, uint32_t, uint16_t, 20, 10, 0, 0)

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
    {32, 8,  0, 4, pa_cccc8,   un_cccc8},
    {64, 16, 0, 4, pa_cccc16,  un_cccc16},
    {24, 8,  0, 3, pa_ccc8,    un_ccc8},
    {48, 16, 0, 3, pa_ccc16,   un_ccc16},
    {16, 8,  0, 2, pa_cc8,     un_cc8},
    {32, 16, 0, 2, pa_cc16,    un_cc16},
    {32, 10, 0, 3, pa_ccc10z2, un_ccc10x2},
};

static int packed_repack(void *user, unsigned i, unsigned x0, unsigned x1)
{
    struct mp_zimg_repack *r = user;

    uint32_t *p1 = (void *)(r->mpi->planes[0] +
                            r->mpi->stride[0] * (ptrdiff_t)(i - r->mpi_y0));

    void *p2[4] = {0};
    for (int p = 0; p < r->num_planes; p++) {
        int s = r->components[p];
        p2[p] = r->tmp->planes[s] +
                r->tmp->stride[s] * (ptrdiff_t)(i & r->zmask[s]);
    }

    r->packed_repack_scanline(p1, p2, x0, x1);

    return 0;
}

struct fringe_rgb_repacker {
    // To avoid making a mess of IMGFMT_*, we use av formats directly.
    enum AVPixelFormat avfmt;
    // If true, use BGR instead of RGB.
    //  False:  LSB - R - G - B - pad - MSB
    //  True:   LSB - B - G - R - pad - MSB
    bool rev_order;
    // Size in bit for each component, strictly from LSB to MSB.
    int bits[3];
    bool be;
};

static const struct fringe_rgb_repacker fringe_rgb_repackers[] = {
    {AV_PIX_FMT_BGR4_BYTE,  false,  {1, 2, 1}},
    {AV_PIX_FMT_RGB4_BYTE,  true,   {1, 2, 1}},
    {AV_PIX_FMT_BGR8,       false,  {3, 3, 2}},
    {AV_PIX_FMT_RGB8,       true,   {2, 3, 3}}, // pixdesc desc. and doc. bug?
    {AV_PIX_FMT_RGB444LE,   true,   {4, 4, 4}},
    {AV_PIX_FMT_RGB444BE,   true,   {4, 4, 4}, .be = true},
    {AV_PIX_FMT_BGR444LE,   false,  {4, 4, 4}},
    {AV_PIX_FMT_BGR444BE,   false,  {4, 4, 4}, .be = true},
    {AV_PIX_FMT_BGR565LE,   false,  {5, 6, 5}},
    {AV_PIX_FMT_BGR565BE,   false,  {5, 6, 5}, .be = true},
    {AV_PIX_FMT_RGB565LE,   true,   {5, 6, 5}},
    {AV_PIX_FMT_RGB565BE,   true,   {5, 6, 5}, .be = true},
    {AV_PIX_FMT_BGR555LE,   false,  {5, 5, 5}},
    {AV_PIX_FMT_BGR555BE,   false,  {5, 5, 5}, .be = true},
    {AV_PIX_FMT_RGB555LE,   true,   {5, 5, 5}},
    {AV_PIX_FMT_RGB555BE,   true,   {5, 5, 5}, .be = true},
};

#define PA_SHIFT_LUT8(name, packed_t)                                       \
    static void name(void *dst, void *src[], int x0, int x1, uint8_t *lut,  \
                     uint8_t s0, uint8_t s1, uint8_t s2) {                  \
        for (int x = x0; x < x1; x++) {                                     \
            ((packed_t *)dst)[x] =                                          \
                (lut[((uint8_t *)src[0])[x] + 256 * 0] << s0) |             \
                (lut[((uint8_t *)src[1])[x] + 256 * 1] << s1) |             \
                (lut[((uint8_t *)src[2])[x] + 256 * 2] << s2);              \
        }                                                                   \
    }


#define UN_SHIFT_LUT8(name, packed_t)                                       \
    static void name(void *src, void *dst[], int x0, int x1, uint8_t *lut,  \
                     uint8_t s0, uint8_t s1, uint8_t s2) {                  \
        for (int x = x0; x < x1; x++) {                                     \
            packed_t c = ((packed_t *)src)[x];                              \
            ((uint8_t *)dst[0])[x] = lut[((c >> s0) & 0xFF) + 256 * 0];     \
            ((uint8_t *)dst[1])[x] = lut[((c >> s1) & 0xFF) + 256 * 1];     \
            ((uint8_t *)dst[2])[x] = lut[((c >> s2) & 0xFF) + 256 * 2];     \
        }                                                                   \
    }

PA_SHIFT_LUT8(pa_shift_lut8_8,  uint8_t)
PA_SHIFT_LUT8(pa_shift_lut8_16, uint16_t)
UN_SHIFT_LUT8(un_shift_lut8_8,  uint8_t)
UN_SHIFT_LUT8(un_shift_lut8_16, uint16_t)

static int fringe_rgb_repack(void *user, unsigned i, unsigned x0, unsigned x1)
{
    struct mp_zimg_repack *r = user;

    void *p1 = r->mpi->planes[0] + r->mpi->stride[0] * (ptrdiff_t)(i - r->mpi_y0);

    void *p2[4] = {0};
    for (int p = 0; p < r->num_planes; p++) {
        int s = r->components[p];
        p2[p] = r->tmp->planes[s] +
                r->tmp->stride[s] * (ptrdiff_t)(i & r->zmask[s]);
    }

    assert(r->comp_size == 1 || r->comp_size == 2);

    void (*repack)(void *p1, void *p2[], int x0, int x1, uint8_t *lut,
                   uint8_t s0, uint8_t s1, uint8_t s2) = NULL;
    if (r->pack) {
        repack = r->comp_size == 1 ? pa_shift_lut8_8 : pa_shift_lut8_16;
    } else {
        repack = r->comp_size == 1 ? un_shift_lut8_8 : un_shift_lut8_16;
    }
    repack(p1, p2, x0, x1, r->comp_lut,
           r->comp_shifts[0], r->comp_shifts[1], r->comp_shifts[2]);

    return 0;
}

static int bitmap_repack(void *user, unsigned i, unsigned x0, unsigned x1)
{
    struct mp_zimg_repack *r = user;

    uint8_t *p1 =
        r->mpi->planes[0] + r->mpi->stride[0] * (ptrdiff_t)(i - r->mpi_y0);
    uint8_t *p2 =
        r->tmp->planes[0] + r->tmp->stride[0] * (ptrdiff_t)(i & r->zmask[0]);

    uint8_t swap = r->comp_size ? 0xFF : 0;
    if (r->pack) {
        // Supposedly zimg aligns this at least on 64 byte boundaries. Simplifies a
        // lot for us.
        assert(!(x0 & 7));

        for (int x = x0; x < x1; x += 8) {
            uint8_t d = 0;
            int max_b = MPMIN(8, x1 - x);
            for (int b = 0; b < max_b; b++)
                d |= (!!p2[x + b]) << (7 - b);
            p1[x / 8] = d ^ swap;
        }
    } else {
        x0 &= ~0x7;

        for (int x = x0; x < x1; x += 8) {
            uint8_t d = p1[x / 8] ^ swap;
            int max_b = MPMIN(8, x1 - x);
            for (int b = 0; b < max_b; b++)
                p2[x + b] = !!(d & (1 << (7 - b)));
        }
    }

    return 0;
}

static int unpack_pal(void *user, unsigned i, unsigned x0, unsigned x1)
{
    struct mp_zimg_repack *r = user;

    uint8_t *src = (void *)(r->mpi->planes[0] +
                            r->mpi->stride[0] * (ptrdiff_t)(i - r->mpi_y0));
    uint32_t *pal = (void *)r->mpi->planes[1];

    uint8_t *dst[4] = {0};
    for (int p = 0; p < r->num_planes; p++) {
        dst[p] = r->tmp->planes[p] +
                 r->tmp->stride[p] * (ptrdiff_t)(i & r->zmask[p]);
    }

    for (int x = x0; x < x1; x++) {
        uint32_t c = pal[src[x]];
        dst[0][x] = (c >>  8) & 0xFF; // G
        dst[1][x] = (c >>  0) & 0xFF; // B
        dst[2][x] = (c >> 16) & 0xFF; // R
        dst[3][x] = (c >> 24) & 0xFF; // A
    }

    return 0;
}

struct fringe_yuv422_repacker {
    // To avoid making a mess of IMGFMT_*, we use av formats directly.
    enum AVPixelFormat avfmt;
    // In bits (depth/8 rounded up gives byte size)
    int8_t depth;
    // Word index of each sample: {y0, y1, cb, cr}
    uint8_t comp[4];
    bool be;
};

static const struct fringe_yuv422_repacker fringe_yuv422_repackers[] = {
    {AV_PIX_FMT_YUYV422,  8, {0, 2, 1, 3}},
    {AV_PIX_FMT_UYVY422,  8, {1, 3, 0, 2}},
    {AV_PIX_FMT_YVYU422,  8, {0, 2, 3, 1}},
#ifdef AV_PIX_FMT_Y210
    {AV_PIX_FMT_Y210LE,  10, {0, 2, 1, 3}},
    {AV_PIX_FMT_Y210BE,  10, {0, 2, 1, 3}, .be = true},
#endif
};

#define PA_P422(name, comp_t)                                               \
    static void name(void *dst, void *src[], int x0, int x1, uint8_t *c) {  \
        for (int x = x0; x < x1; x += 2) {                                  \
            ((comp_t *)dst)[x * 2 + c[0]] = ((comp_t *)src[0])[x + 0];      \
            ((comp_t *)dst)[x * 2 + c[1]] = ((comp_t *)src[0])[x + 1];      \
            ((comp_t *)dst)[x * 2 + c[2]] = ((comp_t *)src[1])[x >> 1];     \
            ((comp_t *)dst)[x * 2 + c[3]] = ((comp_t *)src[2])[x >> 1];     \
        }                                                                   \
    }


#define UN_P422(name, comp_t)                                               \
    static void name(void *src, void *dst[], int x0, int x1, uint8_t *c) {  \
        for (int x = x0; x < x1; x += 2) {                                  \
            ((comp_t *)dst[0])[x + 0]  = ((comp_t *)src)[x * 2 + c[0]];     \
            ((comp_t *)dst[0])[x + 1]  = ((comp_t *)src)[x * 2 + c[1]];     \
            ((comp_t *)dst[1])[x >> 1] = ((comp_t *)src)[x * 2 + c[2]];     \
            ((comp_t *)dst[2])[x >> 1] = ((comp_t *)src)[x * 2 + c[3]];     \
        }                                                                   \
    }

PA_P422(pa_p422_8,  uint8_t)
PA_P422(pa_p422_16, uint16_t)
UN_P422(un_p422_8,  uint8_t)
UN_P422(un_p422_16, uint16_t)

static int fringe_yuv422_repack(void *user, unsigned i, unsigned x0, unsigned x1)
{
    struct mp_zimg_repack *r = user;

    void *p1 = r->mpi->planes[0] + r->mpi->stride[0] * (ptrdiff_t)(i - r->mpi_y0);

    void *p2[4] = {0};
    for (int p = 0; p < r->num_planes; p++) {
        p2[p] = r->tmp->planes[p] +
                r->tmp->stride[p] * (ptrdiff_t)(i & r->zmask[p]);
    }

    assert(r->comp_size == 1 || r->comp_size == 2);

    void (*repack)(void *p1, void *p2[], int x0, int x1, uint8_t *c) = NULL;
    if (r->pack) {
        repack = r->comp_size == 1 ? pa_p422_8 : pa_p422_16;
    } else {
        repack = r->comp_size == 1 ? un_p422_8 : un_p422_16;
    }
    repack(p1, p2, x0, x1, r->comp_map);

    return 0;
}

static int repack_nv(void *user, unsigned i, unsigned x0, unsigned x1)
{
    struct mp_zimg_repack *r = user;

    int xs = r->mpi->fmt.chroma_xs;
    int ys = r->mpi->fmt.chroma_ys;

    if (r->use_buf[0]) {
        // Copy Y.
        int l_h = 1 << ys;
        for (int y = i; y < i + l_h; y++) {
            ptrdiff_t bpp = r->mpi->fmt.bytes[0];
            void *a = r->mpi->planes[0] +
                    r->mpi->stride[0] * (ptrdiff_t)(y - r->mpi_y0) + bpp * x0;
            void *b = r->tmp->planes[0] +
                    r->tmp->stride[0] * (ptrdiff_t)(y & r->zmask[0]) + bpp * x0;
            size_t size = (x1 - x0) * bpp;
            if (r->pack) {
                memcpy(a, b, size);
            } else {
                memcpy(b, a, size);
            }
        }
    }

    uint32_t *p1 = (void *)(r->mpi->planes[1] +
                            r->mpi->stride[1] * (ptrdiff_t)((i - r->mpi_y0) >> ys));

    void *p2[2];
    for (int p = 0; p < 2; p++) {
        int s = r->components[p];
        p2[p] = r->tmp->planes[s] +
                r->tmp->stride[s] * (ptrdiff_t)((i >> ys) & r->zmask[s]);
    }

    r->packed_repack_scanline(p1, p2, x0 >> xs, x1 >> xs);

    return 0;
}

static int repack_entrypoint(void *user, unsigned i, unsigned x0, unsigned x1)
{
    struct mp_zimg_repack *r = user;

    if (r->endian_size && !r->pack) {
        r->mpi = r->tmp_endian;
        r->mpi_y0 = i;
        swap_endian(r, r->mpi, 0, r->user_mpi, i, x0, x1);
    } else {
        r->mpi = r->user_mpi;
        r->mpi_y0 = 0;
    }

    if (r->repack) {
        r->repack(r, i, x0, x1);
    } else {
        repack_align(r, i, x0, x1);
    }

    if (r->endian_size && r->pack)
        swap_endian(r, r->user_mpi, i, r->mpi, i - r->mpi_y0, x0, x1);

    r->mpi = NULL;
    return 0;
}

static void wrap_buffer(struct mp_zimg_repack *r,
                        zimg_image_buffer *buf,
                        struct mp_image *mpi)
{
    *buf = (zimg_image_buffer){ZIMG_API_VERSION};

    bool plane_aligned[4] = {0};
    for (int n = 0; n < r->num_planes; n++) {
        plane_aligned[n] = !((uintptr_t)mpi->planes[n] % ZIMG_ALIGN) &&
                           !(mpi->stride[n] % ZIMG_ALIGN);
    }

    for (int n = 0; n < MP_ARRAY_SIZE(buf->plane); n++) {
        // Note: this is really the only place we have to care about plane
        // permutation (zimg_image_buffer may have a different plane order
        // than the shadow mpi like r->tmp). We never use the zimg indexes
        // in other places.
        int mplane = r->z_planes[n];
        if (mplane < 0)
            continue;

        r->use_buf[mplane] = !plane_aligned[mplane] || r->endian_size;
        if (!(r->pass_through_y && mplane == 0))
            r->use_buf[mplane] |= !!r->repack;

        struct mp_image *tmpi = r->use_buf[mplane] ? r->tmp : mpi;
        buf->plane[n].data = tmpi->planes[mplane];
        buf->plane[n].stride = tmpi->stride[mplane];
        buf->plane[n].mask = r->use_buf[mplane] ? r->zmask[mplane]
                                                : ZIMG_BUFFER_MAX;
    }

    r->user_mpi = mpi;
}

// depth = number of LSB in use
static int find_gbrp_format(int depth, int num_planes)
{
    if (num_planes != 3 && num_planes != 4)
        return 0;
    struct mp_regular_imgfmt desc = {
        .component_type = MP_COMPONENT_TYPE_UINT,
        .forced_csp = MP_CSP_RGB,
        .component_size = depth > 8 ? 2 : 1,
        .component_pad = depth - (depth > 8 ? 16 : 8),
        .num_planes = num_planes,
        .planes = { {1, {2}}, {1, {3}}, {1, {1}}, {1, {4}} },
    };
    return mp_find_regular_imgfmt(&desc);
}

// depth = number of LSB in use
static int find_gray_format(int depth, int num_planes)
{
    if (num_planes != 1 && num_planes != 2)
        return 0;
    struct mp_regular_imgfmt desc = {
        .component_type = MP_COMPONENT_TYPE_UINT,
        .component_size = depth > 8 ? 2 : 1,
        .component_pad = depth - (depth > 8 ? 16 : 8),
        .num_planes = num_planes,
        .planes = { {1, {1}}, {1, {4}} },
    };
    return mp_find_regular_imgfmt(&desc);
}

static void setup_fringe_rgb_packer(struct mp_zimg_repack *r,
                                    struct mp_zimg_context *ctx)
{
    enum AVPixelFormat avfmt = imgfmt2pixfmt(r->zimgfmt);

    const struct fringe_rgb_repacker *fmt = NULL;
    for (int n = 0; n < MP_ARRAY_SIZE(fringe_rgb_repackers); n++) {
        if (fringe_rgb_repackers[n].avfmt == avfmt) {
            fmt = &fringe_rgb_repackers[n];
            break;
        }
    }

    if (!fmt)
        return;

    int depth = 8;
    if (r->pack) {
        // Dither to lowest depth - loses some precision, but result is saner.
        depth = fmt->bits[0];
        for (int n = 0; n < 3; n++)
            depth = MPMIN(depth, fmt->bits[n]);
    }

    r->zimgfmt = find_gbrp_format(depth, 3);
    if (!r->zimgfmt)
        return;
    if (ctx)
        r->comp_lut = talloc_array(ctx, uint8_t, 256 * 3);
    r->repack = fringe_rgb_repack;
    static const int c_order_rgb[] = {3, 1, 2};
    static const int c_order_bgr[] = {2, 1, 3};
    for (int n = 0; n < 3; n++)
        r->components[n] = (fmt->rev_order ? c_order_bgr : c_order_rgb)[n] - 1;

    int bitpos = 0;
    for (int n = 0; n < 3; n++) {
        int bits = fmt->bits[n];
        r->comp_shifts[n] = bitpos;
        if (r->comp_lut) {
            uint8_t *lut = r->comp_lut + 256 * n;
            uint8_t zmax = (1 << depth) - 1;
            uint8_t cmax = (1 << bits) - 1;
            for (int v = 0; v < 256; v++) {
                if (r->pack) {
                    lut[v] = (v * cmax + zmax / 2) / zmax;
                } else {
                    lut[v] = (v & cmax) * zmax / cmax;
                }
            }
        }
        bitpos += bits;
    }

    r->comp_size = (bitpos + 7) / 8;
    assert(r->comp_size == 1 || r->comp_size == 2);

    if (fmt->be) {
        assert(r->comp_size == 2);
        r->endian_size = 2;
        r->endian_items[0] = 1;
    }
}

static void setup_fringe_yuv422_packer(struct mp_zimg_repack *r)
{
    enum AVPixelFormat avfmt = imgfmt2pixfmt(r->zimgfmt);

    const struct fringe_yuv422_repacker *fmt = NULL;
    for (int n = 0; n < MP_ARRAY_SIZE(fringe_yuv422_repackers); n++) {
        if (fringe_yuv422_repackers[n].avfmt == avfmt) {
            fmt = &fringe_yuv422_repackers[n];
            break;
        }
    }

    if (!fmt)
        return;

    r->comp_size = (fmt->depth + 7) / 8;
    assert(r->comp_size == 1 || r->comp_size == 2);

    struct mp_regular_imgfmt yuvfmt = {
        .component_type = MP_COMPONENT_TYPE_UINT,
        // NB: same problem with P010 and not clearing padding.
        .component_size = r->comp_size,
        .num_planes = 3,
        .planes = { {1, {1}}, {1, {2}}, {1, {3}} },
        .chroma_xs = 1,
        .chroma_ys = 0,
    };
    r->zimgfmt = mp_find_regular_imgfmt(&yuvfmt);
    r->repack = fringe_yuv422_repack;
    r->comp_map = (uint8_t *)fmt->comp;

    if (fmt->be) {
        assert(r->comp_size == 2);
        r->endian_size = 2;
        r->endian_items[0] = 4;
    }
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
        r->pass_through_y = true;
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
        int planar_fmt = find_gbrp_format(10, 3);
        if (!planar_fmt)
            return;
        r->zimgfmt = planar_fmt;
        r->repack = packed_repack;
        r->packed_repack_scanline = r->pack ? pa_ccc10z2 : un_ccc10x2;
        static int c_order[] = {3, 2, 1};
        for (int n = 0; n < 3; n++)
            r->components[n] = c_order[n] - 1;
    } else if (r->zimgfmt == IMGFMT_PAL8 && !r->pack) {
        int grap_fmt = find_gbrp_format(8, 4);
        if (!grap_fmt)
            return;
        r->zimgfmt = grap_fmt;
        r->repack = unpack_pal;
    } else {
        enum AVPixelFormat avfmt = imgfmt2pixfmt(r->zimgfmt);
        if (avfmt == AV_PIX_FMT_MONOWHITE || avfmt == AV_PIX_FMT_MONOBLACK) {
            r->zimgfmt = IMGFMT_Y1;
            r->repack = bitmap_repack;
            r->comp_size = avfmt == AV_PIX_FMT_MONOWHITE; // abuse to pass a flag
            return;
        }
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

    int num_real_components = 0;
    bool has_alpha = false;
    for (int n = 0; n < p->num_components; n++) {
        if (p->components[n]) {
            has_alpha |= p->components[n] == 4;
            num_real_components += 1;
        } else {
            // padding must be in MSB or LSB
            if (n != 0 && n != p->num_components - 1)
                return;
        }
    }

    int depth = desc.component_size * 8 + MPMIN(0, desc.component_pad);

    int planar_fmt = num_real_components > 2
        ? find_gbrp_format(depth, num_real_components)
        : find_gray_format(depth, num_real_components);
    if (!planar_fmt)
        return;
    static const int reorder_gbrp[] = {0, 3, 1, 2, 4};
    static const int reorder_gray[] = {0, 1, 0, 0, 4};
    const int *reorder = num_real_components > 2 ? reorder_gbrp : reorder_gray;

    for (int i = 0; i < MP_ARRAY_SIZE(regular_repackers); i++) {
        const struct regular_repacker *pa = &regular_repackers[i];

        // The following may assume little endian (because some repack backends
        // use word access, while the metadata here uses byte access).

        int prepad = p->components[0] ? 0 : 8;
        int first_comp = p->components[0] ? 0 : 1;
        void (*repack_cb)(void *p1, void *p2[], int x0, int x1) =
            r->pack ? pa->pa_scanline : pa->un_scanline;

        if (pa->packed_width != desc.component_size * p->num_components * 8 ||
            pa->component_width != depth ||
            pa->num_components != num_real_components ||
            pa->prepadding != prepad ||
            !repack_cb)
            continue;

        r->repack = packed_repack;
        r->packed_repack_scanline = repack_cb;
        r->zimgfmt = planar_fmt;
        for (int n = 0; n < num_real_components; n++) {
            // Determine permutation that maps component order between the two
            // formats, with has_alpha special case (see above).
            int c = reorder[p->components[first_comp + n]];
            r->components[n] = c == 4 ? num_real_components - 1 : c - 1;
        }
        return;
    }
}

// (If native_fmt!=r->fmt.imgfmt, this is the swap-endian case; native_fmt is NE.)
// (ctx can be NULL for the sake of probing.)
static bool setup_format_ne(zimg_image_format *zfmt, struct mp_zimg_repack *r,
                            int native_fmt, struct mp_zimg_context *ctx)
{
    zimg_image_format_default(zfmt, ZIMG_API_VERSION);

    struct mp_image_params fmt = r->fmt;
    mp_image_params_guess_csp(&fmt);

    r->zimgfmt = native_fmt;

    if (!r->repack)
        setup_nv_packer(r);
    if (!r->repack)
        setup_misc_packer(r);
    if (!r->repack)
        setup_regular_rgb_packer(r);
    if (!r->repack)
        setup_fringe_rgb_packer(r, ctx);
    if (!r->repack)
        setup_fringe_yuv422_packer(r);

    struct mp_regular_imgfmt desc;
    if (!mp_get_regular_imgfmt(&desc, r->zimgfmt))
        return false;

    // no weird stuff
    if (desc.num_planes > 4)
        return false;

    // Endian swapping.
    if (native_fmt != fmt.imgfmt) {
        struct mp_regular_imgfmt ndesc;
        if (!mp_get_regular_imgfmt(&ndesc, native_fmt) || ndesc.num_planes > 4)
            return false;
        r->endian_size = ndesc.component_size;
        if (r->endian_size != 2 && r->endian_size != 4)
            return false;
        for (int n = 0; n < ndesc.num_planes; n++)
            r->endian_items[n] = ndesc.planes[n].num_components;
    }

    for (int n = 0; n < 4; n++)
        r->z_planes[n] = -1;

    // Accept only true planar formats.
    for (int n = 0; n < desc.num_planes; n++) {
        if (desc.planes[n].num_components != 1)
            return false;
        int c = desc.planes[n].components[0];
        if (c < 1 || c > 4)
            return false;
        if (c < 4) {
            // Unfortunately, ffmpeg prefers GBR order for planar RGB, while zimg
            // is sane. This makes it necessary to determine and fix the order.
            r->z_planes[c - 1] = n;
        } else {
            r->z_planes[3] = n; // alpha, always plane 4 in zimg

#if HAVE_ZIMG_ALPHA
            zfmt->alpha = fmt.alpha == MP_ALPHA_PREMUL
                ? ZIMG_ALPHA_PREMULTIPLIED : ZIMG_ALPHA_STRAIGHT;
#else
            return false;
#endif
        }
    }

    r->num_planes = desc.num_planes;

    // Note: formats with subsampled chroma may have odd width or height in
    // mpv and FFmpeg. This is because the width/height is actually a cropping
    // rectangle. Reconstruct the image allocation size and set the cropping.
    zfmt->width = r->real_w = MP_ALIGN_UP(fmt.w, 1 << desc.chroma_xs);
    zfmt->height = r->real_h = MP_ALIGN_UP(fmt.h, 1 << desc.chroma_ys);
    if (!r->pack && ctx) {
        // Relies on ctx->zimg_dst being initialized first.
        struct mp_zimg_repack *dst = ctx->zimg_dst;
        zfmt->active_region.width = dst->real_w * (double)fmt.w / dst->fmt.w;
        zfmt->active_region.height = dst->real_h * (double)fmt.h / dst->fmt.h;
    }

    zfmt->subsample_w = desc.chroma_xs;
    zfmt->subsample_h = desc.chroma_ys;

    zfmt->color_family = ZIMG_COLOR_YUV;
    if (desc.num_planes <= 2) {
        zfmt->color_family = ZIMG_COLOR_GREY;
    } else if (fmt.color.space == MP_CSP_RGB || fmt.color.space == MP_CSP_XYZ) {
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

    // mpv treats _some_ gray formats as RGB; zimg doesn't like this.
    if (zfmt->color_family == ZIMG_COLOR_GREY &&
        zfmt->matrix_coefficients == ZIMG_MATRIX_RGB)
        zfmt->matrix_coefficients = ZIMG_MATRIX_BT470_BG;

    return true;
}

static bool setup_format(zimg_image_format *zfmt, struct mp_zimg_repack *r,
                         bool pack, struct mp_image_params *fmt,
                         struct mp_zimg_context *ctx)
{
    struct mp_zimg_repack repack_init = {
        .pack = pack,
        .fmt = *fmt,
    };
    *r = repack_init;
    if (setup_format_ne(zfmt, r, fmt->imgfmt, ctx))
        return true;
    // Try reverse endian.
    int nimgfmt = mp_find_other_endian(fmt->imgfmt);
    if (!nimgfmt)
        return false;
    *r = repack_init;
    return setup_format_ne(zfmt, r, nimgfmt, ctx);
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

    if (!r->tmp)
        return false;

    for (int n = 1; n < r->tmp->fmt.num_planes; n++) {
        r->zmask[n] = r->zmask[0];
        if (r->zmask[0] != ZIMG_BUFFER_MAX)
            r->zmask[n] = r->zmask[n] >> r->tmp->fmt.ys[n];
    }

    if (r->endian_size && !r->pack) {
        r->tmp_endian = mp_image_alloc(r->fmt.imgfmt, r->fmt.w, h);
        talloc_steal(r, r->tmp_endian);

        if (!r->tmp_endian)
            return false;
    }

    return true;
}

bool mp_zimg_config(struct mp_zimg_context *ctx)
{
    struct zimg_opts *opts = &ctx->opts;

    destroy_zimg(ctx);

    if (ctx->opts_cache)
        mp_zimg_update_from_cmdline(ctx);

    ctx->zimg_src = talloc_zero(NULL, struct mp_zimg_repack);
    ctx->zimg_dst = talloc_zero(NULL, struct mp_zimg_repack);

    zimg_image_format src_fmt, dst_fmt;

    // Note: do zimg_dst first, because zimg_src uses fields from zimg_dst.
    if (!setup_format(&dst_fmt, ctx->zimg_dst, true, &ctx->dst, ctx) ||
        !setup_format(&src_fmt, ctx->zimg_src, false, &ctx->src, ctx))
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
        tmp_size = MP_ALIGN_UP(tmp_size, ZIMG_ALIGN) + ZIMG_ALIGN;
        ctx->zimg_tmp_alloc = ta_alloc_size(NULL, tmp_size);
        if (ctx->zimg_tmp_alloc) {
            ctx->zimg_tmp =
                (void *)MP_ALIGN_UP((uintptr_t)ctx->zimg_tmp_alloc, ZIMG_ALIGN);
        }
    }

    if (!ctx->zimg_tmp_alloc)
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
    wrap_buffer(ctx->zimg_src, &zsrc, src);
    wrap_buffer(ctx->zimg_dst, &zdst, dst);

    // An annoyance.
    zimg_image_buffer_const zsrc_c = {ZIMG_API_VERSION};
    for (int n = 0; n < MP_ARRAY_SIZE(zsrc_c.plane); n++) {
        zsrc_c.plane[n].data = zsrc.plane[n].data;
        zsrc_c.plane[n].stride = zsrc.plane[n].stride;
        zsrc_c.plane[n].mask = zsrc.plane[n].mask;
    }

    // (The API promises to succeed if no user callbacks fail, so no need
    // to check the return value.)
    zimg_filter_graph_process(ctx->zimg_graph, &zsrc_c, &zdst,
                              ctx->zimg_tmp,
                              repack_entrypoint, ctx->zimg_src,
                              repack_entrypoint, ctx->zimg_dst);

    ctx->zimg_src->user_mpi = NULL;
    ctx->zimg_dst->user_mpi = NULL;

    return true;
}

static bool supports_format(int imgfmt, bool out)
{
    struct mp_image_params fmt = {.imgfmt = imgfmt};
    struct mp_zimg_repack t;
    zimg_image_format zfmt;
    return setup_format(&zfmt, &t, out, &fmt, NULL);
}

bool mp_zimg_supports_in_format(int imgfmt)
{
    return supports_format(imgfmt, false);
}

bool mp_zimg_supports_out_format(int imgfmt)
{
    return supports_format(imgfmt, true);
}
