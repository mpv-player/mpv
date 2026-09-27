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

#include "video/csputils.h"

// DVD-Video has 32 subpicture (SPU) streams, mapped to PES substream IDs 0x20..0x3F.
#define MAX_DVD_SPU_STREAMS 32

// Discs restart the in-stream timestamps at cell/segment boundaries. Each
// such segment is mapped to continuous playback time as a timeline
// "generation" with its own raw->playback offset. Streams cross a boundary
// at their own packet position (interleave skew), so the previous generation
// stays valid until every stream has moved past the boundary.

// Backward jitter tolerance between packets of one stream (muxing slack).
// A same-stream timestamp stepping back further starts a new generation.
#define TL_BACK_TOLERANCE 0.1
// A same-stream forward gap larger than this starts a new generation (it
// would otherwise stall playback for the duration of the gap). It should be
// big enough to account for low video framerates and large audio frames.
// Sparse (still-image) video streams are exempt.
#define TL_FWD_THRESHOLD 5.0

struct priv {
    struct demuxer *slave;

    // All outer sh_streams we have ever surfaced to the parent demuxer.
    struct sh_stream **outer_streams;
    int num_outer_streams;

    // Maps the current slave's stream index to its matching outer sh_stream.
    struct sh_stream **slave_to_outer;
    int slave_to_outer_count;

    // DVD-only: pre-registered sub streams keyed by PES substream ID minus
    // 0x20, carrying the disc-level CLUT as extradata.
    struct sh_stream *dvd_subs[MAX_DVD_SPU_STREAMS];

    // DVD-only: retain the last SPU packet per substream. The menu subpicture
    // is one-shot on an unseekable stream; re-deliver it on (re)selection and
    // demux_nav_refresh(), like a refresh seek for ordinary streams.
    struct dvd_sub_hold {
        struct demux_packet *pkt;   // clone, with playback-rebased timestamps
        struct sh_stream *sh;       // outer stream it belongs to
        bool pending;               // re-deliver on next read
    } dvd_sub_hold[MAX_DVD_SPU_STREAMS];

    // DVD subpictures are muxed around cell boundaries, and their raw
    // timestamps alone can't tell which timeline generation they belong to.
    // Defer them until the next mapped a/v packet (or EOF) settles it.
    struct pending_sub {
        struct demux_packet *pkt;   // raw (unmapped) timestamps
        struct sh_stream *sh;
        uint64_t seq;               // av_map_seq at arrival
    } *pending_subs;
    int num_pending_subs;
    uint64_t av_map_seq;            // bumped per mapped a/v packet and at EOF

    // Used to rewrite the raw MPEG timestamps to playback time.
    double base_time;   // playback time at the current reset point
    struct timeline_gen {
        uint32_t id;            // 0 = invalid
        bool have_off;
        double off;             // playback = raw + off
    } tl_cur, tl_prev;
    bool tl_have_prev;
    uint32_t tl_id_counter;
    // Position within the timeline, per outer stream (indexed by sh->index).
    struct stream_timeline {
        uint32_t gen_id;        // generation the stream last mapped into
        double last_raw;        // raw ts (dts preferred) of its last packet
        double last_dur;
        bool was_selected;      // outer selection state at the last reselect
        int64_t select_pos;     // slave position at (re)selection, -1 = off
    } *tl_streams;
    int64_t last_read_pos;      // highest slave packet position seen
    int num_tl_streams;
    bool seek_reinit;   // needs reinit after seek
    uint32_t last_discontinuity_id; // Last source-position-jump id seen from the stream.
    bool nav_active;    // last interactive-nav state pushed to the cache
    uint32_t eof_log_id;    // last logged EOF state, avoids per-read spam
    bool eof_log_still;

    bool is_dvd, is_dvda, is_cdda, is_bd;

    // DVD-Audio still-image video track.
    struct sh_stream *still_sh;
    int last_still_id;
    struct demux_packet *pending_pkt; // still or the audio deferred behind it

    // Sparse-video (slideshow title) detection state.
    struct sh_stream *video_sh;
    double last_video_dts;

    // Seeks into sparse-video titles land at the preceding chapter start,
    // where the target's still frame is. Drop audio until this target.
    double skip_audio_until;
};

static void clear_dvd_sub_holds(struct priv *p)
{
    for (int i = 0; i < MAX_DVD_SPU_STREAMS; i++) {
        talloc_free(p->dvd_sub_hold[i].pkt);
        p->dvd_sub_hold[i] = (struct dvd_sub_hold){0};
    }
    for (int i = 0; i < p->num_pending_subs; i++)
        talloc_free(p->pending_subs[i].pkt);
    p->num_pending_subs = 0;
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

// Whether the slave stream mapped to this outer stream should be enabled.
// DVD SPU streams always stay enabled: the one-shot menu subpicture must be
// captured into dvd_sub_hold even before any sub track is selected.
static bool slave_stream_enabled(struct priv *p, struct sh_stream *outer)
{
    return demux_stream_is_selected(outer) ||
           (p->is_dvd && outer->type == STREAM_SUB);
}

static struct stream_timeline *get_stream_tl(struct priv *p,
                                             struct sh_stream *sh);

static void reselect_streams(demuxer_t *demuxer)
{
    struct priv *p = demuxer->priv;
    if (!p->slave)
        return;
    int num_slave = demux_get_num_stream(p->slave);
    for (int n = 0; n < num_slave && n < p->slave_to_outer_count; n++) {
        struct sh_stream *outer = p->slave_to_outer[n];
        demuxer_select_track(p->slave, demux_get_stream(p->slave, n),
            MP_NOPTS_VALUE, outer && slave_stream_enabled(p, outer));
        if (!outer)
            continue;
        // A re-selected stream must not re-enter the timeline through its
        // stale generation membership. Start it fresh in the current one.
        struct stream_timeline *tl = get_stream_tl(p, outer);
        bool sel = demux_stream_is_selected(outer);
        if (sel && !tl->was_selected) {
            *tl = (struct stream_timeline){
                .last_raw = MP_NOPTS_VALUE,
                .was_selected = true,
                .select_pos = p->last_read_pos,
            };
        } else {
            tl->was_selected = sel;
        }
    }
    arm_dvd_sub_holds(p);
}

static void d_nav_refresh(demuxer_t *demuxer)
{
    struct priv *p = demuxer->priv;
    arm_dvd_sub_holds(p);
}

static void get_disc_lang(struct stream *stream, struct sh_stream *sh, bool dvd)
{
    struct stream_lang_req req = {.type = sh->type, .id = sh->demuxer_id};
    if (dvd && sh->type == STREAM_SUB)
        req.id = req.id & 0x1F; // mpeg ID to index
    stream_control(stream, STREAM_CTRL_GET_LANG, &req);
    if (req.name[0])
        sh->lang = talloc_strdup(sh, req.name);
}

static void add_dvd_streams(demuxer_t *demuxer)
{
    struct priv *p = demuxer->priv;
    struct stream *stream = demuxer->stream;
    if (!p->is_dvd)
        return;
    struct stream_dvd_info_req info;
    if (stream_control(stream, STREAM_CTRL_GET_DVD_INFO, &info) > 0) {
        for (int n = 0; n < MPMIN(MAX_DVD_SPU_STREAMS, info.num_subs); n++) {
            struct sh_stream *sh = demux_alloc_sh_stream(STREAM_SUB);
            sh->demuxer_id = n + 0x20;
            sh->codec->codec = "dvd_subtitle";
            get_disc_lang(stream, sh, true);
            p->dvd_subs[n] = sh;
            MP_TARRAY_APPEND(p, p->outer_streams, p->num_outer_streams, sh);

            // emulate the extradata
            struct mp_csp_params csp = MP_CSP_PARAMS_DEFAULTS;
            struct pl_transform3x3 cmatrix;
            mp_get_csp_matrix(&csp, &cmatrix);

            char *s = talloc_strdup(sh, "");
            s = talloc_asprintf_append(s, "palette: ");
            for (int i = 0; i < 16; i++) {
                int color = info.palette[i];
                int y[3] = {(color >> 16) & 0xff, (color >> 8) & 0xff, color & 0xff};
                int c[3];
                mp_map_fixp_color(&cmatrix, 8, y, 8, c);
                color = (c[2] << 16) | (c[1] << 8) | c[0];

                if (i != 0)
                    s = talloc_asprintf_append(s, ", ");
                s = talloc_asprintf_append(s, "%06x", color);
            }
            s = talloc_asprintf_append(s, "\n");

            sh->codec->extradata = s;
            sh->codec->extradata_size = strlen(s);

            demux_add_sh_stream(demuxer, sh);
        }
    }
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

// Find the outer stream for a slave stream. `ordinal` disambiguates multiple
// slave streams sharing one (type, id): a BD TrueHD PID carries the TrueHD
// stream and its embedded AC-3 compatibility core as two lavf streams with
// the same id, and each must keep its own outer track (and decoder).
static struct sh_stream *find_outer_for_slave(struct priv *p,
                                              struct sh_stream *src,
                                              int ordinal)
{
    if (src->type == STREAM_SUB && src->demuxer_id >= 0x20 &&
        src->demuxer_id <= 0x3F)
    {
        struct sh_stream *sub = p->dvd_subs[src->demuxer_id - 0x20];
        if (sub)
            return sub;
    }
    for (int i = 0; i < p->num_outer_streams; i++) {
        struct sh_stream *sh = p->outer_streams[i];
        if (sh && sh->type == src->type && sh->demuxer_id == src->demuxer_id &&
            ordinal-- == 0)
            return sh;
    }
    return NULL;
}

// Build / rebuild the slave-index -> outer-sh map. For each slave stream reuse
// or register a fresh outer sh_stream as follows and expose it to the parent demuxer.
static void sync_streams(struct demuxer *demuxer)
{
    struct priv *p = demuxer->priv;

    int nav_audio_id = -1;
    struct stream_nav_state nav = {0};
    if (p->is_dvd && stream_control(demuxer->stream, STREAM_CTRL_GET_NAV_STATE, &nav) >= 1)
        nav_audio_id = nav.active_audio_id;

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

        struct sh_stream *outer = find_outer_for_slave(p, src, ordinal);

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
            if (src->type == STREAM_VIDEO) {
                double ar;
                if (stream_control(demuxer->stream, STREAM_CTRL_GET_ASPECT_RATIO, &ar)
                                    == STREAM_OK)
                {
                    struct mp_image_params f = {.w = src->codec->disp_w,
                                                .h = src->codec->disp_h};
                    mp_image_params_set_dsize(&f, 1728 * ar, 1728);
                    outer->codec->par_w = f.p_w;
                    outer->codec->par_h = f.p_h;
                }
            }
            get_disc_lang(demuxer->stream, outer, p->is_dvd);
            MP_TARRAY_APPEND(p, p->outer_streams, p->num_outer_streams, outer);
            demux_add_sh_stream(demuxer, outer);
        } else if (outer->type != STREAM_SUB && outer->codec && src->codec) {
            // Codec change on a reused outer, mostly useful for BD menus, which
            // may be MPEG-2 while the video track is H.264.
            const char *new_codec = src->codec->codec;
            const char *cur_codec = outer->codec->codec;
            if (new_codec && cur_codec && strcmp(new_codec, cur_codec) != 0) {
                MP_VERBOSE(demuxer, "stream %d (id=0x%x) codec changed: %s -> %s\n",
                           n, src->demuxer_id, cur_codec, new_codec);
                adopt_codec_params(outer, src);
            }
        }

        p->slave_to_outer[n] = outer;
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

    // Propagate outer selection state to the slave. Unmapped slave streams
    // (ignored non-presentation streams) are kept deselected so they don't
    // deliver packets.
    for (int n = 0; n < num_slave; n++) {
        struct sh_stream *outer = p->slave_to_outer[n];
        demuxer_select_track(p->slave, demux_get_stream(p->slave, n),
            MP_NOPTS_VALUE, outer && slave_stream_enabled(p, outer));
    }

    // Mirror slave sh_stream_group onto the outer sh_streams. This is needed
    // for the Dolby Vision BL+EL group, it's detected well by lavf. We could use
    // the libbluray `dv_streams[]` info, but it's not available yet in release
    // version, and mapping it through lavf is less code.
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
            outer_member->group = grp;
        }
    }
}

static void refresh_disc_metadata(struct demuxer *demuxer);

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

    p->skip_audio_until = MP_NOPTS_VALUE;

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
        // In slideshow titles the target's still lies at the preceding
        // chapter (cell) start and can't be read backwards from a mid-cell
        // landing. Land there and drop audio up to the actual target.
        double stream_target = seek_pts;
        int stream_flags = flags;
        if (p->video_sh && p->video_sh->still_image &&
            demuxer->num_chapters > 0)
        {
            double snap = 0;
            for (int n = 0; n < demuxer->num_chapters; n++) {
                double c = demuxer->chapters[n].pts;
                if (c <= seek_pts && c > snap)
                    snap = c;
            }
            if (seek_pts - snap > 0.5) {
                MP_VERBOSE(demuxer, "sparse video: landing at chapter start %f, "
                           "skipping audio to %f\n", snap, seek_pts);
                stream_target = snap;
                stream_flags &= ~(unsigned)SEEK_HR; // landing is chosen exactly
                p->skip_audio_until = seek_pts;
            }
        }
        MP_VERBOSE(demuxer, "seek to: %f\n", stream_target);
        double seek_arg[] = {stream_target, stream_flags};
        stream_control(demuxer->stream, STREAM_CTRL_SEEK_TO_TIME, seek_arg);
        if (p->is_bd) {
            // The reposition starts a new byte-stream run. Restart the byte
            // position counters, the slave's drop_buffers below resyncs the
            // avio position so mpegts detects the jump and resets its packet
            // state in place. DVD does not need this. libdvdnav hands out whole
            // 2048-byte blocks and seeks land on VOBU boundaries, so the PS
            // slave resumes exactly at a pack header and keeps no cross-reads.
            stream_rebase_position(demuxer->stream);
            p->last_read_pos = 0;
            for (int n = 0; n < p->num_tl_streams; n++)
                p->tl_streams[n].select_pos = -1;
        }
    }

    if (p->slave && p->slave->desc->drop_buffers)
        p->slave->desc->drop_buffers(p->slave);

    if (resync) {
        stream_control(demuxer->stream, STREAM_CTRL_NAV_DRAIN_ACK, NULL);
        refresh_disc_metadata(demuxer);
    }

    // A DVD jump is fully handled by this seek, adopt the current counter.
    // A BD jump additionally needs the slave reopened, which only the
    // packet-read path does. Keep the counter stale so that still happens,
    // else the old slave keeps parsing the new playlist's data.
    if (p->is_dvd && stream_control(demuxer->stream, STREAM_CTRL_GET_NAV_STATE, &nav) >= 1)
        p->last_discontinuity_id = nav.discontinuity_id;

    clear_dvd_sub_holds(p);

    // Re-inject the still for the new position.
    p->last_still_id = -1;
    TA_FREEP(&p->pending_pkt);

    p->seek_reinit = true;
}

static void reset_pts(demuxer_t *demuxer)
{
    struct priv *p = demuxer->priv;

    double base;
    if (stream_control(demuxer->stream, STREAM_CTRL_GET_CURRENT_TIME, &base) < 1)
        base = 0;

    MP_VERBOSE(demuxer, "reset to time: %f\n", base);

    p->base_time = base;
    p->tl_cur = (struct timeline_gen){
        .id = ++p->tl_id_counter,
    };
    p->tl_have_prev = false;
    for (int n = 0; n < p->num_tl_streams; n++)
        p->tl_streams[n] = (struct stream_timeline){.last_raw = MP_NOPTS_VALUE};
    p->last_video_dts = MP_NOPTS_VALUE;
    p->seek_reinit = false;
}

static struct stream_timeline *get_stream_tl(struct priv *p,
                                             struct sh_stream *sh)
{
    if (sh->index >= p->num_tl_streams) {
        int old = p->num_tl_streams;
        MP_TARRAY_GROW(p, p->tl_streams, sh->index);
        p->num_tl_streams = sh->index + 1;
        for (int n = old; n < p->num_tl_streams; n++)
            p->tl_streams[n] = (struct stream_timeline){.last_raw = MP_NOPTS_VALUE};
    }
    return &p->tl_streams[sh->index];
}

static void start_new_gen(struct priv *p, double off)
{
    p->tl_prev = p->tl_cur;
    p->tl_have_prev = true;
    p->tl_cur = (struct timeline_gen){
        .id = ++p->tl_id_counter,
        .have_off = true,
        .off = off,
    };
}

static void apply_tl_offset(struct demux_packet *pkt, double off)
{
    if (pkt->pts != MP_NOPTS_VALUE)
        pkt->pts += off;
    if (pkt->dts != MP_NOPTS_VALUE)
        pkt->dts += off;
}

// Rewrite an audio/video packet's raw in-stream timestamps to playback time,
// detecting segment boundaries (timestamp resets) on the way.
static void map_av_packet(demuxer_t *demuxer, struct sh_stream *sh,
                          struct demux_packet *pkt)
{
    struct priv *p = demuxer->priv;
    double t = pkt->dts != MP_NOPTS_VALUE ? pkt->dts : pkt->pts;
    struct timeline_gen *g = &p->tl_cur;

    if (t == MP_NOPTS_VALUE) {
        if (g->have_off)
            apply_tl_offset(pkt, g->off);
        return;
    }

    struct stream_timeline *st = get_stream_tl(p, sh);

    if (!g->have_off) {
        // First mapped packet after a reset, raw t plays at base_time.
        g->off = p->base_time - t;
        g->have_off = true;
    }

    if (p->tl_have_prev && st->gen_id == p->tl_prev.id &&
        st->last_raw != MP_NOPTS_VALUE &&
        t >= st->last_raw - TL_BACK_TOLERANCE &&
        (sh->still_image || t <= st->last_raw + TL_FWD_THRESHOLD))
    {
        // This stream hasn't crossed the last boundary yet (interleave
        // skew), its timestamps still continue the previous generation.
        g = &p->tl_prev;
    } else {
        if (st->gen_id != p->tl_cur.id) {
            st->gen_id = p->tl_cur.id;
            st->last_raw = MP_NOPTS_VALUE;
        }
        if (st->last_raw != MP_NOPTS_VALUE &&
            (t < st->last_raw - TL_BACK_TOLERANCE ||
             (!sh->still_image && t > st->last_raw + TL_FWD_THRESHOLD)))
        {
            // Timestamp reset. Start a new generation, placed so that this
            // stream continues seamlessly from its previous packet.
            double off = st->last_raw + st->last_dur + p->tl_cur.off - t;
            MP_VERBOSE(demuxer, "timeline reset: raw %f -> playback %f (%s)\n",
                       t, t + off, stream_type_name(sh->type));
            start_new_gen(p, off);
            st->last_raw = MP_NOPTS_VALUE;
            g = &p->tl_cur;
        }
        st->gen_id = g->id;
    }

    apply_tl_offset(pkt, g->off);

    st->last_raw = t;
    st->last_dur = MPMAX(pkt->duration, 0);
}

// Map a subtitle packet. Subtitles are sparse and their timestamps point at
// display time, so they don't drive boundary detection. They are mapped with
// the current generation. DVD subpictures reach this only after the deferral
// in d_read_packet settled which generation they belong to.
static void map_sub_packet(demuxer_t *demuxer, struct sh_stream *sh,
                           struct demux_packet *pkt)
{
    struct priv *p = demuxer->priv;
    struct timeline_gen *g = &p->tl_cur;
    double t = pkt->pts != MP_NOPTS_VALUE ? pkt->pts : pkt->dts;

    if (t == MP_NOPTS_VALUE) {
        if (g->have_off)
            apply_tl_offset(pkt, g->off);
        return;
    }

    if (!g->have_off) {
        // Subpicture demuxed before any a/v packet (muxed at the segment
        // head). Assume it sits at the playback start of this segment.
        g->off = p->base_time - t;
        g->have_off = true;
    }

    apply_tl_offset(pkt, g->off);
}

static void add_stream_chapters(struct demuxer *demuxer);

// Sync demuxer->edition with the disc's current playback position.
static void sync_initial_edition(struct demuxer *demuxer)
{
    unsigned title;
    if (stream_control(demuxer->stream, STREAM_CTRL_GET_CURRENT_TITLE, &title) >= 1 &&
        title < (unsigned)demuxer->num_editions)
        demuxer->edition = title;
    struct stream_nav_state nav = {0};
    if (stream_control(demuxer->stream, STREAM_CTRL_GET_NAV_STATE, &nav) >= 1
        && nav.menu_active && demuxer->num_editions > 0)
    {
        demuxer->edition = demuxer->num_editions - 1;
    }
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
    p->last_read_pos = 0;
    for (int n = 0; n < p->num_tl_streams; n++)
        p->tl_streams[n].select_pos = -1;
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

// Map a deferred DVD subpicture and deliver it. Retain it for re-delivery,
// drop it if its stream isn't selected.
static bool deliver_dvd_sub(struct demuxer *demuxer, struct sh_stream *sh,
                            struct demux_packet *pkt,
                            struct demux_packet **out_pkt)
{
    struct priv *p = demuxer->priv;

    map_sub_packet(demuxer, sh, pkt);
    MP_TRACE(demuxer, "mapped pkt: type=%d pts=%f dts=%f\n",
             sh->type, pkt->pts, pkt->dts);

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

    // Deliver a deferred subpicture once an a/v packet (or EOF) has settled
    // its timeline generation.
    if (p->num_pending_subs && p->pending_subs[0].seq < p->av_map_seq) {
        struct pending_sub ps = p->pending_subs[0];
        MP_TARRAY_REMOVE_AT(p->pending_subs, p->num_pending_subs, 0);
        return deliver_dvd_sub(demuxer, ps.sh, ps.pkt, out_pkt);
    }

    // A discontinuity reopen failed and the retry above hasn't succeeded
    // yet; report EOF until the stream settles and the reopen goes through.
    if (!p->slave)
        return false;

    struct demux_packet *pkt = demux_read_any_packet(p->slave);
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
            pkt = demux_read_any_packet(p->slave);
        } else if (p->is_bd && p->nav_active) {
            // The slave latches the EOF and stops touching the stream.
            // Peek to run the event loop, the VM progresses only on reads.
            stream_read_peek(demuxer->stream, &(char){0}, 1);
        }
        if (!pkt) {
            p->av_map_seq++;
            if (p->num_pending_subs) {
                struct pending_sub ps = p->pending_subs[0];
                MP_TARRAY_REMOVE_AT(p->pending_subs, p->num_pending_subs, 0);
                return deliver_dvd_sub(demuxer, ps.sh, ps.pkt, out_pkt);
            }
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

    if (pkt->pos >= 0 && pkt->pos > p->last_read_pos)
        p->last_read_pos = pkt->pos;

    if (p->seek_reinit) {
        reset_pts(demuxer);
        refresh_disc_metadata(demuxer);
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
    bool dvd_sub = sh && p->is_dvd && sh->type == STREAM_SUB;
    if (!sh || (!demux_stream_is_selected(sh) && !dvd_sub)) {
        talloc_free(pkt);
        return true;
    }

    // Tag every a/v packet with the outer stream's codec params, like
    // demux_timeline does: disc jumps can change the codec on a reused
    // stream, and a seek flush can discard the first post-change packet,
    // so every packet must carry the signal.
    if (sh->type == STREAM_VIDEO || sh->type == STREAM_AUDIO) {
        pkt->segmented = true;
        pkt->codec = sh->codec;
    }

    pkt->stream = sh->index;

    if (p->is_cdda) {
        *out_pkt = pkt;
        return true;
    }

    // libavformat re-delivers packets buffered during probing when a stream
    // is (re)enabled. On a linear disc stream those are stale, and their raw
    // timestamps would fake a timeline boundary. Drop everything from before
    // the reselect position (with slack for mux interleave).
    struct stream_timeline *tl = get_stream_tl(p, sh);
    if (tl->select_pos > 0) {
        if (pkt->pos >= 0 && pkt->pos + (32 << 20) < tl->select_pos) {
            talloc_free(pkt);
            return true;
        }
        tl->select_pos = -1;
    }

    MP_TRACE(demuxer, "ipts: %d %f %f\n", sh->type, pkt->pts, pkt->dts);

    if (dvd_sub) {
        // Defer until the next a/v packet (or EOF) settles which timeline
        // generation this subpicture belongs to. The packet stays parentless,
        // it is delivered or freed explicitly.
        MP_TARRAY_APPEND(p, p->pending_subs, p->num_pending_subs,
                         (struct pending_sub){
                             .pkt = pkt, .sh = sh, .seq = p->av_map_seq,
                         });
        return true;
    }

    if (sh->type == STREAM_SUB) {
        map_sub_packet(demuxer, sh, pkt);
    } else {
        map_av_packet(demuxer, sh, pkt);
        p->av_map_seq++;
    }

    MP_TRACE(demuxer, "mapped pkt: type=%d pts=%f dts=%f\n",
             sh->type, pkt->pts, pkt->dts);

    // Detect slideshow video, one still per song on audio DVDs. Audio
    // running several seconds past the last video packet cannot happen with
    // really interleaved video.
    if ((p->is_dvd || p->is_dvda) && pkt->dts != MP_NOPTS_VALUE) {
        if (sh->type == STREAM_VIDEO) {
            p->video_sh = sh;
            p->last_video_dts = pkt->dts;
        } else if (sh->type == STREAM_AUDIO && p->video_sh &&
                   !p->video_sh->still_image &&
                   demux_stream_is_selected(p->video_sh) &&
                   p->last_video_dts != MP_NOPTS_VALUE &&
                   pkt->dts - p->last_video_dts > 5.0)
        {
            MP_VERBOSE(demuxer, "no video for %.1fs while audio advances; "
                       "marking video as sparse still images\n",
                       pkt->dts - p->last_video_dts);
            // Stop the starved video stream from forcing eager reads (it
            // would race through the source hunting for the next still).
            demux_set_stream_still_image(demuxer, p->video_sh, true);
        }
    }

    // Drop audio between the chapter-start landing and the seek target
    // (see d_seek).
    if (p->skip_audio_until != MP_NOPTS_VALUE && sh->type == STREAM_AUDIO &&
        pkt->pts != MP_NOPTS_VALUE)
    {
        if (pkt->pts < p->skip_audio_until) {
            talloc_free(pkt);
            return true;
        }
        MP_VERBOSE(demuxer, "audio reached seek target %f\n",
                   p->skip_audio_until);
        p->skip_audio_until = MP_NOPTS_VALUE;
    }

    if (sh->type == STREAM_AUDIO && pkt->pts != MP_NOPTS_VALUE) {
        inject_still(demuxer, pkt->pts);
        // Deliver the still before the audio it belongs to, so the frame is
        // already present when the player starts or restarts here.
        if (p->pending_pkt) {
            struct demux_packet *still = p->pending_pkt;
            p->pending_pkt = pkt;
            *out_pkt = still;
            return 1;
        }
    }

    *out_pkt = pkt;
    return 1;
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
        mp_tags_set_str(new.metadata, "TITLE",
                        mp_tprintf(42, "title: %u (%s)", title + 1, time));
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
        demuxer_add_chapter(demuxer, "", p, 0);
    }
}

static int d_open(demuxer_t *demuxer, enum demux_check check)
{
    struct priv *p = demuxer->priv = talloc_zero(demuxer, struct priv);

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
    p->skip_audio_until = MP_NOPTS_VALUE;

    if (p->is_cdda)
        params.force_format = "+rawaudio";

    char *t = NULL;
    stream_control(demuxer->stream, STREAM_CTRL_GET_DISC_NAME, &t);
    if (t) {
        mp_tags_set_str(demuxer->metadata, "TITLE", t);
        talloc_free(t);
    }

    // Initialize the playback time. We need to read _some_ data to get the
    // correct stream-layer time (at least with libdvdnav).
    stream_read_peek(demuxer->stream, &(char){0}, 1);
    reset_pts(demuxer);

    // Boundaries settled before the first slave open predate any player
    // state to resync, release them so the probe can read.
    if (!p->is_dvd)
        stream_control(demuxer->stream, STREAM_CTRL_NAV_DRAIN_ACK, NULL);

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

    add_dvd_streams(demuxer);
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
