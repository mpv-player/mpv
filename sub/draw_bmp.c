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

#include <stddef.h>
#include <stdbool.h>
#include <assert.h>
#include <math.h>
#include <inttypes.h>

#include <libswscale/swscale.h>
#include <libavutil/common.h>

#include "common/common.h"
#include "draw_bmp.h"
#include "img_convert.h"
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
    int change_id;
    int imgfmt;
    enum mp_csp colorspace;
    enum mp_csp_levels levels;
    int num_imgs;
    struct sub_cache *imgs;
};

struct mp_draw_sub_cache
{
    struct part *parts[MAX_OSD_PARTS];
    struct mp_image *upsample_img;
    struct mp_image upsample_temp;
};


static struct part *get_cache(struct mp_draw_sub_cache *cache,
                              struct sub_bitmaps *sbs, struct mp_image *format);
static bool get_sub_area(struct mp_rect bb, struct mp_image *temp,
                         struct sub_bitmap *sb, struct mp_image *out_area,
                         int *out_src_x, int *out_src_y);

#define CONDITIONAL 1

#define BLEND_CONST_ALPHA(TYPE)                                                 \
    TYPE *dst_r = dst_rp;                                                       \
    for (int x = 0; x < w; x++) {                                               \
        uint32_t srcap = srca_r[x];                                             \
        if (CONDITIONAL && !srcap) continue;                                    \
        srcap *= srcamul; /* now 0..65025 */                                    \
        dst_r[x] = (srcp * srcap + dst_r[x] * (65025 - srcap) + 32512) / 65025; \
    }

// dst = srcp * (srca * srcamul) + dst * (1 - (srca * srcamul))
static void blend_const_alpha(void *dst, int dst_stride, int srcp,
                              uint8_t *srca, int srca_stride, uint8_t srcamul,
                              int w, int h, int bytes)
{
    if (!srcamul)
        return;
    for (int y = 0; y < h; y++) {
        void *dst_rp = (uint8_t *)dst + dst_stride * y;
        uint8_t *srca_r = srca + srca_stride * y;
        if (bytes == 2) {
            BLEND_CONST_ALPHA(uint16_t)
        } else if (bytes == 1) {
            BLEND_CONST_ALPHA(uint8_t)
        }
    }
}

#define BLEND_SRC_ALPHA(TYPE)                                                   \
    TYPE *dst_r = dst_rp, *src_r = src_rp;                                      \
    for (int x = 0; x < w; x++) {                                               \
        uint32_t srcap = srca_r[x];                                             \
        if (CONDITIONAL && !srcap) continue;                                    \
        dst_r[x] = (src_r[x] * srcap + dst_r[x] * (255 - srcap) + 127) / 255;   \
    }

// dst = src * srca + dst * (1 - srca)
static void blend_src_alpha(void *dst, int dst_stride, void *src,
                            int src_stride, uint8_t *srca, int srca_stride,
                            int w, int h, int bytes)
{
    for (int y = 0; y < h; y++) {
        void *dst_rp = (uint8_t *)dst + dst_stride * y;
        void *src_rp = (uint8_t *)src + src_stride * y;
        uint8_t *srca_r = srca + srca_stride * y;
        if (bytes == 2) {
            BLEND_SRC_ALPHA(uint16_t)
        } else if (bytes == 1) {
            BLEND_SRC_ALPHA(uint8_t)
        }
    }
}

#define BLEND_SRC_DST_MUL(TYPE, MAX)                                            \
    TYPE *dst_r = dst_rp;                                                       \
    for (int x = 0; x < w; x++) {                                               \
        uint16_t srcp = src_r[x] * srcmul; /* now 0..65025 */                   \
        dst_r[x] = (srcp * (MAX) + dst_r[x] * (65025 - srcp) + 32512) / 65025;  \
    }

// dst = src * srcmul + dst * (1 - src * srcmul)
static void blend_src_dst_mul(void *dst, int dst_stride,
                              uint8_t *src, int src_stride, uint8_t srcmul,
                              int w, int h, int dst_bytes)
{
    for (int y = 0; y < h; y++) {
        void *dst_rp = (uint8_t *)dst + dst_stride * y;
        uint8_t *src_r = (uint8_t *)src + src_stride * y;
        if (dst_bytes == 2) {
            BLEND_SRC_DST_MUL(uint16_t, 65025)
        } else if (dst_bytes == 1) {
            BLEND_SRC_DST_MUL(uint8_t, 255)
        }
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
    struct mp_image *sba = mp_image_alloc(IMGFMT_Y8, sb->dw, sb->dh);
    struct mp_image *sbi = mp_image_alloc(dst_format->imgfmt, sb->dw, sb->dh);
    if (!sbisrc2 || !sba || !sbi) {
        talloc_free(sbisrc2);
        talloc_free(sba);
        talloc_free(sbi);
        return;
    }

    mp_image_swscale(sbisrc2, &sbisrc, SWS_BILINEAR);
    unpremultiply_and_split_BGR32(sbisrc2, sba);

    sbi->params.color = dst_format->params.color;
    mp_image_swscale(sbi, sbisrc2, SWS_BILINEAR);

    talloc_free(sbisrc2);

    *out_sbi = sbi;
    *out_sba = sba;
}

static void draw_rgba(struct mp_draw_sub_cache *cache, struct mp_rect bb,
                      struct mp_image *temp, int bits,
                      struct sub_bitmaps *sbs)
{
    struct part *part = get_cache(cache, sbs, temp);
    assert(part);

    for (int i = 0; i < sbs->num_parts; ++i) {
        struct sub_bitmap *sb = &sbs->parts[i];

        if (sb->w < 1 || sb->h < 1)
            continue;

        struct mp_image dst;
        int src_x, src_y;
        if (!get_sub_area(bb, temp, sb, &dst, &src_x, &src_y))
            continue;

        struct mp_image *sbi = part->imgs[i].i;
        struct mp_image *sba = part->imgs[i].a;

        if (!(sbi && sba))
            scale_sb_rgba(sb, temp, &sbi, &sba);
        // on OOM, skip drawing
        if (!(sbi && sba))
            continue;

        int bytes = (bits + 7) / 8;
        uint8_t *alpha_p = sba->planes[0] + src_y * sba->stride[0] + src_x;
        for (int p = 0; p < (temp->num_planes > 2 ? 3 : 1); p++) {
            void *src = sbi->planes[p] + src_y * sbi->stride[p] + src_x * bytes;
            blend_src_alpha(dst.planes[p], dst.stride[p], src, sbi->stride[p],
                            alpha_p, sba->stride[0], dst.w, dst.h, bytes);
        }
        if (temp->num_planes >= 4) {
            blend_src_dst_mul(dst.planes[3], dst.stride[3], alpha_p,
                              sba->stride[0], 255, dst.w, dst.h, bytes);
        }

        part->imgs[i].i = talloc_steal(part, sbi);
        part->imgs[i].a = talloc_steal(part, sba);
    }
}

static void draw_ass(struct mp_draw_sub_cache *cache, struct mp_rect bb,
                     struct mp_image *temp, int bits, struct sub_bitmaps *sbs)
{
    struct mp_csp_params cspar = MP_CSP_PARAMS_DEFAULTS;
    mp_csp_set_image_params(&cspar, &temp->params);
    cspar.levels_out = MP_CSP_LEVELS_PC; // RGB (libass.color)
    cspar.input_bits = bits;
    cspar.texture_bits = (bits + 7) / 8 * 8;

    struct mp_cmat yuv2rgb, rgb2yuv;
    bool need_conv = temp->fmt.flags & MP_IMGFLAG_YUV;
    if (need_conv) {
        mp_get_csp_matrix(&cspar, &yuv2rgb);
        mp_invert_cmat(&rgb2yuv, &yuv2rgb);
    }

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
        int color_yuv[3];
        if (need_conv) {
            int rgb[3] = {r, g, b};
            mp_map_fixp_color(&rgb2yuv, 8, rgb, cspar.texture_bits, color_yuv);
        } else {
            color_yuv[0] = g;
            color_yuv[1] = b;
            color_yuv[2] = r;
        }

        int bytes = (bits + 7) / 8;
        uint8_t *alpha_p = (uint8_t *)sb->bitmap + src_y * sb->stride + src_x;
        for (int p = 0; p < (temp->num_planes > 2 ? 3 : 1); p++) {
            blend_const_alpha(dst.planes[p], dst.stride[p], color_yuv[p],
                              alpha_p, sb->stride, a, dst.w, dst.h, bytes);
        }
        if (temp->num_planes >= 4) {
            blend_src_dst_mul(dst.planes[3], dst.stride[3], alpha_p,
                              sb->stride, a, dst.w, dst.h, bytes);
        }
    }
}

static void get_swscale_alignment(const struct mp_image *img, int *out_xstep,
                                  int *out_ystep)
{
    int sx = (1 << img->fmt.chroma_xs);
    int sy = (1 << img->fmt.chroma_ys);

    for (int p = 0; p < img->num_planes; ++p) {
        int bits = img->fmt.bpp[p];
        // the * 2 fixes problems with writing past the destination width
        while (((sx >> img->fmt.chroma_xs) * bits) % (SWS_MIN_BYTE_ALIGN * 8 * 2))
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
    if (!mp_rect_intersection(rc, &img_rect))
        return false;
    int xstep, ystep;
    get_swscale_alignment(img, &xstep, &ystep);
    align_bbox(xstep, ystep, rc);
    return mp_rect_intersection(rc, &img_rect);
}

// Try to find best/closest YUV 444 format (or similar) for imgfmt
static void get_closest_y444_format(int imgfmt, int *out_format, int *out_bits)
{
    struct mp_imgfmt_desc desc = mp_imgfmt_get_desc(imgfmt);
    int planes = desc.flags & MP_IMGFLAG_ALPHA ? 4 : 3;
    int bits = desc.component_bits > 8 ? 16 : 8;
    if (desc.flags & MP_IMGFLAG_RGB) {
        *out_format = mp_imgfmt_find(0, 0, planes, bits, MP_IMGFLAG_RGB_P);
        if (!mp_sws_supported_format(*out_format))
            *out_format = mp_imgfmt_find(0, 0, planes, 8, MP_IMGFLAG_RGB_P);
    } else if (desc.flags & MP_IMGFLAG_YUV_P) {
        *out_format = mp_imgfmt_find(0, 0, planes, bits, MP_IMGFLAG_YUV_P);
    } else {
        *out_format = 0;
    }
    if (!mp_sws_supported_format(*out_format))
        *out_format = IMGFMT_444P; // generic fallback
    *out_bits = mp_imgfmt_get_desc(*out_format).component_bits;
}

static struct part *get_cache(struct mp_draw_sub_cache *cache,
                              struct sub_bitmaps *sbs, struct mp_image *format)
{
    struct part *part = NULL;

    bool use_cache = sbs->format == SUBBITMAP_RGBA;
    if (use_cache) {
        part = cache->parts[sbs->render_index];
        if (part) {
            if (part->change_id != sbs->change_id
                || part->imgfmt != format->imgfmt
                || part->colorspace != format->params.color.space
                || part->levels != format->params.color.levels)
            {
                talloc_free(part);
                part = NULL;
            }
        }
        if (!part) {
            part = talloc(cache, struct part);
            *part = (struct part) {
                .change_id = sbs->change_id,
                .num_imgs = sbs->num_parts,
                .imgfmt = format->imgfmt,
                .levels = format->params.color.levels,
                .colorspace = format->params.color.space,
            };
            part->imgs = talloc_zero_array(part, struct sub_cache,
                                           part->num_imgs);
        }
        assert(part->num_imgs == sbs->num_parts);
        cache->parts[sbs->render_index] = part;
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
    if (!mp_rect_intersection(&dst, &(struct mp_rect){0, 0, temp->w, temp->h}))
        return false;

    *out_src_x = (dst.x0 - sb->x) + bb.x0;
    *out_src_y = (dst.y0 - sb->y) + bb.y0;
    *out_area = *temp;
    mp_image_crop_rc(out_area, dst);

    return true;
}

// Convert the src image to imgfmt (which should be a 444 format)
static struct mp_image *chroma_up(struct mp_draw_sub_cache *cache, int imgfmt,
                                  struct mp_image *src)
{
    if (src->imgfmt == imgfmt)
        return src;

    if (!cache->upsample_img || cache->upsample_img->imgfmt != imgfmt ||
        cache->upsample_img->w < src->w || cache->upsample_img->h < src->h)
    {
        talloc_free(cache->upsample_img);
        cache->upsample_img = mp_image_alloc(imgfmt, src->w, src->h);
        talloc_steal(cache, cache->upsample_img);
        if (!cache->upsample_img)
            return NULL;
    }

    cache->upsample_temp = *cache->upsample_img;
    struct mp_image *temp = &cache->upsample_temp;
    mp_image_set_size(temp, src->w, src->h);

    // The temp image is always YUV, but src not necessarily.
    // Reduce amount of conversions in YUV case (upsampling/shifting only)
    if (src->fmt.flags & MP_IMGFLAG_YUV)
        temp->params.color = src->params.color;

    if (src->imgfmt == IMGFMT_420P) {
        assert(imgfmt == IMGFMT_444P);
        // Faster upsampling: keep Y plane, upsample chroma planes only
        // The whole point is not having swscale copy the Y plane
        struct mp_image t_dst = *temp;
        mp_image_setfmt(&t_dst, IMGFMT_Y8);
        mp_image_set_size(&t_dst, temp->w, temp->h);
        struct mp_image t_src = t_dst;
        mp_image_set_size(&t_src, src->w >> 1, src->h >> 1);
        for (int c = 0; c < 2; c++) {
            t_dst.planes[0] = temp->planes[1 + c];
            t_dst.stride[0] = temp->stride[1 + c];
            t_src.planes[0] = src->planes[1 + c];
            t_src.stride[0] = src->stride[1 + c];
            mp_image_swscale(&t_dst, &t_src, SWS_POINT);
        }
        temp->planes[0] = src->planes[0];
        temp->stride[0] = src->stride[0];
    } else {
        mp_image_swscale(temp, src, SWS_POINT);
    }

    return temp;
}

// Undo chroma_up() (copy temp to old_src if needed)
static void chroma_down(struct mp_image *old_src, struct mp_image *temp)
{
    assert(old_src->w == temp->w && old_src->h == temp->h);
    if (temp != old_src) {
        if (old_src->imgfmt == IMGFMT_420P) {
            // Downsampling, skipping the Y plane (see chroma_up())
            assert(temp->imgfmt == IMGFMT_444P);
            assert(temp->planes[0] == old_src->planes[0]);
            struct mp_image t_dst = *temp;
            mp_image_setfmt(&t_dst, IMGFMT_Y8);
            mp_image_set_size(&t_dst, old_src->w >> 1, old_src->h >> 1);
            struct mp_image t_src = t_dst;
            mp_image_set_size(&t_src, temp->w, temp->h);
            for (int c = 0; c < 2; c++) {
                t_dst.planes[0] = old_src->planes[1 + c];
                t_dst.stride[0] = old_src->stride[1 + c];
                t_src.planes[0] = temp->planes[1 + c];
                t_src.stride[0] = temp->stride[1 + c];
                mp_image_swscale(&t_dst, &t_src, SWS_AREA);
            }
        } else {
            mp_image_swscale(old_src, temp, SWS_AREA); // chroma down
        }
    }
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

    struct mp_draw_sub_cache *cache_ = cache ? *cache : NULL;
    if (!cache_)
        cache_ = talloc_zero(NULL, struct mp_draw_sub_cache);

    int format, bits;
    get_closest_y444_format(dst->imgfmt, &format, &bits);

    struct mp_rect rc_list[MP_SUB_BB_LIST_MAX];
    int num_rc = mp_get_sub_bb_list(sbs, rc_list, MP_SUB_BB_LIST_MAX);

    for (int r = 0; r < num_rc; r++) {
        struct mp_rect bb = rc_list[r];

        if (!align_bbox_for_swscale(dst, &bb))
            return;

        struct mp_image dst_region = *dst;
        mp_image_crop_rc(&dst_region, bb);
        struct mp_image *temp = chroma_up(cache_, format, &dst_region);
        if (!temp)
            continue; // on OOM, skip region

        if (sbs->format == SUBBITMAP_RGBA) {
            draw_rgba(cache_, bb, temp, bits, sbs);
        } else if (sbs->format == SUBBITMAP_LIBASS) {
            draw_ass(cache_, bb, temp, bits, sbs);
        }

        chroma_down(&dst_region, temp);
    }

    if (cache) {
        *cache = cache_;
    } else {
        talloc_free(cache_);
    }
}

// vim: ts=4 sw=4 et tw=80
