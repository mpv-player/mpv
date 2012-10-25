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
 * with mpv; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stddef.h>
#include <stdbool.h>
#include <assert.h>
#include <math.h>
#include <inttypes.h>

#include <libavutil/common.h>

#include "mpcommon.h"
#include "sub/draw_bmp.h"
#include "sub/sub.h"
#include "libmpcodecs/mp_image.h"
#include "libmpcodecs/sws_utils.h"
#include "libmpcodecs/img_format.h"
#include "libvo/csputils.h"

const bool mp_draw_sub_formats[SUBBITMAP_COUNT] = {
    [SUBBITMAP_LIBASS] = true,
    [SUBBITMAP_RGBA] = true,
};

struct sub_cache {
    struct mp_image *i, *a;
};

struct part {
    int bitmap_pos_id;
    int num_imgs;
    struct sub_cache *imgs;
};

struct mp_draw_sub_cache
{
    struct part *parts[MAX_OSD_PARTS];
};

static struct part *get_cache(struct mp_draw_sub_cache **cache,
                              struct sub_bitmaps *sbs);
static bool get_sub_area(struct mp_rect bb, struct mp_image *temp,
                         struct sub_bitmap *sb, struct mp_image *out_area,
                         int *out_src_x, int *out_src_y);

#define ACCURATE
#define CONDITIONAL

static void blend_const16_alpha(void *dst, int dst_stride, uint16_t srcp,
                                uint8_t *srca, int srca_stride, uint8_t srcamul,
                                int w, int h)
{
    if (!srcamul)
        return;
    for (int y = 0; y < h; y++) {
        uint16_t *dst_r = (uint16_t *)((uint8_t *)dst + dst_stride * y);
        uint8_t *srca_r = srca + srca_stride * y;
        for (int x = 0; x < w; x++) {
            uint32_t srcap = srca_r[x];
#ifdef CONDITIONAL
            if (!srcap)
                continue;
#endif
            srcap *= srcamul; // now 0..65025
            dst_r[x] = (srcp * srcap + dst_r[x] * (65025 - srcap) + 32512) / 65025;
        }
    }
}

static void blend_const8_alpha(void *dst, int dst_stride, uint16_t srcp,
                               uint8_t *srca, int srca_stride, uint8_t srcamul,
                               int w, int h)
{
    if (!srcamul)
        return;
    for (int y = 0; y < h; y++) {
        uint8_t *dst_r = (uint8_t *)dst + dst_stride * y;
        uint8_t *srca_r = srca + srca_stride * y;
        for (int x = 0; x < w; x++) {
            uint32_t srcap = srca_r[x];
#ifdef CONDITIONAL
            if (!srcap)
                continue;
#endif
#ifdef ACCURATE
            srcap *= srcamul; // now 0..65025
            dst_r[x] = (srcp * srcap + dst_r[x] * (65025 - srcap) + 32512) / 65025;
#else
            srcap = (srcap * srcamul + 255) >> 8;
            dst_r[x] = (srcp * srcap + dst_r[x] * (255 - srcap) + 255) >> 8;
#endif
        }
    }
}

static void blend_const_alpha(void *dst, int dst_stride, int srcp,
                              uint8_t *srca, int srca_stride, uint8_t srcamul,
                              int w, int h, int bytes)
{
    if (bytes == 2) {
        blend_const16_alpha(dst, dst_stride, srcp, srca, srca_stride, srcamul,
                            w, h);
    } else if (bytes == 1) {
        blend_const8_alpha(dst, dst_stride, srcp, srca, srca_stride, srcamul,
                           w, h);
    }
}

static void blend_src16_alpha(void *dst, int dst_stride, void *src,
                              int src_stride, uint8_t *srca, int srca_stride,
                              int w, int h)
{
    for (int y = 0; y < h; y++) {
        uint16_t *dst_r = (uint16_t *)((uint8_t *)dst + dst_stride * y);
        uint16_t *src_r = (uint16_t *)((uint8_t *)src + src_stride * y);
        uint8_t *srca_r = srca + srca_stride * y;
        for (int x = 0; x < w; x++) {
            uint32_t srcap = srca_r[x];
#ifdef CONDITIONAL
            if (!srcap)
                continue;
#endif
            dst_r[x] = (src_r[x] * srcap + dst_r[x] * (255 - srcap) + 127) / 255;
        }
    }
}

static void blend_src8_alpha(void *dst, int dst_stride, void *src,
                             int src_stride, uint8_t *srca, int srca_stride,
                             int w, int h)
{
    for (int y = 0; y < h; y++) {
        uint8_t *dst_r = (uint8_t *)dst + dst_stride * y;
        uint8_t *src_r = (uint8_t *)src + src_stride * y;
        uint8_t *srca_r = srca + srca_stride * y;
        for (int x = 0; x < w; x++) {
            uint16_t srcap = srca_r[x];
#ifdef CONDITIONAL
            if (!srcap)
                continue;
#endif
#ifdef ACCURATE
            dst_r[x] = (src_r[x] * srcap + dst_r[x] * (255 - srcap) + 127) / 255;
#else
            dst_r[x] = (src_r[x] * srcap + dst_r[x] * (255 - srcap) + 255) >> 8;
#endif
        }
    }
}

static void blend_src_alpha(void *dst, int dst_stride, void *src,
                            int src_stride, uint8_t *srca, int srca_stride,
                            int w, int h, int bytes)
{
    if (bytes == 2) {
        blend_src16_alpha(dst, dst_stride, src, src_stride, srca, srca_stride,
                          w, h);
    } else if (bytes == 1) {
        blend_src8_alpha(dst, dst_stride, src, src_stride, srca, srca_stride,
                         w, h);
    }
}

static void unpremultiply_and_split_BGR32(struct mp_image *img,
                                          struct mp_image *alpha)
{
    for (int y = 0; y < img->h; ++y) {
        uint32_t *irow = (uint32_t *) &img->planes[0][img->stride[0] * y];
        uint8_t *arow = &alpha->planes[0][alpha->stride[0] * y];
        for (int x = 0; x < img->w; ++x) {
            uint32_t pval = irow[x];
            uint8_t aval = (pval >> 24);
            uint8_t rval = (pval >> 16) & 0xFF;
            uint8_t gval = (pval >> 8) & 0xFF;
            uint8_t bval = pval & 0xFF;
            // multiplied = separate * alpha / 255
            // separate = rint(multiplied * 255 / alpha)
            //          = floor(multiplied * 255 / alpha + 0.5)
            //          = floor((multiplied * 255 + 0.5 * alpha) / alpha)
            //          = floor((multiplied * 255 + floor(0.5 * alpha)) / alpha)
            int div = (int) aval;
            int add = div / 2;
            if (aval) {
                rval = FFMIN(255, (rval * 255 + add) / div);
                gval = FFMIN(255, (gval * 255 + add) / div);
                bval = FFMIN(255, (bval * 255 + add) / div);
                irow[x] = bval + (gval << 8) + (rval << 16) + (aval << 24);
            }
            arow[x] = aval;
        }
    }
}

static void scale_sb_rgba(struct sub_bitmap *sb, struct mp_csp_details *csp,
                          int imgfmt, struct mp_image **out_sbi,
                          struct mp_image **out_sba)
{
    struct mp_image *sbisrc = new_mp_image(sb->w, sb->h);
    mp_image_setfmt(sbisrc, IMGFMT_BGR32);
    sbisrc->planes[0] = sb->bitmap;
    sbisrc->stride[0] = sb->stride;
    struct mp_image *sbisrc2 = alloc_mpi(sb->dw, sb->dh, IMGFMT_BGR32);
    mp_image_swscale(sbisrc2, sbisrc, csp, SWS_BILINEAR);

    struct mp_image *sba = alloc_mpi(sb->dw, sb->dh, IMGFMT_Y8);
    unpremultiply_and_split_BGR32(sbisrc2, sba);

    struct mp_image *sbi = alloc_mpi(sb->dw, sb->dh, imgfmt);
    mp_image_swscale(sbi, sbisrc2, csp, SWS_BILINEAR);

    free_mp_image(sbisrc);
    free_mp_image(sbisrc2);

    *out_sbi = sbi;
    *out_sba = sba;
}

static void draw_rgba(struct mp_draw_sub_cache **cache, struct mp_rect bb,
                      struct mp_image *temp, int bits, struct mp_csp_details *csp,
                      struct sub_bitmaps *sbs)
{
    struct part *part = get_cache(cache, sbs);

    for (int i = 0; i < sbs->num_parts; ++i) {
        struct sub_bitmap *sb = &sbs->parts[i];

        // libswscale madness: it requires a minimum width
        // skip it, we can't reasonably handle it
        if (sb->w < 8)
            continue;

        struct mp_image dst;
        int src_x, src_y;
        if (!get_sub_area(bb, temp, sb, &dst, &src_x, &src_y))
            continue;

        struct mp_image *sbi = NULL;
        struct mp_image *sba = NULL;
        if (part) {
            sbi = part->imgs[i].i;
            sba = part->imgs[i].a;
        }

        if (!(sbi && sba))
            scale_sb_rgba(sb, csp, temp->imgfmt, &sbi, &sba);

        int bytes = (bits + 7) / 8;
        uint8_t *alpha_p = sba->planes[0] + src_y * sba->stride[0] + src_x;
        for (int p = 0; p < 3; p++) {
            void *src = sbi->planes[p] + src_y * sbi->stride[p] + src_x * bytes;
            blend_src_alpha(dst.planes[p], dst.stride[p], src, sbi->stride[p],
                            alpha_p, sba->stride[0], dst.w, dst.h, bytes);
        }

        if (part) {
            part->imgs[i].i = talloc_steal(part, sbi);
            part->imgs[i].a = talloc_steal(part, sba);
        } else {
            free_mp_image(sbi);
            free_mp_image(sba);
        }
    }
}

static void draw_ass(struct mp_draw_sub_cache **cache, struct mp_rect bb,
                     struct mp_image *temp, int bits, struct mp_csp_details *csp,
                     struct sub_bitmaps *sbs)
{
    struct mp_csp_params cspar = MP_CSP_PARAMS_DEFAULTS;
    cspar.colorspace = *csp;

    float yuv2rgb[3][4], rgb2yuv[3][4];
    mp_get_yuv2rgb_coeffs(&cspar, yuv2rgb);
    mp_invert_yuv2rgb(rgb2yuv, yuv2rgb);

    for (int i = 0; i < sbs->num_parts; ++i) {
        struct sub_bitmap *sb = &sbs->parts[i];

        struct mp_image dst;
        int src_x, src_y;
        if (!get_sub_area(bb, temp, sb, &dst, &src_x, &src_y))
            continue;

        int r = (sb->libass.color >> 24) & 0xFF;
        int g = (sb->libass.color >> 16) & 0xFF;
        int b = (sb->libass.color >> 8) & 0xFF;
        int a = 255 - (sb->libass.color & 0xFF);
        int color_yuv[3] = {r, g, b};
        mp_map_color(rgb2yuv, bits, color_yuv);

        int bytes = (bits + 7) / 8;
        uint8_t *alpha_p = (uint8_t *)sb->bitmap + src_y * sb->stride + src_x;
        for (int p = 0; p < 3; p++) {
            blend_const_alpha(dst.planes[p], dst.stride[p], color_yuv[p],
                              alpha_p, sb->stride, a, dst.w, dst.h, bytes);
        }
    }
}

static void mp_image_crop(struct mp_image *img, struct mp_rect rc)
{
    for (int p = 0; p < img->num_planes; ++p) {
        int bits = MP_IMAGE_BITS_PER_PIXEL_ON_PLANE(img, p);
        img->planes[p] +=
            (rc.y0 >> (p ? img->chroma_y_shift : 0)) * img->stride[p] +
            (rc.x0 >> (p ? img->chroma_x_shift : 0)) * bits / 8;
    }
    img->w = rc.x1 - rc.x0;
    img->h = rc.y1 - rc.y0;
    img->chroma_width = img->w >> img->chroma_x_shift;
    img->chroma_height = img->h >> img->chroma_y_shift;
    img->display_w = img->display_h = 0;
}

static bool clip_to_bb(struct mp_rect bb, struct mp_rect *rc)
{
    rc->x0 = FFMAX(bb.x0, rc->x0);
    rc->y0 = FFMAX(bb.y0, rc->y0);
    rc->x1 = FFMIN(bb.x1, rc->x1);
    rc->y1 = FFMIN(bb.y1, rc->y1);

    return rc->x1 > rc->x0 && rc->y1 > rc->y0;
}

static void get_swscale_alignment(const struct mp_image *img, int *out_xstep,
                                  int *out_ystep)
{
    int sx = (1 << img->chroma_x_shift);
    int sy = (1 << img->chroma_y_shift);

    // Hack for IMGFMT_Y8
    if (img->chroma_x_shift == 31 && img->chroma_y_shift == 31) {
        sx = 1;
        sy = 1;
    }

    for (int p = 0; p < img->num_planes; ++p) {
        int bits = MP_IMAGE_BITS_PER_PIXEL_ON_PLANE(img, p);
        // the * 2 fixes problems with writing past the destination width
        while (((sx >> img->chroma_x_shift) * bits) % (SWS_MIN_BYTE_ALIGN * 8 * 2))
            sx *= 2;
    }

    *out_xstep = sx;
    *out_ystep = sy;
}

static void align_bbox(int xstep, int ystep, struct mp_rect *rc)
{
    rc->x0 = rc->x0 & ~(xstep - 1);
    rc->y0 = rc->y0 & ~(ystep - 1);
    rc->x1 = FFALIGN(rc->x1, xstep);
    rc->y1 = FFALIGN(rc->y1, ystep);
}

static bool align_bbox_for_swscale(struct mp_image *img, struct mp_rect *rc)
{
    struct mp_rect img_rect = {0, 0, img->w, img->h};
    // Get rid of negative coordinates
    if (!clip_to_bb(img_rect, rc))
        return false;
    int xstep, ystep;
    get_swscale_alignment(img, &xstep, &ystep);
    align_bbox(xstep, ystep, rc);
    return clip_to_bb(img_rect, rc);
}

// Try to find best/closest YUV 444 format for imgfmt
static void get_closest_y444_format(int imgfmt, int *out_format, int *out_bits)
{
#ifdef ACCURATE
    struct mp_image tmp = {0};
    mp_image_setfmt(&tmp, imgfmt);
    if (tmp.flags & MP_IMGFLAG_YUV) {
        int bits;
        if (mp_get_chroma_shift(imgfmt, NULL, NULL, &bits)) {
            switch (bits) {
                case 8:
                    *out_format = IMGFMT_444P;
                    *out_bits = 8;
                    return;
                case 9:
                    *out_format = IMGFMT_444P9;
                    *out_bits = 9;
                    return;
                case 10:
                    *out_format = IMGFMT_444P10;
                    *out_bits = 10;
                    return;
            }
        }
    }
    *out_format = IMGFMT_444P16;
    *out_bits = 16;
#else
    *out_format = IMGFMT_444P;
    *out_bits = 8;
#endif
}

static struct part *get_cache(struct mp_draw_sub_cache **cache,
                              struct sub_bitmaps *sbs)
{
    if (cache && !*cache)
        *cache = talloc_zero(NULL, struct mp_draw_sub_cache);

    struct part *part = NULL;

    bool use_cache = sbs->format == SUBBITMAP_RGBA;
    if (cache && use_cache) {
        part = (*cache)->parts[sbs->render_index];
        if (part && part->bitmap_pos_id != sbs->bitmap_pos_id) {
            talloc_free(part);
            part = NULL;
        }
        if (!part) {
            part = talloc_zero(*cache, struct part);
            part->bitmap_pos_id = sbs->bitmap_pos_id;
            part->num_imgs = sbs->num_parts;
            part->imgs = talloc_zero_array(part, struct sub_cache,
                                           part->num_imgs);
        }
        assert(part->num_imgs == sbs->num_parts);
        (*cache)->parts[sbs->render_index] = part;
    }

    return part;
}

// Return area of intersection between target and sub-bitmap as cropped image
static bool get_sub_area(struct mp_rect bb, struct mp_image *temp,
                         struct sub_bitmap *sb, struct mp_image *out_area,
                         int *out_src_x, int *out_src_y)
{
    // coordinates are relative to the bbox
    struct mp_rect dst = {sb->x - bb.x0, sb->y - bb.y0};
    dst.x1 = dst.x0 + sb->dw;
    dst.y1 = dst.y0 + sb->dh;
    if (!clip_to_bb((struct mp_rect){0, 0, temp->w, temp->h}, &dst))
        return false;

    *out_src_x = (dst.x0 - sb->x) + bb.x0;
    *out_src_y = (dst.y0 - sb->y) + bb.y0;
    *out_area = *temp;
    mp_image_crop(out_area, dst);

    return true;
}

// cache: if not NULL, the function will set *cache to a talloc-allocated cache
//        containing scaled versions of sbs contents - free the cache with
//        talloc_free()
void mp_draw_sub_bitmaps(struct mp_draw_sub_cache **cache, struct mp_image *dst,
                         struct sub_bitmaps *sbs, struct mp_csp_details *csp)
{
    assert(mp_draw_sub_formats[sbs->format]);
    if (!mp_sws_supported_format(dst->imgfmt))
        return;

    int format, bits;
    get_closest_y444_format(dst->imgfmt, &format, &bits);

    struct mp_rect bb;
    if (!sub_bitmaps_bb(sbs, &bb))
        return;

    if (!align_bbox_for_swscale(dst, &bb))
        return;

    struct mp_image *temp;
    struct mp_image dst_region = *dst;
    mp_image_crop(&dst_region, bb);
    if (dst->imgfmt == format) {
        temp = &dst_region;
    } else {
        temp = alloc_mpi(bb.x1 - bb.x0, bb.y1 - bb.y0, format);
        mp_image_swscale(temp, &dst_region, csp, SWS_POINT); // chroma up
    }

    if (sbs->format == SUBBITMAP_RGBA) {
        draw_rgba(cache, bb, temp, bits, csp, sbs);
    } else if (sbs->format == SUBBITMAP_LIBASS) {
        draw_ass(cache, bb, temp, bits, csp, sbs);
    }

    if (temp != &dst_region) {
        mp_image_swscale(&dst_region, temp, csp, SWS_AREA); // chroma down
        free_mp_image(temp);
    }
}

// vim: ts=4 sw=4 et tw=80
