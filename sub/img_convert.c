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

#include <string.h>
#include <assert.h>

#include <libavutil/mem.h>
#include <libavutil/common.h>

#include "mpv_talloc.h"

#include "common/common.h"
#include "img_convert.h"
#include "osd.h"
#include "video/img_format.h"
#include "video/mp_image.h"
#include "video/sws_utils.h"

void mp_blur_rgba_sub_bitmap(struct sub_bitmap *d, double gblur)
{
    struct mp_image *tmp1 = mp_image_alloc(IMGFMT_BGRA, d->w, d->h);
    if (tmp1) { // on OOM, skip region
        struct mp_image s = {0};
        mp_image_setfmt(&s, IMGFMT_BGRA);
        mp_image_set_size(&s, d->w, d->h);
        s.stride[0] = d->stride;
        s.planes[0] = d->bitmap;

        mp_image_copy(tmp1, &s);

        mp_image_sw_blur_scale(&s, tmp1, gblur);
    }
    talloc_free(tmp1);
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
