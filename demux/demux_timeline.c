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

// This represents a single timeline source. (See timeline.pars[]. For each
// timeline_par struct there is a virtual_source.)
struct virtual_source {
    struct timeline_par *tl;

    bool dash, no_clip, delay_open;

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

    struct demux_packet *next;
};

struct priv {
    struct timeline *tl;
    bool owns_tl;

    double duration;

    // As the demuxer user sees it.
    struct virtual_stream **streams;
    int num_streams;

    struct virtual_source **sources;
    int num_sources;
};

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
            if (sh->demuxer_id >= 0 && sh->demuxer_id == vs->sh->demuxer_id)
                other = vs;
        }

        if (!other) {
            MP_WARN(demuxer, "Source stream %d (%s) unused and hidden.\n",
                    n, stream_type_name(sh->type));
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

        for (int n = 0; n < src->num_segments; n++) {
            struct segment *seg = src->segments[n];

            if (!seg->d)
                continue;

            for (int i = 0; i < seg->num_stream_map; i++) {
                bool selected =
                    seg->stream_map[i] && seg->stream_map[i]->selected;

                // This stops demuxer readahead for inactive segments.
                if (!src->current || seg->d != src->current->d)
                    selected = false;
                struct sh_stream *sh = demux_get_stream(seg->d, i);
                demuxer_select_track(seg->d, sh, MP_NOPTS_VALUE, selected);

                update_slave_stats(demuxer, seg->d);
            }
        }

        bool was_selected = src->any_selected;
        src->any_selected = false;

        for (int n = 0; n < src->num_streams; n++)
            src->any_selected |= src->streams[n]->selected;

        if (!was_selected && src->any_selected) {
            src->eof_reached = false;
            src->dts = MP_NOPTS_VALUE;
            TA_FREEP(&src->next);
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
            TA_FREEP(&src->next); // might depend on one of the sub-demuxers
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

    // Note: in delay_open mode, we must _not_ close segments during demuxing,
    // because demuxed packets have demux_packet.codec set to objects owned
    // by the segments. Closing them would create dangling pointers.
    if (!src->delay_open)
        close_lazy_segments(demuxer, src);

    struct demuxer_params params = {
        .init_fragment = src->tl->init_fragment,
        .skip_lavf_probing = src->tl->dash,
        .stream_flags = demuxer->stream_origin,
    };
    src->current->d = demux_open_url(src->current->url, &params,
                                     demuxer->cancel, demuxer->global);
    if (!src->current->d && !demux_cancel_test(demuxer))
        MP_ERR(demuxer, "failed to load segment\n");
    if (src->current->d)
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
    if (!src->no_clip)
        demux_set_ts_offset(new->d, new->start - new->d_start);
    if (!src->no_clip || !init)
        demux_seek(new->d, start_pts, flags);

    for (int n = 0; n < src->num_streams; n++) {
        struct virtual_stream *vs = src->streams[n];
        vs->eos_packets = 0;
    }

    src->eof_reached = false;
    src->eos_packets = 0;
}

static void do_read_next_packet(struct demuxer *demuxer,
                                struct virtual_source *src)
{
    if (src->next)
        return;

    struct segment *seg = src->current;
    if (!seg || !seg->d) {
        src->eof_reached = true;
        return;
    }

    struct demux_packet *pkt = demux_read_any_packet(seg->d);
    if (!pkt || (!src->no_clip && pkt->pts >= seg->end))
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
            return;
        }
        switch_segment(demuxer, src, next, next->start, 0, true);
        return; // reader will retry
    }

    if (pkt->stream < 0 || pkt->stream >= seg->num_stream_map)
        goto drop;

    if (!src->no_clip || src->delay_open) {
        pkt->segmented = true;
        if (!pkt->codec)
            pkt->codec = demux_get_stream(seg->d, pkt->stream)->codec;
    }
    if (!src->no_clip) {
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

    if (pkt->pts != MP_NOPTS_VALUE && !src->no_clip && pkt->pts >= seg->end) {
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
    src->next = pkt;
    return;

drop:
    talloc_free(pkt);
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

    do_read_next_packet(demuxer, src);
    *out_pkt = src->next;
    src->next = NULL;
    return true;
}

static void seek_source(struct demuxer *demuxer, struct virtual_source *src,
                        double pts, int flags)
{
    struct segment *new = src->segments[src->num_segments - 1];
    for (int n = 0; n < src->num_segments; n++) {
        if (pts < src->segments[n]->end) {
            new = src->segments[n];
            break;
        }
    }

    switch_segment(demuxer, src, new, pts, flags, false);

    src->dts = MP_NOPTS_VALUE;
    TA_FREEP(&src->next);
}

static void d_seek(struct demuxer *demuxer, double seek_pts, int flags)
{
    struct priv *p = demuxer->priv;

    seek_pts = seek_pts * ((flags & SEEK_FACTOR) ? p->duration : 1);
    flags &= SEEK_FORWARD | SEEK_HR;

    // The intention is to seek audio streams to the same target as video
    // streams if they are separate streams. Video streams usually have more
    // coarse keyframe snapping, which could leave video without audio.
    struct virtual_source *master = NULL;
    bool has_slaves = false;
    for (int x = 0; x < p->num_sources; x++) {
        struct virtual_source *src = p->sources[x];

        bool any_audio = false, any_video = false;
        for (int i = 0; i < src->num_streams; i++) {
            struct virtual_stream *str = src->streams[i];
            if (str->selected) {
                if (str->sh->type == STREAM_VIDEO)
                    any_video = true;
                if (str->sh->type == STREAM_AUDIO)
                    any_audio = true;
            }
        }

        if (any_video)
            master = src;
        // A true slave stream is audio-only; this also prevents that the master
        // stream is considered a slave stream.
        if (any_audio && !any_video)
            has_slaves = true;
    }

    if (!has_slaves)
        master = NULL;

    if (master) {
        seek_source(demuxer, master, seek_pts, flags);
        do_read_next_packet(demuxer, master);
        if (master->next && master->next->pts != MP_NOPTS_VALUE) {
            // Assume we got a seek target. Actually apply the heuristic.
            MP_VERBOSE(demuxer, "adjust seek target from %f to %f\n", seek_pts,
                       master->next->pts);
            seek_pts = master->next->pts;
            flags &= ~(unsigned)SEEK_FORWARD;
        }
    }

    for (int x = 0; x < p->num_sources; x++) {
        struct virtual_source *src = p->sources[x];
        if (src != master && src->any_selected)
            seek_source(demuxer, src, seek_pts, flags);
    }
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
            MP_VERBOSE(demuxer, " %2d: %12f - %12f [%12f] (",
                       n, seg->start, seg->end, seg->d_start);
            for (int i = 0; i < seg->num_stream_map; i++) {
                struct virtual_stream *vs = seg->stream_map[i];
                MP_VERBOSE(demuxer, "%s%d", i ? " " : "",
                           vs ? vs->sh->index : -1);
            }
            MP_VERBOSE(demuxer, ")\n  source %d:'%s'\n", src_num, seg->url);
        }

        if (src->dash)
            MP_VERBOSE(demuxer, " (Using pseudo-DASH mode.)\n");
    }
    MP_VERBOSE(demuxer, "Total duration: %f\n", p->duration);
}

// Copy various (not all) metadata fields from src to dst, but try not to
// overwrite fields in dst that are unset in src.
// May keep data from src by reference.
// Imperfect and arbitrary, only suited for EDL stuff.
static void apply_meta(struct sh_stream *dst, struct sh_stream *src)
{
    if (src->demuxer_id >= 0)
        dst->demuxer_id = src->demuxer_id;
    if (src->title)
        dst->title = src->title;
    if (src->lang)
        dst->lang = src->lang;
    dst->default_track = src->default_track;
    dst->forced_track = src->forced_track;
    if (src->hls_bitrate)
        dst->hls_bitrate = src->hls_bitrate;
    dst->missing_timestamps = src->missing_timestamps;
    if (src->attached_picture)
        dst->attached_picture = src->attached_picture;
    dst->image = src->image;
}

// This is mostly for EDL user-defined metadata.
static struct sh_stream *find_matching_meta(struct timeline_par *tl, int index)
{
    for (int n = 0; n < tl->num_sh_meta; n++) {
        struct sh_stream *sh = tl->sh_meta[n];
        if (sh->index == index || sh->index < 0)
            return sh;
    }
    return NULL;
}

static bool add_tl(struct demuxer *demuxer, struct timeline_par *tl)
{
    struct priv *p = demuxer->priv;

    struct virtual_source *src = talloc_ptrtype(p, src);
    *src = (struct virtual_source){
        .tl = tl,
        .dash = tl->dash,
        .delay_open = tl->delay_open,
        .no_clip = tl->no_clip || tl->dash,
        .dts = MP_NOPTS_VALUE,
    };

    if (!tl->num_parts)
        return false;

    MP_TARRAY_APPEND(p, p->sources, p->num_sources, src);

    p->duration = MPMAX(p->duration, tl->parts[tl->num_parts - 1].end);

    struct demuxer *meta = tl->track_layout;

    // delay_open streams normally have meta==NULL, and 1 virtual stream
    int num_streams = 0;
    if (tl->delay_open) {
        num_streams = tl->num_sh_meta;
    } else if (meta) {
        num_streams = demux_get_num_stream(meta);
    }
    for (int n = 0; n < num_streams; n++) {
        struct sh_stream *new = NULL;

        if (tl->delay_open) {
            struct sh_stream *tsh = tl->sh_meta[n];
            new = demux_alloc_sh_stream(tsh->type);
            new->codec = tsh->codec;
            apply_meta(new, tsh);
            demuxer->is_network = true;
            demuxer->is_streaming = true;
        } else {
            struct sh_stream *sh = demux_get_stream(meta, n);
            new = demux_alloc_sh_stream(sh->type);
            apply_meta(new, sh);
            new->codec = sh->codec;
            struct sh_stream *tsh = find_matching_meta(tl, n);
            if (tsh)
                apply_meta(new, tsh);
        }

        demux_add_sh_stream(demuxer, new);
        struct virtual_stream *vs = talloc_ptrtype(p, vs);
        *vs = (struct virtual_stream){
            .src = src,
            .sh = new,
        };
        MP_TARRAY_APPEND(p, p->streams, p->num_streams, vs);
        mp_assert(demux_get_stream(demuxer, p->num_streams - 1) == new);
        MP_TARRAY_APPEND(src, src->streams, src->num_streams, vs);
    }

    for (int n = 0; n < tl->num_parts; n++) {
        struct timeline_part *part = &tl->parts[n];

        // demux_timeline already does caching, doing it for the sub-demuxers
        // would be pointless and wasteful.
        if (part->source) {
            demuxer->is_network |= part->source->is_network;
            demuxer->is_streaming |= part->source->is_streaming;
        }

        if (!part->source)
            mp_assert(tl->dash || tl->delay_open);

        struct segment *seg = talloc_ptrtype(src, seg);
        *seg = (struct segment){
            .d = part->source,
            .url = part->source ? part->source->filename : part->url,
            .lazy = !part->source,
            .d_start = part->source_start,
            .start = part->start,
            .end = part->end,
        };

        associate_streams(demuxer, src, seg);

        seg->index = n;
        MP_TARRAY_APPEND(src, src->segments, src->num_segments, seg);
    }

    if (tl->track_layout) {
        demuxer->is_network |= tl->track_layout->is_network;
        demuxer->is_streaming |= tl->track_layout->is_streaming;
    }
    return true;
}

static int d_open(struct demuxer *demuxer, enum demux_check check)
{
    struct priv *p = demuxer->priv = talloc_zero(demuxer, struct priv);
    p->tl = demuxer->params ? demuxer->params->timeline : NULL;
    if (!p->tl || p->tl->num_pars < 1)
        return -1;

    demuxer->chapters = p->tl->chapters;
    demuxer->num_chapters = p->tl->num_chapters;

    struct demuxer *meta = p->tl->meta;
    if (meta) {
        demuxer->metadata = meta->metadata;
        demuxer->attachments = meta->attachments;
        demuxer->num_attachments = meta->num_attachments;
        demuxer->editions = meta->editions;
        demuxer->num_editions = meta->num_editions;
        demuxer->edition = meta->edition;
    }

    for (int n = 0; n < p->tl->num_pars; n++) {
        if (!add_tl(demuxer, p->tl->pars[n]))
            return -1;
    }

    if (!p->num_sources)
        return -1;

    demuxer->is_network |= p->tl->is_network;
    demuxer->is_streaming |= p->tl->is_streaming;

    demuxer->duration = p->duration;

    print_timeline(demuxer);

    demuxer->seekable = true;
    demuxer->partially_seekable = false;

    const char *format_name = "unknown";
    if (meta)
        format_name = meta->filetype ? meta->filetype : meta->desc->name;
    demuxer->filetype = talloc_asprintf(p, "%s/%s", p->tl->format, format_name);

    reselect_streams(demuxer);

    p->owns_tl = true;
    return 0;
}

static void d_close(struct demuxer *demuxer)
{
    struct priv *p = demuxer->priv;

    for (int x = 0; x < p->num_sources; x++) {
        struct virtual_source *src = p->sources[x];

        src->current = NULL;
        TA_FREEP(&src->next);
        close_lazy_segments(demuxer, src);
    }

    if (p->owns_tl) {
        struct demuxer *master = p->tl->demuxer;
        timeline_destroy(p->tl);
        demux_free(master);
    }
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
