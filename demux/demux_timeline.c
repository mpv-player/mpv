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
#include <limits.h>

#include "common/common.h"
#include "common/msg.h"

#include "demux.h"
#include "timeline.h"
#include "stheader.h"
#include "stream/stream.h"

struct segment {
    int index; // index into virtual_source.segments[] (and timeline.parts[])
    double start, end;
    double d_start;
    char *url;
    bool lazy;
    struct demuxer *d;
    // stream_map[sh_stream.index] = virtual_stream, where sh_stream is a stream
    // from the source d, and virtual_stream is a streamexported by the
    // timeline demuxer (virtual_stream.sh). It's used to map the streams of the
    // source onto the set of streams of the virtual timeline.
    // Uses NULL for streams that do not appear in the virtual timeline.
    struct virtual_stream **stream_map;
    int num_stream_map;
};

// Information for each stream on the virtual timeline. (Mirrors streams
// exposed by demux_timeline.)
struct virtual_stream {
    struct sh_stream *sh;       // stream exported by demux_timeline
    bool selected;              // ==demux_stream_is_selected(sh)
    int eos_packets;            // deal with b-frame delay
    struct virtual_source *src; // group this stream is part of
};

// This represents a single timeline source. (See timeline.next. For each
// timeline struct there is a virtual_source.)
struct virtual_source {
    struct timeline *tl;

    bool dash;

    struct segment **segments;
    int num_segments;
    struct segment *current;

    struct virtual_stream **streams;
    int num_streams;

    // Total number of packets received past end of segment. Used
    // to be clever about determining when to switch segments.
    int eos_packets;

    bool eof_reached;
    double dts;                 // highest read DTS (or PTS if no DTS available)
    bool any_selected;          // at least one stream is actually selected
};

struct priv {
    struct timeline *tl;

    double duration;

    // As the demuxer user sees it.
    struct virtual_stream **streams;
    int num_streams;

    struct virtual_source **sources;
    int num_sources;
};

static void add_tl(struct demuxer *demuxer, struct timeline *tl);

static void update_slave_stats(struct demuxer *demuxer, struct demuxer *slave)
{
    demux_report_unbuffered_read_bytes(demuxer, demux_get_bytes_read_hack(slave));
}

static bool target_stream_used(struct segment *seg, struct virtual_stream *vs)
{
    for (int n = 0; n < seg->num_stream_map; n++) {
        if (seg->stream_map[n] == vs)
            return true;
    }
    return false;
}

// Create mapping from segment streams to virtual timeline streams.
static void associate_streams(struct demuxer *demuxer,
                              struct virtual_source *src,
                              struct segment *seg)
{
    if (!seg->d || seg->stream_map)
        return;

    int num_streams = demux_get_num_stream(seg->d);
    for (int n = 0; n < num_streams; n++) {
        struct sh_stream *sh = demux_get_stream(seg->d, n);
        struct virtual_stream *other = NULL;
        for (int i = 0; i < src->num_streams; i++) {
            struct virtual_stream *vs = src->streams[i];

            // The stream must always have the same media type. Also, a stream
            // can't be assigned multiple times.
            if (sh->type != vs->sh->type || target_stream_used(seg, vs))
                continue;

            // By default pick the first matching stream.
            if (!other)
                other = vs;

            // Matching by demuxer ID is supposedly useful and preferable for
            // ordered chapters.
            if (sh->demuxer_id == vs->sh->demuxer_id)
                other = vs;
        }

        MP_TARRAY_APPEND(seg, seg->stream_map, seg->num_stream_map, other);
    }
}

static void reselect_streams(struct demuxer *demuxer)
{
    struct priv *p = demuxer->priv;

    for (int n = 0; n < p->num_streams; n++) {
        struct virtual_stream *vs = p->streams[n];
        vs->selected = demux_stream_is_selected(vs->sh);
    }

    for (int x = 0; x < p->num_sources; x++) {
        struct virtual_source *src = p->sources[x];

        bool was_selected = src->any_selected;
        src->any_selected = false;

        for (int n = 0; n < src->num_segments; n++) {
            struct segment *seg = src->segments[n];

            if (!seg->d)
                continue;

            for (int i = 0; i < seg->num_stream_map; i++) {
                struct sh_stream *sh = demux_get_stream(seg->d, i);
                bool selected =
                    seg->stream_map[i] && seg->stream_map[i]->selected;

                src->any_selected |= selected;

                // This stops demuxer readahead for inactive segments.
                if (!src->current || seg->d != src->current->d)
                    selected = false;
                demuxer_select_track(seg->d, sh, MP_NOPTS_VALUE, selected);

                update_slave_stats(demuxer, seg->d);
            }
        }

        if (!was_selected && src->any_selected) {
            src->eof_reached = false;
            src->dts = MP_NOPTS_VALUE;
        }
    }
}

static void close_lazy_segments(struct demuxer *demuxer,
                                struct virtual_source *src)
{
    // unload previous segment
    for (int n = 0; n < src->num_segments; n++) {
        struct segment *seg = src->segments[n];
        if (seg != src->current && seg->d && seg->lazy) {
            demux_free(seg->d);
            seg->d = NULL;
        }
    }
}

static void reopen_lazy_segments(struct demuxer *demuxer,
                                 struct virtual_source *src)
{
    if (src->current->d)
        return;

    close_lazy_segments(demuxer, src);

    struct demuxer_params params = {
        .init_fragment = src->tl->init_fragment,
        .skip_lavf_probing = true,
    };
    src->current->d = demux_open_url(src->current->url, &params,
                                     demuxer->cancel, demuxer->global);
    if (!src->current->d && !demux_cancel_test(demuxer))
        MP_ERR(demuxer, "failed to load segment\n");
    if (src->current->d)
        demux_disable_cache(src->current->d);
    update_slave_stats(demuxer, src->current->d);
    associate_streams(demuxer, src, src->current);
}

static void switch_segment(struct demuxer *demuxer, struct virtual_source *src,
                           struct segment *new, double start_pts, int flags,
                           bool init)
{
    if (!(flags & SEEK_FORWARD))
        flags |= SEEK_HR;

    MP_VERBOSE(demuxer, "switch to segment %d\n", new->index);

    if (src->current && src->current->d)
        update_slave_stats(demuxer, src->current->d);

    src->current = new;
    reopen_lazy_segments(demuxer, src);
    if (!new->d)
        return;
    reselect_streams(demuxer);
    if (!src->dash)
        demux_set_ts_offset(new->d, new->start - new->d_start);
    if (!src->dash || !init)
        demux_seek(new->d, start_pts, flags);

    for (int n = 0; n < src->num_streams; n++) {
        struct virtual_stream *vs = src->streams[n];
        vs->eos_packets = 0;
    }

    src->eof_reached = false;
    src->eos_packets = 0;
}

static void d_seek(struct demuxer *demuxer, double seek_pts, int flags)
{
    struct priv *p = demuxer->priv;

    double pts = seek_pts * ((flags & SEEK_FACTOR) ? p->duration : 1);

    flags &= SEEK_FORWARD | SEEK_HR;

    for (int x = 0; x < p->num_sources; x++) {
        struct virtual_source *src = p->sources[x];

        struct segment *new = src->segments[src->num_segments - 1];
        for (int n = 0; n < src->num_segments; n++) {
            if (pts < src->segments[n]->end) {
                new = src->segments[n];
                break;
            }
        }

        switch_segment(demuxer, src, new, pts, flags, false);

        src->dts = MP_NOPTS_VALUE;
    }
}

static bool d_read_packet(struct demuxer *demuxer, struct demux_packet **out_pkt)
{
    struct priv *p = demuxer->priv;

    struct virtual_source *src = NULL;

    for (int x = 0; x < p->num_sources; x++) {
        struct virtual_source *cur = p->sources[x];

        if (!cur->any_selected || cur->eof_reached)
            continue;

        if (!cur->current)
            switch_segment(demuxer, cur, cur->segments[0], 0, 0, true);

        if (!cur->any_selected || !cur->current || !cur->current->d)
            continue;

        if (!src || cur->dts == MP_NOPTS_VALUE ||
            (src->dts != MP_NOPTS_VALUE && cur->dts < src->dts))
            src = cur;
    }

    if (!src)
        return false;

    struct segment *seg = src->current;
    assert(seg && seg->d);

    struct demux_packet *pkt = demux_read_any_packet(seg->d);
    if (!pkt || pkt->pts >= seg->end)
        src->eos_packets += 1;

    update_slave_stats(demuxer, seg->d);

    // Test for EOF. Do this here to properly run into EOF even if other
    // streams are disabled etc. If it somehow doesn't manage to reach the end
    // after demuxing a high (bit arbitrary) number of packets, assume one of
    // the streams went EOF early.
    bool eos_reached = src->eos_packets > 0;
    if (eos_reached && src->eos_packets < 100) {
        for (int n = 0; n < src->num_streams; n++) {
            struct virtual_stream *vs = src->streams[n];
            if (vs->selected) {
                int max_packets = 0;
                if (vs->sh->type == STREAM_AUDIO)
                    max_packets = 1;
                if (vs->sh->type == STREAM_VIDEO)
                    max_packets = 16;
                eos_reached &= vs->eos_packets >= max_packets;
            }
        }
    }

    src->eof_reached = false;

    if (eos_reached || !pkt) {
        talloc_free(pkt);

        struct segment *next = NULL;
        for (int n = 0; n < src->num_segments - 1; n++) {
            if (src->segments[n] == seg) {
                next = src->segments[n + 1];
                break;
            }
        }
        if (!next) {
            src->eof_reached = true;
            return false;
        }
        switch_segment(demuxer, src, next, next->start, 0, true);
        return true; // reader will retry
    }

    if (pkt->stream < 0 || pkt->stream >= seg->num_stream_map)
        goto drop;

    if (!src->dash) {
        pkt->segmented = true;
        if (!pkt->codec)
            pkt->codec = demux_get_stream(seg->d, pkt->stream)->codec;
        if (pkt->start == MP_NOPTS_VALUE || pkt->start < seg->start)
            pkt->start = seg->start;
        if (pkt->end == MP_NOPTS_VALUE || pkt->end > seg->end)
            pkt->end = seg->end;
    }

    struct virtual_stream *vs = seg->stream_map[pkt->stream];
    if (!vs)
        goto drop;

    // for refresh seeks, demux.c prefers monotonically increasing packet pos
    // since the packet pos is meaningless anyway for timeline, use it
    if (pkt->pos >= 0)
        pkt->pos |= (seg->index & 0x7FFFULL) << 48;

    if (pkt->pts != MP_NOPTS_VALUE && pkt->pts >= seg->end) {
        // Trust the keyframe flag. Might not always be a good idea, but will
        // be sufficient at least with mkv. The problem is that this flag is
        // not well-defined in libavformat and is container-dependent.
        if (pkt->keyframe || vs->eos_packets == INT_MAX) {
            vs->eos_packets = INT_MAX;
            goto drop;
        } else {
            vs->eos_packets += 1;
        }
    }

    double dts = pkt->dts != MP_NOPTS_VALUE ? pkt->dts : pkt->pts;
    if (src->dts == MP_NOPTS_VALUE || (dts != MP_NOPTS_VALUE && dts > src->dts))
        src->dts = dts;

    pkt->stream = vs->sh->index;
    *out_pkt = pkt;
    return true;

drop:
    talloc_free(pkt);
    return true;
}

static void print_timeline(struct demuxer *demuxer)
{
    struct priv *p = demuxer->priv;

    MP_VERBOSE(demuxer, "Timeline segments:\n");
    for (int x = 0; x < p->num_sources; x++) {
        struct virtual_source *src = p->sources[x];

        if (x >= 1)
            MP_VERBOSE(demuxer, " --- new parallel stream ---\n");

        for (int n = 0; n < src->num_segments; n++) {
            struct segment *seg = src->segments[n];
            int src_num = n;
            for (int i = 0; i < n; i++) {
                if (seg->d && src->segments[i]->d == seg->d) {
                    src_num = i;
                    break;
                }
            }
            MP_VERBOSE(demuxer, " %2d: %12f [%12f] (", n, seg->start, seg->d_start);
            for (int i = 0; i < seg->num_stream_map; i++) {
                struct virtual_stream *vs = seg->stream_map[i];
                MP_VERBOSE(demuxer, "%s%d", i ? " " : "",
                           vs ? vs->sh->index : -1);
            }
            MP_VERBOSE(demuxer, ") %d:'%s'\n", src_num, seg->url);
        }

        if (src->dash)
            MP_VERBOSE(demuxer, " (Using pseudo-DASH mode.)\n");
    }
    MP_VERBOSE(demuxer, "Total duration: %f\n", p->duration);
}

static int d_open(struct demuxer *demuxer, enum demux_check check)
{
    struct priv *p = demuxer->priv = talloc_zero(demuxer, struct priv);
    p->tl = demuxer->params ? demuxer->params->timeline : NULL;
    if (!p->tl || p->tl->num_parts < 1)
        return -1;

    demuxer->chapters = p->tl->chapters;
    demuxer->num_chapters = p->tl->num_chapters;

    struct demuxer *meta = p->tl->track_layout;
    demuxer->metadata = meta->metadata;
    demuxer->attachments = meta->attachments;
    demuxer->num_attachments = meta->num_attachments;
    demuxer->editions = meta->editions;
    demuxer->num_editions = meta->num_editions;
    demuxer->edition = meta->edition;

    for (struct timeline *tl = p->tl; tl; tl = tl->next)
        add_tl(demuxer, tl);

    demuxer->duration = p->duration;

    print_timeline(demuxer);

    demuxer->seekable = true;
    demuxer->partially_seekable = false;

    demuxer->filetype = talloc_asprintf(p, "edl/%s",
                        meta->filetype ? meta->filetype : meta->desc->name);

    reselect_streams(demuxer);

    return 0;
}

static void add_tl(struct demuxer *demuxer, struct timeline *tl)
{
    struct priv *p = demuxer->priv;

    struct virtual_source *src = talloc_ptrtype(p, src);
    *src = (struct virtual_source){
        .tl = tl,
        .dash = tl->dash,
        .dts = MP_NOPTS_VALUE,
    };

    MP_TARRAY_APPEND(p, p->sources, p->num_sources, src);

    p->duration = MPMAX(p->duration, tl->parts[tl->num_parts].start);

    struct demuxer *meta = tl->track_layout;

    int num_streams = demux_get_num_stream(meta);
    for (int n = 0; n < num_streams; n++) {
        struct sh_stream *sh = demux_get_stream(meta, n);
        struct sh_stream *new = demux_alloc_sh_stream(sh->type);
        new->demuxer_id = sh->demuxer_id;
        new->codec = sh->codec;
        new->title = sh->title;
        new->lang = sh->lang;
        new->default_track = sh->default_track;
        new->forced_track = sh->forced_track;
        new->hls_bitrate = sh->hls_bitrate;
        new->missing_timestamps = sh->missing_timestamps;
        new->attached_picture = sh->attached_picture;
        demux_add_sh_stream(demuxer, new);
        struct virtual_stream *vs = talloc_ptrtype(p, vs);
        *vs = (struct virtual_stream){
            .src = src,
            .sh = new,
        };
        MP_TARRAY_APPEND(p, p->streams, p->num_streams, vs);
        assert(demux_get_stream(demuxer, p->num_streams - 1) == new);
        MP_TARRAY_APPEND(src, src->streams, src->num_streams, vs);
    }

    for (int n = 0; n < tl->num_parts; n++) {
        struct timeline_part *part = &tl->parts[n];
        struct timeline_part *next = &tl->parts[n + 1];

        // demux_timeline already does caching, doing it for the sub-demuxers
        // would be pointless and wasteful.
        if (part->source) {
            demux_disable_cache(part->source);
            demuxer->is_network |= part->source->is_network;
        }

        struct segment *seg = talloc_ptrtype(src, seg);
        *seg = (struct segment){
            .d = part->source,
            .url = part->source ? part->source->filename : part->url,
            .lazy = !part->source,
            .d_start = part->source_start,
            .start = part->start,
            .end = next->start,
        };

        associate_streams(demuxer, src, seg);

        seg->index = n;
        MP_TARRAY_APPEND(src, src->segments, src->num_segments, seg);
    }

    demuxer->is_network |= tl->track_layout->is_network;
}

static void d_close(struct demuxer *demuxer)
{
    struct priv *p = demuxer->priv;

    for (int x = 0; x < p->num_sources; x++) {
        struct virtual_source *src = p->sources[x];

        src->current = NULL;
        close_lazy_segments(demuxer, src);
    }

    struct demuxer *master = p->tl->demuxer;
    timeline_destroy(p->tl);
    demux_free(master);
}

static void d_switched_tracks(struct demuxer *demuxer)
{
    reselect_streams(demuxer);
}

const demuxer_desc_t demuxer_desc_timeline = {
    .name = "timeline",
    .desc = "timeline segments",
    .read_packet = d_read_packet,
    .open = d_open,
    .close = d_close,
    .seek = d_seek,
    .switched_tracks = d_switched_tracks,
};
