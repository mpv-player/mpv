/*
 * Common code for packing EOSD images into larger surfaces.
 *
 * This file is part of mplayer2.
 *
 * mplayer2 is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mplayer2 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mplayer2; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <libavutil/common.h>
#include "talloc.h"
#include "mp_msg.h"
#include "eosd_packer.h"

// Initial size of EOSD surface in pixels (x*x)
#define EOSD_SURFACE_INITIAL_SIZE 256

// Allocate an eosd_packer, which can be used to layout and cache the list of
// EOSD images contained in a mp_eosd_images_t into a flat surface.
// It can be free'd with talloc_free().
// Don't forget to call eosd_init() before using it.
struct eosd_packer *eosd_packer_create(void *talloc_ctx) {
    return talloc_zero(talloc_ctx, struct eosd_packer);
}

// Call this when you need to completely reinitialize the EOSD state, e.g. when
// when your EOSD surface was deleted.
// max_width and max_height are the maximum surface sizes that should be
// allowed.
void eosd_packer_reinit(struct eosd_packer *state, uint32_t max_width,
                        uint32_t max_height)
{
    state->max_surface_width = max_width;
    state->max_surface_height = max_height;
    state->surface.w = 0;
    state->surface.h = 0;
    state->targets_count = 0;
}

#define HEIGHT_SORT_BITS 4
static int size_index(struct eosd_target *r)
{
    unsigned int h = r->source.y1;
    int n = av_log2_16bit(h);
    return (n << HEIGHT_SORT_BITS)
        + (- 1 - (h << HEIGHT_SORT_BITS >> n) & (1 << HEIGHT_SORT_BITS) - 1);
}

/* Pack the given rectangles into an area of size w * h.
 * The size of each rectangle is read from .source.x1/.source.y1.
 * The height of each rectangle must be at least 1 and less than 65536.
 * The .source rectangle is then set corresponding to the packed position.
 * 'scratch' must point to work memory for num_rects+16 ints.
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
static int pack_rectangles(struct eosd_target *rects, int num_rects,
                           int w, int h, int *scratch)
{
    int bins[16 << HEIGHT_SORT_BITS];
    int sizes[16 << HEIGHT_SORT_BITS] = {};
    for (int i = 0; i < num_rects; i++)
        sizes[size_index(rects + i)]++;
    int idx = 0;
    for (int i = 0; i < 16 << HEIGHT_SORT_BITS; i += 1 << HEIGHT_SORT_BITS) {
        for (int j = 0; j < 1 << HEIGHT_SORT_BITS; j++) {
            bins[i + j] = idx;
            idx += sizes[i + j];
        }
        scratch[idx++] = -1;
    }
    for (int i = 0; i < num_rects; i++)
        scratch[bins[size_index(rects + i)]++] = i;
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
                int bottom = y + rects[obj].source.y1;
                if (bottom > s.bottom)
                    break;
                int right = s.x + rects[obj].source.x1;
                if (right > w)
                    break;
                bins[s.size]++;
                rects[obj].source.x0 = s.x;
                rects[obj].source.x1 += s.x;
                rects[obj].source.y0 = y;
                rects[obj].source.y1 += y;
                num_rects--;
                if (maxy <= 0)
                    stack[stackpos++] = s;
                s.x = right;
                maxy = FFMAX(maxy, bottom);
            }
            if (maxy > 0)
                s.bottom = maxy;
        }
    }
    return num_rects ? -1 : 0;
}

// padding to reduce interpolation artifacts when doing scaling & filtering
#define EOSD_PADDING 0

// Release all previous images, and packs the images in imgs into state. The
// caller must check the change variables:
// *out_need_reposition == true: sub-image positions changed
// *out_need_upload == true: upload all sub-images again
// *out_need_reallocate == true: resize the EOSD texture to state->surface.w/h
// Logical implications: need_reallocate => need_upload => need_reposition
void eosd_packer_generate(struct eosd_packer *state, mp_eosd_images_t *imgs,
                          bool *out_need_reposition, bool *out_need_upload,
                          bool *out_need_reallocate)
{
    int i;
    ASS_Image *img = imgs->imgs;
    ASS_Image *p;
    struct eosd_surface *sfc = &state->surface;

    *out_need_reposition = imgs->bitmap_pos_id != state->last_bitmap_pos_id;
    *out_need_upload = imgs->bitmap_id != state->last_bitmap_id;
    *out_need_reallocate = false;

    state->last_bitmap_pos_id = imgs->bitmap_pos_id;
    state->last_bitmap_id = imgs->bitmap_id;

    // eosd_reinit() was probably called, force full reupload.
    if (state->targets_count == 0 && img)
        *out_need_upload = true;

    if (!(*out_need_reposition) && !(*out_need_upload))
        return; // Nothing changed, no need to redraw

    state->targets_count = 0;

    *out_need_reposition = true;

    if (!img)
        return; // There's nothing to render!

    if (!(*out_need_upload))
        goto eosd_skip_upload;

    *out_need_upload = true;
    while (1) {
        for (p = img, i = 0; p; p = p->next) {
            if (p->w <= 0 || p->h <= 0)
                continue;
            // Allocate new space for surface/target arrays
            if (i >= state->targets_size) {
                state->targets_size = FFMAX(state->targets_size * 2, 512);
                state->targets  =
                    talloc_realloc_size(state, state->targets,
                                        state->targets_size
                                        * sizeof(*state->targets));
                state->scratch =
                    talloc_realloc_size(state, state->scratch,
                                        (state->targets_size + 16)
                                        * sizeof(*state->scratch));
            }
            state->targets[i].source.x1 = p->w + EOSD_PADDING;
            state->targets[i].source.y1 = p->h + EOSD_PADDING;
            i++;
        }
        if (pack_rectangles(state->targets, i, sfc->w, sfc->h,
                            state->scratch) >= 0)
            break;
        int w = FFMIN(FFMAX(sfc->w * 2, EOSD_SURFACE_INITIAL_SIZE),
                      state->max_surface_width);
        int h = FFMIN(FFMAX(sfc->h * 2, EOSD_SURFACE_INITIAL_SIZE),
                      state->max_surface_height);
        if (w == sfc->w && h == sfc->h) {
            mp_msg(MSGT_VO, MSGL_ERR, "[eosd] EOSD bitmaps do not fit on "
                   "a surface with the maximum supported size\n");
            return;
        }
        sfc->w = w;
        sfc->h = h;
        *out_need_reallocate = true;
    }
    if (*out_need_reallocate) {
        mp_msg(MSGT_VO, MSGL_V, "[eosd] Allocate a %dx%d surface for "
               "EOSD bitmaps.\n", sfc->w, sfc->h);
    }

eosd_skip_upload:
    for (p = img; p; p = p->next) {
        if (p->w <= 0 || p->h <= 0)
            continue;
        struct eosd_target *target = &state->targets[state->targets_count];
        target->source.x1 -= EOSD_PADDING;
        target->source.y1 -= EOSD_PADDING;
        target->dest.x0 = p->dst_x;
        target->dest.y0 = p->dst_y;
        target->dest.x1 = p->w + p->dst_x;
        target->dest.y1 = p->h + p->dst_y;
        target->color = p->color;
        target->ass_img = p;
        state->targets_count++;
    }
}

// Calculate the bounding box of all sub-rectangles in the EOSD surface that
// will be used for EOSD rendering.
// If the bounding box is empty, return false.
bool eosd_packer_calculate_source_bb(struct eosd_packer *state,
                                     struct eosd_rect *out_bb)
{
    struct eosd_rect bb = { state->surface.w, state->surface.h, 0, 0 };

    for (int n = 0; n < state->targets_count; n++) {
        struct eosd_rect s = state->targets[n].source;
        bb.x0 = FFMIN(bb.x0, s.x0);
        bb.y0 = FFMIN(bb.y0, s.y0);
        bb.x1 = FFMAX(bb.x1, s.x1);
        bb.y1 = FFMAX(bb.y1, s.y1);
    }

    // avoid degenerate bounding box if empty
    bb.x0 = FFMIN(bb.x0, bb.x1);
    bb.y0 = FFMIN(bb.y0, bb.y1);

    *out_bb = bb;
    return state->targets_count > 0;
}
