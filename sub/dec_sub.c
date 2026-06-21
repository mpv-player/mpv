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

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <limits.h>

#include "demux/demux.h"
#include "demux/packet_pool.h"
#include "sd.h"
#include "dec_sub.h"
#include "options/m_config.h"
#include "options/options.h"
#include "common/global.h"
#include "common/msg.h"
#include "common/recorder.h"
#include "misc/dispatch.h"
#include "osdep/threads.h"
#include "osdep/timer.h"

extern const struct sd_functions sd_ass;
extern const struct sd_functions sd_lavc;
#if HAVE_SUBRANDR
extern const struct sd_functions sd_sbr;
#endif

static const struct sd_functions *const sd_list[] = {
    &sd_lavc,
#if HAVE_SUBRANDR
    &sd_sbr,
#endif
    &sd_ass,
    NULL
};

// --- Render-ahead (see --sub-render-ahead-frames) -------------------------
// A worker thread renders subtitle bitmaps for upcoming video PTSes into a
// ring, so the VO display path serves a pre-rendered frame instead of calling
// libass synchronously. This hides occasional expensive frames (which would
// otherwise stall the display and drop a frame) as long as rendering keeps up
// on average. Entries are keyed by the raw (pre-delay) video PTS so the VO
// fast path needs no pts_to_subtitle (delay/speed changes flush the ring).
//
// Locking: `ahead.lock` protects the ring and the shared cursor/geometry; the
// VO fast path takes only this lock (never blocking on a render). The actual
// libass render holds the decoder lock `sub->lock`. The worker holds at most
// one of the two at a time (never both), so there is no lock-ordering cycle.
// Rendering happens in increasing PTS order on the single worker, so libass's
// change-detection / change_id stay correct.
struct sub_ahead_entry {
    bool valid;
    double video_pts;
    struct mp_osd_res dim;
    int format;
    uint64_t gen;
    struct sub_bitmaps *bmp;     // owned; NULL means "no subtitles at this pts"
};

struct sub_ahead {
    bool worker_running;
    mp_thread thread;
    mp_mutex lock;
    mp_cond wakeup;
    bool terminate;

    int depth;                   // lookahead frames (0 = feature off)
    double video_fps;            // for the frame interval
    double vo_pts;               // latest raw video pts the VO asked for
    struct mp_osd_res cur_dim;
    int cur_format;
    uint64_t gen;                // bumped on any invalidation

    struct sub_ahead_entry *ring;
    int ring_len;
};

struct dec_sub {
    mp_mutex lock;

    struct mp_log *log;
    struct mpv_global *global;
    struct demux_packet_pool *packet_pool;
    struct mp_subtitle_opts *opts;
    struct mp_subtitle_shared_opts *shared_opts;
    struct m_config_cache *opts_cache;
    struct m_config_cache *shared_opts_cache;

    struct mp_recorder_sink *recorder_sink;

    struct attachment_list *attachments;

    struct sh_stream *sh;
    int play_dir;
    int order;
    double last_pkt_pts;
    bool preload_attempted;
    double video_fps;
    double sub_speed;
    bool sub_visible;

    struct mp_codec_params *codec;
    double start, end;
    char *lang;

    double last_vo_pts;
    struct sd *sd;

    struct sub_ahead ahead;

    struct demux_packet *new_segment;
    struct demux_packet **cached_pkts;
    int cached_pkt_pos;
    int num_cached_pkts;
};

static void update_subtitle_speed(struct dec_sub *sub)
{
    struct mp_subtitle_opts *opts = sub->opts;
    sub->sub_speed = 1.0;

    if (sub->video_fps > 0 && sub->codec->frame_based > 0) {
        MP_VERBOSE(sub, "Frame based format, dummy FPS: %f, video FPS: %f\n",
                   sub->codec->frame_based, sub->video_fps);
        sub->sub_speed *= sub->codec->frame_based / sub->video_fps;
    }

    if (opts->sub_fps && sub->video_fps)
        sub->sub_speed *= opts->sub_fps / sub->video_fps;

    sub->sub_speed *= opts->sub_speed;
}

// Return the subtitle PTS used for a given video PTS.
static double pts_to_subtitle(struct dec_sub *sub, double pts)
{
    struct mp_subtitle_shared_opts *opts = sub->shared_opts;
    float delay = sub->order < 0 ? 0.0f : opts->sub_delay[sub->order];

    if (pts != MP_NOPTS_VALUE)
        pts = (pts * sub->play_dir - delay) / sub->sub_speed;

    return pts;
}

static double pts_from_subtitle(struct dec_sub *sub, double pts)
{
    struct mp_subtitle_shared_opts *opts = sub->shared_opts;
    float delay = sub->order < 0 ? 0.0f : opts->sub_delay[sub->order];

    if (pts != MP_NOPTS_VALUE)
        pts = (pts * sub->sub_speed + delay) * sub->play_dir;

    return pts;
}

// Render bitmaps for an (already subtitle-space) pts. Decoder lock held.
static struct sub_bitmaps *get_bitmaps_locked(struct dec_sub *sub,
                                              struct mp_osd_res dim,
                                              int format, double sub_pts)
{
    if (sub->end != MP_NOPTS_VALUE && sub_pts >= sub->end)
        return NULL;
    if (!sub->sd->driver->get_bitmaps)
        return NULL;
    return sub->sd->driver->get_bitmaps(sub->sd, dim, format, sub_pts);
}

// --- render-ahead ring helpers (ahead.lock held) --------------------------

static void ahead_clear(struct sub_ahead *a)
{
    for (int n = 0; n < a->ring_len; n++) {
        if (a->ring[n].valid) {
            talloc_free(a->ring[n].bmp);
            a->ring[n] = (struct sub_ahead_entry){0};
        }
    }
}

// Index of a current-generation entry for video pts V at the given geometry,
// or -1. V is matched within half a frame interval.
static int ahead_find(struct sub_ahead *a, double V, struct mp_osd_res dim,
                      int format)
{
    double tol = (a->video_fps > 0 ? 1.0 / a->video_fps : 1.0 / 24.0) * 0.5;
    for (int n = 0; n < a->ring_len; n++) {
        struct sub_ahead_entry *e = &a->ring[n];
        if (e->valid && e->gen == a->gen && e->format == format &&
            osd_res_equals(e->dim, dim) && fabs(e->video_pts - V) < tol)
            return n;
    }
    return -1;
}

// Store a freshly rendered frame, taking ownership of bmp. Reuses a free slot,
// else evicts the entry with the oldest (smallest) video pts.
static void ahead_store(struct sub_ahead *a, double V, struct mp_osd_res dim,
                        int format, uint64_t gen, struct sub_bitmaps *bmp)
{
    int slot = -1;
    double oldest = INFINITY;
    for (int n = 0; n < a->ring_len; n++) {
        if (!a->ring[n].valid) {
            slot = n;
            break;
        }
        if (a->ring[n].video_pts < oldest) {
            oldest = a->ring[n].video_pts;
            slot = n;
        }
    }
    if (a->ring[slot].valid)
        talloc_free(a->ring[slot].bmp);
    a->ring[slot] = (struct sub_ahead_entry){
        .valid = true, .video_pts = V, .dim = dim, .format = format,
        .gen = gen, .bmp = bmp,
    };
}

static MP_THREAD_VOID sub_ahead_thread(void *ptr)
{
    struct dec_sub *sub = ptr;
    struct sub_ahead *a = &sub->ahead;
    mp_thread_set_name("sub/ahead");

    mp_mutex_lock(&a->lock);
    while (!a->terminate) {
        double vo = a->vo_pts;
        int depth = a->depth;
        if (depth <= 0 || vo == MP_NOPTS_VALUE) {
            mp_cond_wait(&a->wakeup, &a->lock);
            continue;
        }
        struct mp_osd_res dim = a->cur_dim;
        int format = a->cur_format;
        uint64_t gen = a->gen;
        double interval = a->video_fps > 0 ? 1.0 / a->video_fps : 1.0 / 24.0;

        // Find the nearest upcoming frame not yet in the ring.
        double target = MP_NOPTS_VALUE;
        for (int i = 0; i <= depth; i++) {
            double V = vo + i * interval;
            if (ahead_find(a, V, dim, format) < 0) {
                target = V;
                break;
            }
        }
        if (target == MP_NOPTS_VALUE) {
            // Window full; wait for the VO to advance (or a flush).
            mp_cond_timedwait(&a->wakeup, &a->lock, MP_TIME_MS_TO_NS(50));
            continue;
        }
        bool is_current = target == vo;
        mp_mutex_unlock(&a->lock);

        // Render holding the decoder lock; the VO never takes it on its fast
        // path, so it is not blocked by this.
        mp_mutex_lock(&sub->lock);
        double sub_pts = pts_to_subtitle(sub, target);
        // Don't render ahead past decoded packets (events would be missing);
        // the current frame is always renderable (the VO is showing it).
        bool decoded = is_current || sub->last_pkt_pts == MP_NOPTS_VALUE ||
                       sub_pts <= sub->last_pkt_pts;
        // Don't render ahead across a pending segment switch (wrong decoder).
        if (sub->new_segment && sub_pts >= sub->new_segment->start)
            decoded = false;
        struct sub_bitmaps *bmp = NULL;
        if (decoded)
            bmp = get_bitmaps_locked(sub, dim, format, sub_pts);
        mp_mutex_unlock(&sub->lock);

        mp_mutex_lock(&a->lock);
        if (decoded && gen == a->gen && format == a->cur_format &&
            osd_res_equals(dim, a->cur_dim))
        {
            ahead_store(a, target, dim, format, gen, bmp);
        } else {
            talloc_free(bmp);   // stale (flush/resize during render) or not ready
        }
    }
    mp_mutex_unlock(&a->lock);
    MP_THREAD_RETURN();
}

// (Re)configure the worker from current options. Called with no locks held.
static void sub_ahead_configure(struct dec_sub *sub)
{
    struct sub_ahead *a = &sub->ahead;
    bool driver_ok = sub->sd && sub->sd->driver == &sd_ass;
    int depth = driver_ok ? sub->opts->sub_render_ahead_frames : 0;

    mp_mutex_lock(&a->lock);
    a->depth = depth;
    a->video_fps = sub->video_fps;
    a->gen++;
    ahead_clear(a);
    a->vo_pts = MP_NOPTS_VALUE;
    int want_len = depth > 0 ? depth + 2 : 0;
    if (want_len != a->ring_len) {
        ahead_clear(a);
        a->ring = talloc_realloc(sub, a->ring, struct sub_ahead_entry, want_len);
        for (int n = a->ring_len; n < want_len; n++)
            a->ring[n] = (struct sub_ahead_entry){0};
        a->ring_len = want_len;
    }
    bool spawn = depth > 0 && !a->worker_running;
    if (spawn)
        a->worker_running = mp_thread_create(&a->thread, sub_ahead_thread, sub) == 0;
    mp_cond_signal(&a->wakeup);
    mp_mutex_unlock(&a->lock);
}

static void sub_ahead_stop(struct dec_sub *sub)
{
    struct sub_ahead *a = &sub->ahead;
    if (a->worker_running) {
        mp_mutex_lock(&a->lock);
        a->terminate = true;
        mp_cond_signal(&a->wakeup);
        mp_mutex_unlock(&a->lock);
        mp_thread_join(a->thread);
        a->worker_running = false;
    }
    ahead_clear(a);
}

static void wakeup_demux(void *ctx)
{
    struct mp_dispatch_queue *q = ctx;
    mp_dispatch_interrupt(q);
}

static void destroy_cached_pkts(struct dec_sub *sub)
{
    int index = 0;
    while (index < sub->num_cached_pkts) {
        demux_packet_pool_push(sub->packet_pool, sub->cached_pkts[index]);
        sub->cached_pkts[index] = NULL;
        ++index;
    }
    sub->cached_pkt_pos = 0;
    sub->num_cached_pkts = 0;
}

void sub_destroy(struct dec_sub *sub)
{
    if (!sub)
        return;
    demux_set_stream_wakeup_cb(sub->sh, NULL, NULL);
    sub_ahead_stop(sub);   // join the worker before touching the decoder
    if (sub->sd) {
        sub_reset(sub);
        sub->sd->driver->uninit(sub->sd);
    }
    talloc_free(sub->sd);
    mp_cond_destroy(&sub->ahead.wakeup);
    mp_mutex_destroy(&sub->ahead.lock);
    mp_mutex_destroy(&sub->lock);
    talloc_free(sub);
}

static struct sd *init_decoder(struct dec_sub *sub)
{
    for (int n = 0; sd_list[n]; n++) {
        const struct sd_functions *driver = sd_list[n];
        struct sd *sd = talloc(NULL, struct sd);
        *sd = (struct sd){
            .global = sub->global,
            .log = mp_log_new(sd, sub->log, driver->name),
            .opts = sub->opts,
            .shared_opts = sub->shared_opts,
            .driver = driver,
            .order = sub->order,
            .attachments = sub->attachments,
            .codec = sub->codec,
            .lang = sub->lang,
            .preload_ok = true,
        };

        if (sd->driver->init(sd) >= 0)
            return sd;

        talloc_free(sd);
    }

    MP_ERR(sub, "Could not find subtitle decoder for format '%s'.\n",
           sub->codec->codec);
    return NULL;
}

// Thread-safety of the returned object: all functions are thread-safe,
// except sub_get_bitmaps() and sub_get_text(). Decoder backends (sd_*)
// do not need to acquire locks.
// Ownership of attachments goes to the callee, and is released with
// talloc_free() (even on failure).
struct dec_sub *sub_create(struct mpv_global *global, struct track *track,
                           struct attachment_list *attachments, int order)
{
    mp_assert(track->stream && track->stream->type == STREAM_SUB);

    struct dec_sub *sub = talloc(NULL, struct dec_sub);
    *sub = (struct dec_sub){
        .log = mp_log_new(sub, global->log, "sub"),
        .global = global,
        .packet_pool = demux_packet_pool_get(global),
        .opts_cache = m_config_cache_alloc(sub, global, &mp_subtitle_sub_opts),
        .shared_opts_cache = m_config_cache_alloc(sub, global, &mp_subtitle_shared_sub_opts),
        .sh = track->stream,
        .codec = track->stream->codec,
        .lang = track->lang,
        .attachments = talloc_steal(sub, attachments),
        .play_dir = 1,
        .order = order,
        .last_pkt_pts = MP_NOPTS_VALUE,
        .last_vo_pts = MP_NOPTS_VALUE,
        .start = MP_NOPTS_VALUE,
        .end = MP_NOPTS_VALUE,
    };
    sub->opts = sub->opts_cache->opts;
    sub->shared_opts = sub->shared_opts_cache->opts;
    mp_mutex_init(&sub->lock);
    mp_mutex_init(&sub->ahead.lock);
    mp_cond_init(&sub->ahead.wakeup);

    sub->sd = init_decoder(sub);
    if (sub->sd) {
        update_subtitle_speed(sub);
        sub_ahead_configure(sub);
        return sub;
    }

    sub_destroy(sub);
    return NULL;
}

// Called locked.
static void update_segment(struct dec_sub *sub)
{
    if (sub->new_segment && sub->last_vo_pts != MP_NOPTS_VALUE &&
        sub->last_vo_pts >= sub->new_segment->start)
    {
        MP_VERBOSE(sub, "Switch segment: %f at %f\n", sub->new_segment->start,
                   sub->last_vo_pts);

        sub->codec = sub->new_segment->codec;
        sub->start = sub->new_segment->start;
        sub->end = sub->new_segment->end;
        struct sd *new = init_decoder(sub);
        if (new) {
            sub->sd->driver->uninit(sub->sd);
            talloc_free(sub->sd);
            sub->sd = new;
            update_subtitle_speed(sub);
        } else {
            // We'll just keep the current decoder, and feed it possibly
            // invalid data (not our fault if it crashes or something).
            MP_ERR(sub, "Can't change to new codec.\n");
        }
        sub->sd->driver->decode(sub->sd, sub->new_segment);
        talloc_free(sub->new_segment);
        sub->new_segment = NULL;
    }
}

bool sub_can_preload(struct dec_sub *sub)
{
    bool r;
    mp_mutex_lock(&sub->lock);
    r = sub->sd->driver->accept_packets_in_advance && !sub->preload_attempted;
    mp_mutex_unlock(&sub->lock);
    return r;
}

void sub_preload(struct dec_sub *sub)
{
    mp_mutex_lock(&sub->lock);

    struct mp_dispatch_queue *demux_waiter = mp_dispatch_create(NULL);
    demux_set_stream_wakeup_cb(sub->sh, wakeup_demux, demux_waiter);

    sub->preload_attempted = true;

    for (;;) {
        struct demux_packet *pkt = NULL;
        int r = demux_read_packet_async(sub->sh, &pkt);
        if (r == 0) {
            mp_dispatch_queue_process(demux_waiter, INFINITY);
            continue;
        }
        if (!pkt)
            break;
        sub->sd->driver->decode(sub->sd, pkt);
        MP_TARRAY_APPEND(sub, sub->cached_pkts, sub->num_cached_pkts, pkt);
    }

    demux_set_stream_wakeup_cb(sub->sh, NULL, NULL);
    talloc_free(demux_waiter);

    mp_mutex_unlock(&sub->lock);
}

static bool is_new_segment(struct dec_sub *sub, struct demux_packet *p)
{
    return p->segmented &&
        (p->start != sub->start || p->end != sub->end || p->codec != sub->codec);
}

static bool is_packet_visible(struct demux_packet *p, double video_pts)
{
    return p && p->pts <= video_pts && (video_pts <= p->pts + p->sub_duration ||
           p->sub_duration < 0);
}

static bool update_pkt_cache(struct dec_sub *sub, double video_pts)
{
    if (!sub->cached_pkts[sub->cached_pkt_pos])
        return false;

    struct demux_packet *pkt = sub->cached_pkts[sub->cached_pkt_pos];
    struct demux_packet *next_pkt = sub->cached_pkt_pos + 1 < sub->num_cached_pkts ?
                                    sub->cached_pkts[sub->cached_pkt_pos + 1] : NULL;
    if (!pkt)
        return false;

    double pts = video_pts + sub->shared_opts->sub_delay[sub->order];
    double next_pts = next_pkt ? next_pkt->pts : INT_MAX;
    double end_pts = pkt->sub_duration >= 0 ? pkt->pts + pkt->sub_duration : INT_MAX;

    if (next_pts < pts || end_pts < pts) {
        if (sub->cached_pkt_pos + 1 < sub->num_cached_pkts) {
            TA_FREEP(&sub->cached_pkts[sub->cached_pkt_pos]);
            pkt = NULL;
            sub->cached_pkt_pos++;
        }
        if (next_pts < pts)
            return true;
    }

    if (pkt && pkt->animated == 1)
        return true;

    return false;
}

// Read packets from the demuxer stream passed to sub_create(). Signals if
// enough packets were read and if the subtitle state updated in anyway. If
// packets_read is false, the player should wait until the demuxer signals new
// packets and retry.
void sub_read_packets(struct dec_sub *sub, double video_pts, bool force,
                      bool *packets_read, bool *sub_updated)
{
    *packets_read = true;
    mp_mutex_lock(&sub->lock);
    video_pts = pts_to_subtitle(sub, video_pts);
    while (1) {
        bool read_more = true;
        if (sub->sd->driver->accepts_packet)
            read_more = sub->sd->driver->accepts_packet(sub->sd, video_pts);

        if (!read_more)
            break;

        if (sub->new_segment && sub->new_segment->start < video_pts) {
            sub->last_vo_pts = video_pts;
            update_segment(sub);
        }

        if (sub->new_segment)
            break;

        // (Use this mechanism only if sub_delay matters to avoid corner cases.)
        float delay = sub->order < 0 ? 0.0f : sub->shared_opts->sub_delay[sub->order];
        double min_pts = delay < 0 || force ? video_pts : MP_NOPTS_VALUE;

        struct demux_packet *pkt;
        int st = demux_read_packet_async_until(sub->sh, min_pts, &pkt);
        // Note: "wait" (st==0) happens with non-interleaved streams only, and
        // then we should stop the playloop until a new enough packet has been
        // seen (or the subtitle decoder's queue is full). This usually does not
        // happen for interleaved subtitle streams, which never return "wait"
        // when reading, unless min_pts is set.
        if (st <= 0) {
            *packets_read = st < 0 || (sub->last_pkt_pts != MP_NOPTS_VALUE &&
                                       sub->last_pkt_pts > video_pts);
            break;
        }

        if (sub->recorder_sink)
            mp_recorder_feed_packet(sub->recorder_sink, pkt);

        sub->last_pkt_pts = pkt->pts;
        MP_TARRAY_APPEND(sub, sub->cached_pkts, sub->num_cached_pkts, pkt);

        if (is_new_segment(sub, pkt)) {
            sub->new_segment = demux_copy_packet(sub->packet_pool, pkt);
            // Note that this can be delayed to a much later point in time.
            update_segment(sub);
            break;
        }

        if (!(sub->preload_attempted && sub->sd->preload_ok))
            sub->sd->driver->decode(sub->sd, pkt);
    }
    if (sub->cached_pkts && sub->num_cached_pkts) {
        bool visible = is_packet_visible(sub->cached_pkts[sub->cached_pkt_pos], video_pts);
        *sub_updated = update_pkt_cache(sub, video_pts) || sub->sub_visible != visible;
        sub->sub_visible = visible;
    }
    mp_mutex_unlock(&sub->lock);
}

// Redecode all cached packets if needed.
// Used with UPDATE_SUB_HARD and UPDATE_SUB_FILT.
void sub_redecode_cached_packets(struct dec_sub *sub)
{
    mp_mutex_lock(&sub->lock);
    int index = sub->cached_pkt_pos;
    while (index < sub->num_cached_pkts) {
        sub->sd->driver->decode(sub->sd, sub->cached_pkts[index]);
        ++index;
    }
    mp_mutex_unlock(&sub->lock);
}

// Unref sub_bitmaps.rc to free the result. May return NULL.
struct sub_bitmaps *sub_get_bitmaps(struct dec_sub *sub, struct mp_osd_res dim,
                                    int format, double pts)
{
    struct sub_ahead *a = &sub->ahead;

    // Render-ahead fast path: serve a pre-rendered frame from the worker. Only
    // takes the ring lock, so the display is never blocked on a render.
    mp_mutex_lock(&a->lock);
    if (a->depth > 0 && a->worker_running) {
        a->vo_pts = pts;
        if (a->cur_format != format || !osd_res_equals(a->cur_dim, dim)) {
            a->cur_dim = dim;
            a->cur_format = format;
            a->gen++;
            ahead_clear(a);   // entries were rendered for the old geometry
        }
        int idx = ahead_find(a, pts, dim, format);
        struct sub_bitmaps *res = idx >= 0 && a->ring[idx].bmp ?
                                  sub_bitmaps_copy(NULL, a->ring[idx].bmp) : NULL;
        mp_mutex_unlock(&a->lock);
        mp_cond_signal(&a->wakeup);   // nudge the worker to refill ahead of us
        return res;
    }
    mp_mutex_unlock(&a->lock);

    mp_mutex_lock(&sub->lock);

    pts = pts_to_subtitle(sub, pts);

    sub->last_vo_pts = pts;
    update_segment(sub);

    struct sub_bitmaps *res = get_bitmaps_locked(sub, dim, format, pts);

    mp_mutex_unlock(&sub->lock);
    return res;
}

// The returned string is talloc'ed.
char *sub_get_text(struct dec_sub *sub, double pts, enum sd_text_type type)
{
    mp_mutex_lock(&sub->lock);
    char *text = NULL;

    pts = pts_to_subtitle(sub, pts);

    sub->last_vo_pts = pts;
    update_segment(sub);

    if (sub->sd->driver->get_text)
        text = sub->sd->driver->get_text(sub->sd, pts, type);
    mp_mutex_unlock(&sub->lock);
    return text;
}

char *sub_ass_get_extradata(struct dec_sub *sub)
{
    char *data = NULL;
    mp_mutex_lock(&sub->lock);
    if (strcmp(sub->sd->codec->codec, "ass") != 0)
        goto done;
    char *extradata = sub->sd->codec->extradata;
    int extradata_size = sub->sd->codec->extradata_size;
    data = talloc_strndup(NULL, extradata, extradata_size);
done:
    mp_mutex_unlock(&sub->lock);
    return data;
}

struct sd_times sub_get_times(struct dec_sub *sub, double pts)
{
    mp_mutex_lock(&sub->lock);
    struct sd_times res = { .start = MP_NOPTS_VALUE, .end = MP_NOPTS_VALUE };

    pts = pts_to_subtitle(sub, pts);

    sub->last_vo_pts = pts;
    update_segment(sub);

    if (sub->sd->driver->get_times)
        res = sub->sd->driver->get_times(sub->sd, pts);

    mp_mutex_unlock(&sub->lock);
    return res;
}

void sub_reset(struct dec_sub *sub)
{
    mp_mutex_lock(&sub->lock);
    if (sub->sd->driver->reset)
        sub->sd->driver->reset(sub->sd);
    sub->last_pkt_pts = MP_NOPTS_VALUE;
    sub->last_vo_pts = MP_NOPTS_VALUE;
    destroy_cached_pkts(sub);
    demux_packet_pool_push(sub->packet_pool, sub->new_segment);
    sub->new_segment = NULL;
    mp_mutex_unlock(&sub->lock);

    // Drop pre-rendered frames (seek / track switch / clear-on-seek).
    mp_mutex_lock(&sub->ahead.lock);
    sub->ahead.gen++;
    ahead_clear(&sub->ahead);
    sub->ahead.vo_pts = MP_NOPTS_VALUE;
    mp_mutex_unlock(&sub->ahead.lock);
}

void sub_select(struct dec_sub *sub, bool selected)
{
    mp_mutex_lock(&sub->lock);
    if (sub->sd->driver->select)
        sub->sd->driver->select(sub->sd, selected);
    mp_mutex_unlock(&sub->lock);
}

int sub_control(struct dec_sub *sub, enum sd_ctrl cmd, void *arg)
{
    int r = CONTROL_UNKNOWN;
    mp_mutex_lock(&sub->lock);
    bool propagate = false;
    bool reconfigure_ahead = false;
    switch (cmd) {
    case SD_CTRL_SET_VIDEO_DEF_FPS:
        sub->video_fps = *(double *)arg;
        update_subtitle_speed(sub);
        reconfigure_ahead = true;   // frame interval + pts mapping changed
        break;
    case SD_CTRL_SUB_STEP: {
        double *a = arg;
        double arg2[2] = {a[0], a[1]};
        arg2[0] = pts_to_subtitle(sub, arg2[0]);
        if (sub->sd->driver->control)
            r = sub->sd->driver->control(sub->sd, cmd, arg2);
        if (r == CONTROL_OK)
            a[0] = pts_from_subtitle(sub, arg2[0]);
        break;
    }
    case SD_CTRL_UPDATE_OPTS: {
        uint64_t flags = *(uint64_t *)arg;
        if (m_config_cache_update(sub->opts_cache))
            update_subtitle_speed(sub);
        m_config_cache_update(sub->shared_opts_cache);
        propagate = true;
        if (flags & UPDATE_SUB_HARD) {
            // forget about the previous preload because
            // UPDATE_SUB_HARD will cause a sub reinit
            // that clears all preloaded sub packets
            sub->preload_attempted = false;
        }
        reconfigure_ahead = true;   // depth / delay / speed may have changed
        break;
    }
    default:
        propagate = true;
    }
    if (propagate && sub->sd->driver->control)
        r = sub->sd->driver->control(sub->sd, cmd, arg);
    // Re-evaluate the render-ahead worker (depth, fps, pts mapping). Holds the
    // ahead lock; the decoder lock is held here, matching sub_reset's order.
    if (reconfigure_ahead)
        sub_ahead_configure(sub);
    mp_mutex_unlock(&sub->lock);
    return r;
}

void sub_set_recorder_sink(struct dec_sub *sub, struct mp_recorder_sink *sink)
{
    mp_mutex_lock(&sub->lock);
    sub->recorder_sink = sink;
    mp_mutex_unlock(&sub->lock);
}

void sub_set_play_dir(struct dec_sub *sub, int dir)
{
    mp_mutex_lock(&sub->lock);
    sub->play_dir = dir;
    mp_mutex_unlock(&sub->lock);
}

bool sub_is_primary_visible(struct dec_sub *sub)
{
    mp_mutex_lock(&sub->lock);
    bool ret = sub->shared_opts->sub_visibility[0];
    mp_mutex_unlock(&sub->lock);
    return ret;
}

bool sub_is_secondary_visible(struct dec_sub *sub)
{
    mp_mutex_lock(&sub->lock);
    bool ret = sub->shared_opts->sub_visibility[1];
    mp_mutex_unlock(&sub->lock);
    return ret;
}

static int sub_line_cmp(const void *a, const void *b)
{
    const struct sub_line *la = a, *lb = b;
    if (la->start < lb->start) return -1;
    if (la->start > lb->start) return  1;
    return 0;
}

static void dedup_sub_lines(struct sub_lines *lines)
{
    int window_start = 0;
    int window_end = 1;
    int current_shift = 0;

    while (window_end < lines->num_entries) {
        struct sub_line next = lines->entries[window_end + current_shift];
        for (int i = window_start; i < window_end; ++i) {
            if (lines->entries[i].end < next.start) {
                struct sub_line tmp = lines->entries[window_start];
                lines->entries[window_start++] = lines->entries[i];
                lines->entries[i] = tmp;
                continue;
            }

            if (!strcmp(lines->entries[i].text, next.text)) {
                lines->entries[i].end = MPMAX(next.end, lines->entries[i].end);
                TA_FREEP(&next.text);
                ++current_shift, --lines->num_entries;
                goto skip;
            }
        }

        lines->entries[window_end++] = next;

    skip:;
    }
}

struct sub_lines *sub_get_lines(struct dec_sub *sub)
{
    mp_mutex_lock(&sub->lock);
    struct sub_lines *res = NULL;
    if (sub->sd->driver->get_lines) {
        res = sub->sd->driver->get_lines(sub->sd);
        qsort(res->entries, res->num_entries, sizeof(res->entries[0]),
              sub_line_cmp);
        dedup_sub_lines(res);
        // dedup may sometimes reorder lines to keep its window smaller
        qsort(res->entries, res->num_entries, sizeof(res->entries[0]),
              sub_line_cmp);
    }
    mp_mutex_unlock(&sub->lock);
    return res;
}
