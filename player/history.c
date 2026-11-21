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

#include <math.h>

#include "audio/aframe.h"
#include "audio/format.h"
#include "common/common.h"
#include "common/msg.h"
#include "demux/demux.h"
#include "filters/f_output_chain.h"
#include "filters/filter.h"
#include "misc/node.h"
#include "mpv/client.h"
#include "options/options.h"
#include "video/img_format.h"

#include "core.h"
#include "history.h"

struct history_queue {
    struct mp_frame *frames;
    int num;
};

// contiguous range of frames
struct history_range {
    // 0 video, 1 audio
    struct history_queue q[2];
};

struct history_reader {
    int pos; // next frame of the current range to replay, -1 if not replaying
    bool eof; // filter chain returned EOF
    bool skip; // see history_skip()
};

struct playback_history {
    // least recently used first
    struct history_range **ranges;
    int num_ranges;
    // range we're currently replaying (last element in ranges)
    struct history_range *cur;
    // Missing frame after cur. Start new range after replay.
    bool gap;
    struct history_reader readers[2];
    int64_t bytes;
};

static const char *inactive_reason(struct MPContext *mpctx)
{
    struct vo_chain *vo_c = mpctx->vo_chain;
    if (!mpctx->opts->video_history)
        return "disabled";
    if (!vo_c)
        return "no-video";
    if (mpctx->play_dir < 0 || mpctx->opts->play_dir < 0)
        return "reverse-playback";
    if (vo_c->is_sparse || vo_c->is_coverart || (vo_c->track && vo_c->track->image) ||
        mpctx->lavfi || mpctx->encode_lavc_ctx)
        return "unsupported-pipeline";
    if (IMGFMT_IS_HWACCEL(vo_c->filter->output_params.imgfmt))
        return "hardware-decoding";
    if (mpctx->ao_chain && mpctx->ao_filter_fmt &&
        !af_fmt_is_pcm(mp_aframe_get_format(mpctx->ao_filter_fmt)))
        return "audio-passthrough";
    return NULL;
}

bool history_active(struct MPContext *mpctx)
{
    return !inactive_reason(mpctx);
}

static void free_range(struct MPContext *mpctx, int index)
{
    struct playback_history *h = mpctx->history;
    struct history_range *r = h->ranges[index];
    if (r == h->cur) {
        h->cur = NULL;
        h->readers[0] = h->readers[1] = (struct history_reader){.pos = -1};
    }
    for (int t = 0; t < 2; t++) {
        for (int n = 0; n < r->q[t].num; n++) {
            h->bytes -= mp_frame_approx_size(r->q[t].frames[n]);
            mp_frame_unref(&r->q[t].frames[n]);
        }
    }
    talloc_free(r);
    MP_TARRAY_REMOVE_AT(h->ranges, h->num_ranges, index);
}

void history_reset(struct MPContext *mpctx)
{
    struct playback_history *h = mpctx->history;
    while (h && h->num_ranges)
        free_range(mpctx, h->num_ranges - 1);
    TA_FREEP(&mpctx->history);
}

void history_end_range(struct MPContext *mpctx)
{
    struct playback_history *h = mpctx->history;
    if (h && h->cur && h->cur->q[0].num) {
        h->cur = NULL;
        h->readers[0] = h->readers[1] = (struct history_reader){.pos = -1};
    } else if (h && h->cur) {
        free_range(mpctx, h->num_ranges - 1);
    }
}

bool history_replaying(struct MPContext *mpctx, enum mp_frame_type type)
{
    struct playback_history *h = mpctx->history;
    return h && h->readers[type == MP_FRAME_AUDIO].pos >= 0;
}

void history_invalidate(struct MPContext *mpctx)
{
    if (history_replaying(mpctx, MP_FRAME_VIDEO) ||
        history_replaying(mpctx, MP_FRAME_AUDIO))
    {
        queue_seek(mpctx, MPSEEK_ABSOLUTE, get_current_time(mpctx),
                   MPSEEK_EXACT, 0);
    }
    history_reset(mpctx);
}

static double queue_pts(struct history_queue *q, int n)
{
    return mp_frame_get_pts(q->frames[n]);
}

static double range_duration(struct history_range *r)
{
    struct history_queue *v = &r->q[0], *a = &r->q[1];
    if (!v->num)
        return 0;
    double start = queue_pts(v, 0);
    if (a->num)
        start = MPMIN(start, queue_pts(a, 0));
    return queue_pts(v, v->num - 1) - start;
}

// Drop oldest frames of the least recently used range until the history
// is within the limits.
static void trim(struct MPContext *mpctx)
{
    struct MPOpts *opts = mpctx->opts;
    struct playback_history *h = mpctx->history;
    while (h->num_ranges) {
        double duration = 0;
        for (int n = 0; n < h->num_ranges; n++)
            duration += range_duration(h->ranges[n]);
        if (duration <= opts->video_history &&
            h->bytes <= opts->video_history_max_bytes)
            break;
        // Frames at the replay position are still needed.
        struct history_range *r = h->ranges[0];
        int t = -1;
        for (int n = 0; n < 2; n++) {
            if (r->q[n].num && !(r == h->cur && h->readers[n].pos == 0) &&
                (t < 0 || queue_pts(&r->q[n], 0) < queue_pts(&r->q[t], 0)))
                t = n;
        }
        if (t < 0)
            break;
        struct history_queue *q = &r->q[t];
        h->bytes -= mp_frame_approx_size(q->frames[0]);
        mp_frame_unref(&q->frames[0]);
        MP_TARRAY_REMOVE_AT(q->frames, q->num, 0);
        if (r == h->cur && h->readers[t].pos > 0)
            h->readers[t].pos--;
        if (!r->q[0].num)
            free_range(mpctx, 0);
    }
}

void history_add(struct MPContext *mpctx, struct mp_frame frame)
{
    struct playback_history *h = mpctx->history;
    if (!history_active(mpctx)) {
        if (h)
            h->gap = true;
        return;
    }
    if (!h) {
        h = mpctx->history = talloc_zero(mpctx, struct playback_history);
        h->readers[0].pos = h->readers[1].pos = -1;
    }

    int type = frame.type == MP_FRAME_AUDIO;
    double pts = mp_frame_get_pts(frame);
    if (pts == MP_NOPTS_VALUE) {
        h->gap = true;
        return;
    }
    if (h->cur && h->cur->q[type].num) {
        struct history_queue *q = &h->cur->q[type];
        struct mp_frame prev = q->frames[q->num - 1];
        if (type == 0) {
            // Same rule as in handle_new_frame().
            double diff = pts - mp_frame_get_pts(prev);
            if (diff <= 0 || diff >= (mpctx->demuxer->ts_resets_possible ? 5 : 1e4))
                h->gap = true;
        } else if (fabs(pts - mp_aframe_end_pts(prev.data)) > 0.001) {
            h->gap = true;
        }
    }
    if (h->gap || !h->cur) {
        if (h->readers[0].pos >= 0 || h->readers[1].pos >= 0)
            return;
        history_end_range(mpctx);
        h->cur = talloc_zero(h, struct history_range);
        MP_TARRAY_APPEND(h, h->ranges, h->num_ranges, h->cur);
        h->gap = false;
    }

    struct mp_frame ref = mp_frame_ref(frame);
    if (!ref.type) {
        h->gap = true;
        return;
    }
    struct history_queue *q = &h->cur->q[type];
    MP_TARRAY_APPEND(h->cur, q->frames, q->num, ref);
    h->bytes += mp_frame_approx_size(ref);
    trim(mpctx);
}

void history_skip(struct MPContext *mpctx, enum mp_frame_type type,
                  struct mp_pin *pin)
{
    struct playback_history *h = mpctx->history;
    struct history_reader *rd = h ? &h->readers[type == MP_FRAME_AUDIO] : NULL;
    if (!rd || !rd->skip)
        return;
    struct history_queue *q = &h->cur->q[type == MP_FRAME_AUDIO];
    struct mp_frame frame = mp_pin_out_read(pin);
    if (!frame.type)
        return;
    bool cached = false;
    if (q->num && frame.type == MP_FRAME_VIDEO) {
        cached = mp_frame_get_pts(frame) <= queue_pts(q, q->num - 1);
    } else if (q->num && frame.type == MP_FRAME_AUDIO) {
        double end = mp_aframe_end_pts(q->frames[q->num - 1].data);
        mp_aframe_clip_timestamps(frame.data, end, MP_NOPTS_VALUE);
        cached = !mp_aframe_get_size(frame.data);
    }
    if (cached) {
        // Only one frame per call to avoid latency.
        mp_frame_unref(&frame);
        mp_pin_out_request_data(pin);
        mp_wakeup_core(mpctx);
    } else {
        mp_pin_out_unread(pin, frame);
        rd->skip = false;
    }
}

struct mp_frame history_read(struct MPContext *mpctx, enum mp_frame_type type,
                             struct mp_pin *pin, bool *replay)
{
    struct playback_history *h = mpctx->history;
    struct history_reader *rd = h ? &h->readers[type == MP_FRAME_AUDIO] : NULL;
    *replay = false;
    if (rd && rd->pos >= 0) {
        struct history_queue *q = &h->cur->q[type == MP_FRAME_AUDIO];
        *replay = true;
        if (rd->pos < q->num)
            return mp_frame_ref(q->frames[rd->pos++]);
        if (rd->skip)
            return MP_NO_FRAME;
        *replay = false;
    }
    struct mp_frame frame = mp_pin_out_read(pin);
    if (rd) {
        rd->pos = -1;
        if (frame.type == MP_FRAME_EOF)
            rd->eof = true;
        if (frame.type == MP_FRAME_NONE && rd->eof)
            return MP_EOF_FRAME;
    }
    return frame;
}

void history_unread(struct MPContext *mpctx, struct mp_frame frame,
                    struct mp_pin *pin, bool replay)
{
    if (replay) {
        mpctx->history->readers[frame.type == MP_FRAME_AUDIO].pos--;
        mp_frame_unref(&frame);
    } else {
        mp_pin_out_unread(pin, frame);
    }
}

// range of video pts in r that can seek to
static bool range_bounds(struct MPContext *mpctx, struct history_range *r,
                         double *start, double *end)
{
    struct history_queue *v = &r->q[0], *a = &r->q[1];
    if (!v->num)
        return false;
    *start = queue_pts(v, 0);
    *end = queue_pts(v, v->num - 1);
    if (mpctx->ao_chain) {
        if (!a->num)
            return false;
        double delay = mpctx->opts->audio_delay;
        *start = MPMAX(*start, queue_pts(a, 0) + delay);
        *end = MPMIN(*end, mp_aframe_end_pts(a->frames[a->num - 1].data) + delay);
    }
    return *start <= *end;
}

// index of the video frame in r that the seek goes to or -1.
static int find_target(struct MPContext *mpctx, struct history_range *r,
                       double pts, int step)
{
    struct history_queue *v = &r->q[0];
    double start, end;
    if (!range_bounds(mpctx, r, &start, &end) || pts < start)
        return -1;

    int pos = 0;
    if (step) {
        while (pos < v->num && fabs(queue_pts(v, pos) - pts) > 1e-6)
            pos++;
        if (pos == v->num)
            return -1;
        pos += step;
    } else {
        // same tolerance as hr-seeks in video_output_image()
        while (pos < v->num && queue_pts(v, pos) < pts - .005)
            pos++;
    }
    if (pos < 0 || pos >= v->num)
        return -1;
    double target = queue_pts(v, pos);
    return target >= start && target <= end ? pos : -1;
}

bool history_seek(struct MPContext *mpctx, double *pts, int step,
                  double *resume_pts)
{
    struct playback_history *h = mpctx->history;
    if (!h || !history_active(mpctx))
        return false;

    struct history_range *r = NULL;
    int pos = -1;
    for (int n = h->num_ranges - 1; n >= 0 && pos < 0; n--) {
        r = h->ranges[n];
        pos = find_target(mpctx, r, *pts, step);
    }
    if (pos < 0)
        return false;

    bool resume = r != h->cur || h->gap;
    if (r != h->cur) {
        // This can only remove the last range, which is not r.
        history_end_range(mpctx);
        for (int n = 0; n < h->num_ranges; n++) {
            if (h->ranges[n] == r) {
                MP_TARRAY_REMOVE_AT(h->ranges, h->num_ranges, n);
                break;
            }
        }
        MP_TARRAY_APPEND(h, h->ranges, h->num_ranges, r);
        h->cur = r;
    }
    h->gap = false;

    struct history_queue *v = &r->q[0], *a = &r->q[1];
    double target = queue_pts(v, pos);
    *resume_pts = resume ? queue_pts(v, v->num - 1) : MP_NOPTS_VALUE;
    h->readers[0].pos = pos;
    if (mpctx->ao_chain) {
        int apos = 0;
        while (apos < a->num && mp_aframe_end_pts(a->frames[apos].data) <=
                                target - mpctx->opts->audio_delay)
            apos++;
        h->readers[1].pos = apos;
        if (resume) {
            *resume_pts = MPMIN(*resume_pts,
                                mp_aframe_end_pts(a->frames[a->num - 1].data));
        }
    }
    for (int n = 0; n < 2; n++) {
        struct history_reader *rd = &h->readers[n];
        // A previous seek into the same range can still be skipping.
        rd->skip = rd->pos >= 0 && (resume || rd->skip);
        rd->eof &= !resume;
    }
    MP_VERBOSE(mpctx, "seeking in video history: %f -> %f\n", *pts, target);
    *pts = target;
    return true;
}

void history_get_state(struct MPContext *mpctx, struct mpv_node *node)
{
    struct playback_history *h = mpctx->history;
    const char *reason = inactive_reason(mpctx);
    node_init(node, MPV_FORMAT_NODE_MAP, NULL);
    node_map_add_int64(node, "bytes", h ? h->bytes : 0);
    bool ready = false;
    struct mpv_node *ranges = node_map_add(node, "ranges", MPV_FORMAT_NODE_ARRAY);
    for (int n = 0; h && !reason && n < h->num_ranges; n++) {
        double start, end;
        if (range_bounds(mpctx, h->ranges[n], &start, &end)) {
            struct mpv_node *range = node_array_add(ranges, MPV_FORMAT_NODE_MAP);
            node_map_add_double(range, "start", start);
            node_map_add_double(range, "end", end);
            ready = true;
        }
    }
    node_map_add_string(node, "status", reason ? reason : ready ? "ready" : "filling");
}
