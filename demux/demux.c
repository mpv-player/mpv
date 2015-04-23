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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <pthread.h>

#include <math.h>

#include <sys/types.h>
#include <sys/stat.h>

#include "config.h"
#include "options/options.h"
#include "talloc.h"
#include "common/msg.h"
#include "common/global.h"
#include "osdep/threads.h"

#include "stream/stream.h"
#include "demux.h"
#include "stheader.h"

#include "audio/format.h"

// Demuxer list
extern const struct demuxer_desc demuxer_desc_edl;
extern const struct demuxer_desc demuxer_desc_cue;
extern const demuxer_desc_t demuxer_desc_rawaudio;
extern const demuxer_desc_t demuxer_desc_rawvideo;
extern const demuxer_desc_t demuxer_desc_tv;
extern const demuxer_desc_t demuxer_desc_mf;
extern const demuxer_desc_t demuxer_desc_matroska;
extern const demuxer_desc_t demuxer_desc_lavf;
extern const demuxer_desc_t demuxer_desc_libass;
extern const demuxer_desc_t demuxer_desc_subreader;
extern const demuxer_desc_t demuxer_desc_playlist;
extern const demuxer_desc_t demuxer_desc_disc;
extern const demuxer_desc_t demuxer_desc_rar;

/* Please do not add any new demuxers here. If you want to implement a new
 * demuxer, add it to libavformat, except for wrappers around external
 * libraries and demuxers requiring binary support. */

const demuxer_desc_t *const demuxer_list[] = {
    &demuxer_desc_disc,
    &demuxer_desc_edl,
    &demuxer_desc_cue,
    &demuxer_desc_rawaudio,
    &demuxer_desc_rawvideo,
#if HAVE_TV
    &demuxer_desc_tv,
#endif
#if HAVE_LIBASS
    &demuxer_desc_libass,
#endif
    &demuxer_desc_matroska,
    &demuxer_desc_rar,
    &demuxer_desc_lavf,
    &demuxer_desc_mf,
    &demuxer_desc_playlist,
    // Pretty aggressive, so should be last.
    &demuxer_desc_subreader,
    NULL
};

struct demux_internal {
    struct mp_log *log;

    // The demuxer runs potentially in another thread, so we keep two demuxer
    // structs; the real demuxer can access the shadow struct only.
    // Since demuxer and user threads both don't use locks, a third demuxer
    // struct d_buffer is used to copy data between them in a synchronized way.
    struct demuxer *d_thread;   // accessed by demuxer impl. (producer)
    struct demuxer *d_user;     // accessed by player (consumer)
    struct demuxer *d_buffer;   // protected by lock; used to sync d_user/thread

    // The lock protects the packet queues (struct demux_stream), d_buffer,
    // and some minor fields like thread_paused.
    pthread_mutex_t lock;
    pthread_cond_t wakeup;
    pthread_t thread;

    // -- All the following fields are protected by lock.

    bool thread_paused;
    int thread_request_pause;   // counter, if >0, make demuxer thread pause
    bool thread_terminate;
    bool threading;
    void (*wakeup_cb)(void *ctx);
    void *wakeup_cb_ctx;

    bool warned_queue_overflow;
    bool last_eof;              // last actual global EOF status
    bool eof;                   // whether we're in EOF state (reset for retry)
    bool idle;
    bool autoselect;
    double min_secs;
    int min_packs;
    int min_bytes;

    bool tracks_switched;       // thread needs to inform demuxer of this

    bool seeking;               // there's a seek queued
    int seek_flags;             // flags for next seek (if seeking==true)
    double seek_pts;

    bool refresh_seeks_enabled;
    bool start_refresh_seek;

    // Cached state.
    bool force_cache_update;
    double time_length;
    struct mp_nav_event *nav_event;
    struct mp_tags *stream_metadata;
    int64_t stream_size;
    int64_t stream_cache_size;
    int64_t stream_cache_fill;
    int stream_cache_idle;
    // Updated during init only.
    char *stream_base_filename;
};

struct demux_stream {
    struct demux_internal *in;
    enum stream_type type;
    // all fields are protected by in->lock
    bool selected;          // user wants packets from this stream
    bool active;            // try to keep at least 1 packet queued
                            // if false, this stream is disabled, or passively
                            // read (like subtitles)
    bool eof;               // end of demuxed stream? (true if all buffer empty)
    bool refreshing;
    size_t packs;           // number of packets in buffer
    size_t bytes;           // total bytes of packets in buffer
    double base_ts;         // timestamp of the last packet returned to decoder
    double last_ts;         // timestamp of the last packet added to queue
    double last_br_ts;      // timestamp of last packet bitrate was calculated
    size_t last_br_bytes;   // summed packet sizes since last bitrate calculation
    double bitrate;
    int64_t last_pos;
    struct demux_packet *head;
    struct demux_packet *tail;
};

// Return "a", or if that is NOPTS, return "def".
#define PTS_OR_DEF(a, def) ((a) == MP_NOPTS_VALUE ? (def) : (a))
// If one of the values is NOPTS, always pick the other one.
#define MP_PTS_MIN(a, b) MPMIN(PTS_OR_DEF(a, b), PTS_OR_DEF(b, a))
#define MP_PTS_MAX(a, b) MPMAX(PTS_OR_DEF(a, b), PTS_OR_DEF(b, a))

static void demuxer_sort_chapters(demuxer_t *demuxer);
static void *demux_thread(void *pctx);
static void update_cache(struct demux_internal *in);

// called locked
static void ds_flush(struct demux_stream *ds)
{
    demux_packet_t *dp = ds->head;
    while (dp) {
        demux_packet_t *dn = dp->next;
        free_demux_packet(dp);
        dp = dn;
    }
    ds->head = ds->tail = NULL;
    ds->packs = 0;
    ds->bytes = 0;
    ds->last_ts = ds->base_ts = ds->last_br_ts = MP_NOPTS_VALUE;
    ds->last_br_bytes = 0;
    ds->bitrate = -1;
    ds->eof = false;
    ds->active = false;
    ds->refreshing = false;
    ds->last_pos = -1;
}

struct sh_stream *new_sh_stream(demuxer_t *demuxer, enum stream_type type)
{
    assert(demuxer == demuxer->in->d_thread);

    if (demuxer->num_streams > MAX_SH_STREAMS) {
        MP_WARN(demuxer, "Too many streams.\n");
        return NULL;
    }

    int demuxer_id = 0;
    for (int n = 0; n < demuxer->num_streams; n++) {
        if (demuxer->streams[n]->type == type)
            demuxer_id++;
    }

    struct sh_stream *sh = talloc_ptrtype(demuxer, sh);
    *sh = (struct sh_stream) {
        .type = type,
        .index = demuxer->num_streams,
        .ff_index = demuxer->num_streams,
        .demuxer_id = demuxer_id, // may be overwritten by demuxer
        .ds = talloc(sh, struct demux_stream),
    };
    *sh->ds = (struct demux_stream) {
        .in = demuxer->in,
        .type = sh->type,
        .selected = demuxer->in->autoselect,
    };
    MP_TARRAY_APPEND(demuxer, demuxer->streams, demuxer->num_streams, sh);
    switch (sh->type) {
    case STREAM_VIDEO: sh->video = talloc_zero(demuxer, struct sh_video); break;
    case STREAM_AUDIO: sh->audio = talloc_zero(demuxer, struct sh_audio); break;
    case STREAM_SUB:   sh->sub = talloc_zero(demuxer, struct sh_sub); break;
    }

    return sh;
}

void free_demuxer(demuxer_t *demuxer)
{
    if (!demuxer)
        return;
    struct demux_internal *in = demuxer->in;
    assert(demuxer == in->d_user);

    demux_stop_thread(demuxer);

    if (demuxer->desc->close)
        demuxer->desc->close(in->d_thread);
    for (int n = 0; n < demuxer->num_streams; n++)
        ds_flush(demuxer->streams[n]->ds);
    pthread_mutex_destroy(&in->lock);
    pthread_cond_destroy(&in->wakeup);
    talloc_free(in->nav_event);
    talloc_free(demuxer);
}

void free_demuxer_and_stream(struct demuxer *demuxer)
{
    if (!demuxer)
        return;
    struct stream *s = demuxer->stream;
    free_demuxer(demuxer);
    free_stream(s);
}

// Start the demuxer thread, which reads ahead packets on its own.
void demux_start_thread(struct demuxer *demuxer)
{
    struct demux_internal *in = demuxer->in;
    assert(demuxer == in->d_user);

    if (!in->threading) {
        in->threading = true;
        if (pthread_create(&in->thread, NULL, demux_thread, in))
            in->threading = false;
    }
}

void demux_stop_thread(struct demuxer *demuxer)
{
    struct demux_internal *in = demuxer->in;
    assert(demuxer == in->d_user);

    if (in->threading) {
        pthread_mutex_lock(&in->lock);
        in->thread_terminate = true;
        pthread_cond_signal(&in->wakeup);
        pthread_mutex_unlock(&in->lock);
        pthread_join(in->thread, NULL);
        in->threading = false;
        in->thread_terminate = false;
    }
}

// The demuxer thread will call cb(ctx) if there's a new packet, or EOF is reached.
void demux_set_wakeup_cb(struct demuxer *demuxer, void (*cb)(void *ctx), void *ctx)
{
    struct demux_internal *in = demuxer->in;
    pthread_mutex_lock(&in->lock);
    in->wakeup_cb = cb;
    in->wakeup_cb_ctx = ctx;
    pthread_mutex_unlock(&in->lock);
}

const char *stream_type_name(enum stream_type type)
{
    switch (type) {
    case STREAM_VIDEO:  return "video";
    case STREAM_AUDIO:  return "audio";
    case STREAM_SUB:    return "sub";
    default:            return "unknown";
    }
}

// Returns the same value as demuxer->fill_buffer: 1 ok, 0 EOF/not selected.
int demux_add_packet(struct sh_stream *stream, demux_packet_t *dp)
{
    struct demux_stream *ds = stream ? stream->ds : NULL;
    if (!dp || !ds) {
        talloc_free(dp);
        return 0;
    }
    struct demux_internal *in = ds->in;
    pthread_mutex_lock(&in->lock);

    bool drop = false;
    if (ds->refreshing) {
        // Resume reading once the old position was reached (i.e. we start
        // returning packets where we left off before the refresh).
        drop = dp->pos <= ds->last_pos;
        if (dp->pos >= ds->last_pos)
            ds->refreshing = false;
    }

    if (!ds->selected || in->seeking || drop) {
        pthread_mutex_unlock(&in->lock);
        talloc_free(dp);
        return 0;
    }

    dp->stream = stream->index;
    dp->next = NULL;

    ds->last_pos = dp->pos;
    ds->packs++;
    ds->bytes += dp->len;
    if (ds->tail) {
        // next packet in stream
        ds->tail->next = dp;
        ds->tail = dp;
    } else {
        // first packet in stream
        ds->head = ds->tail = dp;
    }

    // obviously not true anymore
    ds->eof = false;
    in->last_eof = in->eof = false;

    // For video, PTS determination is not trivial, but for other media types
    // distinguishing PTS and DTS is not useful.
    if (stream->type != STREAM_VIDEO && dp->pts == MP_NOPTS_VALUE)
        dp->pts = dp->dts;

    double ts = dp->dts == MP_NOPTS_VALUE ? dp->pts : dp->dts;
    if (ts != MP_NOPTS_VALUE && (ts > ds->last_ts || ts + 10 < ds->last_ts))
        ds->last_ts = ts;
    if (ds->base_ts == MP_NOPTS_VALUE)
        ds->base_ts = ds->last_ts;

    MP_DBG(in, "append packet to %s: size=%d pts=%f dts=%f pos=%"PRIi64" "
           "[num=%zd size=%zd]\n", stream_type_name(stream->type),
           dp->len, dp->pts, dp->dts, dp->pos, ds->packs, ds->bytes);

    if (ds->in->wakeup_cb && !ds->head->next)
        ds->in->wakeup_cb(ds->in->wakeup_cb_ctx);
    pthread_cond_signal(&in->wakeup);
    pthread_mutex_unlock(&in->lock);
    return 1;
}

// Returns true if there was "progress" (lock was released temporarily).
static bool read_packet(struct demux_internal *in)
{
    in->eof = false;
    in->idle = true;

    // Check if we need to read a new packet. We do this if all queues are below
    // the minimum, or if a stream explicitly needs new packets. Also includes
    // safe-guards against packet queue overflow.
    bool active = false, read_more = false;
    size_t packs = 0, bytes = 0;
    for (int n = 0; n < in->d_buffer->num_streams; n++) {
        struct demux_stream *ds = in->d_buffer->streams[n]->ds;
        active |= ds->active;
        read_more |= ds->active && !ds->head;
        packs += ds->packs;
        bytes += ds->bytes;
        if (ds->active && ds->last_ts != MP_NOPTS_VALUE && in->min_secs > 0)
            read_more |= ds->last_ts - ds->base_ts < in->min_secs;
    }
    MP_DBG(in, "packets=%zd, bytes=%zd, active=%d, more=%d\n",
           packs, bytes, active, read_more);
    if (packs >= MAX_PACKS || bytes >= MAX_PACK_BYTES) {
        if (!in->warned_queue_overflow) {
            in->warned_queue_overflow = true;
            MP_ERR(in, "Too many packets in the demuxer packet queues:\n");
            for (int n = 0; n < in->d_buffer->num_streams; n++) {
                struct demux_stream *ds = in->d_buffer->streams[n]->ds;
                if (ds->selected) {
                    MP_ERR(in, "  %s/%d: %zd packets, %zd bytes\n",
                           stream_type_name(ds->type), n, ds->packs, ds->bytes);
                }
            }
        }
        for (int n = 0; n < in->d_buffer->num_streams; n++) {
            struct demux_stream *ds = in->d_buffer->streams[n]->ds;
            ds->eof |= !ds->head;
        }
        pthread_cond_signal(&in->wakeup);
        return false;
    }
    if (packs < in->min_packs && bytes < in->min_bytes)
        read_more |= active;

    if (!read_more)
        return false;

    // Actually read a packet. Drop the lock while doing so, because waiting
    // for disk or network I/O can take time.
    in->idle = false;
    pthread_mutex_unlock(&in->lock);
    struct demuxer *demux = in->d_thread;
    bool eof = !demux->desc->fill_buffer || demux->desc->fill_buffer(demux) <= 0;
    update_cache(in);
    pthread_mutex_lock(&in->lock);

    if (eof) {
        for (int n = 0; n < in->d_buffer->num_streams; n++) {
            struct demux_stream *ds = in->d_buffer->streams[n]->ds;
            ds->eof = true;
        }
        // If we had EOF previously, then don't wakeup (avoids wakeup loop)
        if (!in->last_eof) {
            if (in->wakeup_cb)
                in->wakeup_cb(in->wakeup_cb_ctx);
            pthread_cond_signal(&in->wakeup);
            MP_VERBOSE(in, "EOF reached.\n");
        }
    }
    in->eof = in->last_eof = eof;
    return true;
}

// must be called locked; may temporarily unlock
static void ds_get_packets(struct demux_stream *ds)
{
    const char *t = stream_type_name(ds->type);
    struct demux_internal *in = ds->in;
    MP_DBG(in, "reading packet for %s\n", t);
    in->eof = false; // force retry
    ds->eof = false;
    while (ds->selected && !ds->head && !ds->eof) {
        ds->active = true;
        // Note: the following code marks EOF if it can't continue
        if (in->threading) {
            MP_VERBOSE(in, "waiting for demux thread (%s)\n", t);
            pthread_cond_signal(&in->wakeup);
            pthread_cond_wait(&in->wakeup, &in->lock);
        } else {
            read_packet(in);
        }
    }
}

// An obscure mechanism to get stream switching to be executed faster.
// On a switch, it seeks back, and then grabs all packets that were
// "missing" from the packet queue of the newly selected stream.
static void start_refreshing(struct demux_internal *in)
{
    struct demuxer *demux = in->d_thread;

    in->start_refresh_seek = false;

    double start_ts = MP_NOPTS_VALUE;
    for (int n = 0; n < demux->num_streams; n++) {
        struct demux_stream *ds = demux->streams[n]->ds;
        if (ds->type == STREAM_VIDEO || ds->type == STREAM_AUDIO)
            start_ts = MP_PTS_MIN(start_ts, ds->base_ts);
    }

    if (start_ts == MP_NOPTS_VALUE || !demux->desc->seek || !demux->seekable ||
        demux->partially_seekable || !demux->allow_refresh_seeks)
        return;

    for (int n = 0; n < demux->num_streams; n++) {
        struct demux_stream *ds = demux->streams[n]->ds;
        // Streams which didn't read any packets yet can return all packets,
        // or they'd be stuck forever; affects newly selected streams too.
        if (ds->last_pos != -1)
            ds->refreshing = true;
    }

    pthread_mutex_unlock(&in->lock);

    // Seek back to player's current position, with a small offset added.
    in->d_thread->desc->seek(in->d_thread, start_ts - 1.0,
                             SEEK_ABSOLUTE | SEEK_BACKWARD | SEEK_HR);

    pthread_mutex_lock(&in->lock);
}

static void execute_trackswitch(struct demux_internal *in)
{
    in->tracks_switched = false;

    pthread_mutex_unlock(&in->lock);

    if (in->d_thread->desc->control)
        in->d_thread->desc->control(in->d_thread, DEMUXER_CTRL_SWITCHED_TRACKS, 0);

    pthread_mutex_lock(&in->lock);

    if (in->start_refresh_seek)
        start_refreshing(in);
}

static void execute_seek(struct demux_internal *in)
{
    int flags = in->seek_flags;
    double pts = in->seek_pts;
    in->seeking = false;

    pthread_mutex_unlock(&in->lock);

    if (in->d_thread->desc->seek)
        in->d_thread->desc->seek(in->d_thread, pts, flags);

    pthread_mutex_lock(&in->lock);
}

static void *demux_thread(void *pctx)
{
    struct demux_internal *in = pctx;
    mpthread_set_name("demux");
    pthread_mutex_lock(&in->lock);
    while (!in->thread_terminate) {
        in->thread_paused = in->thread_request_pause > 0;
        if (in->thread_paused) {
            pthread_cond_signal(&in->wakeup);
            pthread_cond_wait(&in->wakeup, &in->lock);
            continue;
        }
        if (in->tracks_switched) {
            execute_trackswitch(in);
            continue;
        }
        if (in->seeking) {
            execute_seek(in);
            continue;
        }
        if (!in->eof) {
            if (read_packet(in))
                continue; // read_packet unlocked, so recheck conditions
        }
        if (in->force_cache_update) {
            pthread_mutex_unlock(&in->lock);
            update_cache(in);
            pthread_mutex_lock(&in->lock);
            in->force_cache_update = false;
            continue;
        }
        pthread_cond_signal(&in->wakeup);
        pthread_cond_wait(&in->wakeup, &in->lock);
    }
    pthread_mutex_unlock(&in->lock);
    return NULL;
}

static struct demux_packet *dequeue_packet(struct demux_stream *ds)
{
    if (!ds->head)
        return NULL;
    struct demux_packet *pkt = ds->head;
    ds->head = pkt->next;
    pkt->next = NULL;
    if (!ds->head)
        ds->tail = NULL;
    ds->bytes -= pkt->len;
    ds->packs--;

    double ts = pkt->dts == MP_NOPTS_VALUE ? pkt->pts : pkt->dts;
    if (ts != MP_NOPTS_VALUE)
        ds->base_ts = ts;

    if (pkt->keyframe) {
        // Update bitrate - only at keyframe points, because we use the
        // (possibly) reordered packet timestamps instead of realtime.
        double d = ts - ds->last_br_ts;
        if (ts == MP_NOPTS_VALUE || ds->last_br_ts == MP_NOPTS_VALUE || d < 0) {
            ds->bitrate = -1;
            ds->last_br_ts = ts;
            ds->last_br_bytes = 0;
        } else if (d > 0 && d >= 0.5) { // a window of least 500ms for UI purposes
            ds->bitrate = ds->last_br_bytes / d;
            ds->last_br_ts = ts;
            ds->last_br_bytes = 0;
        }
    }
    ds->last_br_bytes += pkt->len;

    // This implies this function is actually called from "the" user thread.
    if (pkt->pos >= ds->in->d_user->filepos)
        ds->in->d_user->filepos = pkt->pos;

    return pkt;
}

// Read a packet from the given stream. The returned packet belongs to the
// caller, who has to free it with talloc_free(). Might block. Returns NULL
// on EOF.
struct demux_packet *demux_read_packet(struct sh_stream *sh)
{
    struct demux_stream *ds = sh ? sh->ds : NULL;
    struct demux_packet *pkt = NULL;
    if (ds) {
        pthread_mutex_lock(&ds->in->lock);
        ds_get_packets(ds);
        pkt = dequeue_packet(ds);
        pthread_cond_signal(&ds->in->wakeup); // possibly read more
        pthread_mutex_unlock(&ds->in->lock);
    }
    return pkt;
}

// Poll the demuxer queue, and if there's a packet, return it. Otherwise, just
// make the demuxer thread read packets for this stream, and if there's at
// least one packet, call the wakeup callback.
// Unlike demux_read_packet(), this always enables readahead (which means you
// must not use it on interleaved subtitle streams).
// Returns:
//   < 0: EOF was reached, *out_pkt=NULL
//  == 0: no new packet yet, but maybe later, *out_pkt=NULL
//   > 0: new packet read, *out_pkt is set
int demux_read_packet_async(struct sh_stream *sh, struct demux_packet **out_pkt)
{
    struct demux_stream *ds = sh ? sh->ds : NULL;
    int r = -1;
    *out_pkt = NULL;
    if (ds) {
        if (ds->in->threading) {
            pthread_mutex_lock(&ds->in->lock);
            *out_pkt = dequeue_packet(ds);
            r = *out_pkt ? 1 : ((ds->eof || !ds->selected) ? -1 : 0);
            ds->active = ds->selected; // enable readahead
            ds->in->eof = false; // force retry
            pthread_cond_signal(&ds->in->wakeup); // possibly read more
            pthread_mutex_unlock(&ds->in->lock);
        } else {
            *out_pkt = demux_read_packet(sh);
            r = *out_pkt ? 1 : -1;
        }
    }
    return r;
}

// Return the pts of the next packet that demux_read_packet() would return.
// Might block. Sometimes used to force a packet read, without removing any
// packets from the queue.
double demux_get_next_pts(struct sh_stream *sh)
{
    double res = MP_NOPTS_VALUE;
    if (sh) {
        pthread_mutex_lock(&sh->ds->in->lock);
        ds_get_packets(sh->ds);
        if (sh->ds->head)
            res = sh->ds->head->pts;
        pthread_mutex_unlock(&sh->ds->in->lock);
    }
    return res;
}

// Return whether a packet is queued. Never blocks, never forces any reads.
bool demux_has_packet(struct sh_stream *sh)
{
    bool has_packet = false;
    if (sh) {
        pthread_mutex_lock(&sh->ds->in->lock);
        has_packet = sh->ds->head;
        pthread_mutex_unlock(&sh->ds->in->lock);
    }
    return has_packet;
}

// Read and return any packet we find.
struct demux_packet *demux_read_any_packet(struct demuxer *demuxer)
{
    assert(!demuxer->in->threading); // doesn't work with threading
    bool read_more = true;
    while (read_more) {
        for (int n = 0; n < demuxer->num_streams; n++) {
            struct sh_stream *sh = demuxer->streams[n];
            sh->ds->active = sh->ds->selected; // force read_packet() to read
            struct demux_packet *pkt = dequeue_packet(sh->ds);
            if (pkt)
                return pkt;
        }
        // retry after calling this
        pthread_mutex_lock(&demuxer->in->lock);
        read_more = read_packet(demuxer->in);
        read_more &= !demuxer->in->eof;
        pthread_mutex_unlock(&demuxer->in->lock);
    }
    return NULL;
}

// ====================================================================

void demuxer_help(struct mp_log *log)
{
    int i;

    mp_info(log, "Available demuxers:\n");
    mp_info(log, " demuxer:   info:\n");
    for (i = 0; demuxer_list[i]; i++) {
        mp_info(log, "%10s  %s\n",
                demuxer_list[i]->name, demuxer_list[i]->desc);
    }
}

static const char *d_level(enum demux_check level)
{
    switch (level) {
    case DEMUX_CHECK_FORCE:  return "force";
    case DEMUX_CHECK_UNSAFE: return "unsafe";
    case DEMUX_CHECK_REQUEST:return "request";
    case DEMUX_CHECK_NORMAL: return "normal";
    }
    abort();
}

static int decode_float(char *str, float *out)
{
    char *rest;
    float dec_val;

    dec_val = strtod(str, &rest);
    if (!rest || (rest == str) || !isfinite(dec_val))
        return -1;

    *out = dec_val;
    return 0;
}

static int decode_gain(demuxer_t *demuxer, const char *tag, float *out)
{
    char *tag_val = NULL;
    float dec_val;

    tag_val = mp_tags_get_str(demuxer->metadata, tag);
    if (!tag_val)
        return -1;

    if (decode_float(tag_val, &dec_val)) {
        mp_msg(demuxer->log, MSGL_ERR, "Invalid replaygain value\n");
        return -1;
    }

    *out = dec_val;
    return 0;
}

static int decode_peak(demuxer_t *demuxer, const char *tag, float *out)
{
    char *tag_val = NULL;
    float dec_val;

    *out = 1.0;

    tag_val = mp_tags_get_str(demuxer->metadata, tag);
    if (!tag_val)
        return 0;

    if (decode_float(tag_val, &dec_val))
        return 0;

    if (dec_val == 0.0)
        return 0;

    *out = dec_val;
    return 0;
}

static void demux_export_replaygain(demuxer_t *demuxer)
{
    float tg, tp, ag, ap;

    if (!decode_gain(demuxer, "REPLAYGAIN_TRACK_GAIN", &tg) &&
        !decode_peak(demuxer, "REPLAYGAIN_TRACK_PEAK", &tp) &&
        !decode_gain(demuxer, "REPLAYGAIN_ALBUM_GAIN", &ag) &&
        !decode_peak(demuxer, "REPLAYGAIN_ALBUM_PEAK", &ap))
    {
        struct replaygain_data *rgain = talloc_ptrtype(demuxer, rgain);

        rgain->track_gain = tg;
        rgain->track_peak = tp;
        rgain->album_gain = ag;
        rgain->album_peak = ap;

        for (int n = 0; n < demuxer->num_streams; n++) {
            struct sh_stream *sh = demuxer->streams[n];
            if (sh->audio && !sh->audio->replaygain_data)
                sh->audio->replaygain_data = rgain;
        }
    }
}

// Copy all fields from src to dst, depending on event flags.
static void demux_copy(struct demuxer *dst, struct demuxer *src)
{
    if (src->events & DEMUX_EVENT_INIT) {
        // Note that we do as shallow copies as possible. We expect the data
        // that is not-copied (only referenced) to be immutable.
        // This implies e.g. that no chapters are added after initialization.
        dst->chapters = src->chapters;
        dst->num_chapters = src->num_chapters;
        dst->editions = src->editions;
        dst->num_editions = src->num_editions;
        dst->edition = src->edition;
        dst->attachments = src->attachments;
        dst->num_attachments = src->num_attachments;
        dst->matroska_data = src->matroska_data;
        dst->playlist = src->playlist;
        dst->seekable = src->seekable;
        dst->partially_seekable = src->partially_seekable;
        dst->filetype = src->filetype;
        dst->ts_resets_possible = src->ts_resets_possible;
        dst->rel_seeks = src->rel_seeks;
        dst->allow_refresh_seeks = src->allow_refresh_seeks;
        dst->fully_read = src->fully_read;
        dst->start_time = src->start_time;
        dst->priv = src->priv;
    }
    if (src->events & DEMUX_EVENT_STREAMS) {
        // The stream structs themselves are immutable.
        for (int n = dst->num_streams; n < src->num_streams; n++)
            MP_TARRAY_APPEND(dst, dst->streams, dst->num_streams, src->streams[n]);
    }
    if (src->events & DEMUX_EVENT_METADATA) {
        talloc_free(dst->metadata);
        dst->metadata = mp_tags_dup(dst, src->metadata);
    }
    dst->events |= src->events;
    src->events = 0;
}

// This is called by demuxer implementations if certain parameters change
// at runtime.
// events is one of DEMUX_EVENT_*
// The code will copy the fields references by the events to the user-thread.
void demux_changed(demuxer_t *demuxer, int events)
{
    assert(demuxer == demuxer->in->d_thread); // call from demuxer impl. only
    struct demux_internal *in = demuxer->in;

    demuxer->events |= events;

    update_cache(in);

    pthread_mutex_lock(&in->lock);

    if (demuxer->events & DEMUX_EVENT_INIT)
        demuxer_sort_chapters(demuxer);
    if (demuxer->events & (DEMUX_EVENT_METADATA | DEMUX_EVENT_STREAMS))
        demux_export_replaygain(demuxer);

    demux_copy(in->d_buffer, demuxer);

    if (in->wakeup_cb)
        in->wakeup_cb(in->wakeup_cb_ctx);
    pthread_mutex_unlock(&in->lock);
}

// Called by the user thread (i.e. player) to update metadata and other things
// from the demuxer thread.
void demux_update(demuxer_t *demuxer)
{
    assert(demuxer == demuxer->in->d_user);
    struct demux_internal *in = demuxer->in;

    if (!in->threading)
        update_cache(in);

    pthread_mutex_lock(&in->lock);
    demux_copy(demuxer, in->d_buffer);
    if (in->stream_metadata && (demuxer->events & DEMUX_EVENT_METADATA))
        mp_tags_merge(demuxer->metadata, in->stream_metadata);
    pthread_mutex_unlock(&in->lock);
}

static void demux_init_cache(struct demuxer *demuxer)
{
    struct demux_internal *in = demuxer->in;
    struct stream *stream = demuxer->stream;

    char *base = NULL;
    stream_control(stream, STREAM_CTRL_GET_BASE_FILENAME, &base);
    in->stream_base_filename = talloc_steal(demuxer, base);
}

static struct demuxer *open_given_type(struct mpv_global *global,
                                       struct mp_log *log,
                                       const struct demuxer_desc *desc,
                                       struct stream *stream,
                                       struct demuxer_params *params,
                                       enum demux_check check)
{
    struct demuxer *demuxer = talloc_ptrtype(NULL, demuxer);
    *demuxer = (struct demuxer) {
        .desc = desc,
        .stream = stream,
        .seekable = stream->seekable,
        .filepos = -1,
        .opts = global->opts,
        .global = global,
        .log = mp_log_new(demuxer, log, desc->name),
        .glog = log,
        .filename = talloc_strdup(demuxer, stream->url),
        .events = DEMUX_EVENT_ALL,
    };
    demuxer->seekable = stream->seekable;
    if (demuxer->stream->uncached_stream &&
        !demuxer->stream->uncached_stream->seekable)
        demuxer->seekable = false;

    struct demux_internal *in = demuxer->in = talloc_ptrtype(demuxer, in);
    *in = (struct demux_internal){
        .log = demuxer->log,
        .d_thread = talloc(demuxer, struct demuxer),
        .d_buffer = talloc(demuxer, struct demuxer),
        .d_user = demuxer,
        .min_secs = demuxer->opts->demuxer_min_secs,
        .min_packs = demuxer->opts->demuxer_min_packs,
        .min_bytes = demuxer->opts->demuxer_min_bytes,
    };
    pthread_mutex_init(&in->lock, NULL);
    pthread_cond_init(&in->wakeup, NULL);

    if (stream->uncached_stream)
        in->min_secs = MPMAX(in->min_secs, demuxer->opts->demuxer_min_secs_cache);

    *in->d_thread = *demuxer;
    *in->d_buffer = *demuxer;

    in->d_thread->metadata = talloc_zero(in->d_thread, struct mp_tags);
    in->d_user->metadata = talloc_zero(in->d_user, struct mp_tags);
    in->d_buffer->metadata = talloc_zero(in->d_buffer, struct mp_tags);

    mp_verbose(log, "Trying demuxer: %s (force-level: %s)\n",
               desc->name, d_level(check));

    if (stream->seekable) // not for DVD/BD/DVB in particular
        stream_seek(stream, 0);

    // Peek this much data to avoid that stream_read() run by some demuxers
    // will flush previous peeked data.
    stream_peek(stream, STREAM_BUFFER_SIZE);

    in->d_thread->params = params; // temporary during open()
    int ret = demuxer->desc->open(in->d_thread, check);
    if (ret >= 0) {
        in->d_thread->params = NULL;
        if (in->d_thread->filetype)
            mp_verbose(log, "Detected file format: %s (%s)\n",
                       in->d_thread->filetype, desc->desc);
        else
            mp_verbose(log, "Detected file format: %s\n", desc->desc);
        if (!in->d_thread->seekable)
            mp_verbose(log, "Stream is not seekable.\n");
        // Pretend we can seek if we can't seek, but there's a cache.
        if (!in->d_thread->seekable && stream->uncached_stream) {
            mp_verbose(log, "Enabling seeking because stream cache is active.\n");
            in->d_thread->seekable = true;
            in->d_thread->partially_seekable = true;
        }
        demux_init_cache(demuxer);
        demux_changed(in->d_thread, DEMUX_EVENT_ALL);
        demux_update(demuxer);
        return demuxer;
    }

    free_demuxer(demuxer);
    return NULL;
}

static const int d_normal[]  = {DEMUX_CHECK_NORMAL, DEMUX_CHECK_UNSAFE, -1};
static const int d_request[] = {DEMUX_CHECK_REQUEST, -1};
static const int d_force[]   = {DEMUX_CHECK_FORCE, -1};

// params can be NULL
struct demuxer *demux_open(struct stream *stream, struct demuxer_params *params,
                           struct mpv_global *global)
{
    const int *check_levels = d_normal;
    const struct demuxer_desc *check_desc = NULL;
    struct mp_log *log = mp_log_new(NULL, global->log, "!demux");
    struct demuxer *demuxer = NULL;
    char *force_format = params ? params->force_format : NULL;

    if (!force_format)
        force_format = stream->demuxer;

    if (force_format && force_format[0]) {
        check_levels = d_request;
        if (force_format[0] == '+') {
            force_format += 1;
            check_levels = d_force;
        }
        for (int n = 0; demuxer_list[n]; n++) {
            if (strcmp(demuxer_list[n]->name, force_format) == 0)
                check_desc = demuxer_list[n];
        }
        if (!check_desc) {
            mp_err(log, "Demuxer %s does not exist.\n", force_format);
            goto done;
        }
    }

    // Test demuxers from first to last, one pass for each check_levels[] entry
    for (int pass = 0; check_levels[pass] != -1; pass++) {
        enum demux_check level = check_levels[pass];
        for (int n = 0; demuxer_list[n]; n++) {
            const struct demuxer_desc *desc = demuxer_list[n];
            if (!check_desc || desc == check_desc) {
                demuxer = open_given_type(global, log, desc, stream, params, level);
                if (demuxer) {
                    talloc_steal(demuxer, log);
                    log = NULL;
                    goto done;
                }
            }
        }
    }

done:
    talloc_free(log);
    return demuxer;
}

// Convenience function: open the stream, enable the cache (according to params
// and global opts.), open the demuxer.
// (use free_demuxer_and_stream() to free the underlying stream too)
struct demuxer *demux_open_url(const char *url,
                                struct demuxer_params *params,
                                struct mp_cancel *cancel,
                                struct mpv_global *global)
{
    struct MPOpts *opts = global->opts;
    struct stream *s = stream_create(url, STREAM_READ, cancel, global);
    if (!s)
        return NULL;
    if (!(params && params->disable_cache))
        stream_enable_cache(&s, &opts->stream_cache);
    struct demuxer *d = demux_open(s, params, global);
    if (!d)
        free_stream(s);
    return d;
}

static void flush_locked(demuxer_t *demuxer)
{
    for (int n = 0; n < demuxer->num_streams; n++)
        ds_flush(demuxer->streams[n]->ds);
    demuxer->in->warned_queue_overflow = false;
    demuxer->in->eof = false;
    demuxer->in->last_eof = false;
    demuxer->in->idle = true;
    demuxer->filepos = -1; // implicitly synchronized
}

// clear the packet queues
void demux_flush(demuxer_t *demuxer)
{
    pthread_mutex_lock(&demuxer->in->lock);
    flush_locked(demuxer);
    pthread_mutex_unlock(&demuxer->in->lock);
}

int demux_seek(demuxer_t *demuxer, double rel_seek_secs, int flags)
{
    struct demux_internal *in = demuxer->in;
    assert(demuxer == in->d_user);

    if (!demuxer->seekable) {
        MP_WARN(demuxer, "Cannot seek in this file.\n");
        return 0;
    }

    if ((flags & SEEK_FACTOR) && !(flags & SEEK_ABSOLUTE)) {
        MP_WARN(demuxer, "Invalid seek flags.\n");
        return 0;
    }

    if (rel_seek_secs == MP_NOPTS_VALUE && (flags & SEEK_ABSOLUTE))
        return 0;

    if (!(flags & (SEEK_BACKWARD | SEEK_FORWARD))) {
        if (flags & SEEK_ABSOLUTE || rel_seek_secs < 0) {
            flags |= SEEK_BACKWARD;
        } else {
            flags |= SEEK_FORWARD;
        }
    }

    pthread_mutex_lock(&in->lock);

    flush_locked(demuxer);
    in->seeking = true;
    in->seek_flags = flags;
    in->seek_pts = rel_seek_secs;

    if (!in->threading)
        execute_seek(in);

    pthread_cond_signal(&in->wakeup);
    pthread_mutex_unlock(&in->lock);

    return 1;
}

// Enable doing a "refresh seek" on the next stream switch.
// Note that this by design does not disable ongoing refresh seeks, and
// does not affect previous stream switch commands (even if they were
// asynchronous).
void demux_set_enable_refresh_seeks(struct demuxer *demuxer, bool enabled)
{
    struct demux_internal *in = demuxer->in;
    pthread_mutex_lock(&in->lock);
    in->refresh_seeks_enabled = enabled;
    pthread_mutex_unlock(&in->lock);
}

struct sh_stream *demuxer_stream_by_demuxer_id(struct demuxer *d,
                                               enum stream_type t, int id)
{
    for (int n = 0; n < d->num_streams; n++) {
        struct sh_stream *s = d->streams[n];
        if (s->type == t && s->demuxer_id == id)
            return d->streams[n];
    }
    return NULL;
}

void demuxer_switch_track(struct demuxer *demuxer, enum stream_type type,
                          struct sh_stream *stream)
{
    assert(!stream || stream->type == type);

    for (int n = 0; n < demuxer->num_streams; n++) {
        struct sh_stream *cur = demuxer->streams[n];
        if (cur->type == type)
            demuxer_select_track(demuxer, cur, cur == stream);
    }
}

void demuxer_select_track(struct demuxer *demuxer, struct sh_stream *stream,
                          bool selected)
{
    struct demux_internal *in = demuxer->in;
    pthread_mutex_lock(&in->lock);
    bool update = false;
    // don't flush buffers if stream is already selected / unselected
    if (stream->ds->selected != selected) {
        stream->ds->selected = selected;
        stream->ds->active = false;
        ds_flush(stream->ds);
        if (selected && in->refresh_seeks_enabled && in->threading)
            in->start_refresh_seek = true;
        update = true;
    }
    pthread_mutex_unlock(&in->lock);
    if (update)
        demux_control(demuxer, DEMUXER_CTRL_SWITCHED_TRACKS, NULL);
}

void demux_set_stream_autoselect(struct demuxer *demuxer, bool autoselect)
{
    assert(!demuxer->in->threading); // laziness
    demuxer->in->autoselect = autoselect;
}

bool demux_stream_is_selected(struct sh_stream *stream)
{
    if (!stream)
        return false;
    bool r = false;
    pthread_mutex_lock(&stream->ds->in->lock);
    r = stream->ds->selected;
    pthread_mutex_unlock(&stream->ds->in->lock);
    return r;
}

int demuxer_add_attachment(demuxer_t *demuxer, struct bstr name,
                           struct bstr type, struct bstr data)
{
    if (!(demuxer->num_attachments % 32))
        demuxer->attachments = talloc_realloc(demuxer, demuxer->attachments,
                                              struct demux_attachment,
                                              demuxer->num_attachments + 32);

    struct demux_attachment *att =
        demuxer->attachments + demuxer->num_attachments;
    att->name = talloc_strndup(demuxer->attachments, name.start, name.len);
    att->type = talloc_strndup(demuxer->attachments, type.start, type.len);
    att->data = talloc_size(demuxer->attachments, data.len);
    memcpy(att->data, data.start, data.len);
    att->data_size = data.len;

    return demuxer->num_attachments++;
}

static int chapter_compare(const void *p1, const void *p2)
{
    struct demux_chapter *c1 = (void *)p1;
    struct demux_chapter *c2 = (void *)p2;

    if (c1->pts > c2->pts)
        return 1;
    else if (c1->pts < c2->pts)
        return -1;
    return c1->original_index > c2->original_index ? 1 :-1; // never equal
}

static void demuxer_sort_chapters(demuxer_t *demuxer)
{
    qsort(demuxer->chapters, demuxer->num_chapters,
          sizeof(struct demux_chapter), chapter_compare);
}

int demuxer_add_chapter(demuxer_t *demuxer, struct bstr name,
                        double pts, uint64_t demuxer_id)
{
    struct demux_chapter new = {
        .original_index = demuxer->num_chapters,
        .pts = pts,
        .name = name.len ? bstrdup0(demuxer, name) : NULL,
        .metadata = talloc_zero(demuxer, struct mp_tags),
        .demuxer_id = demuxer_id,
    };
    mp_tags_set_bstr(new.metadata, bstr0("TITLE"), name);
    MP_TARRAY_APPEND(demuxer, demuxer->chapters, demuxer->num_chapters, new);
    return demuxer->num_chapters - 1;
}

double demuxer_get_time_length(struct demuxer *demuxer)
{
    double len;
    if (demux_control(demuxer, DEMUXER_CTRL_GET_TIME_LENGTH, &len) > 0)
        return len;
    return -1;
}

// must be called not locked
static void update_cache(struct demux_internal *in)
{
    struct demuxer *demuxer = in->d_thread;
    struct stream *stream = demuxer->stream;

    // Don't lock while querying the stream.
    double time_length = -1;
    struct mp_tags *stream_metadata = NULL;
    int64_t stream_size = -1;
    int64_t stream_cache_size = -1;
    int64_t stream_cache_fill = -1;
    int stream_cache_idle = -1;
    struct mp_nav_event *nav_event = NULL;

    pthread_mutex_lock(&in->lock);
    bool need_nav_event = !in->nav_event;;
    pthread_mutex_unlock(&in->lock);

    if (demuxer->desc->control) {
        demuxer->desc->control(demuxer, DEMUXER_CTRL_GET_TIME_LENGTH,
                               &time_length);
        if (need_nav_event)
            demuxer->desc->control(demuxer, DEMUXER_CTRL_GET_NAV_EVENT, &nav_event);
    }

    stream_control(stream, STREAM_CTRL_GET_METADATA, &stream_metadata);
    stream_control(stream, STREAM_CTRL_GET_SIZE, &stream_size);
    stream_control(stream, STREAM_CTRL_GET_CACHE_SIZE, &stream_cache_size);
    stream_control(stream, STREAM_CTRL_GET_CACHE_FILL, &stream_cache_fill);
    stream_control(stream, STREAM_CTRL_GET_CACHE_IDLE, &stream_cache_idle);

    pthread_mutex_lock(&in->lock);
    in->time_length = time_length;
    in->stream_size = stream_size;
    in->stream_cache_size = stream_cache_size;
    in->stream_cache_fill = stream_cache_fill;
    in->stream_cache_idle = stream_cache_idle;
    if (stream_metadata) {
        talloc_free(in->stream_metadata);
        in->stream_metadata = talloc_steal(in, stream_metadata);
        in->d_buffer->events |= DEMUX_EVENT_METADATA;
    }
    in->nav_event = nav_event ? nav_event : in->nav_event;
    pthread_mutex_unlock(&in->lock);
}

// must be called locked
static int cached_stream_control(struct demux_internal *in, int cmd, void *arg)
{
    // If the cache is active, wake up the thread to possibly update cache state.
    if (in->stream_cache_size >= 0) {
        in->force_cache_update = true;
        pthread_cond_signal(&in->wakeup);
    }

    switch (cmd) {
    case STREAM_CTRL_GET_CACHE_SIZE:
        if (in->stream_cache_size < 0)
            return STREAM_UNSUPPORTED;
        *(int64_t *)arg = in->stream_cache_size;
        return STREAM_OK;
    case STREAM_CTRL_GET_CACHE_FILL:
        if (in->stream_cache_fill < 0)
            return STREAM_UNSUPPORTED;
        *(int64_t *)arg = in->stream_cache_fill;
        return STREAM_OK;
    case STREAM_CTRL_GET_CACHE_IDLE:
        if (in->stream_cache_idle < 0)
            return STREAM_UNSUPPORTED;
        *(int *)arg = in->stream_cache_idle;
        return STREAM_OK;
    case STREAM_CTRL_GET_SIZE:
        if (in->stream_size < 0)
            return STREAM_UNSUPPORTED;
        *(int64_t *)arg = in->stream_size;
        return STREAM_OK;
    case STREAM_CTRL_GET_BASE_FILENAME:
        if (!in->stream_base_filename)
            return STREAM_UNSUPPORTED;
        *(char **)arg = talloc_strdup(NULL, in->stream_base_filename);
        return STREAM_OK;
    }
    return STREAM_ERROR;
}

// must be called locked
static int cached_demux_control(struct demux_internal *in, int cmd, void *arg)
{
    switch (cmd) {
    case DEMUXER_CTRL_GET_TIME_LENGTH:
        if (in->time_length < 0)
            return DEMUXER_CTRL_NOTIMPL;
        *(double *)arg = in->time_length;
        return DEMUXER_CTRL_OK;
    case DEMUXER_CTRL_STREAM_CTRL: {
        struct demux_ctrl_stream_ctrl *c = arg;
        int r = cached_stream_control(in, c->ctrl, c->arg);
        if (r == STREAM_ERROR)
            break;
        c->res = r;
        return DEMUXER_CTRL_OK;
    }
    case DEMUXER_CTRL_SWITCHED_TRACKS:
        in->tracks_switched = true;
        pthread_cond_signal(&in->wakeup);
        return DEMUXER_CTRL_OK;
    case DEMUXER_CTRL_GET_BITRATE_STATS: {
        double *rates = arg;
        for (int n = 0; n < STREAM_TYPE_COUNT; n++)
            rates[n] = 0;
        for (int n = 0; n < in->d_user->num_streams; n++) {
            struct demux_stream *ds = in->d_user->streams[n]->ds;
            rates[ds->type] += MPMAX(0, ds->bitrate);
        }
        return DEMUXER_CTRL_OK;
    }
    case DEMUXER_CTRL_GET_READER_STATE: {
        struct demux_ctrl_reader_state *r = arg;
        *r = (struct demux_ctrl_reader_state){
            .eof = in->last_eof,
            .ts_range = {MP_NOPTS_VALUE, MP_NOPTS_VALUE},
            .ts_duration = -1,
        };
        int num_packets = 0;
        for (int n = 0; n < in->d_user->num_streams; n++) {
            struct demux_stream *ds = in->d_user->streams[n]->ds;
            if (ds->active) {
                r->underrun |= !ds->head && !ds->eof;
                r->ts_range[0] = MP_PTS_MAX(r->ts_range[0], ds->base_ts);
                r->ts_range[1] = MP_PTS_MIN(r->ts_range[1], ds->last_ts);
                num_packets += ds->packs;
            }
        }
        r->idle = (in->idle && !r->underrun) || r->eof;
        r->underrun &= !r->idle;
        if (r->ts_range[0] != MP_NOPTS_VALUE && r->ts_range[1] != MP_NOPTS_VALUE)
            r->ts_duration = MPMAX(0, r->ts_range[1] - r->ts_range[0]);
        if (!num_packets || in->seeking)
            r->ts_duration = 0;
        return DEMUXER_CTRL_OK;
    }
    case DEMUXER_CTRL_GET_NAV_EVENT:
        if (!in->nav_event)
            return DEMUXER_CTRL_NOTIMPL;
        *(struct mp_nav_event **)arg = in->nav_event;
        in->nav_event = NULL;
        return DEMUXER_CTRL_OK;

    }
    return DEMUXER_CTRL_DONTKNOW;
}

int demux_control(demuxer_t *demuxer, int cmd, void *arg)
{
    struct demux_internal *in = demuxer->in;

    if (in->threading) {
        pthread_mutex_lock(&in->lock);
        int cr = cached_demux_control(in, cmd, arg);
        pthread_mutex_unlock(&in->lock);
        if (cr != DEMUXER_CTRL_DONTKNOW)
            return cr;
    }

    int r = DEMUXER_CTRL_NOTIMPL;
    demux_pause(demuxer);
    if (cmd == DEMUXER_CTRL_STREAM_CTRL) {
        struct demux_ctrl_stream_ctrl *c = arg;
        if (in->threading)
            MP_VERBOSE(demuxer, "blocking for STREAM_CTRL %d\n", c->ctrl);
        c->res = stream_control(demuxer->stream, c->ctrl, c->arg);
        if (c->res != STREAM_UNSUPPORTED)
            r = DEMUXER_CTRL_OK;
    }
    if (r != DEMUXER_CTRL_OK) {
        if (in->threading)
            MP_VERBOSE(demuxer, "blocking for DEMUXER_CTRL %d\n", cmd);
        if (demuxer->desc->control)
            r = demuxer->desc->control(demuxer->in->d_thread, cmd, arg);
    }
    demux_unpause(demuxer);
    return r;
}

int demux_stream_control(demuxer_t *demuxer, int ctrl, void *arg)
{
    struct demux_ctrl_stream_ctrl c = {ctrl, arg, STREAM_UNSUPPORTED};
    demux_control(demuxer, DEMUXER_CTRL_STREAM_CTRL, &c);
    return c.res;
}

// Make the demuxer thread stop doing anything.
// demux_unpause() wakes up the thread again.
// Can be nested with other calls, but trying to read packets may deadlock.
void demux_pause(demuxer_t *demuxer)
{
    struct demux_internal *in = demuxer->in;
    assert(demuxer == in->d_user);

    if (!in->threading)
        return;

    MP_VERBOSE(in, "pause demux thread\n");

    pthread_mutex_lock(&in->lock);
    in->thread_request_pause++;
    pthread_cond_signal(&in->wakeup);
    while (!in->thread_paused)
        pthread_cond_wait(&in->wakeup, &in->lock);
    pthread_mutex_unlock(&in->lock);
}

void demux_unpause(demuxer_t *demuxer)
{
    struct demux_internal *in = demuxer->in;
    assert(demuxer == in->d_user);

    if (!in->threading)
        return;

    pthread_mutex_lock(&in->lock);
    assert(in->thread_request_pause > 0);
    in->thread_request_pause--;
    pthread_cond_signal(&in->wakeup);
    pthread_mutex_unlock(&in->lock);
}

bool demux_cancel_test(struct demuxer *demuxer)
{
    return mp_cancel_test(demuxer->stream->cancel);
}

struct demux_chapter *demux_copy_chapter_data(struct demux_chapter *c, int num)
{
    struct demux_chapter *new = talloc_array(NULL, struct demux_chapter, num);
    for (int n = 0; n < num; n++) {
        new[n] = c[n];
        new[n].name = talloc_strdup(new, new[n].name);
        if (new[n].metadata)
            new[n].metadata = mp_tags_dup(new, new[n].metadata);
    }
    return new;
}
