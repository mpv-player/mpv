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

#include <libavutil/buffer.h>

#include "common/common.h"
#include "filters/f_autoconvert.h"
#include "filters/filter_internal.h"
#include "video/mp_image.h"

#include "refqueue.h"

struct mp_refqueue {
    struct mp_filter *filter;
    struct mp_autoconvert *conv;
    struct mp_pin *in, *out;

    struct mp_image *in_format;

    // Buffered frame in case of format changes.
    struct mp_image *next;

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

static bool mp_refqueue_has_output(struct mp_refqueue *q);

static void refqueue_dtor(void *p)
{
    struct mp_refqueue *q = p;
    mp_refqueue_flush(q);
    mp_image_unrefp(&q->in_format);
    talloc_free(q->conv->f);
}

struct mp_refqueue *mp_refqueue_alloc(struct mp_filter *f)
{
    struct mp_refqueue *q = talloc_zero(f, struct mp_refqueue);
    talloc_set_destructor(q, refqueue_dtor);
    q->filter = f;

    q->conv = mp_autoconvert_create(f);
    if (!q->conv)
        abort();

    q->in = q->conv->f->pins[1];
    mp_pin_connect(q->conv->f->pins[0], f->ppins[0]);
    q->out = f->ppins[1];

    mp_refqueue_flush(q);
    return q;
}

void mp_refqueue_add_in_format(struct mp_refqueue *q, int fmt, int subfmt)
{
    mp_autoconvert_add_imgfmt(q->conv, fmt, subfmt);
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
    mp_image_unrefp(&q->next);
}

static void mp_refqueue_add_input(struct mp_refqueue *q, struct mp_image *img)
{
    assert(img);

    MP_TARRAY_INSERT_AT(q, q->queue, q->num_queue, 0, img);
    q->pos++;

    assert(q->pos >= 0 && q->pos < q->num_queue);
}

static bool mp_refqueue_need_input(struct mp_refqueue *q)
{
    return q->pos < q->needed_future_frames && !q->eof;
}

static bool mp_refqueue_has_output(struct mp_refqueue *q)
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

// Advance to next input frame (skips fields even in field output mode).
static void mp_refqueue_next(struct mp_refqueue *q)
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

// Advance current field, depending on interlace flags.
static void mp_refqueue_next_field(struct mp_refqueue *q)
{
    if (!mp_refqueue_has_output(q))
        return;

    if (!output_next_field(q))
        mp_refqueue_next(q);
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

// Return non-NULL if a format change happened. A format change is defined by
// a change in image parameters, using broad enough checks that happen to be
// sufficient for all users of refqueue.
// On format change, the refqueue transparently drains remaining frames, and
// once that is done, this function returns a mp_image reference of the new
// frame. Reinit the low level video processor based on it, and then leave the
// reference alone and continue normally.
// All frames returned in the future will have a compatible format.
struct mp_image *mp_refqueue_execute_reinit(struct mp_refqueue *q)
{
    if (mp_refqueue_has_output(q) || !q->next)
        return NULL;

    struct mp_image *cur = q->next;
    q->next = NULL;

    mp_image_unrefp(&q->in_format);
    mp_refqueue_flush(q);

    q->in_format = mp_image_new_ref(cur);
    if (!q->in_format)
        abort();
    mp_image_unref_data(q->in_format);

    mp_refqueue_add_input(q, cur);
    return cur;
}

// Main processing function. Call this in the filter process function.
// Returns if enough input frames are available for filtering, and output pin
// needs data; in other words, if this returns true, you render a frame and
// output it.
// If this returns true, you must call mp_refqueue_write_out_pin() to make
// progress.
bool mp_refqueue_can_output(struct mp_refqueue *q)
{
    if (!mp_pin_in_needs_data(q->out))
        return false;

    // Strictly return any output first to reduce latency.
    if (mp_refqueue_has_output(q))
        return true;

    if (q->next) {
        // Make it call again for mp_refqueue_execute_reinit().
        mp_filter_internal_mark_progress(q->filter);
        return false;
    }

    struct mp_frame frame = mp_pin_out_read(q->in);
    if (frame.type == MP_FRAME_NONE)
        return false;

    if (frame.type == MP_FRAME_EOF) {
        q->eof = true;
        if (mp_refqueue_has_output(q)) {
            mp_pin_out_unread(q->in, frame);
            return true;
        }
        mp_pin_in_write(q->out, frame);
        mp_refqueue_flush(q);
        return false;
    }

    if (frame.type != MP_FRAME_VIDEO) {
        MP_ERR(q->filter, "unsupported frame type\n");
        mp_frame_unref(&frame);
        mp_filter_internal_mark_failed(q->filter);
        return false;
    }

    struct mp_image *img = frame.data;

    if (!q->in_format || !!q->in_format->hwctx != !!img->hwctx ||
        (img->hwctx && img->hwctx->data != q->in_format->hwctx->data) ||
        !mp_image_params_equal(&q->in_format->params, &img->params))
    {
        q->next = img;
        q->eof = true;
        mp_filter_internal_mark_progress(q->filter);
        return false;
    }

    mp_refqueue_add_input(q, img);

    if (mp_refqueue_has_output(q))
        return true;

    mp_pin_out_request_data(q->in);
    return false;
}

// (Accepts NULL for generic errors.)
void mp_refqueue_write_out_pin(struct mp_refqueue *q, struct mp_image *mpi)
{
    if (mpi) {
        mp_pin_in_write(q->out, MAKE_FRAME(MP_FRAME_VIDEO, mpi));
    } else {
        MP_WARN(q->filter, "failed to output frame\n");
        mp_filter_internal_mark_failed(q->filter);
    }
    mp_refqueue_next_field(q);
}

// Return frame for current format (without data). Reference is owned by q,
// might go away on further queue accesses. NULL if none yet.
struct mp_image *mp_refqueue_get_format(struct mp_refqueue *q)
{
    return q->in_format;
}
