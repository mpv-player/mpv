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
#include "stheader.h"

#include "video/csputils.h"

// DVD-Video has 32 subpicture (SPU) streams, mapped to PES substream IDs 0x20..0x3F.
#define MAX_DVD_SPU_STREAMS 32

// If the timestamp difference between subsequent packets is this big, assume
// a reset. It should be big enough to account for 1. low video framerates and
// large audio frames, and 2. bad interleaving.
#define DTS_RESET_THRESHOLD 5.0

struct priv {
    struct demuxer *slave;

    // All outer sh_streams we have ever surfaced to the parent demuxer.
    struct sh_stream **outer_streams;
    int num_outer_streams;

    // Maps the current slave's stream index to its matching outer sh_stream.
    struct sh_stream **slave_to_outer;
    int slave_to_outer_count;

    // Per slave-stream-index flag: when set, the next packet from that slave
    // stream is tagged segmented + pkt->codec=outer->codec so the decoder
    // wrapper reinitialises for the freshly-refreshed codec.
    bool *needs_segment_marker;
    int needs_segment_marker_count;

    // DVD-only: pre-registered sub streams keyed by PES substream ID minus
    // 0x20, carrying the disc-level CLUT as extradata.
    struct sh_stream *dvd_subs[MAX_DVD_SPU_STREAMS];

    // Used to rewrite the raw MPEG timestamps to playback time.
    double base_time;   // playback display start time of current segment
    double base_dts;    // packet DTS that maps to base_time
    double last_dts;    // DTS of previously demuxed packet
    bool seek_reinit;   // needs reinit after seek
    uint32_t last_discontinuity_id; // Last source-position-jump id seen from the stream.
    bool nav_active;    // last interactive-nav state pushed to the cache

    bool is_dvd, is_cdda;
};

static void reselect_streams(demuxer_t *demuxer)
{
    struct priv *p = demuxer->priv;
    int num_slave = demux_get_num_stream(p->slave);
    for (int n = 0; n < num_slave && n < p->slave_to_outer_count; n++) {
        struct sh_stream *outer = p->slave_to_outer[n];
        if (outer) {
            demuxer_select_track(p->slave, demux_get_stream(p->slave, n),
                MP_NOPTS_VALUE, demux_stream_is_selected(outer));
        }
    }
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

static struct sh_stream *find_outer_for_slave(struct priv *p,
                                              struct sh_stream *src)
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
        if (sh && sh->type == src->type && sh->demuxer_id == src->demuxer_id)
            return sh;
    }
    return NULL;
}

// Build / rebuild the slave-index -> outer-sh map. For each slave stream reuse
// or register a fresh outer sh_stream as follows and expose it to the parent demuxer.
static void sync_streams(struct demuxer *demuxer)
{
    struct priv *p = demuxer->priv;
    int num_slave = demux_get_num_stream(p->slave);

    if (num_slave > p->slave_to_outer_count) {
        MP_TARRAY_GROW(p, p->slave_to_outer, num_slave - 1);
        MP_TARRAY_GROW(p, p->needs_segment_marker, num_slave - 1);
        for (int n = p->slave_to_outer_count; n < num_slave; n++) {
            p->slave_to_outer[n] = NULL;
            p->needs_segment_marker[n] = false;
        }
        p->slave_to_outer_count = num_slave;
        p->needs_segment_marker_count = num_slave;
    }

    for (int n = 0; n < num_slave; n++) {
        struct sh_stream *src = demux_get_stream(p->slave, n);
        struct sh_stream *outer = find_outer_for_slave(p, src);

        if (!outer) {
            outer = demux_alloc_sh_stream(src->type);
            adopt_codec_params(outer, src);
            outer->demuxer_id = src->demuxer_id;
            outer->dependent_track = src->dependent_track;
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
                MP_VERBOSE(demuxer, "stream %d codec changed: %s -> %s\n",
                           n, cur_codec, new_codec);
                adopt_codec_params(outer, src);
                p->needs_segment_marker[n] = true;
            }
        }

        p->slave_to_outer[n] = outer;
    }

    // Propagate outer selection state to the slave.
    for (int n = 0; n < num_slave; n++) {
        struct sh_stream *outer = p->slave_to_outer[n];
        if (outer) {
            demuxer_select_track(p->slave, demux_get_stream(p->slave, n),
                MP_NOPTS_VALUE, demux_stream_is_selected(outer));
        }
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

    MP_VERBOSE(demuxer, "seek to: %f\n", seek_pts);

    double seek_arg[] = {seek_pts, flags};
    stream_control(demuxer->stream, STREAM_CTRL_SEEK_TO_TIME, seek_arg);

    if (p->slave->desc->drop_buffers)
        p->slave->desc->drop_buffers(p->slave);

    p->seek_reinit = true;
}

static void reset_pts(demuxer_t *demuxer)
{
    struct priv *p = demuxer->priv;

    double base;
    if (stream_control(demuxer->stream, STREAM_CTRL_GET_CURRENT_TIME, &base) < 1)
        base = 0;

    MP_VERBOSE(demuxer, "reset to time: %f\n", base);

    p->base_dts = p->last_dts = MP_NOPTS_VALUE;
    p->base_time = base;
    p->seek_reinit = false;
}

static void add_stream_chapters(struct demuxer *demuxer);

// Sync demuxer->edition with the disc's current playback position. The disc
// nav state takes precedence: if a menu is active, point at the synthetic
// "Disc Menu" entry add_stream_editions() appended at num_editions - 1;
// otherwise mirror the stream's GET_CURRENT_TITLE.
static void sync_initial_edition(struct demuxer *demuxer)
{
    unsigned title;
    if (stream_control(demuxer->stream, STREAM_CTRL_GET_CURRENT_TITLE, &title) >= 1)
        demuxer->edition = title;
    struct stream_nav_state nav = {0};
    if (stream_control(demuxer->stream, STREAM_CTRL_GET_NAV_STATE, &nav) >= 1
        && nav.menu_active && demuxer->num_editions > 0)
    {
        demuxer->edition = demuxer->num_editions - 1;
    }
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
    // Discard anything the stream wrapper buffered before the disc-nav
    // discontinuity.
    stream_drop_buffers(demuxer->stream);
    p->slave = demux_open_url("-", &params, demuxer->cancel, demuxer->global);
    if (!p->slave) {
        MP_ERR(demuxer, "Failed to reopen slave demuxer after discontinuity\n");
        return false;
    }

    for (int n = 0; n < p->slave_to_outer_count; n++) {
        p->slave_to_outer[n] = NULL;
        p->needs_segment_marker[n] = false;
    }

    sync_streams(demuxer);

    // Refresh duration / chapters / edition for the new playlist.
    double len;
    if (stream_control(demuxer->stream, STREAM_CTRL_GET_TIME_LENGTH, &len) >= 1)
        demux_set_duration(demuxer, len);
    else
        demux_set_duration(demuxer, -1);

    for (int n = 0; n < demuxer->num_chapters; n++)
        talloc_free(demuxer->chapters[n].metadata);
    demuxer->num_chapters = 0;
    add_stream_chapters(demuxer);

    sync_initial_edition(demuxer);

    demux_lists_changed(demuxer);

    return true;
}

static bool d_read_packet(struct demuxer *demuxer, struct demux_packet **out_pkt)
{
    struct priv *p = demuxer->priv;

    struct stream_nav_state nav = {0};
    if (stream_control(demuxer->stream, STREAM_CTRL_GET_NAV_STATE, &nav) >= 1) {
        if (nav.nav_active != p->nav_active) {
            p->nav_active = nav.nav_active;
            demux_set_nav_active(demuxer, nav.nav_active);
        }
        if (nav.discontinuity_id != p->last_discontinuity_id) {
            MP_VERBOSE(demuxer, "discontinuity %u->%u, reopening slave\n",
                       p->last_discontinuity_id, nav.discontinuity_id);
            if (!reopen_slave(demuxer))
                return false;
            if (stream_control(demuxer->stream, STREAM_CTRL_GET_NAV_STATE, &nav) >= 1)
                p->last_discontinuity_id = nav.discontinuity_id;
            p->seek_reinit = true;
        }
    }

    struct demux_packet *pkt = demux_read_any_packet(p->slave);
    if (!pkt) {
        // The slave can hit EOF mid-playback when the stream layer breaks
        // its read at a disc-driven discontinuity.
        struct stream_nav_state nav2 = {0};
        if (stream_control(demuxer->stream, STREAM_CTRL_GET_NAV_STATE, &nav2) >= 1
            && nav2.discontinuity_id != p->last_discontinuity_id)
        {
            MP_VERBOSE(demuxer, "discontinuity %u->%u at EOF, reopening slave\n",
                       p->last_discontinuity_id, nav2.discontinuity_id);
            if (!reopen_slave(demuxer))
                return false;
            p->last_discontinuity_id = nav2.discontinuity_id;
            p->seek_reinit = true;
            pkt = demux_read_any_packet(p->slave);
        }
        if (!pkt)
            return false;
    }

    demux_update(p->slave, MP_NOPTS_VALUE);

    if (p->seek_reinit)
        reset_pts(demuxer);

    int slave_index = pkt->stream;
    if (demux_get_num_stream(p->slave) > p->slave_to_outer_count ||
        slave_index >= p->slave_to_outer_count ||
        !p->slave_to_outer[slave_index])
    {
        sync_streams(demuxer);
    }

    struct sh_stream *sh = slave_index < p->slave_to_outer_count
                              ? p->slave_to_outer[slave_index] : NULL;
    if (!sh || !demux_stream_is_selected(sh)) {
        talloc_free(pkt);
        return true;
    }

    // First packet from a slave stream whose matched outer just had its
    // codec refreshed gets tagged as a new segment so f_decoder_wrapper
    // drains and reinits the decoder.
    if (slave_index < p->needs_segment_marker_count &&
        p->needs_segment_marker[slave_index])
    {
        p->needs_segment_marker[slave_index] = false;
        pkt->segmented = true;
        pkt->codec = sh->codec;
        pkt->start = MP_NOPTS_VALUE;
        pkt->end = MP_NOPTS_VALUE;
    }

    pkt->stream = sh->index;

    if (p->is_cdda) {
        *out_pkt = pkt;
        return true;
    }

    MP_TRACE(demuxer, "ipts: %d %f %f\n", sh->type, pkt->pts, pkt->dts);

    if (sh->type == STREAM_SUB) {
        if (p->base_dts == MP_NOPTS_VALUE)
            MP_WARN(demuxer, "subtitle packet along PTS reset\n");
    } else if (pkt->dts != MP_NOPTS_VALUE) {
        // Use the very first DTS to rebase the start time of the MPEG stream
        // to the playback time.
        if (p->base_dts == MP_NOPTS_VALUE)
            p->base_dts = pkt->dts;

        if (p->last_dts == MP_NOPTS_VALUE)
            p->last_dts = pkt->dts;

        if (fabs(p->last_dts - pkt->dts) >= DTS_RESET_THRESHOLD) {
            MP_VERBOSE(demuxer, "PTS discontinuity: %f->%f\n", p->last_dts, pkt->dts);
            p->base_time += p->last_dts - p->base_dts;
            p->base_dts = pkt->dts;
            if (pkt->duration > 0)
                p->base_dts -= pkt->duration;
        }
        p->last_dts = pkt->dts;
    }

    if (p->base_dts != MP_NOPTS_VALUE) {
        double delta = -p->base_dts + p->base_time;
        if (pkt->pts != MP_NOPTS_VALUE)
            pkt->pts += delta;
        if (pkt->dts != MP_NOPTS_VALUE)
            pkt->dts += delta;
    }

    MP_TRACE(demuxer, "opts: %d %f %f\n", sh->type, pkt->pts, pkt->dts);

    *out_pkt = pkt;
    return 1;
}

static void add_stream_editions(struct demuxer *demuxer)
{
    unsigned titles = 0;
    if (stream_control(demuxer->stream, STREAM_CTRL_GET_NUM_TITLES, &titles) != STREAM_OK)
        return;
    for (unsigned title = 0; title < titles; ++title) {
        double duration = title;
        if (stream_control(demuxer->stream, STREAM_CTRL_GET_TITLE_LENGTH, &duration) != STREAM_OK)
            continue;

        struct demux_edition new = {
            .demuxer_id = title,
            .default_edition = false,
            .metadata = talloc_zero(demuxer, struct mp_tags),
        };
        MP_TARRAY_APPEND(demuxer, demuxer->editions, demuxer->num_editions, new);

        char *time = mp_format_time(duration, true);
        double playlist = title;
        if (stream_control(demuxer->stream, STREAM_CTRL_GET_TITLE_PLAYLIST, &playlist) == STREAM_OK)
            time = talloc_asprintf_append(time, ") (%05.0f.mpls", playlist);
        mp_tags_set_str(new.metadata, "TITLE",
                        mp_tprintf(42, "title: %u (%s)", title + 1, time));
        talloc_free(time);
    }

    // Append a synthetic "Disc Menu" entry.
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

    p->slave = demux_open_url("-", &params, demuxer->cancel, demuxer->global);
    if (!p->slave)
        return -1;

    // Can be seekable even if the stream isn't.
    demuxer->seekable = true;
    // Partially seekable to refresh seek on track changes.
    demuxer->partially_seekable = true;

    add_dvd_streams(demuxer);
    sync_streams(demuxer);
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
};
