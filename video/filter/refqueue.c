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
    int flags;

    bool second_field; // current frame has to output a second field yet
    bool eof;

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
    mp_refqueue_flush(q);
    talloc_free(q);
}

// The minimum number of frames required before and after the current frame.
void mp_refqueue_set_refs(struct mp_refqueue *q, int past, int future)
{
    assert(past >= 0 && future >= 0);
    q->needed_past_frames = past;
    q->needed_future_frames = MPMAX(future, 1); // at least 1 for determining PTS
}

// MP_MODE_* flags
void mp_refqueue_set_mode(struct mp_refqueue *q, int flags)
{
    q->flags = flags;
}

// Whether the current frame should be deinterlaced.
bool mp_refqueue_should_deint(struct mp_refqueue *q)
{
    if (!mp_refqueue_has_output(q) || !(q->flags & MP_MODE_DEINT))
        return false;

    return (q->queue[q->pos]->fields & MP_IMGFIELD_INTERLACED) ||
           !(q->flags & MP_MODE_INTERLACED_ONLY);
}

// Whether the current output frame (field) is the top field, bottom field
// otherwise. (Assumes the caller forces deinterlacing.)
bool mp_refqueue_is_top_field(struct mp_refqueue *q)
{
    if (!mp_refqueue_has_output(q))
        return false;

    return !!(q->queue[q->pos]->fields & MP_IMGFIELD_TOP_FIRST) ^ q->second_field;
}

// Whether top-field-first mode is enabled.
bool mp_refqueue_top_field_first(struct mp_refqueue *q)
{
    if (!mp_refqueue_has_output(q))
        return false;

    return q->queue[q->pos]->fields & MP_IMGFIELD_TOP_FIRST;
}

// Discard all state.
void mp_refqueue_flush(struct mp_refqueue *q)
{
    for (int n = 0; n < q->num_queue; n++)
        talloc_free(q->queue[n]);
    q->num_queue = 0;
    q->pos = -1;
    q->second_field = false;
    q->eof = false;
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

static bool output_next_field(struct mp_refqueue *q)
{
    if (q->second_field)
        return false;
    if (!(q->flags & MP_MODE_OUTPUT_FIELDS))
        return false;
    if (!mp_refqueue_should_deint(q))
        return false;

    assert(q->pos >= 0);

    // If there's no (reasonable) timestamp, also skip the field.
    if (q->pos == 0)
        return false;

    double pts = q->queue[q->pos]->pts;
    double next_pts = q->queue[q->pos - 1]->pts;
    if (pts == MP_NOPTS_VALUE || next_pts == MP_NOPTS_VALUE)
        return false;

    double frametime = next_pts - pts;
    if (frametime <= 0.0 || frametime >= 1.0)
        return false;

    q->queue[q->pos]->pts = pts + frametime / 2;
    q->second_field = true;
    return true;
}

// Advance current field, depending on interlace flags.
void mp_refqueue_next_field(struct mp_refqueue *q)
{
    if (!mp_refqueue_has_output(q))
        return;

    if (!output_next_field(q))
        mp_refqueue_next(q);
}

// Advance to next input frame (skips fields even in field output mode).
void mp_refqueue_next(struct mp_refqueue *q)
{
    if (!mp_refqueue_has_output(q))
        return;

    q->pos--;
    q->second_field = false;

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

// Same as mp_refqueue_get(), but return the frame which contains a field
// relative to the current field's position.
struct mp_image *mp_refqueue_get_field(struct mp_refqueue *q, int pos)
{
    // If the current field is the second field (conceptually), then pos=1
    // needs to get the next frame. Similarly, pos=-1 needs to get the current
    // frame, so round towards negative infinity.
    int round = mp_refqueue_top_field_first(q) != mp_refqueue_is_top_field(q);
    int frame = (pos < 0 ? pos - (1 - round) : pos + round) / 2;
    return mp_refqueue_get(q, frame);
}

bool mp_refqueue_is_second_field(struct mp_refqueue *q)
{
    return mp_refqueue_has_output(q) && q->second_field;
}
