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
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>
#include <assert.h>

#include <libavutil/mem.h>
#include <libavutil/common.h>

#include "talloc.h"

#include "common/common.h"
#include "img_convert.h"
#include "osd.h"
#include "video/img_format.h"
#include "video/mp_image.h"
#include "video/sws_utils.h"

struct osd_conv_cache {
    struct sub_bitmap part[MP_SUB_BB_LIST_MAX];
    struct sub_bitmap *parts;
    void *scratch;
};

struct osd_conv_cache *osd_conv_cache_new(void)
{
    return talloc_zero(NULL, struct osd_conv_cache);
}

static void rgba_to_premultiplied_rgba(uint32_t *colors, size_t count)
{
    for (int n = 0; n < count; n++) {
        uint32_t c = colors[n];
        unsigned b = c & 0xFF;
        unsigned g = (c >> 8) & 0xFF;
        unsigned r = (c >> 16) & 0xFF;
        unsigned a = (c >> 24) & 0xFF;
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
        struct mp_image *image = mp_image_alloc(IMGFMT_BGRA, s->w, s->h);
        talloc_steal(c->parts, image);
        if (!image) {
            // on OOM, skip the region by making it 0 sized
            d->w = d->h = d->dw = d->dh = 0;
            continue;
        }

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
        struct mp_image *temp = mp_image_alloc(IMGFMT_BGRA, s->w + pad * 2,
                                                            s->h + pad * 2);
        if (!temp)
            continue; // on OOM, skip region
        memset_pic(temp->planes[0], 0, temp->w * 4, temp->h, temp->stride[0]);
        uint8_t *p0 = temp->planes[0] + pad * 4 + pad * temp->stride[0];
        memcpy_pic(p0, s->bitmap, s->w * 4, s->h, temp->stride[0], s->stride);

        double sx = (double)s->dw / s->w;
        double sy = (double)s->dh / s->h;

        d->x = s->x - pad * sx;
        d->y = s->y - pad * sy;
        d->w = d->dw = s->dw + pad * 2 * sx;
        d->h = d->dh = s->dh + pad * 2 * sy;
        struct mp_image *image = mp_image_alloc(IMGFMT_BGRA, d->w, d->h);
        talloc_steal(c->parts, image);
        if (image) {
            d->stride = image->stride[0];
            d->bitmap = image->planes[0];

            mp_image_sw_blur_scale(image, temp, gblur);
        } else {
            // on OOM, skip region
            *d = *s;
        }

        talloc_free(temp);
    }
    return true;
}

// If RGBA parts need scaling, scale them.
bool osd_scale_rgba(struct osd_conv_cache *c, struct sub_bitmaps *imgs)
{
    struct sub_bitmaps src = *imgs;
    if (src.format != SUBBITMAP_RGBA)
        return false;

    bool need_scale = false;
    for (int n = 0; n < src.num_parts; n++) {
        struct sub_bitmap *sb = &src.parts[n];
        if (sb->w != sb->dw || sb->h != sb->dh)
            need_scale = true;
    }
    if (!need_scale)
        return false;

    talloc_free(c->parts);
    imgs->parts = c->parts = talloc_array(c, struct sub_bitmap, src.num_parts);

    // Note: we scale all parts, since most likely all need scaling anyway, and
    //       to get a proper copy of all data in the imgs list.
    for (int n = 0; n < src.num_parts; n++) {
        struct sub_bitmap *d = &imgs->parts[n];
        struct sub_bitmap *s = &src.parts[n];

        struct mp_image src_image = {0};
        mp_image_setfmt(&src_image, IMGFMT_BGRA);
        mp_image_set_size(&src_image, s->w, s->h);
        src_image.planes[0] = s->bitmap;
        src_image.stride[0] = s->stride;

        d->x = s->x;
        d->y = s->y;
        d->w = d->dw = s->dw;
        d->h = d->dh = s->dh;
        struct mp_image *image = mp_image_alloc(IMGFMT_BGRA, d->w, d->h);
        talloc_steal(c->parts, image);
        if (image) {
            d->stride = image->stride[0];
            d->bitmap = image->planes[0];

            mp_image_swscale(image, &src_image, mp_sws_fast_flags);
        } else {
            // on OOM, skip the region; just don't scale it
            *d = *s;
        }
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

    struct mp_rect bb_list[MP_SUB_BB_LIST_MAX];
    int num_bb = mp_get_sub_bb_list(&src, bb_list, MP_SUB_BB_LIST_MAX);

    imgs->format = SUBBITMAP_RGBA;
    imgs->parts = c->part;
    imgs->num_parts = num_bb;

    size_t newsize = 0;
    for (int n = 0; n < num_bb; n++) {
        struct mp_rect bb = bb_list[n];
        int w = bb.x1 - bb.x0;
        int h = bb.y1 - bb.y0;
        int stride = w * 4;
        newsize += h * stride;
    }

    if (talloc_get_size(c->scratch) < newsize) {
        talloc_free(c->scratch);
        c->scratch = talloc_array(c, uint8_t, newsize);
    }

    uint8_t *data = c->scratch;

    for (int n = 0; n < num_bb; n++) {
        struct mp_rect bb = bb_list[n];
        struct sub_bitmap *bmp = &c->part[n];

        bmp->x = bb.x0;
        bmp->y = bb.y0;
        bmp->w = bmp->dw = bb.x1 - bb.x0;
        bmp->h = bmp->dh = bb.y1 - bb.y0;
        bmp->stride = bmp->w * 4;
        bmp->bitmap = data;
        data += bmp->h * bmp->stride;

        memset_pic(bmp->bitmap, 0, bmp->w * 4, bmp->h, bmp->stride);

        for (int p = 0; p < src.num_parts; p++) {
            struct sub_bitmap *s = &src.parts[p];

            // Assume mp_get_sub_bb_list() never splits sub bitmaps
            // So we don't clip/adjust the size of the sub bitmap
            if (s->x > bb.x1 || s->x + s->w < bb.x0 ||
                s->y > bb.y1 || s->y + s->h < bb.y0)
                continue;

            draw_ass_rgba(s->bitmap, s->w, s->h, s->stride,
                          bmp->bitmap, bmp->stride,
                          s->x - bb.x0, s->y - bb.y0,
                          s->libass.color);
        }
    }

    return true;
}

bool mp_sub_bitmaps_bb(struct sub_bitmaps *imgs, struct mp_rect *out_bb)
{
    struct mp_rect bb = {INT_MAX, INT_MAX, INT_MIN, INT_MIN};
    for (int n = 0; n < imgs->num_parts; n++) {
        struct sub_bitmap *p = &imgs->parts[n];
        bb.x0 = FFMIN(bb.x0, p->x);
        bb.y0 = FFMIN(bb.y0, p->y);
        bb.x1 = FFMAX(bb.x1, p->x + p->dw);
        bb.y1 = FFMAX(bb.y1, p->y + p->dh);
    }

    // avoid degenerate bounding box if empty
    bb.x0 = FFMIN(bb.x0, bb.x1);
    bb.y0 = FFMIN(bb.y0, bb.y1);

    *out_bb = bb;

    return bb.x0 < bb.x1 && bb.y0 < bb.y1;
}

// Merge bounding rectangles if they're closer than the given amount of pixels.
// Avoids having too many rectangles due to spacing between letter.
#define MERGE_RC_PIXELS 50

static void remove_intersecting_rcs(struct mp_rect *list, int *count)
{
    int M = MERGE_RC_PIXELS;
    bool changed = true;
    while (changed) {
        changed = false;
        for (int a = 0; a < *count; a++) {
            struct mp_rect *rc_a = &list[a];
            for (int b = *count - 1; b > a; b--) {
                struct mp_rect *rc_b = &list[b];
                if (rc_a->x0 - M <= rc_b->x1 && rc_a->x1 + M >= rc_b->x0 &&
                    rc_a->y0 - M <= rc_b->y1 && rc_a->y1 + M >= rc_b->y0)
                {
                    mp_rect_union(rc_a, rc_b);
                    MP_TARRAY_REMOVE_AT(list, *count, b);
                    changed = true;
                }
            }
        }
    }
}

// Cluster the given subrectangles into a small numbers of bounding rectangles,
// and store them into list. E.g. when subtitles and toptitles are visible at
// the same time, there should be two bounding boxes, so that the video between
// the text is left untouched (need to resample less pixels -> faster).
// Returns number of rectangles added to out_rc_list (<= rc_list_count)
// NOTE: some callers assume that sub bitmaps are never split or partially
//       covered by returned rectangles.
int mp_get_sub_bb_list(struct sub_bitmaps *sbs, struct mp_rect *out_rc_list,
                       int rc_list_count)
{
    int M = MERGE_RC_PIXELS;
    int num_rc = 0;
    for (int n = 0; n < sbs->num_parts; n++) {
        struct sub_bitmap *sb = &sbs->parts[n];
        struct mp_rect bb = {sb->x, sb->y, sb->x + sb->dw, sb->y + sb->dh};
        bool intersects = false;
        for (int r = 0; r < num_rc; r++) {
            struct mp_rect *rc = &out_rc_list[r];
            if ((bb.x0 - M <= rc->x1 && bb.x1 + M >= rc->x0 &&
                 bb.y0 - M <= rc->y1 && bb.y1 + M >= rc->y0) ||
                num_rc == rc_list_count)
            {
                mp_rect_union(rc, &bb);
                intersects = true;
                break;
            }
        }
        if (!intersects) {
            out_rc_list[num_rc++] = bb;
            remove_intersecting_rcs(out_rc_list, &num_rc);
        }
    }
    remove_intersecting_rcs(out_rc_list, &num_rc);
    return num_rc;
}
