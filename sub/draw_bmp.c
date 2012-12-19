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

#include "core/mp_common.h"
#include "sub/draw_bmp.h"
#include "sub/sub.h"
#include "video/mp_image.h"
#include "video/sws_utils.h"
#include "video/img_format.h"
#include "video/csputils.h"

const bool mp_draw_sub_formats[SUBBITMAP_COUNT] = {
    [SUBBITMAP_LIBASS] = true,
    [SUBBITMAP_RGBA] = true,
};

struct sub_cache {
    struct mp_image *i, *a;
};

struct part {
    int bitmap_pos_id;
    int imgfmt;
    enum mp_csp colorspace;
    enum mp_csp_levels levels;
    int num_imgs;
    struct sub_cache *imgs;
};

struct mp_draw_sub_cache
{
    struct part *parts[MAX_OSD_PARTS];
};

static struct part *get_cache(struct mp_draw_sub_cache **cache,
                              struct sub_bitmaps *sbs, struct mp_image *format);
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

// dst_format merely contains the target colorspace/format information
static void scale_sb_rgba(struct sub_bitmap *sb, struct mp_image *dst_format,
                          struct mp_image **out_sbi, struct mp_image **out_sba)
{
    struct mp_image sbisrc = {0};
    mp_image_setfmt(&sbisrc, IMGFMT_BGR32);
    mp_image_set_size(&sbisrc, sb->w, sb->h);
    sbisrc.planes[0] = sb->bitmap;
    sbisrc.stride[0] = sb->stride;
    struct mp_image *sbisrc2 = mp_image_alloc(IMGFMT_BGR32, sb->dw, sb->dh);
    mp_image_swscale(sbisrc2, &sbisrc, SWS_BILINEAR);

    struct mp_image *sba = mp_image_alloc(IMGFMT_Y8, sb->dw, sb->dh);
    unpremultiply_and_split_BGR32(sbisrc2, sba);

    struct mp_image *sbi = mp_image_alloc(dst_format->imgfmt, sb->dw, sb->dh);
    sbi->colorspace = dst_format->colorspace;
    sbi->levels = dst_format->levels;
    mp_image_swscale(sbi, sbisrc2, SWS_BILINEAR);

    talloc_free(sbisrc2);

    *out_sbi = sbi;
    *out_sba = sba;
}

static void draw_rgba(struct mp_draw_sub_cache **cache, struct mp_rect bb,
                      struct mp_image *temp, int bits,
                      struct sub_bitmaps *sbs)
{
    struct part *part = get_cache(cache, sbs, temp);

    for (int i = 0; i < sbs->num_parts; ++i) {
        struct sub_bitmap *sb = &sbs->parts[i];

        if (sb->w < 1 || sb->h < 1)
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
            scale_sb_rgba(sb, temp, &sbi, &sba);

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
                     struct mp_image *temp, int bits, struct sub_bitmaps *sbs)
{
    struct mp_csp_params cspar = MP_CSP_PARAMS_DEFAULTS;
    cspar.colorspace.format = temp->colorspace;
    cspar.colorspace.levels_in = temp->levels;
    cspar.colorspace.levels_out = MP_CSP_LEVELS_PC; // RGB (libass.color)
    cspar.int_bits_in = bits;
    cspar.int_bits_out = 8;

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
        if (dst.flags & MP_IMGFLAG_YUV) {
            mp_map_int_color(rgb2yuv, bits, color_yuv);
        } else {
            assert(dst.imgfmt == IMGFMT_GBRP);
            color_yuv[0] = g;
            color_yuv[1] = b;
            color_yuv[2] = r;
        }

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
        img->planes[p] +=
            (rc.y0 >> img->fmt.ys[p]) * img->stride[p] +
            (rc.x0 >> img->fmt.xs[p]) * img->fmt.bpp[p] / 8;
    }
    mp_image_set_size(img, rc.x1 - rc.x0, rc.y1 - rc.y0);
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
        int bits = img->fmt.bpp[p];
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

// Post condition, if true returned: rc is inside img
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
                case 12:
                    *out_format = IMGFMT_444P12;
                    *out_bits = 12;
                    return;
                case 14:
                    *out_format = IMGFMT_444P14;
                    *out_bits = 14;
                    return;
            }
        }
    } else {
        *out_format = IMGFMT_GBRP;
        *out_bits = 8;
        return;
    }
    *out_format = IMGFMT_444P16;
    *out_bits = 16;
#else
    *out_format = IMGFMT_444P;
    *out_bits = 8;
#endif
}

static struct part *get_cache(struct mp_draw_sub_cache **cache,
                              struct sub_bitmaps *sbs, struct mp_image *format)
{
    if (cache && !*cache)
        *cache = talloc_zero(NULL, struct mp_draw_sub_cache);

    struct part *part = NULL;

    bool use_cache = sbs->format == SUBBITMAP_RGBA;
    if (cache && use_cache) {
        part = (*cache)->parts[sbs->render_index];
        if (part) {
            if (part->bitmap_pos_id != sbs->bitmap_pos_id
                || part->imgfmt != format->imgfmt
                || part->colorspace != format->colorspace
                || part->levels != format->levels)
            {
                talloc_free(part);
                part = NULL;
            }
        }
        if (!part) {
            part = talloc(*cache, struct part);
            *part = (struct part) {
                .bitmap_pos_id = sbs->bitmap_pos_id,
                .num_imgs = sbs->num_parts,
                .imgfmt = format->imgfmt,
                .levels = format->levels,
                .colorspace = format->colorspace,
            };
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
                         struct sub_bitmaps *sbs)
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
        // temp is always YUV, dst_region not
        // reduce amount of conversions in YUV case (upsampling/shifting only)
        if (dst_region.flags & MP_IMGFLAG_YUV) {
            temp->colorspace = dst_region.colorspace;
            temp->levels = dst_region.levels;
        }
        mp_image_swscale(temp, &dst_region, SWS_POINT); // chroma up
    }

    if (sbs->format == SUBBITMAP_RGBA) {
        draw_rgba(cache, bb, temp, bits, sbs);
    } else if (sbs->format == SUBBITMAP_LIBASS) {
        draw_ass(cache, bb, temp, bits, sbs);
    }

    if (temp != &dst_region) {
        mp_image_swscale(&dst_region, temp, SWS_AREA); // chroma down
        free_mp_image(temp);
    }
}

struct mp_draw_sub_backup
{
    bool valid;
    struct mp_image *image;                     // backed up image parts
    struct line_ext *lines[MP_MAX_PLANES];      // backup range for each line
};

struct line_ext {
    int x0, x1; // x1 is exclusive
};

struct mp_draw_sub_backup *mp_draw_sub_backup_new(void)
{
    return talloc_zero(NULL, struct mp_draw_sub_backup);
}

// Signal that the full image is valid (nothing to backup).
void mp_draw_sub_backup_reset(struct mp_draw_sub_backup *backup)
{
    backup->valid = true;
    if (backup->image) {
        for (int p = 0; p < MP_MAX_PLANES; p++) {
            int h = backup->image->h;
            for (int y = 0; y < h; y++) {
                struct line_ext *ext = &backup->lines[p][y];
                ext->x0 = ext->x1 = -1;
            }
        }
    }
}

static void backup_realloc(struct mp_draw_sub_backup *backup,
                           struct mp_image *img)
{
    if (backup->image && backup->image->imgfmt == img->imgfmt
        && backup->image->w == img->w && backup->image->h == img->h)
        return;

    talloc_free_children(backup);
    backup->image = alloc_mpi(img->w, img->h, img->imgfmt);
    talloc_steal(backup, backup->image);
    for (int p = 0; p < MP_MAX_PLANES; p++) {
        backup->lines[p] = talloc_array(backup, struct line_ext,
                                        backup->image->h);
    }
    mp_draw_sub_backup_reset(backup);
}

static void copy_line(struct mp_image *dst, struct mp_image *src,
                      int p, int plane_y, int x0, int x1)
{
    int bits = dst->fmt.bpp[p];
    int xs = p ? dst->chroma_x_shift : 0;
    memcpy(dst->planes[p] + plane_y * dst->stride[p] + (x0 >> xs) * bits / 8,
           src->planes[p] + plane_y * src->stride[p] + (x0 >> xs) * bits / 8,
           ((x1 - x0) >> xs) * bits / 8);
}

static void backup_rect(struct mp_draw_sub_backup *backup, struct mp_image *img,
                        int plane, struct mp_rect rc)
{
    if (!align_bbox_for_swscale(img, &rc))
        return;
    int ys = plane ? img->chroma_y_shift : 0;
    int yp = ys ? ((1 << ys) - 1) : 0;
    for (int y = (rc.y0 >> ys); y < ((rc.y1 + yp) >> ys); y++) {
        struct line_ext *ext = &backup->lines[plane][y];
        if (ext->x0 == -1) {
            copy_line(backup->image, img, plane, y, rc.x0, rc.x1);
            ext->x0 = rc.x0;
            ext->x1 = rc.x1;
        } else {
            if (rc.x0 < ext->x0) {
                copy_line(backup->image, img, plane, y, rc.x0, ext->x0);
                ext->x0 = rc.x0;
            }
            if (ext->x1 < rc.x1) {
                copy_line(backup->image, img, plane, y, ext->x1, rc.x1);
                ext->x1 = rc.x1;
            }
        }
    }
}

void mp_draw_sub_backup_add(struct mp_draw_sub_backup *backup,
                            struct mp_image *img, struct sub_bitmaps *sbs)
{
    backup_realloc(backup, img);

    for (int p = 0; p < img->num_planes; p++) {
        for (int i = 0; i < sbs->num_parts; ++i) {
            struct sub_bitmap *sb = &sbs->parts[i];
            struct mp_rect rc = {sb->x, sb->y, sb->x + sb->dw, sb->y + sb->dh};
            backup_rect(backup, img, p, rc);
        }
    }
}

bool mp_draw_sub_backup_restore(struct mp_draw_sub_backup *backup,
                                struct mp_image *buffer)
{
    if (!backup->image || backup->image->imgfmt != buffer->imgfmt
        || backup->image->w != buffer->w || backup->image->h != buffer->h
        || !backup->valid)
    {
        backup->valid = false;
        return false;
    }
    struct mp_image *img = backup->image;
    for (int p = 0; p < img->num_planes; p++) {
        int ys = p ? img->chroma_y_shift : 0;
        int yp = ys ? ((1 << ys) - 1) : 0;
        int p_h = ((img->h + yp) >> ys);
        for (int y = 0; y < p_h; y++) {
            struct line_ext *ext = &backup->lines[p][y];
            if (ext->x0 < ext->x1) {
                copy_line(buffer, img, p, y, ext->x0, ext->x1);
            }
        }
    }
    return true;
}

// vim: ts=4 sw=4 et tw=80
