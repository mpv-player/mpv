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

struct osd_conv_cache {
    struct sub_bitmap part;
    // for osd_conv_cache_alloc_old_p() (SUBBITMAP_PLANAR)
    int allocated, stride;
    struct old_osd_planar bmp;
    // for osd_conv_cache_alloc_old() (SUBBITMAP_OLD_PLANAR)
    unsigned char *packed;
};

static int osd_conv_cache_destroy(void *p)
{
    struct osd_conv_cache *c = p;
    av_free(c->bmp.bitmap);
    av_free(c->bmp.alpha);
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

static void osd_conv_cache_alloc_old(struct osd_conv_cache *c, int w, int h)
{
    size_t size = talloc_get_size(c->packed);
    size_t new_size = w * 2 * h;
    if (new_size > size)
        c->packed = talloc_realloc(c, c->packed, unsigned char, new_size);
    c->part = (struct sub_bitmap) {
        .bitmap = c->packed,
        .stride = w * 2,
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

// SUBBITMAP_OLD_PLANAR -> SUBBITMAP_OLD
bool osd_conv_old_p_to_old(struct osd_conv_cache *c, struct sub_bitmaps *imgs)
{
    struct sub_bitmaps src = *imgs;
    if (src.format != SUBBITMAP_OLD_PLANAR || src.num_parts > 1)
        return false;

    imgs->format = SUBBITMAP_OLD;
    imgs->num_parts = 0;
    imgs->parts = NULL;

    if (src.num_parts == 0)
        return true;

    struct sub_bitmap *s = &src.parts[0];
    struct old_osd_planar *p = s->bitmap;

    osd_conv_cache_alloc_old(c, s->w, s->h);

    for (int y = 0; y < s->h; y++) {
        unsigned char *y_src = p->bitmap + s->stride * y;
        unsigned char *y_srca = p->alpha + s->stride * y;
        unsigned char *cur = c->packed + y * s->w * 2;
        for (int x = 0; x < s->w; x++) {
            cur[x*2+0] = y_src[x];
            cur[x*2+1] = -y_srca[x];
        }
    }

    c->part.x = s->x;
    c->part.y = s->y;

    imgs->parts = &c->part;
    imgs->num_parts = 1;
    return true;
}
