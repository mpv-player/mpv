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
#include "repack.h"
#include "video/csputils.h"
#include "video/fmt-conversion.h"
#include "video/img_format.h"
#include "video/mp_image.h"

enum repack_step_type {
    REPACK_STEP_FLOAT,
    REPACK_STEP_REPACK,
    REPACK_STEP_ENDIAN,
};

struct repack_step {
    enum repack_step_type type;
    // 0=input, 1=output
    struct mp_image *buf[2];
    bool user_buf[2]; // user_buf[n]==true if buf[n] = user src/dst buffer
    struct mp_imgfmt_desc fmt[2];
    struct mp_image *tmp; // output buffer, if needed
};

struct mp_repack {
    bool pack;                  // if false, this is for unpacking
    int flags;
    int imgfmt_user;            // original mp format (unchanged endian)
    int imgfmt_a;               // original mp format (possibly packed format,
                                // swapped endian)
    int imgfmt_b;               // equivalent unpacked/planar format
    struct mp_imgfmt_desc fmt_a;// ==imgfmt_a
    struct mp_imgfmt_desc fmt_b;// ==imgfmt_b

    void (*repack)(struct mp_repack *rp,
                   struct mp_image *a, int a_x, int a_y,
                   struct mp_image *b, int b_x, int b_y, int w);

    bool passthrough_y;         // possible luma plane optimization for e.g. nv12
    int endian_size;            // endian swap; 0=none, 2/4=swap word size

    // For packed_repack.
    int components[4];          // b[n] = mp_image.planes[components[n]]
    //  pack:   a is dst, b is src
    //  unpack: a is src, b is dst
    void (*packed_repack_scanline)(void *a, void *b[], int w);

    // Fringe RGB/YUV.
    uint8_t comp_size;
    uint8_t comp_map[6];
    uint8_t comp_shifts[3];
    uint8_t *comp_lut;
    void (*repack_fringe_yuv)(void *dst, void *src[], int w, uint8_t *c);

    // F32 repacking.
    int f32_comp_size;
    float f32_m[4], f32_o[4];
    uint32_t f32_pmax[4];
    enum pl_color_system f32_csp_space;
    enum pl_color_levels f32_csp_levels;

    // REPACK_STEP_REPACK: if true, need to copy this plane
    bool copy_buf[4];

    struct repack_step steps[4];
    int num_steps;

    bool configured;
};

// depth = number of LSB in use
static int find_gbrp_format(int depth, int num_planes)
{
    if (num_planes != 3 && num_planes != 4)
        return 0;
    struct mp_regular_imgfmt desc = {
        .component_type = MP_COMPONENT_TYPE_UINT,
        .forced_csp = PL_COLOR_SYSTEM_RGB,
        .component_size = depth > 8 ? 2 : 1,
        .component_pad = depth - (depth > 8 ? 16 : 8),
        .num_planes = num_planes,
        .planes = { {1, {2}}, {1, {3}}, {1, {1}}, {1, {4}} },
    };
    return mp_find_regular_imgfmt(&desc);
}

// depth = number of LSB in use
static int find_yuv_format(int depth, int num_planes)
{
    if (num_planes < 1 || num_planes > 4)
        return 0;
    struct mp_regular_imgfmt desc = {
        .component_type = MP_COMPONENT_TYPE_UINT,
        .component_size = depth > 8 ? 2 : 1,
        .component_pad = depth - (depth > 8 ? 16 : 8),
        .num_planes = num_planes,
        .planes = { {1, {1}}, {1, {2}}, {1, {3}}, {1, {4}} },
    };
    if (num_planes == 2)
        desc.planes[1].components[0] = 4;
    return mp_find_regular_imgfmt(&desc);
}

// Copy one line on the plane p.
static void copy_plane(struct mp_image *dst, int dst_x, int dst_y,
                       struct mp_image *src, int src_x, int src_y,
                       int w, int p)
{
    // Number of lines on this plane.
    int h = (1 << dst->fmt.chroma_ys) - (1 << dst->fmt.ys[p]) + 1;
    size_t size = mp_image_plane_bytes(dst, p, dst_x, w);

    assert(dst->fmt.bpp[p] == src->fmt.bpp[p]);

    for (int y = 0; y < h; y++) {
        void *pd = mp_image_pixel_ptr_ny(dst, p, dst_x, dst_y + y);
        void *ps = mp_image_pixel_ptr_ny(src, p, src_x, src_y + y);
        memcpy(pd, ps, size);
    }
}

// Swap endian for one line.
static void swap_endian(struct mp_image *dst, int dst_x, int dst_y,
                        struct mp_image *src, int src_x, int src_y,
                        int w, int endian_size)
{
    assert(src->fmt.num_planes == dst->fmt.num_planes);

    for (int p = 0; p < dst->fmt.num_planes; p++) {
        int xs = dst->fmt.xs[p];
        int bpp = dst->fmt.bpp[p] / 8;
        int words_per_pixel = bpp / endian_size;
        int num_words = ((w + (1 << xs) - 1) >> xs) * words_per_pixel;
        // Number of lines on this plane.
        int h = (1 << dst->fmt.chroma_ys) - (1 << dst->fmt.ys[p]) + 1;

        assert(src->fmt.bpp[p] == bpp * 8);

        for (int y = 0; y < h; y++) {
            void *s = mp_image_pixel_ptr_ny(src, p, src_x, src_y + y);
            void *d = mp_image_pixel_ptr_ny(dst, p, dst_x, dst_y + y);
            switch (endian_size) {
            case 2:
                for (int x = 0; x < num_words; x++)
                    ((uint16_t *)d)[x] = av_bswap16(((uint16_t *)s)[x]);
                break;
            case 4:
                for (int x = 0; x < num_words; x++)
                    ((uint32_t *)d)[x] = av_bswap32(((uint32_t *)s)[x]);
                break;
            default:
                MP_ASSERT_UNREACHABLE();
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
    static void name(void *dst, void *src[], int w) {                       \
        for (int x = 0; x < w; x++) {                                       \
            ((packed_t *)dst)[x] =                                          \
                ((packed_t)((plane_t *)src[0])[x] << (sh_c0)) |             \
                ((packed_t)((plane_t *)src[1])[x] << (sh_c1)) |             \
                ((packed_t)((plane_t *)src[2])[x] << (sh_c2)) |             \
                ((packed_t)((plane_t *)src[3])[x] << (sh_c3));              \
        }                                                                   \
    }

#define UN_WORD_4(name, packed_t, plane_t, sh_c0, sh_c1, sh_c2, sh_c3, mask)\
    static void name(void *src, void *dst[], int w) {                       \
        for (int x = 0; x < w; x++) {                                       \
            packed_t c = ((packed_t *)src)[x];                              \
            ((plane_t *)dst[0])[x] = (c >> (sh_c0)) & (mask);               \
            ((plane_t *)dst[1])[x] = (c >> (sh_c1)) & (mask);               \
            ((plane_t *)dst[2])[x] = (c >> (sh_c2)) & (mask);               \
            ((plane_t *)dst[3])[x] = (c >> (sh_c3)) & (mask);               \
        }                                                                   \
    }


#define PA_WORD_3(name, packed_t, plane_t, sh_c0, sh_c1, sh_c2, pad)        \
    static void name(void *dst, void *src[], int w) {                       \
        for (int x = 0; x < w; x++) {                                       \
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
    static void name(void *src, void *dst[], int w) {                       \
        for (int x = 0; x < w; x++) {                                       \
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
UN_WORD_3(un_ccc16x16, uint64_t, uint16_t, 0, 16, 32, 0xFFFFu)
PA_WORD_3(pa_ccc16z16, uint64_t, uint16_t, 0, 16, 32, 0)

#define PA_WORD_2(name, packed_t, plane_t, sh_c0, sh_c1, pad)               \
    static void name(void *dst, void *src[], int w) {                       \
        for (int x = 0; x < w; x++) {                                       \
            ((packed_t *)dst)[x] = (pad) |                                  \
                ((packed_t)((plane_t *)src[0])[x] << (sh_c0)) |             \
                ((packed_t)((plane_t *)src[1])[x] << (sh_c1));              \
        }                                                                   \
    }

#define UN_WORD_2(name, packed_t, plane_t, sh_c0, sh_c1, mask)              \
    static void name(void *src, void *dst[], int w) {                       \
        for (int x = 0; x < w; x++) {                                       \
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
    static void name(void *dst, void *src[], int w) {                       \
        comp_t *r = dst;                                                    \
        for (int x = 0; x < w; x++) {                                       \
            *r++ = ((comp_t *)src[0])[x];                                   \
            *r++ = ((comp_t *)src[1])[x];                                   \
            *r++ = ((comp_t *)src[2])[x];                                   \
        }                                                                   \
    }

#define UN_SEQ_3(name, comp_t)                                              \
    static void name(void *src, void *dst[], int w) {                       \
        comp_t *r = src;                                                    \
        for (int x = 0; x < w; x++) {                                       \
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
    void (*pa_scanline)(void *a, void *b[], int w);
    void (*un_scanline)(void *a, void *b[], int w);
};

static const struct regular_repacker regular_repackers[] = {
    {32, 8,  0, 3, pa_ccc8z8,   un_ccc8x8},
    {32, 8,  8, 3, pa_z8ccc8,   un_x8ccc8},
    {32, 8,  0, 4, pa_cccc8,    un_cccc8},
    {64, 16, 0, 4, pa_cccc16,   un_cccc16},
    {64, 16, 0, 3, pa_ccc16z16, un_ccc16x16},
    {24, 8,  0, 3, pa_ccc8,     un_ccc8},
    {48, 16, 0, 3, pa_ccc16,    un_ccc16},
    {16, 8,  0, 2, pa_cc8,      un_cc8},
    {32, 16, 0, 2, pa_cc16,     un_cc16},
    {32, 10, 0, 3, pa_ccc10z2,  un_ccc10x2},
};

static void packed_repack(struct mp_repack *rp,
                          struct mp_image *a, int a_x, int a_y,
                          struct mp_image *b, int b_x, int b_y, int w)
{
    uint32_t *pa = mp_image_pixel_ptr(a, 0, a_x, a_y);

    void *pb[4] = {0};
    for (int p = 0; p < b->num_planes; p++) {
        int s = rp->components[p];
        pb[p] = mp_image_pixel_ptr(b, s, b_x, b_y);
    }

    rp->packed_repack_scanline(pa, pb, w);
}

// Tries to set a packer/unpacker for component-wise byte aligned formats.
static void setup_packed_packer(struct mp_repack *rp)
{
    struct mp_imgfmt_desc desc = mp_imgfmt_get_desc(rp->imgfmt_a);
    if (!(desc.flags & MP_IMGFLAG_HAS_COMPS) ||
        !(desc.flags & MP_IMGFLAG_TYPE_UINT) ||
        !(desc.flags & MP_IMGFLAG_NE) ||
        desc.num_planes != 1)
        return;

    int num_real_components = 0;
    int components[4] = {0};
    for (int n = 0; n < MP_NUM_COMPONENTS; n++) {
        if (!desc.comps[n].size)
            continue;
        if (desc.comps[n].size != desc.comps[0].size ||
            desc.comps[n].pad != desc.comps[0].pad ||
            desc.comps[n].offset % desc.comps[0].size)
            return;
        int item = desc.comps[n].offset / desc.comps[0].size;
        if (item >= 4)
            return;
        components[item] = n + 1;
        num_real_components++;
    }

    int depth = desc.comps[0].size + MPMIN(0, desc.comps[0].pad);

    static const int reorder_gbrp[] = {0, 3, 1, 2, 4};
    static const int reorder_yuv[] = {0, 1, 2, 3, 4};
    int planar_fmt = 0;
    const int *reorder = NULL;
    if (desc.flags & MP_IMGFLAG_COLOR_YUV) {
        planar_fmt = find_yuv_format(depth, num_real_components);
        reorder = reorder_yuv;
    } else {
        planar_fmt = find_gbrp_format(depth, num_real_components);
        reorder = reorder_gbrp;
    }
    if (!planar_fmt)
        return;

    for (int i = 0; i < MP_ARRAY_SIZE(regular_repackers); i++) {
        const struct regular_repacker *pa = &regular_repackers[i];

        // The following may assume little endian (because some repack backends
        // use word access, while the metadata here uses byte access).

        int prepad = components[0] ? 0 : 8;
        int first_comp = components[0] ? 0 : 1;
        void (*repack_cb)(void *pa, void *pb[], int w) =
            rp->pack ? pa->pa_scanline : pa->un_scanline;

        if (pa->packed_width != desc.bpp[0] ||
            pa->component_width != depth ||
            pa->num_components != num_real_components ||
            pa->prepadding != prepad ||
            !repack_cb)
            continue;

        rp->repack = packed_repack;
        rp->packed_repack_scanline = repack_cb;
        rp->imgfmt_b = planar_fmt;
        for (int n = 0; n < num_real_components; n++) {
            // Determine permutation that maps component order between the two
            // formats, with has_alpha special case (see above).
            int c = reorder[components[first_comp + n]];
            rp->components[n] = c == 4 ? num_real_components - 1 : c - 1;
        }
        return;
    }
}

#define PA_SHIFT_LUT8(name, packed_t)                                       \
    static void name(void *dst, void *src[], int w, uint8_t *lut,           \
                     uint8_t s0, uint8_t s1, uint8_t s2) {                  \
        for (int x = 0; x < w; x++) {                                       \
            ((packed_t *)dst)[x] =                                          \
                (lut[((uint8_t *)src[0])[x] + 256 * 0] << s0) |             \
                (lut[((uint8_t *)src[1])[x] + 256 * 1] << s1) |             \
                (lut[((uint8_t *)src[2])[x] + 256 * 2] << s2);              \
        }                                                                   \
    }


#define UN_SHIFT_LUT8(name, packed_t)                                       \
    static void name(void *src, void *dst[], int w, uint8_t *lut,           \
                     uint8_t s0, uint8_t s1, uint8_t s2) {                  \
        for (int x = 0; x < w; x++) {                                       \
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

static void fringe_rgb_repack(struct mp_repack *rp,
                              struct mp_image *a, int a_x, int a_y,
                              struct mp_image *b, int b_x, int b_y, int w)
{
    void *pa = mp_image_pixel_ptr(a, 0, a_x, a_y);

    void *pb[4] = {0};
    for (int p = 0; p < b->num_planes; p++) {
        int s = rp->components[p];
        pb[p] = mp_image_pixel_ptr(b, s, b_x, b_y);
    }

    assert(rp->comp_size == 1 || rp->comp_size == 2);

    void (*repack)(void *pa, void *pb[], int w, uint8_t *lut,
                   uint8_t s0, uint8_t s1, uint8_t s2) = NULL;
    if (rp->pack) {
        repack = rp->comp_size == 1 ? pa_shift_lut8_8 : pa_shift_lut8_16;
    } else {
        repack = rp->comp_size == 1 ? un_shift_lut8_8 : un_shift_lut8_16;
    }
    repack(pa, pb, w, rp->comp_lut,
           rp->comp_shifts[0], rp->comp_shifts[1], rp->comp_shifts[2]);
}

static void setup_fringe_rgb_packer(struct mp_repack *rp)
{
    struct mp_imgfmt_desc desc = mp_imgfmt_get_desc(rp->imgfmt_a);
    if (!(desc.flags & MP_IMGFLAG_HAS_COMPS))
        return;

    if (desc.bpp[0] > 16 || (desc.bpp[0] % 8u) ||
        mp_imgfmt_get_forced_csp(rp->imgfmt_a) != PL_COLOR_SYSTEM_RGB ||
        desc.num_planes != 1 || desc.comps[3].size)
        return;

    int depth = desc.comps[0].size;
    for (int n = 0; n < 3; n++) {
        struct mp_imgfmt_comp_desc *c = &desc.comps[n];

        if (c->size < 1 || c->size > 8 || c->pad)
            return;

        if (rp->flags & REPACK_CREATE_ROUND_DOWN) {
            depth = MPMIN(depth, c->size);
        } else {
            depth = MPMAX(depth, c->size);
        }
    }
    if (rp->flags & REPACK_CREATE_EXPAND_8BIT)
        depth = 8;

    rp->imgfmt_b = find_gbrp_format(depth, 3);
    if (!rp->imgfmt_b)
        return;
    rp->comp_lut = talloc_array(rp, uint8_t, 256 * 3);
    rp->repack = fringe_rgb_repack;
    for (int n = 0; n < 3; n++)
        rp->components[n] = ((int[]){3, 1, 2})[n] - 1;

    for (int n = 0; n < 3; n++) {
        int bits = desc.comps[n].size;
        rp->comp_shifts[n] = desc.comps[n].offset;
        if (rp->comp_lut) {
            uint8_t *lut = rp->comp_lut + 256 * n;
            uint8_t zmax = (1 << depth) - 1;
            uint8_t cmax = (1 << bits) - 1;
            for (int v = 0; v < 256; v++) {
                if (rp->pack) {
                    lut[v] = (v * cmax + zmax / 2) / zmax;
                } else {
                    lut[v] = (v & cmax) * zmax / cmax;
                }
            }
        }
    }

    rp->comp_size = (desc.bpp[0] + 7) / 8;
    assert(rp->comp_size == 1 || rp->comp_size == 2);

    if (desc.endian_shift) {
        assert(rp->comp_size == 2 && (1 << desc.endian_shift) == 2);
        rp->endian_size = 2;
    }
}

static void unpack_pal(struct mp_repack *rp,
                       struct mp_image *a, int a_x, int a_y,
                       struct mp_image *b, int b_x, int b_y, int w)
{
    uint8_t *src = mp_image_pixel_ptr(a, 0, a_x, a_y);
    uint32_t *pal = (void *)a->planes[1];

    uint8_t *dst[4] = {0};
    for (int p = 0; p < b->num_planes; p++)
        dst[p] = mp_image_pixel_ptr(b, p, b_x, b_y);

    for (int x = 0; x < w; x++) {
        uint32_t c = pal[src[x]];
        dst[0][x] = (c >>  8) & 0xFF; // G
        dst[1][x] = (c >>  0) & 0xFF; // B
        dst[2][x] = (c >> 16) & 0xFF; // R
        dst[3][x] = (c >> 24) & 0xFF; // A
    }
}

static void bitmap_repack(struct mp_repack *rp,
                          struct mp_image *a, int a_x, int a_y,
                          struct mp_image *b, int b_x, int b_y, int w)
{
    uint8_t *pa = mp_image_pixel_ptr(a, 0, a_x, a_y);
    uint8_t *pb = mp_image_pixel_ptr(b, 0, b_x, b_y);

    if (rp->pack) {
        for (unsigned x = 0; x < w; x += 8) {
            uint8_t d = 0;
            int max_b = MPMIN(8, w - x);
            for (int bp = 0; bp < max_b; bp++)
                d |= (rp->comp_lut[pb[x + bp]]) << (7 - bp);
            pa[x / 8] = d;
        }
    } else {
        for (unsigned x = 0; x < w; x += 8) {
            uint8_t d = pa[x / 8];
            int max_b = MPMIN(8, w - x);
            for (int bp = 0; bp < max_b; bp++)
                pb[x + bp] = rp->comp_lut[d & (1 << (7 - bp))];
        }
    }
}

static void setup_misc_packer(struct mp_repack *rp)
{
    if (rp->imgfmt_a == IMGFMT_PAL8 && !rp->pack) {
        int grap_fmt = find_gbrp_format(8, 4);
        if (!grap_fmt)
            return;
        rp->imgfmt_b = grap_fmt;
        rp->repack = unpack_pal;
    } else {
        enum AVPixelFormat avfmt = imgfmt2pixfmt(rp->imgfmt_a);
        if (avfmt == AV_PIX_FMT_MONOWHITE || avfmt == AV_PIX_FMT_MONOBLACK) {
            rp->comp_lut = talloc_array(rp, uint8_t, 256);
            rp->imgfmt_b = IMGFMT_Y1;
            int max = 1;
            if (rp->flags & REPACK_CREATE_EXPAND_8BIT) {
                rp->imgfmt_b = IMGFMT_Y8;
                max = 255;
            }
            bool inv = avfmt == AV_PIX_FMT_MONOWHITE;
            for (int n = 0; n < 256; n++) {
                rp->comp_lut[n] = rp->pack ? (inv ^ (n >= (max + 1) / 2))
                                           : ((inv ^ !!n) ? max : 0);
            }
            rp->repack = bitmap_repack;
            return;
        }
    }
}

#define PA_P422(name, comp_t)                                               \
    static void name(void *dst, void *src[], int w, uint8_t *c) {           \
        for (int x = 0; x < w; x += 2) {                                    \
            ((comp_t *)dst)[x * 2 + c[0]] = ((comp_t *)src[0])[x + 0];      \
            ((comp_t *)dst)[x * 2 + c[1]] = ((comp_t *)src[0])[x + 1];      \
            ((comp_t *)dst)[x * 2 + c[4]] = ((comp_t *)src[1])[x >> 1];     \
            ((comp_t *)dst)[x * 2 + c[5]] = ((comp_t *)src[2])[x >> 1];     \
        }                                                                   \
    }


#define UN_P422(name, comp_t)                                               \
    static void name(void *src, void *dst[], int w, uint8_t *c) {           \
        for (int x = 0; x < w; x += 2) {                                    \
            ((comp_t *)dst[0])[x + 0]  = ((comp_t *)src)[x * 2 + c[0]];     \
            ((comp_t *)dst[0])[x + 1]  = ((comp_t *)src)[x * 2 + c[1]];     \
            ((comp_t *)dst[1])[x >> 1] = ((comp_t *)src)[x * 2 + c[4]];     \
            ((comp_t *)dst[2])[x >> 1] = ((comp_t *)src)[x * 2 + c[5]];     \
        }                                                                   \
    }

PA_P422(pa_p422_8,  uint8_t)
PA_P422(pa_p422_16, uint16_t)
UN_P422(un_p422_8,  uint8_t)
UN_P422(un_p422_16, uint16_t)

static void pa_p411_8(void *dst, void *src[], int w, uint8_t *c)
{
    for (int x = 0; x < w; x += 4) {
        ((uint8_t *)dst)[x / 4 * 6 + c[0]] = ((uint8_t *)src[0])[x + 0];
        ((uint8_t *)dst)[x / 4 * 6 + c[1]] = ((uint8_t *)src[0])[x + 1];
        ((uint8_t *)dst)[x / 4 * 6 + c[2]] = ((uint8_t *)src[0])[x + 2];
        ((uint8_t *)dst)[x / 4 * 6 + c[3]] = ((uint8_t *)src[0])[x + 3];
        ((uint8_t *)dst)[x / 4 * 6 + c[4]] = ((uint8_t *)src[1])[x >> 2];
        ((uint8_t *)dst)[x / 4 * 6 + c[5]] = ((uint8_t *)src[2])[x >> 2];
    }
}


static void un_p411_8(void *src, void *dst[], int w, uint8_t *c)
{
    for (int x = 0; x < w; x += 4) {
        ((uint8_t *)dst[0])[x + 0]  = ((uint8_t *)src)[x / 4 * 6 + c[0]];
        ((uint8_t *)dst[0])[x + 1]  = ((uint8_t *)src)[x / 4 * 6 + c[1]];
        ((uint8_t *)dst[0])[x + 2]  = ((uint8_t *)src)[x / 4 * 6 + c[2]];
        ((uint8_t *)dst[0])[x + 3]  = ((uint8_t *)src)[x / 4 * 6 + c[3]];
        ((uint8_t *)dst[1])[x >> 2] = ((uint8_t *)src)[x / 4 * 6 + c[4]];
        ((uint8_t *)dst[2])[x >> 2] = ((uint8_t *)src)[x / 4 * 6 + c[5]];
    }
}

static void fringe_yuv_repack(struct mp_repack *rp,
                              struct mp_image *a, int a_x, int a_y,
                              struct mp_image *b, int b_x, int b_y, int w)
{
    void *pa = mp_image_pixel_ptr(a, 0, a_x, a_y);

    void *pb[4] = {0};
    for (int p = 0; p < b->num_planes; p++)
        pb[p] = mp_image_pixel_ptr(b, p, b_x, b_y);

    rp->repack_fringe_yuv(pa, pb, w, rp->comp_map);
}

static void setup_fringe_yuv_packer(struct mp_repack *rp)
{
    struct mp_imgfmt_desc desc = mp_imgfmt_get_desc(rp->imgfmt_a);
    if (!(desc.flags & MP_IMGFLAG_PACKED_SS_YUV) ||
        mp_imgfmt_desc_get_num_comps(&desc) != 3 ||
        desc.align_x > 4)
        return;

    uint8_t y_loc[4];
    if (!mp_imgfmt_get_packed_yuv_locations(desc.id, y_loc))
        return;

    for (int n = 0; n < MP_NUM_COMPONENTS; n++) {
        if (!desc.comps[n].size)
            continue;
        if (desc.comps[n].size != desc.comps[0].size ||
            desc.comps[n].pad < 0 ||
            desc.comps[n].offset % desc.comps[0].size)
            return;
        if (n == 1 || n == 2) {
            rp->comp_map[4 + (n - 1)] =
                desc.comps[n].offset / desc.comps[0].size;
        }
    }
    for (int n = 0; n < desc.align_x; n++) {
        if (y_loc[n] % desc.comps[0].size)
            return;
        rp->comp_map[n] = y_loc[n] / desc.comps[0].size;
    }

    if (desc.comps[0].size == 8 && desc.align_x == 2) {
        rp->repack_fringe_yuv = rp->pack ? pa_p422_8 : un_p422_8;
    } else if (desc.comps[0].size == 16 && desc.align_x == 2) {
        rp->repack_fringe_yuv = rp->pack ? pa_p422_16 : un_p422_16;
    } else if (desc.comps[0].size == 8 && desc.align_x == 4) {
        rp->repack_fringe_yuv = rp->pack ? pa_p411_8 : un_p411_8;
    }

    if (!rp->repack_fringe_yuv)
        return;

    struct mp_regular_imgfmt yuvfmt = {
        .component_type = MP_COMPONENT_TYPE_UINT,
        // NB: same problem with P010 and not clearing padding.
        .component_size = desc.comps[0].size / 8u,
        .num_planes = 3,
        .planes = { {1, {1}}, {1, {2}}, {1, {3}} },
        .chroma_xs = desc.chroma_xs,
        .chroma_ys = 0,
    };
    rp->imgfmt_b = mp_find_regular_imgfmt(&yuvfmt);
    rp->repack = fringe_yuv_repack;

    if (desc.endian_shift) {
        rp->endian_size = 1 << desc.endian_shift;
        assert(rp->endian_size == 2);
    }
}

static void repack_nv(struct mp_repack *rp,
                      struct mp_image *a, int a_x, int a_y,
                      struct mp_image *b, int b_x, int b_y, int w)
{
    int xs = a->fmt.chroma_xs;

    uint32_t *pa = mp_image_pixel_ptr(a, 1, a_x, a_y);

    void *pb[2];
    for (int p = 0; p < 2; p++) {
        int s = rp->components[p];
        pb[p] = mp_image_pixel_ptr(b, s, b_x, b_y);
    }

    rp->packed_repack_scanline(pa, pb, (w + (1 << xs) - 1) >> xs);
}

static void setup_nv_packer(struct mp_repack *rp)
{
    struct mp_regular_imgfmt desc;
    if (!mp_get_regular_imgfmt(&desc, rp->imgfmt_a))
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

        void (*repack_cb)(void *pa, void *pb[], int w) =
            rp->pack ? pa->pa_scanline : pa->un_scanline;

        if (pa->packed_width != desc.component_size * 2 * 8 ||
            pa->component_width != desc.component_size * 8 ||
            pa->num_components != 2 ||
            pa->prepadding != 0 ||
            !repack_cb)
            continue;

        rp->repack = repack_nv;
        rp->passthrough_y = true;
        rp->packed_repack_scanline = repack_cb;
        rp->imgfmt_b = planar_fmt;
        rp->components[0] = desc.planes[1].components[0] - 1;
        rp->components[1] = desc.planes[1].components[1] - 1;
        return;
    }
}

#define PA_F32(name, packed_t)                                              \
    static void name(void *dst, float *src, int w, float m, float o,        \
                     uint32_t p_max) {                                      \
        for (int x = 0; x < w; x++) {                                       \
            ((packed_t *)dst)[x] =                                          \
                MPCLAMP(lrint((src[x] + o) * m), 0, (packed_t)p_max);       \
        }                                                                   \
    }

#define UN_F32(name, packed_t)                                              \
    static void name(void *src, float *dst, int w, float m, float o,        \
                     uint32_t unused) {                                     \
        for (int x = 0; x < w; x++)                                         \
            dst[x] = ((packed_t *)src)[x] * m + o;                          \
    }

PA_F32(pa_f32_8, uint8_t)
UN_F32(un_f32_8, uint8_t)
PA_F32(pa_f32_16, uint16_t)
UN_F32(un_f32_16, uint16_t)

// In all this, float counts as "unpacked".
static void repack_float(struct mp_repack *rp,
                         struct mp_image *a, int a_x, int a_y,
                         struct mp_image *b, int b_x, int b_y, int w)
{
    assert(rp->f32_comp_size == 1 || rp->f32_comp_size == 2);

    void (*packer)(void *a, float *b, int w, float fm, float fb, uint32_t max)
        = rp->pack ? (rp->f32_comp_size == 1 ? pa_f32_8 : pa_f32_16)
                   : (rp->f32_comp_size == 1 ? un_f32_8 : un_f32_16);

    for (int p = 0; p < b->num_planes; p++) {
        int h = (1 << b->fmt.chroma_ys) - (1 << b->fmt.ys[p]) + 1;
        for (int y = 0; y < h; y++) {
            void *pa = mp_image_pixel_ptr_ny(a, p, a_x, a_y + y);
            void *pb = mp_image_pixel_ptr_ny(b, p, b_x, b_y + y);

            packer(pa, pb, w >> b->fmt.xs[p], rp->f32_m[p], rp->f32_o[p],
                   rp->f32_pmax[p]);
        }
    }
}

static void update_repack_float(struct mp_repack *rp)
{
    if (!rp->f32_comp_size)
        return;

    // Image in input format.
    struct mp_image *ui =  rp->pack ? rp->steps[rp->num_steps - 1].buf[1]
                                    : rp->steps[0].buf[0];
    enum pl_color_system csp = ui->params.repr.sys;
    enum pl_color_levels levels = ui->params.repr.levels;
    if (rp->f32_csp_space == csp && rp->f32_csp_levels == levels)
        return;

    // The fixed point format.
    struct mp_regular_imgfmt desc = {0};
    mp_get_regular_imgfmt(&desc, rp->imgfmt_b);
    assert(desc.component_size);

    int comp_bits = desc.component_size * 8 + MPMIN(desc.component_pad, 0);
    for (int p = 0; p < desc.num_planes; p++) {
        double m, o;
        mp_get_csp_uint_mul(csp, levels, comp_bits, desc.planes[p].components[0],
                            &m, &o);
        rp->f32_m[p] = rp->pack ? 1.0 / m : m;
        rp->f32_o[p] = rp->pack ? -o      : o;
        rp->f32_pmax[p] = (1u << comp_bits) - 1;
    }

    rp->f32_csp_space = csp;
    rp->f32_csp_levels = levels;
}

void repack_line(struct mp_repack *rp, int dst_x, int dst_y,
                 int src_x, int src_y, int w)
{
    assert(rp->configured);

    struct repack_step *first = &rp->steps[0];
    struct repack_step *last = &rp->steps[rp->num_steps - 1];

    assert(dst_x >= 0 && dst_y >= 0 && src_x >= 0 && src_y >= 0 && w >= 0);
    assert(dst_x + w <= MP_ALIGN_UP(last->buf[1]->w, last->fmt[1].align_x));
    assert(src_x + w <= MP_ALIGN_UP(first->buf[0]->w, first->fmt[0].align_x));
    assert(dst_y < last->buf[1]->h);
    assert(src_y < first->buf[0]->h);
    assert(!(dst_x & (last->fmt[1].align_x - 1)));
    assert(!(src_x & (first->fmt[0].align_x - 1)));
    assert(!(w & ((1 << first->fmt[0].chroma_xs) - 1)));
    assert(!(dst_y & (last->fmt[1].align_y - 1)));
    assert(!(src_y & (first->fmt[0].align_y - 1)));

    for (int n = 0; n < rp->num_steps; n++) {
        struct repack_step *rs = &rp->steps[n];

        // When writing to temporary buffers, always write to the start (maybe
        // helps with locality).
        int sx = rs->user_buf[0] ? src_x : 0;
        int sy = rs->user_buf[0] ? src_y : 0;
        int dx = rs->user_buf[1] ? dst_x : 0;
        int dy = rs->user_buf[1] ? dst_y : 0;

        struct mp_image *buf_a = rs->buf[rp->pack];
        struct mp_image *buf_b = rs->buf[!rp->pack];
        int a_x = rp->pack ? dx : sx;
        int a_y = rp->pack ? dy : sy;
        int b_x = rp->pack ? sx : dx;
        int b_y = rp->pack ? sy : dy;

        switch (rs->type) {
        case REPACK_STEP_REPACK: {
            if (rp->repack)
                rp->repack(rp, buf_a, a_x, a_y, buf_b, b_x, b_y, w);

            for (int p = 0; p < rs->fmt[0].num_planes; p++) {
                if (rp->copy_buf[p])
                    copy_plane(rs->buf[1], dx, dy, rs->buf[0], sx, sy, w, p);
            }
            break;
        }
        case REPACK_STEP_ENDIAN:
            swap_endian(rs->buf[1], dx, dy, rs->buf[0], sx, sy, w,
                        rp->endian_size);
            break;
        case REPACK_STEP_FLOAT:
            repack_float(rp, buf_a, a_x, a_y, buf_b, b_x, b_y, w);
            break;
        }
    }
}

static bool setup_format_ne(struct mp_repack *rp)
{
    if (!rp->imgfmt_b)
        setup_nv_packer(rp);
    if (!rp->imgfmt_b)
        setup_misc_packer(rp);
    if (!rp->imgfmt_b)
        setup_packed_packer(rp);
    if (!rp->imgfmt_b)
        setup_fringe_rgb_packer(rp);
    if (!rp->imgfmt_b)
        setup_fringe_yuv_packer(rp);
    if (!rp->imgfmt_b)
        rp->imgfmt_b = rp->imgfmt_a; // maybe it was planar after all

    struct mp_regular_imgfmt desc;
    if (!mp_get_regular_imgfmt(&desc, rp->imgfmt_b))
        return false;

    // no weird stuff
    if (desc.num_planes > 4)
        return false;

    // Endian swapping.
    if (rp->imgfmt_a != rp->imgfmt_user &&
        rp->imgfmt_a == mp_find_other_endian(rp->imgfmt_user))
    {
        struct mp_imgfmt_desc desc_a = mp_imgfmt_get_desc(rp->imgfmt_a);
        struct mp_imgfmt_desc desc_u = mp_imgfmt_get_desc(rp->imgfmt_user);
        rp->endian_size = 1 << desc_u.endian_shift;
        if (!desc_a.endian_shift && rp->endian_size != 2 && rp->endian_size != 4)
            return false;
    }

    // Accept only true planar formats (with known components and no padding).
    for (int n = 0; n < desc.num_planes; n++) {
        if (desc.planes[n].num_components != 1)
            return false;
        int c = desc.planes[n].components[0];
        if (c < 1 || c > 4)
            return false;
    }

    rp->fmt_a = mp_imgfmt_get_desc(rp->imgfmt_a);
    rp->fmt_b = mp_imgfmt_get_desc(rp->imgfmt_b);

    // This is if we did a pack step.

    if (rp->flags & REPACK_CREATE_PLANAR_F32) {
        // imgfmt_b with float32 component type.
        struct mp_regular_imgfmt fdesc = desc;
        fdesc.component_type = MP_COMPONENT_TYPE_FLOAT;
        fdesc.component_size = 4;
        fdesc.component_pad = 0;
        int ffmt = mp_find_regular_imgfmt(&fdesc);
        if (!ffmt)
            return false;
        if (ffmt != rp->imgfmt_b) {
            if (desc.component_type != MP_COMPONENT_TYPE_UINT ||
                (desc.component_size != 1 && desc.component_size != 2))
                return false;
            rp->f32_comp_size = desc.component_size;
            rp->f32_csp_space = PL_COLOR_SYSTEM_COUNT;
            rp->f32_csp_levels = PL_COLOR_LEVELS_COUNT;
            rp->steps[rp->num_steps++] = (struct repack_step) {
                .type = REPACK_STEP_FLOAT,
                .fmt = {
                    mp_imgfmt_get_desc(ffmt),
                    rp->fmt_b,
                },
            };
        }
    }

    rp->steps[rp->num_steps++] = (struct repack_step) {
        .type = REPACK_STEP_REPACK,
        .fmt = { rp->fmt_b, rp->fmt_a },
    };

    if (rp->endian_size) {
        rp->steps[rp->num_steps++] = (struct repack_step) {
            .type = REPACK_STEP_ENDIAN,
            .fmt = {
                rp->fmt_a,
                mp_imgfmt_get_desc(rp->imgfmt_user),
            },
        };
    }

    // Reverse if unpack (to reflect actual data flow)
    if (!rp->pack) {
        for (int n = 0; n < rp->num_steps / 2; n++) {
            MPSWAP(struct repack_step, rp->steps[n],
                   rp->steps[rp->num_steps - 1 - n]);
        }
        for (int n = 0; n < rp->num_steps; n++) {
            struct repack_step *rs = &rp->steps[n];
            MPSWAP(struct mp_imgfmt_desc, rs->fmt[0], rs->fmt[1]);
        }
    }

    for (int n = 0; n < rp->num_steps - 1; n++)
        assert(rp->steps[n].fmt[1].id == rp->steps[n + 1].fmt[0].id);

    return true;
}

static void reset_params(struct mp_repack *rp)
{
    rp->num_steps = 0;
    rp->imgfmt_b = 0;
    rp->repack = NULL;
    rp->passthrough_y = false;
    rp->endian_size = 0;
    rp->packed_repack_scanline = NULL;
    rp->comp_size = 0;
    talloc_free(rp->comp_lut);
    rp->comp_lut = NULL;
}

static bool setup_format(struct mp_repack *rp)
{
    reset_params(rp);
    rp->imgfmt_a = rp->imgfmt_user;
    if (setup_format_ne(rp))
        return true;
    // Try reverse endian.
    reset_params(rp);
    rp->imgfmt_a = mp_find_other_endian(rp->imgfmt_user);
    return rp->imgfmt_a && setup_format_ne(rp);
}

struct mp_repack *mp_repack_create_planar(int imgfmt, bool pack, int flags)
{
    struct mp_repack *rp = talloc_zero(NULL, struct mp_repack);
    rp->imgfmt_user = imgfmt;
    rp->pack = pack;
    rp->flags = flags;

    if (!setup_format(rp)) {
        talloc_free(rp);
        return NULL;
    }

    return rp;
}

int mp_repack_get_format_src(struct mp_repack *rp)
{
    return rp->steps[0].fmt[0].id;
}

int mp_repack_get_format_dst(struct mp_repack *rp)
{
    return rp->steps[rp->num_steps - 1].fmt[1].id;
}

int mp_repack_get_align_x(struct mp_repack *rp)
{
    // We really want the LCM between those, but since only one of them is
    // packed (or they're the same format), and the chroma subsampling is the
    // same for both, only the packed one matters.
    return rp->fmt_a.align_x;
}

int mp_repack_get_align_y(struct mp_repack *rp)
{
    return rp->fmt_a.align_y; // should be the same for packed/planar formats
}

static void image_realloc(struct mp_image **img, int fmt, int w, int h)
{
    if (*img && (*img)->imgfmt == fmt && (*img)->w == w && (*img)->h == h)
        return;
    talloc_free(*img);
    *img = mp_image_alloc(fmt, w, h);
}

bool repack_config_buffers(struct mp_repack *rp,
                           int dst_flags, struct mp_image *dst,
                           int src_flags, struct mp_image *src,
                           bool *enable_passthrough)
{
    struct repack_step *rs_first = &rp->steps[0];
    struct repack_step *rs_last = &rp->steps[rp->num_steps - 1];

    rp->configured = false;

    assert(dst && src);

    int buf_w = MPMAX(dst->w, src->w);

    assert(dst->imgfmt == rs_last->fmt[1].id);
    assert(src->imgfmt == rs_first->fmt[0].id);

    // Chain/allocate buffers.

    for (int n = 0; n < rp->num_steps; n++)
        rp->steps[n].buf[0] = rp->steps[n].buf[1] = NULL;

    rs_first->buf[0] = src;
    rs_last->buf[1] = dst;

    for (int n = 0; n < rp->num_steps; n++) {
        struct repack_step *rs = &rp->steps[n];

        if (!rs->buf[0]) {
            assert(n > 0);
            rs->buf[0] = rp->steps[n - 1].buf[1];
        }

        if (rs->buf[1])
            continue;

        // Note: since repack_line() can have different src/dst offsets, we
        //       can't do true in-place in general.
        bool can_inplace = rs->type == REPACK_STEP_ENDIAN &&
                           rs->buf[0] != src && rs->buf[0] != dst;
        if (can_inplace) {
            rs->buf[1] = rs->buf[0];
            continue;
        }

        if (rs != rs_last) {
            struct repack_step *next = &rp->steps[n + 1];
            if (next->buf[0]) {
                rs->buf[1] = next->buf[0];
                continue;
            }
        }

        image_realloc(&rs->tmp, rs->fmt[1].id, buf_w, rs->fmt[1].align_y);
        if (!rs->tmp)
            return false;
        talloc_steal(rp, rs->tmp);
        rs->buf[1] = rs->tmp;
    }

    for (int n = 0; n < rp->num_steps; n++) {
        struct repack_step *rs = &rp->steps[n];
        rs->user_buf[0] = rs->buf[0] == src || rs->buf[0] == dst;
        rs->user_buf[1] = rs->buf[1] == src || rs->buf[1] == dst;
    }

    // If repacking is the only operation. It's also responsible for simply
    // copying src to dst if absolutely no filtering is done.
    bool may_passthrough =
        rp->num_steps == 1 && rp->steps[0].type == REPACK_STEP_REPACK;

    for (int p = 0; p < rp->fmt_b.num_planes; p++) {
        // (All repack callbacks copy, except nv12 does not copy luma.)
        bool repack_copies_plane = rp->repack && !(rp->passthrough_y && p == 0);

        bool can_pt = may_passthrough && !repack_copies_plane &&
                      enable_passthrough && enable_passthrough[p];

        // Copy if needed, unless the repack callback does it anyway.
        rp->copy_buf[p] = !repack_copies_plane && !can_pt;

        if (enable_passthrough)
            enable_passthrough[p] = can_pt && !rp->copy_buf[p];
    }

    if (enable_passthrough) {
        for (int n = rp->fmt_b.num_planes; n < MP_MAX_PLANES; n++)
            enable_passthrough[n] = false;
    }

    update_repack_float(rp);

    rp->configured = true;

    return true;
}
