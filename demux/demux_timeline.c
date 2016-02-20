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

struct segment {
    int index;
    double start, end;
    double d_start;
    struct demuxer *d;
    // stream_map[sh_stream.index] = index into priv.streams, where sh_stream
    // is a stream from the source d. It's used to map the streams of the
    // source onto the set of streams of the virtual timeline.
    // Uses -1 for streams that do not appear in the virtual timeline.
    int *stream_map;
    int num_stream_map;
};

// Information for each stream on the virtual timeline. (Mirrors streams
// exposed by demux_timeline.)
struct virtual_stream {
    struct sh_stream *sh;       // stream exported by demux_timeline
    bool selected;              // ==demux_stream_is_selected(sh)
    bool new_segment;           // whether a new segment needs to be signaled
    int eos_packets;            // deal with b-frame delay
};

struct priv {
    struct timeline *tl;

    double duration;

    struct segment **segments;
    int num_segments;
    struct segment *current;

    // As the demuxer user sees it.
    struct virtual_stream *streams;
    int num_streams;

    // Total number of packets received past end of segment. Used
    // to be clever about determining when to switch segments.
    int eos_packets;

    double seek_pts;
};

static void reselect_streams(struct demuxer *demuxer)
{
    struct priv *p = demuxer->priv;

    for (int n = 0; n < p->num_streams; n++) {
        struct virtual_stream *vs = &p->streams[n];
        vs->selected = demux_stream_is_selected(vs->sh);
    }

    for (int n = 0; n < p->num_segments; n++) {
        struct segment *seg = p->segments[n];
        for (int i = 0; i < seg->num_stream_map; i++) {
            struct sh_stream *sh = demux_get_stream(seg->d, i);
            bool selected = false;
            if (seg->stream_map[i] >= 0)
                selected = p->streams[seg->stream_map[i]].selected;
            // This stops demuxer readahead for inactive segments.
            if (!p->current || seg->d != p->current->d)
                selected = false;
            demuxer_select_track(seg->d, sh, selected);
        }
    }
}

static void switch_segment(struct demuxer *demuxer, struct segment *new,
                           double start_pts, int flags)
{
    struct priv *p = demuxer->priv;

    if (p->current == new)
        return;

    if (!(flags & (SEEK_FORWARD | SEEK_BACKWARD)))
        flags |= SEEK_BACKWARD;

    MP_VERBOSE(demuxer, "switch to segment %d\n", new->index);

    p->current = new;
    reselect_streams(demuxer);
    demux_set_ts_offset(new->d, new->start - new->d_start);
    demux_seek(new->d, start_pts, flags | SEEK_ABSOLUTE);

    for (int n = 0; n < p->num_streams; n++) {
        struct virtual_stream *vs = &p->streams[n];
        vs->new_segment = true;
        vs->eos_packets = 0;
    }

    p->eos_packets = 0;
}

static void d_seek(struct demuxer *demuxer, double rel_seek_secs, int flags)
{
    struct priv *p = demuxer->priv;

    double pts = p->seek_pts;
    if (flags & SEEK_ABSOLUTE)
        pts = 0.0f;

    if (flags & SEEK_FACTOR) {
        pts += p->duration * rel_seek_secs;
    } else {
        pts += rel_seek_secs;
    }

    flags &= SEEK_FORWARD | SEEK_BACKWARD | SEEK_HR;

    struct segment *new = p->segments[p->num_segments - 1];
    for (int n = 0; n < p->num_segments; n++) {
        if (pts < p->segments[n]->end) {
            new = p->segments[n];
            break;
        }
    }

    p->current = NULL; // force seek
    switch_segment(demuxer, new, pts, flags);

    p->seek_pts = pts;
}

static int d_fill_buffer(struct demuxer *demuxer)
{
    struct priv *p = demuxer->priv;

    if (!p->current)
        switch_segment(demuxer, p->segments[0], 0, 0);

    struct segment *seg = p->current;

    struct demux_packet *pkt = demux_read_any_packet(seg->d);
    if (!pkt || pkt->pts >= seg->end)
        p->eos_packets += 1;

    // Test for EOF. Do this here to properly run into EOF even if other
    // streams are disabled etc. If it somehow doesn't manage to reach the end
    // after demuxing a high (bit arbitrary) number of packets, assume one of
    // the streams went EOF early.
    bool eos_reached = p->eos_packets > 0;
    if (eos_reached && p->eos_packets < 100) {
        for (int n = 0; n < p->num_streams; n++) {
            struct virtual_stream *vs = &p->streams[n];
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

    if (eos_reached || !pkt) {
        talloc_free(pkt);

        struct segment *next = NULL;
        for (int n = 0; n < p->num_segments - 1; n++) {
            if (p->segments[n] == seg) {
                next = p->segments[n + 1];
                break;
            }
        }
        if (!next)
            return 0;
        switch_segment(demuxer, next, next->start, 0);
        return 1; // reader will retry
    }

    if (pkt->stream < 0 || pkt->stream > seg->num_stream_map)
        goto drop;

    if (!pkt->codec)
        pkt->codec = demux_get_stream(seg->d, pkt->stream)->codec;

    if (pkt->start == MP_NOPTS_VALUE || pkt->start < seg->start)
        pkt->start = seg->start;
    if (pkt->end == MP_NOPTS_VALUE || pkt->end > seg->end)
        pkt->end = seg->end;

    pkt->stream = seg->stream_map[pkt->stream];
    if (pkt->stream < 0)
        goto drop;

    struct virtual_stream *vs = &p->streams[pkt->stream];

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

    pkt->new_segment |= vs->new_segment;
    vs->new_segment = false;

    demux_add_packet(vs->sh, pkt);
    return 1;

drop:
    talloc_free(pkt);
    return 1;
}

static void print_timeline(struct demuxer *demuxer)
{
    struct priv *p = demuxer->priv;

    MP_VERBOSE(demuxer, "Timeline segments:\n");
    for (int n = 0; n < p->num_segments; n++) {
        struct segment *seg = p->segments[n];
        int src_num = -1;
        for (int i = 0; i < p->tl->num_sources; i++) {
            if (p->tl->sources[i] == seg->d) {
                src_num = i;
                break;
            }
        }
        MP_VERBOSE(demuxer, " %2d: %12f [%12f] (", n, seg->start, seg->d_start);
        for (int i = 0; i < seg->num_stream_map; i++)
            MP_VERBOSE(demuxer, "%s%d", i ? " " : "", seg->stream_map[i]);
        MP_VERBOSE(demuxer, ") %d:'%s'\n", src_num, seg->d->filename);
    }
    MP_VERBOSE(demuxer, "Total duration: %f\n", p->duration);
}

static bool target_stream_used(struct segment *seg, int target_index)
{
    for (int n = 0; n < seg->num_stream_map; n++) {
        if (seg->stream_map[n] == target_index)
            return true;
    }
    return false;
}

// Create mapping from segment streams to virtual timeline streams.
static void associate_streams(struct demuxer *demuxer, struct segment *seg)
{
    struct priv *p = demuxer->priv;

    int counts[STREAM_TYPE_COUNT] = {0};

    int num_streams = demux_get_num_stream(seg->d);
    for (int n = 0; n < num_streams; n++) {
        struct sh_stream *sh = demux_get_stream(seg->d, n);
        // Try associating by demuxer ID (supposedly useful for ordered chapters).
        struct sh_stream *other =
            demuxer_stream_by_demuxer_id(demuxer, sh->type, sh->demuxer_id);
        if (!other || !target_stream_used(seg, other->index)) {
            // Try to associate the first unused stream with matching media type.
            for (int i = 0; i < p->num_streams; i++) {
                struct sh_stream *cur = p->streams[i].sh;
                if (cur->type == sh->type && !target_stream_used(seg, cur->index))
                {
                    other = cur;
                    break;
                }
            }
        }

        MP_TARRAY_APPEND(seg, seg->stream_map, seg->num_stream_map,
                         other ? other->index : -1);

        counts[sh->type] += 1;
    }
}

static int d_open(struct demuxer *demuxer, enum demux_check check)
{
    struct priv *p = demuxer->priv = talloc_zero(demuxer, struct priv);
    p->tl = demuxer->params ? demuxer->params->timeline : NULL;
    if (!p->tl || p->tl->num_parts < 1)
        return -1;

    p->duration = p->tl->parts[p->tl->num_parts].start;

    demuxer->chapters = p->tl->chapters;
    demuxer->num_chapters = p->tl->num_chapters;

    struct demuxer *meta = p->tl->track_layout;
    demuxer->metadata = meta->metadata;
    demuxer->attachments = meta->attachments;
    demuxer->num_attachments = meta->num_attachments;

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
        demux_add_sh_stream(demuxer, new);
        struct virtual_stream vs = {
            .sh = new,
        };
        MP_TARRAY_APPEND(p, p->streams, p->num_streams, vs);
    }

    for (int n = 0; n < p->tl->num_parts; n++) {
        struct timeline_part *part = &p->tl->parts[n];
        struct timeline_part *next = &p->tl->parts[n + 1];

        struct segment *seg = talloc_ptrtype(p, seg);
        *seg = (struct segment){
            .d = part->source,
            .d_start = part->source_start,
            .start = part->start,
            .end = next->start,
        };

        associate_streams(demuxer, seg);

        seg->index = n;
        MP_TARRAY_APPEND(p, p->segments, p->num_segments, seg);
    }

    print_timeline(demuxer);

    demuxer->seekable = true;
    demuxer->partially_seekable = true;

    reselect_streams(demuxer);

    return 0;
}

static void d_close(struct demuxer *demuxer)
{
    struct priv *p = demuxer->priv;
    struct demuxer *master = p->tl->demuxer;
    timeline_destroy(p->tl);
    free_demuxer(master);
}

static int d_control(struct demuxer *demuxer, int cmd, void *arg)
{
    struct priv *p = demuxer->priv;

    switch (cmd) {
    case DEMUXER_CTRL_GET_TIME_LENGTH: {
        *(double *)arg = p->duration;
        return DEMUXER_CTRL_OK;
    }
    case DEMUXER_CTRL_SWITCHED_TRACKS:
        reselect_streams(demuxer);
        return DEMUXER_CTRL_OK;
    }
    return DEMUXER_CTRL_NOTIMPL;
}

const demuxer_desc_t demuxer_desc_timeline = {
    .name = "timeline",
    .desc = "timeline segment merging wrapper",
    .fill_buffer = d_fill_buffer,
    .open = d_open,
    .close = d_close,
    .seek = d_seek,
    .control = d_control,
};
