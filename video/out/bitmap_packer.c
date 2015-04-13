/*
 * Calculate how to pack bitmap rectangles into a larger surface
 *
 * Copyright 2009, 2012 Uoti Urpala
 *
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

#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

#include <libavutil/common.h>

#include "talloc.h"
#include "bitmap_packer.h"
#include "common/common.h"
#include "sub/dec_sub.h"
#include "video/mp_image.h"

#define IS_POWER_OF_2(x) (((x) > 0) && !(((x) - 1) & (x)))

void packer_reset(struct bitmap_packer *packer)
{
    struct bitmap_packer old = *packer;
    *packer = (struct bitmap_packer) {
        .w_max = old.w_max,
        .h_max = old.h_max,
    };
    talloc_free_children(packer);
}

void packer_get_bb(struct bitmap_packer *packer, struct pos out_bb[2])
{
    out_bb[0] = (struct pos) {0};
    out_bb[1] = (struct pos) {
        FFMIN(packer->used_width + packer->padding, packer->w),
        FFMIN(packer->used_height + packer->padding, packer->h),
    };
}

#define HEIGHT_SORT_BITS 4
static int size_index(int s)
{
    int n = av_log2_16bit(s);
    return (n << HEIGHT_SORT_BITS)
       + ((- 1 - (s << HEIGHT_SORT_BITS >> n)) & ((1 << HEIGHT_SORT_BITS) - 1));
}

/* Pack the given rectangles into an area of size w * h.
 * The size of each rectangle is read from in[i].x / in[i].y.
 * The height of each rectangle must be less than 65536.
 * 'scratch' must point to work memory for num_rects+16 ints.
 * The packed position for rectangle number i is set in out[i].
 * Return 0 on success, -1 if the rectangles did not fit in w*h.
 *
 * The rectangles are placed in rows in order approximately sorted by
 * height (the approximate sorting is simpler than a full one would be,
 * and allows the algorithm to work in linear time). Additionally, to
 * reduce wasted space when there are a few tall rectangles, empty
 * lower-right parts of rows are filled recursively when the size of
 * rectangles in the row drops past a power-of-two threshold. So if a
 * row starts with rectangles of size 3x50, 10x40 and 5x20 then the
 * free rectangle with corners (13, 20)-(w, 50) is filled recursively.
 */
static int pack_rectangles(struct pos *in, struct pos *out, int num_rects,
                           int w, int h, int *scratch, int *used_width)
{
    int bins[16 << HEIGHT_SORT_BITS];
    int sizes[16 << HEIGHT_SORT_BITS] = { 0 };
    for (int i = 0; i < num_rects; i++)
        sizes[size_index(in[i].y)]++;
    int idx = 0;
    for (int i = 0; i < 16 << HEIGHT_SORT_BITS; i += 1 << HEIGHT_SORT_BITS) {
        for (int j = 0; j < 1 << HEIGHT_SORT_BITS; j++) {
            bins[i + j] = idx;
            idx += sizes[i + j];
        }
        scratch[idx++] = -1;
    }
    for (int i = 0; i < num_rects; i++)
        scratch[bins[size_index(in[i].y)]++] = i;
    for (int i = 0; i < 16; i++)
        bins[i] = bins[i << HEIGHT_SORT_BITS] - sizes[i << HEIGHT_SORT_BITS];
    struct {
        int size, x, bottom;
    } stack[16] = {{15, 0, h}}, s = {};
    int stackpos = 1;
    int y;
    while (stackpos) {
        y = s.bottom;
        s = stack[--stackpos];
        s.size++;
        while (s.size--) {
            int maxy = -1;
            int obj;
            while ((obj = scratch[bins[s.size]]) >= 0) {
                int bottom = y + in[obj].y;
                if (bottom > s.bottom)
                    break;
                int right = s.x + in[obj].x;
                if (right > w)
                    break;
                bins[s.size]++;
                out[obj] = (struct pos){s.x, y};
                num_rects--;
                if (maxy < 0)
                    stack[stackpos++] = s;
                s.x = right;
                maxy = FFMAX(maxy, bottom);
            }
            *used_width = FFMAX(*used_width, s.x);
            if (maxy > 0)
                s.bottom = maxy;
        }
    }
    return num_rects ? -1 : y;
}

int packer_pack(struct bitmap_packer *packer)
{
    if (packer->count == 0)
        return 0;
    int w_orig = packer->w, h_orig = packer->h;
    struct pos *in = packer->in;
    int xmax = 0, ymax = 0;
    for (int i = 0; i < packer->count; i++) {
        if (in[i].x <= packer->padding || in[i].y <= packer->padding)
            in[i] = (struct pos){0, 0};
        if (in[i].x < 0 || in [i].x > 65535 || in[i].y < 0 || in[i].y > 65535) {
            fprintf(stderr, "Invalid OSD / subtitle bitmap size\n");
            abort();
        }
        xmax = FFMAX(xmax, in[i].x);
        ymax = FFMAX(ymax, in[i].y);
    }
    xmax = FFMAX(0, xmax - packer->padding);
    ymax = FFMAX(0, ymax - packer->padding);
    if (xmax > packer->w)
        packer->w = 1 << (av_log2(xmax - 1) + 1);
    if (ymax > packer->h)
        packer->h = 1 << (av_log2(ymax - 1) + 1);
    while (1) {
        int used_width = 0;
        int y = pack_rectangles(in, packer->result, packer->count,
                                packer->w + packer->padding,
                                packer->h + packer->padding,
                                packer->scratch, &used_width);
        if (y >= 0) {
            // No padding at edges
            packer->used_width = FFMIN(used_width, packer->w);
            packer->used_height = FFMIN(y, packer->h);
            assert(packer->w == 0 || IS_POWER_OF_2(packer->w));
            assert(packer->h == 0 || IS_POWER_OF_2(packer->h));
            return packer->w != w_orig || packer->h != h_orig;
        }
        if (packer->w <= packer->h && packer->w != packer->w_max)
            packer->w = FFMIN(packer->w * 2, packer->w_max);
        else if (packer->h != packer->h_max)
            packer->h = FFMIN(packer->h * 2, packer->h_max);
        else {
            packer->w = w_orig;
            packer->h = h_orig;
            return -1;
        }
    }
}

void packer_set_size(struct bitmap_packer *packer, int size)
{
    packer->count = size;
    if (size <= packer->asize)
        return;
    packer->asize = FFMAX(packer->asize * 2, size);
    talloc_free(packer->result);
    talloc_free(packer->scratch);
    packer->in = talloc_realloc(packer, packer->in, struct pos, packer->asize);
    packer->result = talloc_array_ptrtype(packer, packer->result,
                                          packer->asize);
    packer->scratch = talloc_array_ptrtype(packer, packer->scratch,
                                           packer->asize + 16);
}

int packer_pack_from_subbitmaps(struct bitmap_packer *packer,
                                struct sub_bitmaps *b)
{
    packer->count = 0;
    if (b->format == SUBBITMAP_EMPTY)
        return 0;
    packer_set_size(packer, b->num_parts);
    int a = packer->padding;
    for (int i = 0; i < b->num_parts; i++)
        packer->in[i] = (struct pos){b->parts[i].w + a, b->parts[i].h + a};
    return packer_pack(packer);
}

void packer_copy_subbitmaps(struct bitmap_packer *packer, struct sub_bitmaps *b,
                            void *data, int pixel_stride, int stride)
{
    assert(packer->count == b->num_parts);
    if (packer->padding) {
        struct pos bb[2];
        packer_get_bb(packer, bb);
        memset_pic(data, 0, bb[1].x * pixel_stride, bb[1].y, stride);
    }
    for (int n = 0; n < packer->count; n++) {
        struct sub_bitmap *s = &b->parts[n];
        struct pos p = packer->result[n];

        void *pdata = (uint8_t *)data + p.y * stride + p.x * pixel_stride;
        memcpy_pic(pdata, s->bitmap, s->w * pixel_stride, s->h,
                   stride, s->stride);
    }
}
