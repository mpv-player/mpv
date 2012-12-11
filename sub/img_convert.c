/*
 * This file is part of mplayer.
 *
 * mplayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mplayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mplayer.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>
#include <assert.h>

#include <libavutil/mem.h>
#include <libavutil/common.h>

#include "talloc.h"

#include "img_convert.h"
#include "sub.h"
#include "video/img_format.h"
#include "video/mp_image.h"
#include "video/sws_utils.h"
#include "video/memcpy_pic.h"

struct osd_conv_cache {
    struct sub_bitmap part;
    struct sub_bitmap *parts;
};

struct osd_conv_cache *osd_conv_cache_new(void)
{
    return talloc_zero(NULL, struct osd_conv_cache);
}

static void rgba_to_premultiplied_rgba(uint32_t *colors, size_t count)
{
    for (int n = 0; n < count; n++) {
        uint32_t c = colors[n];
        int b = c & 0xFF;
        int g = (c >> 8) & 0xFF;
        int r = (c >> 16) & 0xFF;
        int a = (c >> 24) & 0xFF;
        b = b * a / 255;
        g = g * a / 255;
        r = r * a / 255;
        colors[n] = b | (g << 8) | (r << 16) | (a << 24);
    }
}

bool osd_conv_idx_to_rgba(struct osd_conv_cache *c, struct sub_bitmaps *imgs)
{
    struct sub_bitmaps src = *imgs;
    if (src.format != SUBBITMAP_INDEXED)
        return false;

    imgs->format = SUBBITMAP_RGBA;
    talloc_free(c->parts);
    imgs->parts = c->parts = talloc_array(c, struct sub_bitmap, src.num_parts);

    for (int n = 0; n < src.num_parts; n++) {
        struct sub_bitmap *d = &imgs->parts[n];
        struct sub_bitmap *s = &src.parts[n];
        struct osd_bmp_indexed sb = *(struct osd_bmp_indexed *)s->bitmap;

        rgba_to_premultiplied_rgba(sb.palette, 256);

        *d = *s;
        struct mp_image *image = alloc_mpi(s->w, s->h, IMGFMT_BGRA);
        talloc_steal(c->parts, image);
        d->stride = image->stride[0];
        d->bitmap = image->planes[0];

        for (int y = 0; y < s->h; y++) {
            uint8_t *inbmp = sb.bitmap + y * s->stride;
            uint32_t *outbmp = (uint32_t*)((uint8_t*)d->bitmap + y * d->stride);
            for (int x = 0; x < s->w; x++)
                *outbmp++ = sb.palette[*inbmp++];
        }
    }
    return true;
}

bool osd_conv_blur_rgba(struct osd_conv_cache *c, struct sub_bitmaps *imgs,
                        double gblur)
{
    struct sub_bitmaps src = *imgs;
    if (src.format != SUBBITMAP_RGBA)
        return false;

    talloc_free(c->parts);
    imgs->parts = c->parts = talloc_array(c, struct sub_bitmap, src.num_parts);

    for (int n = 0; n < src.num_parts; n++) {
        struct sub_bitmap *d = &imgs->parts[n];
        struct sub_bitmap *s = &src.parts[n];

        // add a transparent padding border to reduce artifacts
        int pad = 5;
        struct mp_image *temp = alloc_mpi(s->w + pad * 2, s->h + pad * 2,
                                          IMGFMT_BGRA);
        memset_pic(temp->planes[0], 0, temp->w * 4, temp->h, temp->stride[0]);
        uint8_t *p0 = temp->planes[0] + pad * 4 + pad * temp->stride[0];
        memcpy_pic(p0, s->bitmap, s->w * 4, s->h, temp->stride[0], s->stride);

        double sx = (double)s->dw / s->w;
        double sy = (double)s->dh / s->h;

        d->x = s->x - pad * sx;
        d->y = s->y - pad * sy;
        d->w = d->dw = s->dw + pad * 2 * sx;
        d->h = d->dh = s->dh + pad * 2 * sy;
        struct mp_image *image = alloc_mpi(d->w, d->h, IMGFMT_BGRA);
        talloc_steal(c->parts, image);
        d->stride = image->stride[0];
        d->bitmap = image->planes[0];

        mp_image_sw_blur_scale(image, temp, gblur);

        talloc_free(temp);
    }
    return true;
}

static void rgba_to_gray(uint32_t *colors, size_t count)
{
    for (int n = 0; n < count; n++) {
        uint32_t c = colors[n];
        int b = c & 0xFF;
        int g = (c >> 8) & 0xFF;
        int r = (c >> 16) & 0xFF;
        int a = (c >> 24) & 0xFF;
        r = g = b = (r + g + b) / 3;
        colors[n] = b | (g << 8) | (r << 16) | (a << 24);
    }
}

bool osd_conv_idx_to_gray(struct osd_conv_cache *c, struct sub_bitmaps *imgs)
{
    struct sub_bitmaps src = *imgs;
    if (src.format != SUBBITMAP_INDEXED)
        return false;

    talloc_free(c->parts);
    imgs->parts = c->parts = talloc_array(c, struct sub_bitmap, src.num_parts);

    for (int n = 0; n < src.num_parts; n++) {
        struct sub_bitmap *d = &imgs->parts[n];
        struct sub_bitmap *s = &src.parts[n];
        struct osd_bmp_indexed sb = *(struct osd_bmp_indexed *)s->bitmap;

        rgba_to_gray(sb.palette, 256);

        *d = *s;
        d->bitmap = talloc_memdup(c->parts, &sb, sizeof(sb));
    }
    return true;
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

bool osd_conv_ass_to_rgba(struct osd_conv_cache *c, struct sub_bitmaps *imgs)
{
    struct sub_bitmaps src = *imgs;
    if (src.format != SUBBITMAP_LIBASS)
        return false;
    assert(!src.scaled); // ASS is always unscaled

    struct sub_bitmap *bmp = &c->part;

    imgs->format = SUBBITMAP_RGBA;
    imgs->parts = bmp;
    imgs->num_parts = 0;

    struct mp_rect bb;
    if (!sub_bitmaps_bb(&src, &bb))
        return true;

    bmp->x = bb.x0;
    bmp->y = bb.y0;
    bmp->w = bmp->dw = bb.x1 - bb.x0;
    bmp->h = bmp->dh = bb.y1 - bb.y0;
    bmp->stride = bmp->w * 4;
    size_t newsize = bmp->h * bmp->stride;
    if (talloc_get_size(bmp->bitmap) < newsize) {
        talloc_free(bmp->bitmap);
        bmp->bitmap = talloc_array(c, char, newsize);
    }

    memset_pic(bmp->bitmap, 0, bmp->w * 4, bmp->h, bmp->stride);

    for (int n = 0; n < src.num_parts; n++) {
        struct sub_bitmap *s = &src.parts[n];

        draw_ass_rgba(s->bitmap, s->w, s->h, s->stride,
                      bmp->bitmap, bmp->stride,
                      s->x - bb.x0, s->y - bb.y0,
                      s->libass.color);
    }

    imgs->num_parts = 1;
    return true;
}
