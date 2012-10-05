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
#include "spudec.h"

struct osd_conv_cache {
    struct sub_bitmap part;
    struct sub_bitmap *parts;
    // for osd_conv_cache_alloc_old_p() (SUBBITMAP_PLANAR)
    int allocated, stride;
    struct old_osd_planar bmp;
    // for osd_conv_idx_to_old_p(), a spudec.c handle
    void *spudec;
};

static int osd_conv_cache_destroy(void *p)
{
    struct osd_conv_cache *c = p;
    av_free(c->bmp.bitmap);
    av_free(c->bmp.alpha);
    if (c->spudec)
        spudec_free(c->spudec);
    return 0;
}

struct osd_conv_cache *osd_conv_cache_new(void)
{
    struct osd_conv_cache *c = talloc_zero(NULL, struct osd_conv_cache);
    talloc_set_destructor(c, &osd_conv_cache_destroy);
    return c;
}

// allocates/enlarges the alpha/bitmap buffer
static void osd_conv_cache_alloc_old_p(struct osd_conv_cache *c, int w, int h)
{
    assert(w > 0 && h > 0);
    c->stride = (w + 7) & (~7);
    int len = c->stride * h;
    if (c->allocated < len) {
        av_free(c->bmp.bitmap);
        av_free(c->bmp.alpha);
        c->allocated = len;
        c->bmp.bitmap = av_malloc(len);
        c->bmp.alpha  = av_malloc(len);
    }
    memset(c->bmp.bitmap, sub_bg_color, len);
    memset(c->bmp.alpha, sub_bg_alpha, len);
    c->part = (struct sub_bitmap) {
        .bitmap = &c->bmp,
        .stride = c->stride,
        .w = w, .h = h,
        .dw = w, .dh = h,
    };
}

static void draw_alpha_ass_to_old(unsigned char *src, int src_w, int src_h,
                                  int src_stride, unsigned char *dst_a,
                                  unsigned char *dst_i, size_t dst_stride,
                                  int dst_x, int dst_y, uint32_t color)
{
    const unsigned int r = (color >> 24) & 0xff;
    const unsigned int g = (color >> 16) & 0xff;
    const unsigned int b = (color >>  8) & 0xff;
    const unsigned int a = 0xff - (color & 0xff);

    int gray = (r + g + b) / 3; // not correct

    dst_a += dst_y * dst_stride + dst_x;
    dst_i += dst_y * dst_stride + dst_x;

    int src_skip = src_stride - src_w;
    int dst_skip = dst_stride - src_w;

    for (int y = 0; y < src_h; y++) {
        for (int x = 0; x < src_w; x++) {
            unsigned char as = (*src * a) >> 8;
            unsigned char bs = (gray * as) >> 8;
            // to mplayer scale
            as = -as;

            unsigned char *a = dst_a;
            unsigned char *b = dst_i;

            // NOTE: many special cases, because alpha=0 means transparency,
            //       while alpha=1..255 is opaque..transparent
            if (as) {
                *b = ((*b * as) >> 8) + bs;
                if (*a) {
                    *a = (*a * as) >> 8;
                    if (*a < 1)
                        *a = 1;
                } else {
                    *a = as;
                }
            }

            dst_a++;
            dst_i++;
            src++;
        }
        dst_a += dst_skip;
        dst_i += dst_skip;
        src += src_skip;
    }
}

static void render_ass_to_old(unsigned char *a, unsigned char *i,
                              size_t stride, int x, int y,
                              struct sub_bitmaps *imgs)
{
    for (int n = 0; n < imgs->num_parts; n++) {
        struct sub_bitmap *p = &imgs->parts[n];
        draw_alpha_ass_to_old(p->bitmap, p->w, p->h, p->stride, a, i, stride,
                              x + p->x, y + p->y, p->libass.color);
    }
}

// SUBBITMAP_LIBASS -> SUBBITMAP_OLD_PLANAR
bool osd_conv_ass_to_old_p(struct osd_conv_cache *c, struct sub_bitmaps *imgs)
{
    struct sub_bitmaps src = *imgs;
    if (src.format != SUBBITMAP_LIBASS || src.scaled)
        return false;

    imgs->format = SUBBITMAP_OLD_PLANAR;
    imgs->num_parts = 0;
    imgs->parts = NULL;

    int x1, y1, x2, y2;
    if (!sub_bitmaps_bb(&src, &x1, &y1, &x2, &y2))
        return true;

    osd_conv_cache_alloc_old_p(c, x2 - x1, y2 - y1);

    render_ass_to_old(c->bmp.alpha, c->bmp.bitmap, c->stride, -x1, -y1, &src);

    c->part.x = x1;
    c->part.y = y1;

    imgs->parts = &c->part;
    imgs->num_parts = 1;
    return true;
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
        d->stride = s->w * 4;
        d->bitmap = talloc_size(c->parts, s->h * d->stride);

        uint32_t *outbmp = d->bitmap;
        for (int y = 0; y < s->h; y++) {
            uint8_t *inbmp = sb.bitmap + y * s->stride;
            for (int x = 0; x < s->w; x++)
                *outbmp++ = sb.palette[*inbmp++];
        }
    }
    return true;
}

bool osd_conv_idx_to_old_p(struct osd_conv_cache *c, struct sub_bitmaps *imgs,
                           int screen_w, int screen_h)
{
    struct sub_bitmaps src = *imgs;
    if (src.format != SUBBITMAP_INDEXED)
        return false;

    imgs->format = SUBBITMAP_OLD_PLANAR;
    imgs->num_parts = 0;
    imgs->parts = NULL;
    if (src.num_parts == 0)
        return true;

    // assume they are all evenly scaled (and size 0 is not possible)
    // could put more effort into it to reduce rounding errors, but it doesn't
    // make much sense anyway
    struct sub_bitmap *s0 = &src.parts[0];
    double scale_x = (double)s0->w / s0->dw;
    double scale_y = (double)s0->h / s0->dh;
    double scale = FFMIN(scale_x, scale_y);

    int xmin, ymin, xmax, ymax;

    xmin = ymin = INT_MAX;
    xmax = ymax = INT_MIN;
    for (int n = 0; n < src.num_parts; n++) {
        struct sub_bitmap *s = &src.parts[n];
        int sx = s->x * scale;
        int sy = s->y * scale;
        xmin = FFMIN(xmin, sx);
        ymin = FFMIN(ymin, sy);
        xmax = FFMAX(xmax, sx + s->w);
        ymax = FFMAX(ymax, sy + s->h);
    }

    int w = xmax - xmin;
    int h = ymax - ymin;

    struct spu_packet_t *packet = spudec_packet_create(xmin, ymin, w, h);
    if (!packet)
        return false;
    spudec_packet_clear(packet);
    for (int n = 0; n < src.num_parts; n++) {
        struct sub_bitmap *s = &src.parts[n];
        struct osd_bmp_indexed *sb = s->bitmap;
        int sx = s->x * scale;
        int sy = s->y * scale;
        assert(sx >= xmin);
        assert(sy >= ymin);
        assert(sx - xmin + s->w <= w);
        assert(sy - ymin + s->h <= h);
        // assumes sub-images are not overlapping
        spudec_packet_fill(packet, sb->bitmap, s->stride, sb->palette,
                           sx - xmin, sy - ymin, s->w, s->h);
    }
    if (!c->spudec)
        c->spudec = spudec_new_scaled(NULL, 0, 0, NULL, 0);
    spudec_packet_send(c->spudec, packet, MP_NOPTS_VALUE, MP_NOPTS_VALUE);
    spudec_set_res(c->spudec, screen_w * scale, screen_h * scale);
    spudec_heartbeat(c->spudec, 0);
    spudec_get_bitmap(c->spudec, screen_w, screen_h, imgs);
    imgs->render_index = src.render_index;
    imgs->bitmap_id = src.bitmap_id;
    imgs->bitmap_pos_id = src.bitmap_pos_id;
    return true;
}
