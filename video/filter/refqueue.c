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

#include <assert.h>

#include "common/common.h"
#include "video/mp_image.h"

#include "refqueue.h"

struct mp_refqueue {
    int needed_past_frames;
    int needed_future_frames;

    bool eof;
    double past_pts;

    // Queue of input frames, used to determine past/current/future frames.
    // queue[0] is the newest frame, queue[num_queue - 1] the oldest.
    struct mp_image **queue;
    int num_queue;
    // queue[pos] is the current frame, unless pos is an invalid index.
    int pos;
};

struct mp_refqueue *mp_refqueue_alloc(void)
{
    struct mp_refqueue *q = talloc_zero(NULL, struct mp_refqueue);
    mp_refqueue_flush(q);
    return q;
}

void mp_refqueue_free(struct mp_refqueue *q)
{
    talloc_free(q);
}

// The minimum number of frames required before and after the current frame.
void mp_refqueue_set_refs(struct mp_refqueue *q, int past, int future)
{
    assert(past >= 0 && future >= 0);
    q->needed_past_frames = past;
    q->needed_future_frames = future;
}

// Discard all state.
void mp_refqueue_flush(struct mp_refqueue *q)
{
    for (int n = 0; n < q->num_queue; n++)
        talloc_free(q->queue[n]);
    q->num_queue = 0;
    q->pos = -1;
    q->eof = false;
    q->past_pts = MP_NOPTS_VALUE;
}

// Add a new frame to the queue. (Call mp_refqueue_next() to advance the
// current frame and to discard unneeded past frames.)
// Ownership goes to the mp_refqueue.
// Passing NULL means EOF, in which case mp_refqueue_need_input() will return
// false even if not enough future frames are available.
void mp_refqueue_add_input(struct mp_refqueue *q, struct mp_image *img)
{
    q->eof = !img;
    if (!img)
        return;

    MP_TARRAY_INSERT_AT(q, q->queue, q->num_queue, 0, img);
    q->pos++;

    assert(q->pos >= 0 && q->pos < q->num_queue);
}

bool mp_refqueue_need_input(struct mp_refqueue *q)
{
    return q->pos < q->needed_future_frames && !q->eof;
}

bool mp_refqueue_has_output(struct mp_refqueue *q)
{
    return q->pos >= 0 && !mp_refqueue_need_input(q);
}

// Advance current frame by 1 (or 2 fields if interlaced).
void mp_refqueue_next(struct mp_refqueue *q)
{
    if (!mp_refqueue_has_output(q))
        return;

    q->past_pts = q->queue[q->pos]->pts;

    q->pos--;

    assert(q->pos >= -1 && q->pos < q->num_queue);

    // Discard unneeded past frames.
    while (q->num_queue - (q->pos + 1) > q->needed_past_frames) {
        assert(q->num_queue > 0);
        talloc_free(q->queue[q->num_queue - 1]);
        q->num_queue--;
    }

    assert(q->pos >= -1 && q->pos < q->num_queue);
}

// Return a frame by relative position:
//  -1: first past frame
//   0: current frame
//   1: first future frame
// Caller doesn't get ownership. Return NULL if unavailable.
struct mp_image *mp_refqueue_get(struct mp_refqueue *q, int pos)
{
    int i = q->pos - pos;
    return i >= 0 && i < q->num_queue ? q->queue[i] : NULL;
}

// Get the pts of field 0/1 (0 being the first to output).
double mp_refqueue_get_field_pts(struct mp_refqueue *q, int field)
{
    assert(field == 0 || field == 1);

    if (q->pos < 0)
        return MP_NOPTS_VALUE;

    double pts = q->queue[q->pos]->pts;

    if (field == 0 || pts == MP_NOPTS_VALUE)
        return pts;

    if (q->past_pts == MP_NOPTS_VALUE)
        return MP_NOPTS_VALUE;

    double frametime = pts - q->past_pts;
    if (frametime <= 0.0 || frametime >= 1.0)
        return MP_NOPTS_VALUE;

    return pts + frametime / 2;
}

