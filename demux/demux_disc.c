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

#include <inttypes.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "common/common.h"
#include "common/msg.h"

#include "stream/stream.h"
#include "video/mp_image.h"
#include "demux.h"
#include "packet.h"
#include "stheader.h"

// DVD-Video has 32 subpicture (SPU) streams, mapped to PES substream IDs 0x20..0x3F.
#define MAX_DVD_SPU_STREAMS 32

// The disc stream describes its data as segments (a DVD VOBU, a Blu-ray
// clip): where each starts in the byte stream, how its timestamps map to
// playback time, and which of the streams the disc declares have data in it.
// Packets are mapped by their byte position, so the demuxer neither guesses
// timeline boundaries from timestamps nor the presence of a stream from the
// packets it fails to see.

struct priv {
    struct demuxer *slave;

    // All outer sh_streams we have ever surfaced to the parent demuxer.
    struct sh_stream **outer_streams;
    int num_outer_streams;

    // Maps the current slave's stream index to its matching outer sh_stream.
    struct sh_stream **slave_to_outer;
    int slave_to_outer_count;

    // Per outer stream, indexed by sh->index.
    struct outer_state {
        bool bare;          // declared by the disc, no codec parameters yet
        int64_t seg;        // id of the segment its last packet mapped into
    } *ostate;
    int num_ostate;

    // The streams the disc declares for what it plays.
    struct sh_stream **decl;
    int num_decl;

    // The segments collected from the stream, oldest first. segs[i] has the
    // id first_seg_id + i, ids keep counting across seeks.
    struct stream_segment *segs;
    int num_segs;
    int64_t first_seg_id;
    int64_t applied_seg;    // id of the segment whose presence was applied

    // Drop audio before this playback time, see d_seek().
    double skip_until;

    // DVD-only: retain the last SPU packet per substream. The menu subpicture
    // is one-shot on an unseekable stream; re-deliver it on (re)selection and
    // demux_nav_refresh(), like a refresh seek for ordinary streams.
    struct dvd_sub_hold {
        struct demux_packet *pkt;   // clone, with playback-rebased timestamps
        struct sh_stream *sh;       // outer stream it belongs to
        bool pending;               // re-deliver on next read
    } dvd_sub_hold[MAX_DVD_SPU_STREAMS];

    bool seek_reinit;   // refresh the metadata after a seek
    uint32_t last_discontinuity_id; // Last source-position-jump id seen from the stream.
    bool nav_active;    // last interactive-nav state pushed to the cache
    uint32_t eof_log_id;    // last logged EOF state, avoids per-read spam
    bool eof_log_still;
    int disc_audio_id; // last audio track selected by the disc itself, -1 = unknown

    bool is_dvd, is_dvda, is_cdda, is_bd;

    // DVD-Audio still-image video track.
    struct sh_stream *still_sh;
    int last_still_id;
    struct demux_packet *pending_pkt; // still or the audio deferred behind it
};

static void clear_dvd_sub_holds(struct priv *p)
{
    for (int i = 0; i < MAX_DVD_SPU_STREAMS; i++) {
        talloc_free(p->dvd_sub_hold[i].pkt);
        p->dvd_sub_hold[i] = (struct dvd_sub_hold){0};
    }
}

// Mark retained subpictures of selected streams for re-delivery.
static void arm_dvd_sub_holds(struct priv *p)
{
    for (int i = 0; i < MAX_DVD_SPU_STREAMS; i++) {
        struct dvd_sub_hold *h = &p->dvd_sub_hold[i];
        if (h->pkt && h->sh && demux_stream_is_selected(h->sh))
            h->pending = true;
    }
}

static struct outer_state *get_ostate(struct priv *p, struct sh_stream *sh)
{
    if (sh->index >= p->num_ostate) {
        int old = p->num_ostate;
        MP_TARRAY_GROW(p, p->ostate, sh->index);
        p->num_ostate = sh->index + 1;
        for (int n = old; n < p->num_ostate; n++)
            p->ostate[n] = (struct outer_state){.seg = -1};
    }
    return &p->ostate[sh->index];
}

// Tell the disc VM which audio track is playing, but only when the user moved
// it. Echoing back what the disc just chose would fight it during transitions.
static void push_active_track(demuxer_t *demuxer)
{
    struct priv *p = demuxer->priv;
    if (!p->is_dvd)
        return;

    int selected = -1;
    for (int i = 0; i < p->num_outer_streams; i++) {
        struct sh_stream *sh = p->outer_streams[i];
        if (sh && sh->type == STREAM_AUDIO && demux_stream_is_selected(sh)) {
            selected = sh->demuxer_id;
            break;
        }
    }
    if (selected < 0 || selected == p->disc_audio_id)
        return;

    struct stream_track_req req = {.type = STREAM_AUDIO, .id = selected};
    if (stream_control(demuxer->stream, STREAM_CTRL_SET_ACTIVE_TRACK, &req) >= 1)
        p->disc_audio_id = selected;
}

// The slave delivers every presentation stream, selection is applied to
// the outer streams when their packets pass through.
static void reselect_streams(demuxer_t *demuxer)
{
    struct priv *p = demuxer->priv;
    push_active_track(demuxer);
    arm_dvd_sub_holds(p);
}

static void d_nav_refresh(demuxer_t *demuxer)
{
    struct priv *p = demuxer->priv;
    arm_dvd_sub_holds(p);
}

static void get_disc_track_info(struct stream *stream, struct sh_stream *sh, bool dvd)
{
    struct stream_track_req req = {.type = sh->type, .id = sh->demuxer_id};
    if (dvd && sh->type == STREAM_SUB)
        req.id = req.id & 0x1F; // mpeg ID to index
    stream_control(stream, STREAM_CTRL_GET_TRACK_INFO, &req);
    if (req.name[0])
        sh->lang = talloc_strdup(sh, req.name);
    sh->forced_track |= !!(req.flags & TRACK_FORCED);
    sh->visual_impaired_track |= !!(req.flags & TRACK_VISUAL_IMPAIRED);
    sh->hearing_impaired_track |= !!(req.flags & TRACK_HEARING_IMPAIRED);
    sh->commentary_track |= !!(req.flags & TRACK_COMMENTARY);
}

// A DVD keeps the subtitle palette in the PGC, not in the subpicture data, so
// hand it to the decoder as emulated extradata.
static void set_dvd_sub_palette(struct demuxer *demuxer, struct sh_stream *sh)
{
    struct stream_dvd_info_req info;
    if (stream_control(demuxer->stream, STREAM_CTRL_GET_DVD_INFO, &info) <= 0)
        return;

    char *s = talloc_strdup(sh, "palette: ");
    for (int i = 0; i < 16; i++)
        s = talloc_asprintf_append(s, "%s%06x", i ? ", " : "", info.palette[i]);
    s = talloc_asprintf_append(s, "\n");

    talloc_free(sh->codec->extradata);
    sh->codec->extradata = s;
    sh->codec->extradata_size = strlen(s);
}

static void set_disc_aspect(struct demuxer *demuxer, struct sh_stream *sh)
{
    double ar;
    if (sh->type != STREAM_VIDEO || sh->codec->disp_w <= 0 ||
        sh->codec->disp_h <= 0 ||
        stream_control(demuxer->stream, STREAM_CTRL_GET_ASPECT_RATIO, &ar)
            != STREAM_OK)
        return;
    struct mp_image_params f = {.w = sh->codec->disp_w, .h = sh->codec->disp_h};
    mp_image_params_set_dsize(&f, 1728 * ar, 1728);
    sh->codec->par_w = f.p_w;
    sh->codec->par_h = f.p_h;
}

// Take ownership of a slave sh_stream's codec params into the outer demuxer
// so it survives a slave reopen.
static void adopt_codec_params(struct sh_stream *outer, struct sh_stream *src)
{
    if (outer->codec != src->codec) {
        if (!outer->ds)
            talloc_free(outer->codec);
        outer->codec = src->codec;
        talloc_steal(outer, outer->codec);
    }
    outer->codec->first_packet = NULL;
    outer->codec->decoder = NULL;
    outer->codec->decoder_desc = NULL;
}

// Whether a slave stream carries a Blu-ray presentation stream the player
// decodes itself.
static bool bd_stream_pid_valid(struct sh_stream *src)
{
    int id = src->demuxer_id;
    switch (src->type) {
    case STREAM_VIDEO: // primary (0x1011) + additional streams such as the
                       // Dolby Vision EL (0x1015), secondary video (PiP)
        return (id >= 0x1011 && id <= 0x101F) ||
               (id >= 0x1B00 && id <= 0x1B1F);
    case STREAM_AUDIO: // primary, secondary
        return (id >= 0x1100 && id <= 0x111F) ||
               (id >= 0x1A00 && id <= 0x1A1F);
    case STREAM_SUB:   // PG, TextST
        return (id >= 0x1200 && id <= 0x12FF) ||
               (id >= 0x1800 && id <= 0x181F);
    default:
        return false;
    }
}

// Find the outer stream for a (type, id). `ordinal` disambiguates multiple
// slave streams sharing one: a BD TrueHD PID carries the TrueHD stream and
// its embedded AC-3 compatibility core as two lavf streams with the same id,
// and each must keep its own outer track (and decoder).
static struct sh_stream *find_outer(struct priv *p, int type, int id,
                                    int ordinal)
{
    for (int i = 0; i < p->num_outer_streams; i++) {
        struct sh_stream *sh = p->outer_streams[i];
        if (sh && sh->type == type && sh->demuxer_id == id && ordinal-- == 0)
            return sh;
    }
    return NULL;
}

// Register the streams the disc declares for what it is playing, so they
// exist, and can be selected, before their data arrives. Those already known
// by (type, id) are kept.
static void register_declared_streams(struct demuxer *demuxer)
{
    struct priv *p = demuxer->priv;
    struct stream_streams_req req = {0};
    if (stream_control(demuxer->stream, STREAM_CTRL_GET_STREAMS, &req) < 1)
        return;
    struct stream_nav_state nav = {0};
    int active_audio = -1;
    if (stream_control(demuxer->stream, STREAM_CTRL_GET_NAV_STATE, &nav) >= 1)
        active_audio = nav.active_audio_id;

    p->num_decl = 0;
    for (int n = 0; n < req.num; n++) {
        struct stream_decl_stream *d = &req.streams[n];
        struct sh_stream *sh = find_outer(p, d->type, d->id, 0);
        if (!sh) {
            sh = demux_alloc_sh_stream(d->type);
            sh->demuxer_id = d->id;
            sh->codec->codec = d->codec;
            if (d->type == STREAM_AUDIO && d->id == active_audio) {
                MP_VERBOSE(demuxer, "marking audio id 0x%x as default track\n",
                           active_audio);
                sh->default_track = true;
            }
            get_disc_track_info(demuxer->stream, sh, p->is_dvd);
            if (p->is_dvd && sh->type == STREAM_SUB)
                set_dvd_sub_palette(demuxer, sh);
            MP_VERBOSE(demuxer, "declared stream: type=%s id=0x%x codec=%s\n",
                       stream_type_name(d->type), d->id,
                       d->codec ? d->codec : "?");
            MP_TARRAY_APPEND(p, p->outer_streams, p->num_outer_streams, sh);
            demux_add_sh_stream(demuxer, sh);
            get_ostate(p, sh)->bare = true;
        }
        MP_TARRAY_APPEND(p, p->decl, p->num_decl, sh);
    }
    talloc_free(req.streams);
}

// Build / rebuild the slave-index -> outer-sh map. For each slave stream reuse
// or register a fresh outer sh_stream as follows and expose it to the parent demuxer.
static void sync_streams(struct demuxer *demuxer)
{
    struct priv *p = demuxer->priv;

    int nav_audio_id = -1;
    struct stream_nav_state nav = {0};
    if (p->is_dvd && stream_control(demuxer->stream, STREAM_CTRL_GET_NAV_STATE, &nav) >= 1) {
        nav_audio_id = nav.active_audio_id;
        p->disc_audio_id = nav_audio_id;
    }

    int num_slave = demux_get_num_stream(p->slave);
    if (num_slave > p->slave_to_outer_count) {
        MP_TARRAY_GROW(p, p->slave_to_outer, num_slave - 1);
        for (int n = p->slave_to_outer_count; n < num_slave; n++)
            p->slave_to_outer[n] = NULL;
        p->slave_to_outer_count = num_slave;
    }

    for (int n = 0; n < num_slave; n++) {
        struct sh_stream *src = demux_get_stream(p->slave, n);

        if (p->is_bd && !bd_stream_pid_valid(src)) {
            MP_VERBOSE(demuxer, "ignoring non-presentation stream: "
                       "type=%s id=0x%x codec=%s\n",
                       stream_type_name(src->type), src->demuxer_id,
                       src->codec->codec ? src->codec->codec : "?");
            continue;
        }

        // Ordinal of this slave stream among those sharing its (type, id).
        int ordinal = 0;
        for (int m = 0; m < n; m++) {
            struct sh_stream *prev = demux_get_stream(p->slave, m);
            if (prev->type == src->type && prev->demuxer_id == src->demuxer_id)
                ordinal++;
        }

        struct sh_stream *outer = find_outer(p, src->type, src->demuxer_id,
                                             ordinal);

        if (!outer) {
            MP_VERBOSE(demuxer, "new stream: slave=%d type=%s id=0x%x codec=%s\n",
                       n, stream_type_name(src->type), src->demuxer_id,
                       src->codec->codec ? src->codec->codec : "?");
            outer = demux_alloc_sh_stream(src->type);
            adopt_codec_params(outer, src);
            outer->demuxer_id = src->demuxer_id;
            outer->dependent_track = src->dependent_track;
            if (src->type == STREAM_AUDIO && src->demuxer_id == nav_audio_id) {
                MP_VERBOSE(demuxer, "marking audio id 0x%x as default track\n",
                           nav_audio_id);
                outer->default_track = true;
            }
            set_disc_aspect(demuxer, outer);
            get_disc_track_info(demuxer->stream, outer, p->is_dvd);
            if (p->is_dvd && outer->type == STREAM_SUB)
                set_dvd_sub_palette(demuxer, outer);
            MP_TARRAY_APPEND(p, p->outer_streams, p->num_outer_streams, outer);
            demux_add_sh_stream(demuxer, outer);
        } else if (get_ostate(p, outer)->bare) {
            // A declared stream meets its data. Subtitles keep what the disc
            // declared (the DVD palette), the slave adds nothing to it.
            MP_VERBOSE(demuxer, "stream %d (id=0x%x) %s parameters arrived\n",
                       n, src->demuxer_id,
                       src->codec->codec ? src->codec->codec : "?");
            if (outer->type != STREAM_SUB) {
                adopt_codec_params(outer, src);
                set_disc_aspect(demuxer, outer);
            }
            get_ostate(p, outer)->bare = false;
        } else if (outer->type != STREAM_SUB && outer->codec && src->codec) {
            // Codec change on a reused outer, mostly useful for BD menus, which
            // may be MPEG-2 while the video track is H.264.
            const char *new_codec = src->codec->codec;
            const char *cur_codec = outer->codec->codec;
            if (new_codec && cur_codec && strcmp(new_codec, cur_codec) != 0) {
                MP_VERBOSE(demuxer, "stream %d (id=0x%x) codec changed: %s -> %s\n",
                           n, src->demuxer_id, cur_codec, new_codec);
                adopt_codec_params(outer, src);
                set_disc_aspect(demuxer, outer);
            }
        }

        p->slave_to_outer[n] = outer;
    }

    // Mirror slave sh_stream_group onto the outer sh_streams. This is needed
    // for the Dolby Vision BL+EL group, it's detected well by lavf. We could use
    // the libbluray `dv_streams[]` info, but it's not available yet in release
    // version, and mapping it through lavf is less code. The group has to form
    // before the selection is propagated to the slave below.
    for (int n = 0; n < num_slave; n++) {
        struct sh_stream *outer = p->slave_to_outer[n];
        if (!outer || outer->group)
            continue;
        struct sh_stream *src = demux_get_stream(p->slave, n);
        if (!src || !src->group)
            continue;
        struct sh_stream_group *grp = talloc_zero(outer, struct sh_stream_group);
        for (int m = 0; m < src->group->num_members; m++) {
            struct sh_stream *member = src->group->members[m];
            if (!member || member->index < 0 ||
                member->index >= p->slave_to_outer_count)
                continue;
            struct sh_stream *outer_member = p->slave_to_outer[member->index];
            if (!outer_member)
                continue;
            MP_TARRAY_APPEND(grp, grp->members, grp->num_members, outer_member);
        }
        demux_set_stream_group(demuxer, grp);
    }

    // Outer demuxer may have seen PMT with track that is not longer present in
    // the stream. Mark such tracks as absent, so that they don't deliver packets.
    // For now limited to dependent_tracks, to work around a missing DV EL.
    // All other tracks stays as in in outer demuxer and are deselected.
    // There is no way to "remove" a track from outer demuxer.
    if (p->is_bd) {
        for (int i = 0; i < p->num_outer_streams; i++) {
            struct sh_stream *sh = p->outer_streams[i];
            if (!sh || !sh->dependent_track)
                continue;
            bool mapped = false;
            for (int n = 0; n < num_slave && !mapped; n++)
                mapped = p->slave_to_outer[n] == sh;
            demux_set_stream_absent(demuxer, sh, !mapped);
        }
    }

    // The slave delivers every mapped stream. Unmapped slave streams (ignored
    // non-presentation streams) are kept deselected so they don't deliver
    // packets.
    for (int n = 0; n < num_slave; n++) {
        demuxer_select_track(p->slave, demux_get_stream(p->slave, n),
                             MP_NOPTS_VALUE, !!p->slave_to_outer[n]);
    }
}

static void reset_segments(struct priv *p)
{
    p->first_seg_id += p->num_segs;
    TA_FREEP(&p->segs);
    p->num_segs = 0;
    p->applied_seg = -1;
    for (int n = 0; n < p->num_ostate; n++)
        p->ostate[n].seg = -1;
}

static struct stream_segment *seg_by_id(struct priv *p, int64_t id)
{
    int64_t i = id - p->first_seg_id;
    return i >= 0 && i < p->num_segs ? &p->segs[i] : NULL;
}

// Collect the segments the stream recorded since the last call.
static void pull_segments(struct demuxer *demuxer)
{
    struct priv *p = demuxer->priv;
    struct stream_segments_req req = {0};
    if (stream_control(demuxer->stream, STREAM_CTRL_GET_SEGMENTS, &req) < 1)
        return;
    for (int n = 0; n < req.num; n++) {
        struct stream_segment *s = &req.segs[n];
        MP_DBG(demuxer, "segment %"PRId64": pos=%"PRId64" raw=%f time=%f "
               "present=%d\n", p->first_seg_id + p->num_segs, s->pos,
               s->raw_base, s->time, s->present ? s->num_present : -1);
        MP_TARRAY_APPEND(p, p->segs, p->num_segs, *s);
        talloc_steal(p->segs, s->present);
    }
    talloc_free(req.segs);

    // Drop the segments no delivering stream is still behind.
    int64_t keep = p->applied_seg;
    for (int i = 0; i < p->num_outer_streams; i++) {
        struct sh_stream *sh = p->outer_streams[i];
        if (!sh || !demux_stream_is_selected(sh) || sh->absent)
            continue;
        int64_t seg = get_ostate(p, sh)->seg;
        if (seg >= 0 && seg < keep)
            keep = seg;
    }
    int64_t drop = keep - p->first_seg_id;
    if (drop > 0 && drop < p->num_segs) {
        for (int n = 0; n < drop; n++)
            talloc_free(p->segs[n].present);
        memmove(p->segs, p->segs + drop,
                (p->num_segs - drop) * sizeof(p->segs[0]));
        p->num_segs -= drop;
        p->first_seg_id += drop;
    }
}

// The segment holding stream position pos: the last one starting at or
// before it. Returns its id, or -1 if none does.
static int64_t find_segment(struct priv *p, int64_t pos)
{
    int lo = 0, hi = p->num_segs - 1, found = -1;
    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;
        if (p->segs[mid].pos <= pos) {
            found = mid;
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }
    return found < 0 ? -1 : p->first_seg_id + found;
}

static bool segment_has_stream(struct stream_segment *seg, struct sh_stream *sh)
{
    if (!seg->present)
        return true;
    for (int n = 0; n < seg->num_present; n++) {
        if (seg->present[n].type == sh->type &&
            seg->present[n].id == sh->demuxer_id)
            return true;
    }
    return false;
}

// Set every declared stream present or absent as the segment says.
static void apply_presence(struct demuxer *demuxer, int64_t id)
{
    struct priv *p = demuxer->priv;
    struct stream_segment *seg = seg_by_id(p, id);
    for (int n = 0; seg && n < p->num_decl; n++) {
        struct sh_stream *sh = p->decl[n];
        demux_set_stream_absent(demuxer, sh, !segment_has_stream(seg, sh));
    }
    p->applied_seg = id;
}

// Rewrite the packet's in-stream timestamps to playback time with the
// segment its position falls in, and apply that segment's stream presence
// when the packet is the first to reach it.
static void map_packet(struct demuxer *demuxer, struct sh_stream *sh,
                       struct demux_packet *pkt)
{
    struct priv *p = demuxer->priv;
    struct outer_state *os = get_ostate(p, sh);

    int64_t id = -1;
    if (pkt->pos >= 0 && p->num_segs) {
        id = find_segment(p, pkt->pos);
        if (id < 0)
            id = p->first_seg_id;
    } else if (os->seg >= 0 && seg_by_id(p, os->seg)) {
        // A parser's later frame from the same input packet, no position
        // of its own.
        id = os->seg;
    } else {
        id = p->applied_seg;
    }
    struct stream_segment *seg = seg_by_id(p, id);
    if (!seg) {
        os->seg = -1;
        return;
    }
    if (id > p->applied_seg)
        apply_presence(demuxer, id);

    double t = pkt->dts != MP_NOPTS_VALUE ? pkt->dts : pkt->pts;
    if (seg->raw_base == MP_NOPTS_VALUE && t != MP_NOPTS_VALUE) {
        MP_VERBOSE(demuxer, "segment %"PRId64": raw %f plays at %f\n",
                   id, t, seg->time);
        seg->raw_base = t;
    }
    if (seg->raw_base != MP_NOPTS_VALUE) {
        double off = seg->time - seg->raw_base;
        if (pkt->pts != MP_NOPTS_VALUE)
            pkt->pts += off;
        if (pkt->dts != MP_NOPTS_VALUE)
            pkt->dts += off;
    }
    os->seg = id;
}

static void add_stream_chapters(struct demuxer *demuxer);

// Sync demuxer->edition with the disc's current playback position.
static void sync_initial_edition(struct demuxer *demuxer)
{
    struct stream_nav_state nav = {0};
    bool in_disc_nav = stream_control(demuxer->stream, STREAM_CTRL_GET_NAV_STATE, &nav) >= 1
                       && nav.nav_active && demuxer->num_editions > 0;

    unsigned title;
    if (stream_control(demuxer->stream, STREAM_CTRL_GET_CURRENT_TITLE, &title) >= 1 &&
        title < (unsigned)demuxer->num_editions)
    {
        demuxer->edition = title;
    } else if (in_disc_nav) {
        // The disc is playing something the title list does not contain, an
        // intro or a menu. Point at the synthetic entry instead of leaving
        // whichever edition happened to be marked before.
        demuxer->edition = demuxer->num_editions - 1;
    }
    if (in_disc_nav && nav.menu_active)
        demuxer->edition = demuxer->num_editions - 1;
}

static void refresh_disc_metadata(struct demuxer *demuxer)
{
    double len;
    if (stream_control(demuxer->stream, STREAM_CTRL_GET_TIME_LENGTH, &len) >= 1)
        demux_set_duration(demuxer, len);
    else
        demux_set_duration(demuxer, -1);

    // Old chapter metadata is not freed here, they are parented to demuxer.
    demuxer->chapters = NULL;
    demuxer->num_chapters = 0;
    add_stream_chapters(demuxer);
    sync_initial_edition(demuxer);
    demux_lists_changed(demuxer);
}

static void d_seek(demuxer_t *demuxer, double seek_pts, int flags)
{
    struct priv *p = demuxer->priv;

    if (p->is_cdda) {
        demux_seek(p->slave, seek_pts, flags);
        return;
    }

    if (flags & SEEK_FACTOR) {
        double tmp = 0;
        stream_control(demuxer->stream, STREAM_CTRL_GET_TIME_LENGTH, &tmp);
        seek_pts *= tmp;
    }

    // If the disc VM jumped on its own, this seek is the resync for it. The
    // VM is already at the destination, only flush and release the held data.
    struct stream_nav_state nav = {0};
    bool have_nav = stream_control(demuxer->stream, STREAM_CTRL_GET_NAV_STATE, &nav) >= 1;
    bool resync = have_nav && nav.drain_pending;
    // The BD equivalent of a jump resync, the slave reopen for it is still
    // pending on the packet-read path.
    bool bd_jump = have_nav && !p->is_dvd && nav.discontinuity_id != p->last_discontinuity_id;

    p->skip_until = MP_NOPTS_VALUE;

    if (resync) {
        MP_VERBOSE(demuxer, "resync seek at jump boundary (disc id %u)\n",
                   nav.discontinuity_id);
    } else if (bd_jump) {
        // The disc VM is already at the jump destination. Don't reposition
        // it with a stale time, just flush our side. The read path performs
        // the slave reopen when it sees the counter mismatch.
        MP_VERBOSE(demuxer, "resync seek for disc jump (id %u), "
                   "slave reopen pending\n", nav.discontinuity_id);
    } else {
        MP_VERBOSE(demuxer, "seek to: %f\n", seek_pts);
        struct stream_seek_req req = {
            .target = seek_pts,
            .flags = flags,
            .skip_until = MP_NOPTS_VALUE,
        };
        stream_control(demuxer->stream, STREAM_CTRL_SEEK_TO_TIME, &req);
        if (req.skip_until != MP_NOPTS_VALUE) {
            // The stream stopped at the picture shown at the target.
            MP_VERBOSE(demuxer, "dropping audio before %f\n", req.skip_until);
            p->skip_until = req.skip_until;
        }
        if (p->is_bd) {
            // The reposition starts a new byte-stream run. Restart the byte
            // position counters, the slave's drop_buffers below resyncs the
            // avio position so mpegts detects the jump and resets its packet
            // state in place. DVD does not need this. libdvdnav hands out whole
            // 2048-byte blocks and seeks land on VOBU boundaries, so the PS
            // slave resumes exactly at a pack header and keeps no cross-reads.
            stream_rebase_position(demuxer->stream);
        }
    }

    reset_segments(p);

    if (p->slave && p->slave->desc->drop_buffers)
        p->slave->desc->drop_buffers(p->slave);

    if (resync) {
        stream_control(demuxer->stream, STREAM_CTRL_NAV_DRAIN_ACK, NULL);
        refresh_disc_metadata(demuxer);
        // The jump may have changed what the disc plays.
        register_declared_streams(demuxer);
    }

    // A DVD jump is fully handled by this seek, adopt the current counter.
    // A BD jump additionally needs the slave reopened, which only the
    // packet-read path does. Keep the counter stale so that still happens,
    // else the old slave keeps parsing the new playlist's data.
    if (p->is_dvd && stream_control(demuxer->stream, STREAM_CTRL_GET_NAV_STATE, &nav) >= 1)
        p->last_discontinuity_id = nav.discontinuity_id;

    // The menu subpicture is one-shot. Resyncing while a still is held leaves
    // the same menu on screen with no data left to redraw it, so keep what we
    // retained and re-deliver it. A genuinely new subpicture replaces the hold
    // when it arrives.
    if (p->is_dvd && resync && nav.still_active) {
        arm_dvd_sub_holds(p);
    } else {
        clear_dvd_sub_holds(p);
    }

    // Re-inject the still for the new position.
    p->last_still_id = -1;
    TA_FREEP(&p->pending_pkt);

    p->seek_reinit = true;
}

static bool reopen_slave(struct demuxer *demuxer)
{
    struct priv *p = demuxer->priv;

    struct demuxer_params params = {
        .force_format = "+lavf",
        .external_stream = demuxer->stream,
        .stream_flags = demuxer->stream_origin,
        .depth = demuxer->depth + 1,
    };
    if (p->is_cdda)
        params.force_format = "+rawaudio";

    demux_free(p->slave);
    clear_dvd_sub_holds(p);
    // Discard anything the stream wrapper buffered before the disc-nav
    // discontinuity, and restart byte positions at 0: the post-jump data is
    // a new stream, and the fresh slave expects to probe it from the start
    // (the disc stream is linear and can't rewind to old positions anyway).
    stream_rebase_position(demuxer->stream);
    reset_segments(p);
    // Between positions an open would only probe an EOF. The peek pumps
    // the stream's event loop, the bytes stay buffered for the probe.
    uint8_t hdr[192];
    if (stream_read_peek(demuxer->stream, hdr, sizeof(hdr)) <= 0) {
        p->slave = NULL;
        return false;
    }
    MP_VERBOSE(demuxer, "reopening slave demuxer\n");
    p->slave = demux_open_url("-", &params, demuxer->cancel, demuxer->global);
    if (!p->slave) {
        // Happens when the stream is between positions (e.g. the disc VM is
        // mid-jump and probing hit EOF). last_discontinuity_id is left stale
        // on purpose: the next read/seek retries the reopen.
        MP_WARN(demuxer, "Failed to reopen slave demuxer, will retry.\n");
        return false;
    }

    for (int n = 0; n < p->slave_to_outer_count; n++)
        p->slave_to_outer[n] = NULL;

    register_declared_streams(demuxer);
    sync_streams(demuxer);
    refresh_disc_metadata(demuxer);

    return true;
}

// Handle a BD nav discontinuity (title/menu jump)
static bool process_discontinuity(struct demuxer *demuxer, uint32_t new_id)
{
    struct priv *p = demuxer->priv;
    mp_assert(!p->is_dvd); // DVD doesn't need reopen.

    if (!reopen_slave(demuxer))
        return false;
    p->last_discontinuity_id = new_id;
    p->seek_reinit = true;
    return true;
}

// Create the DVD-Audio still-image video track, if the stream provides stills.
static void add_still_stream(struct demuxer *demuxer)
{
    struct priv *p = demuxer->priv;
    struct stream_still_req sr = {.time = 0};
    if (stream_control(demuxer->stream, STREAM_CTRL_GET_STILL, &sr) != STREAM_OK)
        return;
    if (!sr.has_stills)
        return;
    struct sh_stream *sh = demux_alloc_sh_stream(STREAM_VIDEO);
    sh->codec->codec = "mpeg2video";
    sh->title = talloc_strdup(sh, "Cover");
    sh->still_image = true;     // sparse: one frame per track, read lazily
    p->still_sh = sh;
    p->last_still_id = -1;
    demux_add_sh_stream(demuxer, sh);
}

// Inject the still for playback time `time` as a video packet, when the still
// track is selected and the shown still changed.
static void inject_still(struct demuxer *demuxer, double time)
{
    struct priv *p = demuxer->priv;
    if (!p->still_sh || p->pending_pkt ||
        !demux_stream_is_selected(p->still_sh))
        return;
    struct stream_still_req sr = {.time = time};
    if (stream_control(demuxer->stream, STREAM_CTRL_GET_STILL, &sr) != STREAM_OK)
        return;
    if (sr.id < 0 || sr.id == p->last_still_id || !sr.data || sr.data_size <= 0)
        return;
    p->last_still_id = sr.id;
    struct demux_packet *dp =
        new_demux_packet_from(demuxer->packet_pool, sr.data, sr.data_size);
    if (!dp)
        return;
    dp->stream = p->still_sh->index;
    dp->pts = dp->dts = time;
    dp->keyframe = true;
    p->pending_pkt = dp;
}

// Retain a mapped DVD subpicture for re-delivery and deliver it, or drop it
// if its stream isn't selected.
static bool deliver_dvd_sub(struct demuxer *demuxer, struct sh_stream *sh,
                            struct demux_packet *pkt,
                            struct demux_packet **out_pkt)
{
    struct priv *p = demuxer->priv;

    int idx = sh->demuxer_id - 0x20;
    if (idx >= 0 && idx < MAX_DVD_SPU_STREAMS) {
        struct dvd_sub_hold *h = &p->dvd_sub_hold[idx];
        talloc_free(h->pkt);
        h->pkt = demux_copy_packet(demuxer->packet_pool, pkt);
        h->sh = sh;
        h->pending = false;
    }
    MP_DBG(demuxer, "dvd sub 0x%x packet: pts=%f selected=%d (held)\n",
           sh->demuxer_id, pkt->pts, demux_stream_is_selected(sh));
    if (!demux_stream_is_selected(sh)) {
        talloc_free(pkt);
        return true;
    }
    *out_pkt = pkt;
    return true;
}

static bool d_read_packet(struct demuxer *demuxer, struct demux_packet **out_pkt)
{
    struct priv *p = demuxer->priv;

    if (p->pending_pkt) {
        *out_pkt = p->pending_pkt;
        p->pending_pkt = NULL;
        return true;
    }

    bool menu_active = false;
    struct stream_nav_state nav = {0};
    if (stream_control(demuxer->stream, STREAM_CTRL_GET_NAV_STATE, &nav) >= 1) {
        menu_active = nav.menu_active;
        if (nav.nav_active != p->nav_active) {
            p->nav_active = nav.nav_active;
            demux_set_nav_active(demuxer, nav.nav_active);
        }
        // DVD holds an EOF at jump boundaries and the player resyncs
        // through the seek path. BD has no in-band boundary, handle it here,
        // after the resync seek.
        if (!p->is_dvd && nav.discontinuity_id != p->last_discontinuity_id) {
            if (nav.drain_pending)
                return false;
            MP_VERBOSE(demuxer, "discontinuity %u->%u, handling\n",
                       p->last_discontinuity_id, nav.discontinuity_id);
            if (!process_discontinuity(demuxer, nav.discontinuity_id))
                return false;
        }
    }

    // Re-deliver a retained menu subpicture.
    if (menu_active) {
        for (int i = 0; i < MAX_DVD_SPU_STREAMS; i++) {
            struct dvd_sub_hold *h = &p->dvd_sub_hold[i];
            if (!h->pending || !h->pkt || !h->sh ||
                !demux_stream_is_selected(h->sh))
                continue;
            h->pending = false;
            struct demux_packet *rp = demux_copy_packet(demuxer->packet_pool,
                                                        h->pkt);
            if (!rp)
                continue;
            MP_DBG(demuxer, "re-delivering held dvd sub 0x%x (pts=%f)\n",
                   h->sh->demuxer_id, rp->pts);
            rp->stream = h->sh->index;
            *out_pkt = rp;
            return true;
        }
    }

    // A discontinuity reopen failed and the retry above hasn't succeeded
    // yet; report EOF until the stream settles and the reopen goes through.
    if (!p->slave)
        return false;

    bool new_streams = false;
    struct demux_packet *pkt = demux_read_any_packet(p->slave, &new_streams);
    pull_segments(demuxer);
    if (!pkt && new_streams) {
        // Map and enable the new slave streams before more data goes by,
        // the slave drops the packets of streams nobody selected.
        sync_streams(demuxer);
        return true;
    }
    if (!pkt) {
        // EOF is a real still (hold the frame), a held jump boundary (the
        // player resyncs it via the seek path), or (BD) an out-of-band jump.
        struct stream_nav_state nav2 = {0};
        bool have_nav2 = stream_control(demuxer->stream, STREAM_CTRL_GET_NAV_STATE, &nav2) >= 1;
        if (have_nav2 && !p->is_dvd && !nav2.drain_pending &&
            nav2.discontinuity_id != p->last_discontinuity_id)
        {
            MP_VERBOSE(demuxer, "discontinuity %u->%u at EOF, handling\n",
                       p->last_discontinuity_id, nav2.discontinuity_id);
            if (!process_discontinuity(demuxer, nav2.discontinuity_id))
                return false;
            pkt = demux_read_any_packet(p->slave, NULL);
            pull_segments(demuxer);
        } else if (p->is_bd && p->nav_active) {
            // The slave latches the EOF and stops touching the stream.
            // Peek to run the event loop, the VM progresses only on reads.
            stream_read_peek(demuxer->stream, &(char){0}, 1);
        }
        if (!pkt) {
            if (p->eof_log_id != nav2.discontinuity_id ||
                p->eof_log_still != nav2.still_active)
            {
                MP_VERBOSE(demuxer, "slave EOF (disc id %u, still %d)\n",
                           have_nav2 ? nav2.discontinuity_id : 0,
                           have_nav2 ? nav2.still_active : -1);
                p->eof_log_id = nav2.discontinuity_id;
                p->eof_log_still = nav2.still_active;
            }
            return false;
        }
    }

    demux_update(p->slave, MP_NOPTS_VALUE);

    if (p->seek_reinit) {
        refresh_disc_metadata(demuxer);
        p->seek_reinit = false;
    }

    int slave_index = pkt->stream;
    if (demux_get_num_stream(p->slave) > p->slave_to_outer_count ||
        slave_index >= p->slave_to_outer_count ||
        !p->slave_to_outer[slave_index])
    {
        sync_streams(demuxer);
    }

    struct sh_stream *sh = slave_index < p->slave_to_outer_count
                              ? p->slave_to_outer[slave_index] : NULL;
    if (!sh) {
        talloc_free(pkt);
        return true;
    }

    pkt->stream = sh->index;

    if (p->is_cdda) {
        if (!demux_stream_is_selected(sh)) {
            talloc_free(pkt);
            return true;
        }
        *out_pkt = pkt;
        return true;
    }

    MP_TRACE(demuxer, "ipts: %d %f %f pos=%"PRId64"\n", sh->type, pkt->pts,
             pkt->dts, pkt->pos);

    // Every packet places its stream in the timeline, selected or not, so
    // the segment facts are applied as the data goes by.
    map_packet(demuxer, sh, pkt);

    // The one-shot menu subpicture must be captured even before any sub
    // track is selected.
    bool dvd_sub = p->is_dvd && sh->type == STREAM_SUB;
    if (!demux_stream_is_selected(sh) && !dvd_sub) {
        talloc_free(pkt);
        return true;
    }

    MP_TRACE(demuxer, "mapped pkt: type=%d pts=%f dts=%f\n",
             sh->type, pkt->pts, pkt->dts);

    if (dvd_sub)
        return deliver_dvd_sub(demuxer, sh, pkt, out_pkt);

    // Tag every a/v packet with the outer stream's codec params, like
    // demux_timeline does: disc jumps can change the codec on a reused
    // stream, and a seek flush can discard the first post-change packet,
    // so every packet must carry the signal.
    if (sh->type == STREAM_VIDEO || sh->type == STREAM_AUDIO) {
        pkt->segmented = true;
        pkt->codec = sh->codec;
    }

    // Drop the audio between the landing and the seek target (see d_seek).
    if (p->skip_until != MP_NOPTS_VALUE && sh->type == STREAM_AUDIO &&
        pkt->pts != MP_NOPTS_VALUE)
    {
        if (pkt->pts < p->skip_until) {
            talloc_free(pkt);
            return true;
        }
        MP_VERBOSE(demuxer, "audio reached seek target %f\n", p->skip_until);
        p->skip_until = MP_NOPTS_VALUE;
    }

    if (sh->type == STREAM_AUDIO && pkt->pts != MP_NOPTS_VALUE) {
        inject_still(demuxer, pkt->pts);
        // Deliver the still before the audio it belongs to, so the frame is
        // already present when the player starts or restarts here.
        if (p->pending_pkt) {
            struct demux_packet *still = p->pending_pkt;
            p->pending_pkt = pkt;
            *out_pkt = still;
            return true;
        }
    }

    *out_pkt = pkt;
    return true;
}

static void add_stream_editions(struct demuxer *demuxer)
{
    unsigned titles = 0;
    if (stream_control(demuxer->stream, STREAM_CTRL_GET_NUM_TITLES, &titles) != STREAM_OK)
        return;
    for (unsigned title = 0; title < titles; ++title) {
        struct demux_edition new = {
            .demuxer_id = title,
            .default_edition = false,
            .metadata = talloc_zero(demuxer, struct mp_tags),
        };
        MP_TARRAY_APPEND(demuxer, demuxer->editions, demuxer->num_editions, new);

        double duration = title;
        if (stream_control(demuxer->stream, STREAM_CTRL_GET_TITLE_LENGTH, &duration) != STREAM_OK) {
            mp_tags_set_str(new.metadata, "TITLE",
                            mp_tprintf(42, "title: %u", title + 1));
            continue;
        }

        char *time = mp_format_time(duration, true);
        double playlist = title;
        if (stream_control(demuxer->stream, STREAM_CTRL_GET_TITLE_PLAYLIST, &playlist) == STREAM_OK)
            time = talloc_asprintf_append(time, ") (%05.0f.mpls", playlist);
        struct stream_name_req name = {.index = title};
        if (stream_control(demuxer->stream, STREAM_CTRL_GET_TITLE_NAME, &name) >= 1) {
            time = talloc_asprintf_append(time, ") (%s", name.name);
            talloc_free(name.name);
        }
        char *label = talloc_asprintf(NULL, "title: %u (%s)", title + 1, time);
        mp_tags_set_str(new.metadata, "TITLE", label);
        talloc_free(label);
        talloc_free(time);
    }

    // Append a synthetic "Disc Menu" entry, if the disc has menu support.
    struct stream_nav_state nav = {0};
    if (stream_control(demuxer->stream, STREAM_CTRL_GET_NAV_STATE, &nav) < 1 ||
        !nav.nav_active)
        return;
    struct demux_edition menu = {
        .demuxer_id = titles,
        .default_edition = false,
        .metadata = talloc_zero(demuxer, struct mp_tags),
    };
    MP_TARRAY_APPEND(demuxer, demuxer->editions, demuxer->num_editions, menu);
    mp_tags_set_str(menu.metadata, "TITLE", "Disc Menu");
}

static void add_stream_chapters(struct demuxer *demuxer)
{
    int num = 0;
    if (stream_control(demuxer->stream, STREAM_CTRL_GET_NUM_CHAPTERS, &num) < 1)
        return;
    for (int n = 0; n < num; n++) {
        double p = n;
        if (stream_control(demuxer->stream, STREAM_CTRL_GET_CHAPTER_TIME, &p) < 1)
            continue;
        struct stream_name_req req = {.index = n};
        stream_control(demuxer->stream, STREAM_CTRL_GET_CHAPTER_NAME, &req);
        demuxer_add_chapter(demuxer, req.name ? req.name : "", p, 0);
        talloc_free(req.name);
    }
}

static int d_open(demuxer_t *demuxer, enum demux_check check)
{
    struct priv *p = demuxer->priv = talloc_zero(demuxer, struct priv);
    p->disc_audio_id = -1;
    p->applied_seg = -1;
    p->skip_until = MP_NOPTS_VALUE;

    if (check != DEMUX_CHECK_FORCE)
        return -1;

    struct demuxer_params params = {
        .force_format = "+lavf",
        .external_stream = demuxer->stream,
        .stream_flags = demuxer->stream_origin,
        .depth = demuxer->depth + 1,
    };

    struct stream *cur = demuxer->stream;
    const char *sname = "";
    if (cur->info)
        sname = cur->info->name;

    p->is_cdda = strcmp(sname, "cdda") == 0;
    p->is_dvd = strcmp(sname, "dvdnav") == 0 ||
                strcmp(sname, "ifo_dvdnav") == 0;
    p->is_dvda = strcmp(sname, "dvda") == 0 ||
                 strcmp(sname, "ifo_dvda") == 0;
    p->is_bd = strcmp(sname, "bd") == 0 ||
               strcmp(sname, "bdmv/bluray") == 0;

    if (p->is_cdda)
        params.force_format = "+rawaudio";

    char *t = NULL;
    stream_control(demuxer->stream, STREAM_CTRL_GET_DISC_NAME, &t);
    if (t) {
        mp_tags_set_str(demuxer->metadata, "TITLE", t);
        talloc_free(t);
    }

    // The disc VM settles at its starting position only when data is read
    // (at least with libdvdnav), and what it plays there is what the stream
    // declares below.
    stream_read_peek(demuxer->stream, &(char){0}, 1);

    // Boundaries settled before the first slave open predate any player
    // state to resync, release them so the probe can read.
    if (!p->is_dvd)
        stream_control(demuxer->stream, STREAM_CTRL_NAV_DRAIN_ACK, NULL);

    register_declared_streams(demuxer);

    p->slave = demux_open_url("-", &params, demuxer->cancel, demuxer->global);
    if (!p->slave)
        return -1;

    // Jumps during open/probe predate the slave and must not be handled by
    // dropping the data it just buffered. Hold at boundaries from here on.
    struct stream_nav_state nav = {0};
    if (stream_control(demuxer->stream, STREAM_CTRL_GET_NAV_STATE, &nav) >= 1)
        p->last_discontinuity_id = nav.discontinuity_id;
    if (p->is_dvd)
        stream_control(demuxer->stream, STREAM_CTRL_NAV_DRAIN_ENABLE, NULL);

    // Can be seekable even if the stream isn't.
    demuxer->seekable = true;
    demuxer->partially_seekable = !p->is_cdda;
    demuxer->no_cache_seeking = !p->is_cdda;

    sync_streams(demuxer);
    add_still_stream(demuxer);
    add_stream_chapters(demuxer);
    add_stream_editions(demuxer);

    double len;
    if (stream_control(demuxer->stream, STREAM_CTRL_GET_TIME_LENGTH, &len) >= 1)
        demuxer->duration = len;

    sync_initial_edition(demuxer);

    return 0;
}

static void d_close(demuxer_t *demuxer)
{
    struct priv *p = demuxer->priv;
    clear_dvd_sub_holds(p);
    TA_FREEP(&p->pending_pkt);
    demux_free(p->slave);
}

const demuxer_desc_t demuxer_desc_disc = {
    .name = "disc",
    .desc = "CD/DVD/BD wrapper",
    .read_packet = d_read_packet,
    .open = d_open,
    .close = d_close,
    .seek = d_seek,
    .switched_tracks = reselect_streams,
    .nav_refresh = d_nav_refresh,
};
