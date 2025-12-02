/*
 * Copyright (C) 2006 Evgeniy Stepanov <eugeni.stepanov@gmail.com>
 *
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

#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <assert.h>

#include "config.h"

#include <ass/ass.h>
#include <ass/ass_types.h>
#if HAVE_SUBRANDR
#include <subrandr/subrandr.h>
#endif

#include "common/common.h"
#include "packer.h"
#include "img_convert.h"
#include "osd.h"
#include "video/out/bitmap_packer.h"
#include "video/mp_image.h"

struct mp_sub_packer {
    struct sub_bitmap *cached_parts; // only for the array memory
    struct sub_bitmap *cached_subrandr_images;
    struct mp_image *cached_img;
    struct sub_bitmaps cached_subs;
    bool cached_subs_valid;
    struct sub_bitmap rgba_imgs[MP_SUB_BB_LIST_MAX];
    struct bitmap_packer *packer;
};

// Free with talloc_free().
struct mp_sub_packer *mp_sub_packer_alloc(void *ta_parent)
{
    struct mp_sub_packer *p = talloc_zero(ta_parent, struct mp_sub_packer);
    p->packer = talloc_zero(p, struct bitmap_packer);
    p->packer->padding = 1; // assume bilinear sampling
    return p;
}

static bool pack(struct mp_sub_packer *p, struct sub_bitmaps *res, int imgfmt)
{
    packer_set_size(p->packer, res->num_parts);

    for (int n = 0; n < res->num_parts; n++)
        p->packer->in[n] = (struct pos){res->parts[n].w, res->parts[n].h};

    if (p->packer->count == 0 || packer_pack(p->packer) < 0)
        return false;

    struct pos bb[2];
    packer_get_bb(p->packer, bb);

    res->packed_w = bb[1].x;
    res->packed_h = bb[1].y;

    if (!p->cached_img || p->cached_img->w < res->packed_w ||
                          p->cached_img->h < res->packed_h ||
                          p->cached_img->imgfmt != imgfmt)
    {
        talloc_free(p->cached_img);
        p->cached_img = mp_image_alloc(imgfmt, p->packer->w, p->packer->h);
        if (!p->cached_img) {
            packer_reset(p->packer);
            return false;
        }
        talloc_steal(p, p->cached_img);
    }

    if (!mp_image_make_writeable(p->cached_img)) {
        packer_reset(p->packer);
        return false;
    }

    res->packed = p->cached_img;

    for (int n = 0; n < res->num_parts; n++) {
        struct sub_bitmap *b = &res->parts[n];
        struct pos pos = p->packer->result[n];

        b->src_x = pos.x;
        b->src_y = pos.y;
    }

    return true;
}

static void fill_padding_1(uint8_t *base, int w, int h, int stride, int padding)
{
    for (int row = 0; row < h; ++row) {
        uint8_t *row_ptr = base + row * stride;
        uint8_t left_pixel = row_ptr[0];
        uint8_t right_pixel = row_ptr[w - 1];

        for (int i = 1; i <= padding; ++i)
            row_ptr[-i] = left_pixel;

        for (int i = 0; i < padding; ++i)
            row_ptr[w + i] = right_pixel;
    }

    int row_bytes = (w + 2 * padding);
    uint8_t *top_row = base - padding;
    for (int i = 1; i <= padding; ++i)
        memcpy(base - i * stride - padding, top_row, row_bytes);

    uint8_t *last_row = base + (h - 1) * stride - padding;
    for (int i = 0; i < padding; ++i)
        memcpy(base + (h + i) * stride - padding, last_row, row_bytes);
}

static void fill_padding_4(uint8_t *base, int w, int h, int stride, int padding)
{
    for (int row = 0; row < h; ++row) {
        uint32_t *row_ptr = (uint32_t *)(base + row * stride);
        uint32_t left_pixel = row_ptr[0];
        uint32_t right_pixel = row_ptr[w - 1];

        for (int i = 1; i <= padding; ++i)
            row_ptr[-i] = left_pixel;

        for (int i = 0; i < padding; ++i)
            row_ptr[w + i] = right_pixel;
    }

    int row_bytes = (w + 2 * padding) * 4;
    uint8_t *top_row = base - padding * 4;
    for (int i = 1; i <= padding; ++i)
        memcpy(base - i * stride - padding * 4, top_row, row_bytes);

    uint8_t *last_row = base + (h - 1) * stride - padding * 4;
    for (int i = 0; i < padding; ++i)
        memcpy(base + (h + i) * stride - padding * 4, last_row, row_bytes);
}

static void draw_ass_rgba(unsigned char *src, int src_w, int src_h,
                          int src_stride, unsigned char *dst, size_t dst_stride,
                          int dst_x, int dst_y, uint32_t color)
{
    const unsigned int r = (color >> 24) & 0xff;
    const unsigned int g = (color >> 16) & 0xff;
    const unsigned int b = (color >>  8) & 0xff;
    const unsigned int a = 0xff - (color & 0xff);

    dst += dst_y * dst_stride + dst_x * 4;

    for (int y = 0; y < src_h; y++, dst += dst_stride, src += src_stride) {
        uint32_t *dstrow = (uint32_t *) dst;
        for (int x = 0; x < src_w; x++) {
            const unsigned int v = src[x];
            int rr = (r * a * v);
            int gg = (g * a * v);
            int bb = (b * a * v);
            int aa =      a * v;
            uint32_t dstpix = dstrow[x];
            unsigned int dstb =  dstpix        & 0xFF;
            unsigned int dstg = (dstpix >>  8) & 0xFF;
            unsigned int dstr = (dstpix >> 16) & 0xFF;
            unsigned int dsta = (dstpix >> 24) & 0xFF;
            dstb = (bb       + dstb * (255 * 255 - aa)) / (255 * 255);
            dstg = (gg       + dstg * (255 * 255 - aa)) / (255 * 255);
            dstr = (rr       + dstr * (255 * 255 - aa)) / (255 * 255);
            dsta = (aa * 255 + dsta * (255 * 255 - aa)) / (255 * 255);
            dstrow[x] = dstb | (dstg << 8) | (dstr << 16) | (dsta << 24);
        }
    }
}

static bool pack_libass(struct mp_sub_packer *p, struct sub_bitmaps *res)
{
    if (!pack(p, res, IMGFMT_Y8))
        return false;

    int padding = p->packer->padding;
    uint8_t *base = res->packed->planes[0];
    int stride = res->packed->stride[0];

    for (int n = 0; n < res->num_parts; n++) {
        struct sub_bitmap *b = &res->parts[n];
        void *pdata = base + b->src_y * stride + b->src_x;
        memcpy_pic(pdata, b->bitmap, b->w, b->h, stride, b->stride);
        fill_padding_1(pdata, b->w, b->h, stride, padding);

        b->bitmap = pdata;
        b->stride = stride;
    }

    return true;
}

static bool pack_rgba(struct mp_sub_packer *p, struct sub_bitmaps *res)
{
    struct mp_rect bb_list[MP_SUB_BB_LIST_MAX];
    int num_bb = mp_get_sub_bb_list(res, bb_list, MP_SUB_BB_LIST_MAX);

    struct sub_bitmaps imgs = {
        .change_id = res->change_id,
        .format = SUBBITMAP_BGRA,
        .parts = p->rgba_imgs,
        .num_parts = num_bb,
    };

    for (int n = 0; n < imgs.num_parts; n++) {
        imgs.parts[n].w = bb_list[n].x1 - bb_list[n].x0;
        imgs.parts[n].h = bb_list[n].y1 - bb_list[n].y0;
    }

    if (!pack(p, &imgs, IMGFMT_BGRA))
        return false;

    int padding = p->packer->padding;
    uint8_t *base = imgs.packed->planes[0];
    int stride = imgs.packed->stride[0];

    for (int n = 0; n < num_bb; n++) {
        struct mp_rect bb = bb_list[n];
        struct sub_bitmap *b = &imgs.parts[n];

        b->x = bb.x0;
        b->y = bb.y0;
        b->w = b->dw = mp_rect_w(bb);
        b->h = b->dh = mp_rect_h(bb);
        b->stride = stride;
        b->bitmap = base + b->stride * b->src_y + b->src_x * 4;
        memset_pic(b->bitmap, 0, b->w * 4, b->h, b->stride);

        for (int i = 0; i < res->num_parts; i++) {
            struct sub_bitmap *s = &res->parts[i];

            // Assume mp_get_sub_bb_list() never splits sub bitmaps
            // So we don't clip/adjust the size of the sub bitmap
            if (s->x >= b->x + b->w || s->x + s->w <= b->x ||
                s->y >= b->y + b->h || s->y + s->h <= b->y)
                continue;

            draw_ass_rgba(s->bitmap, s->w, s->h, s->stride,
                          b->bitmap, b->stride,
                          s->x - b->x, s->y - b->y,
                          s->libass.color);
        }
        fill_padding_4(b->bitmap, b->w, b->h, b->stride, padding);
    }

    *res = imgs;
    return true;
}

// Pack the contents of image_lists[0] to image_lists[num_image_lists-1] into
// a single image, and make *out point to it. *out is completely overwritten.
// If libass reported any change, image_lists_changed must be set (it then
// repacks all images). preferred_osd_format can be set to a desired
// sub_bitmap_format. Currently, only SUBBITMAP_LIBASS is supported.
void mp_sub_packer_pack_ass(struct mp_sub_packer *p, ASS_Image **image_lists,
                        int num_image_lists, bool image_lists_changed, bool video_color_space,
                        int preferred_osd_format, struct sub_bitmaps *out)
{
    int format = preferred_osd_format == SUBBITMAP_BGRA ? SUBBITMAP_BGRA
                                                        : SUBBITMAP_LIBASS;

    if (p->cached_subs_valid && !image_lists_changed &&
        p->cached_subs.format == format)
    {
        *out = p->cached_subs;
        return;
    }

    *out = (struct sub_bitmaps){.change_id = 1};
    p->cached_subs_valid = false;

    struct sub_bitmaps res = {
        .change_id = image_lists_changed,
        .format = SUBBITMAP_LIBASS,
        .parts = p->cached_parts,
        .video_color_space = video_color_space,
    };

    for (int n = 0; n < num_image_lists; n++) {
        for (struct ass_image *img = image_lists[n]; img; img = img->next) {
            if (img->w == 0 || img->h == 0)
                continue;
            MP_TARRAY_GROW(p, p->cached_parts, res.num_parts);
            res.parts = p->cached_parts;
            struct sub_bitmap *b = &res.parts[res.num_parts];
            b->bitmap = img->bitmap;
            b->stride = img->stride;
            b->libass.color = img->color;
            b->dw = b->w = img->w;
            b->dh = b->h = img->h;
            b->x = img->dst_x;
            b->y = img->dst_y;
            res.num_parts++;
        }
    }

    bool r = false;
    if (format == SUBBITMAP_BGRA) {
        r = pack_rgba(p, &res);
    } else {
        r = pack_libass(p, &res);
    }

    if (!r)
        return;

    *out = res;
    p->cached_subs = res;
    p->cached_subs.change_id = 0;
    p->cached_subs_valid = true;
}

#if HAVE_SUBRANDR
// Pack the images in `res` into a BGRA8 atlas and populate `res->parts`
// with instances of the images as described by `pass`.
static bool pack_subrandr(struct mp_sub_packer *p, struct sub_bitmaps *res,
                          struct sbr_instanced_raster_pass *pass)
{
    if (!pack(p, res, IMGFMT_BGRA))
        return false;

    int padding = p->packer->padding;
    sbr_bgra8 *base = (sbr_bgra8 *)res->packed->planes[0];
    int byte_stride = res->packed->stride[0];
    int pixel_stride = byte_stride / 4;
    struct sbr_output_instance *instances = sbr_instanced_raster_pass_get_instances(pass);

    for (int n = 0; n < res->num_parts; n++) {
        struct sub_bitmap *b = &res->parts[n];
        sbr_output_image_rasterize_into(b->subrandr.image, pass, b->src_x, b->src_y,
                                        base, res->packed_w, res->packed_h, pixel_stride);

        void *pdata = base + b->src_y * pixel_stride + b->src_x;
        fill_padding_4(pdata, b->w, b->h, byte_stride, padding);
        b->bitmap = pdata;
    }

    res->parts = NULL;
    res->num_parts = 0;
    res->format = SUBBITMAP_BGRA;
    for (struct sbr_output_instance *instance = instances; instance; instance = instance->next) {
        if (!instance->base->user_data)
            continue;

        MP_TARRAY_GROW(p, p->cached_parts, res->num_parts);
        res->parts = p->cached_parts;
        struct sub_bitmap *inst_b = &res->parts[res->num_parts];
        struct sub_bitmap *image_b = &p->cached_subrandr_images[(size_t)instance->base->user_data - 1];

        *inst_b = (struct sub_bitmap){
            .x = instance->dst_x,      .y = instance->dst_y,
            .dw = instance->dst_width, .dh = instance->dst_height,
            .w = instance->src_width,  .h = instance->src_height,
            .src_x = image_b->src_x + instance->src_off_x,
            .src_y = image_b->src_y + instance->src_off_y,
            .bitmap = image_b->bitmap, .stride = byte_stride,
        };
        res->num_parts++;
    }

    return true;
}

// Pack the images in `pass` into a single image, make `out` point to it,
// and populate `out->parts` to correctly describe all instances in `pass`.
void mp_sub_packer_pack_sbr(struct mp_sub_packer *p, sbr_instanced_raster_pass *pass,
                            struct sub_bitmaps *out)
{
    *out = (struct sub_bitmaps){.change_id = 1};
    p->cached_subs_valid = false;

    struct sub_bitmaps res = {
        .change_id = (unsigned)p->cached_subs.change_id + 1,
        .format = SUBBITMAP_SUBRANDR,
        .parts = p->cached_parts,
        .video_color_space = false,
    };

    struct sbr_output_instance *instances = sbr_instanced_raster_pass_get_instances(pass);
    for (struct sbr_output_instance *instance = instances; instance; instance = instance->next) {
        // If `user_data` is non-null then this base image was already appended.
        if (instance->base->user_data)
            continue;

        MP_TARRAY_GROW(p, p->cached_subrandr_images, res.num_parts);
        res.parts = p->cached_subrandr_images;
        struct sub_bitmap *b = &res.parts[res.num_parts];
        b->subrandr.image = instance->base;
        b->w = instance->base->width, b->h = instance->base->height;
        // Store the index of the `sub_bitmap` for this output image into `user_data`.
        // This index is incremented by one to differentiate index `0` from an absent index.
        // Will be read in `pack_subrandr` after packing to determine where each image
        // was actually packed to in the atlas.
        instance->base->user_data = (void *)(uintptr_t)(res.num_parts + 1);
        res.num_parts++;
    }

    if (!pack_subrandr(p, &res, pass))
        return;

    *out = res;
    p->cached_subs = res;
    p->cached_subs_valid = true;
}

const struct sub_bitmaps *mp_sub_packer_get_cached(struct mp_sub_packer *p) {
    if (p->cached_subs_valid)
        return &p->cached_subs;
    return NULL;
}
#endif
