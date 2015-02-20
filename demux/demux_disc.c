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

#include <string.h>
#include <math.h>
#include <assert.h>

#include "common/common.h"
#include "common/msg.h"

#include "stream/stream.h"
#include "demux.h"
#include "stheader.h"

#include "video/csputils.h"

struct priv {
    struct demuxer *slave;
    // streams[slave_stream_index] == our_stream
    struct sh_stream **streams;
    int num_streams;
    // This contains each DVD sub stream, or NULL. Needed because DVD packets
    // can come arbitrarily late in the MPEG stream, so the slave demuxer
    // might add the streams only later.
    struct sh_stream *dvd_subs[32];
    // Used to rewrite the raw MPEG timestamps to playback time.
    double base_time;   // playback display start time of current segment
    double base_dts;    // packet DTS that maps to base_time
    double last_dts;    // DTS of previously demuxed packet
    double seek_pts;
    bool seek_reinit;   // needs reinit after seek
};

// If the timestamp difference between subsequent packets is this big, assume
// a reset. It should be big enough to account for 1. low video framerates and
// large audio frames, and 2. bad interleaving.
#define DTS_RESET_THRESHOLD 5.0

static void reselect_streams(demuxer_t *demuxer)
{
    struct priv *p = demuxer->priv;
    for (int n = 0; n < MPMIN(p->slave->num_streams, p->num_streams); n++) {
        if (p->streams[n]) {
            demuxer_select_track(p->slave, p->slave->streams[n],
                demux_stream_is_selected(p->streams[n]));
        }
    }
}

static void get_disc_lang(struct stream *stream, struct sh_stream *sh)
{
    struct stream_lang_req req = {.type = sh->type, .id = sh->demuxer_id};
    if (stream->uncached_type == STREAMTYPE_DVD && sh->type == STREAM_SUB)
        req.id = req.id & 0x1F; // mpeg ID to index
    stream_control(stream, STREAM_CTRL_GET_LANG, &req);
    if (req.name[0])
        sh->lang = talloc_strdup(sh, req.name);
}

static void add_dvd_streams(demuxer_t *demuxer)
{
    struct priv *p = demuxer->priv;
    struct stream *stream = demuxer->stream;
    if (stream->uncached_type != STREAMTYPE_DVD)
        return;
    struct stream_dvd_info_req info;
    if (stream_control(stream, STREAM_CTRL_GET_DVD_INFO, &info) > 0) {
        for (int n = 0; n < MPMIN(32, info.num_subs); n++) {
            struct sh_stream *sh = new_sh_stream(demuxer, STREAM_SUB);
            if (!sh)
                break;
            sh->demuxer_id = n + 0x20;
            sh->codec = "dvd_subtitle";
            get_disc_lang(stream, sh);
            // p->streams _must_ match with p->slave->streams, so we can't add
            // it yet - it has to be done when the real stream appears, which
            // could be right on start, or any time later.
            p->dvd_subs[n] = sh;

            // emulate the extradata
            struct mp_csp_params csp = MP_CSP_PARAMS_DEFAULTS;
            csp.int_bits_in = 8;
            csp.int_bits_out = 8;
            struct mp_cmat cmatrix;
            mp_get_yuv2rgb_coeffs(&csp, &cmatrix);

            char *s = talloc_strdup(sh, "");
            s = talloc_asprintf_append(s, "palette: ");
            for (int i = 0; i < 16; i++) {
                int color = info.palette[i];
                int c[3] = {(color >> 16) & 0xff, (color >> 8) & 0xff, color & 0xff};
                mp_map_int_color(&cmatrix, 8, c);
                color = (c[2] << 16) | (c[1] << 8) | c[0];

                if (i != 0)
                    s = talloc_asprintf_append(s, ", ");
                s = talloc_asprintf_append(s, "%06x", color);
            }
            s = talloc_asprintf_append(s, "\n");

            sh->sub->extradata = s;
            sh->sub->extradata_len = strlen(s);
        }
    }
}

static void add_streams(demuxer_t *demuxer)
{
    struct priv *p = demuxer->priv;

    for (int n = p->num_streams; n < p->slave->num_streams; n++) {
        struct sh_stream *src = p->slave->streams[n];
        if (src->sub) {
            struct sh_stream *sub = NULL;
            if (src->demuxer_id >= 0x20 && src->demuxer_id <= 0x3F)
                sub = p->dvd_subs[src->demuxer_id - 0x20];
            if (sub) {
                assert(p->num_streams == n); // directly mapped
                MP_TARRAY_APPEND(p, p->streams, p->num_streams, sub);
                continue;
            }
        }
        struct sh_stream *sh = new_sh_stream(demuxer, src->type);
        if (!sh)
            break;
        assert(p->num_streams == n); // directly mapped
        MP_TARRAY_APPEND(p, p->streams, p->num_streams, sh);
        // Copy all stream fields that might be relevant
        sh->codec = talloc_strdup(sh, src->codec);
        sh->format = src->format;
        sh->lav_headers = src->lav_headers;
        sh->demuxer_id = src->demuxer_id;
        if (src->video) {
            double ar;
            if (stream_control(demuxer->stream, STREAM_CTRL_GET_ASPECT_RATIO, &ar)
                                == STREAM_OK)
                sh->video->aspect = ar;
        }
        if (src->audio)
            sh->audio = src->audio;
        get_disc_lang(demuxer->stream, sh);
    }
    reselect_streams(demuxer);
}

static void d_seek(demuxer_t *demuxer, double rel_seek_secs, int flags)
{
    struct priv *p = demuxer->priv;

    if (demuxer->stream->uncached_type == STREAMTYPE_CDDA) {
        demux_seek(p->slave, rel_seek_secs, flags);
        return;
    }

    double pts = p->seek_pts;
    if (flags & SEEK_ABSOLUTE)
        pts = 0.0f;
    double base_pts = pts; // to what pts is relative

    if (flags & SEEK_FACTOR) {
        double tmp = 0;
        stream_control(demuxer->stream, STREAM_CTRL_GET_TIME_LENGTH, &tmp);
        pts += tmp * rel_seek_secs;
    } else {
        pts += rel_seek_secs;
    }

    MP_VERBOSE(demuxer, "seek to: %f\n", pts);

    double seek_arg[] = {pts, base_pts, flags};
    stream_control(demuxer->stream, STREAM_CTRL_SEEK_TO_TIME, seek_arg);
    demux_control(p->slave, DEMUXER_CTRL_RESYNC, NULL);

    p->seek_pts = pts;
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

static int d_fill_buffer(demuxer_t *demuxer)
{
    struct priv *p = demuxer->priv;

    struct demux_packet *pkt = demux_read_any_packet(p->slave);
    if (!pkt)
        return 0;

    demux_update(p->slave);

    if (p->seek_reinit)
        reset_pts(demuxer);

    add_streams(demuxer);
    if (pkt->stream >= p->num_streams) { // out of memory?
        talloc_free(pkt);
        return 0;
    }

    struct sh_stream *sh = p->streams[pkt->stream];
    if (!demux_stream_is_selected(sh)) {
        talloc_free(pkt);
        return 1;
    }

    if (demuxer->stream->uncached_type == STREAMTYPE_CDDA) {
        demux_add_packet(sh, pkt);
        return 1;
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
            MP_WARN(demuxer, "PTS discontinuity: %f->%f\n", p->last_dts, pkt->dts);
            p->base_time += p->last_dts - p->base_dts;
            p->base_dts = pkt->dts - pkt->duration;
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

    if (pkt->pts != MP_NOPTS_VALUE)
        p->seek_pts = pkt->pts;

    demux_add_packet(sh, pkt);
    return 1;
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
        demuxer_add_chapter(demuxer, bstr0(""), p, 0);
    }
}

static int d_open(demuxer_t *demuxer, enum demux_check check)
{
    struct priv *p = demuxer->priv = talloc_zero(demuxer, struct priv);

    if (check != DEMUX_CHECK_FORCE)
        return -1;

    struct demuxer_params params = {.force_format = "+lavf"};

    if (demuxer->stream->uncached_type == STREAMTYPE_CDDA)
        params.force_format = "+rawaudio";

    char *t = NULL;
    stream_control(demuxer->stream, STREAM_CTRL_GET_DISC_NAME, &t);
    if (t) {
        mp_tags_set_bstr(demuxer->metadata, bstr0("TITLE"), bstr0(t));
        talloc_free(t);
    }

    // Initialize the playback time. We need to read _some_ data to get the
    // correct stream-layer time (at least with libdvdnav).
    stream_peek(demuxer->stream, 1);
    reset_pts(demuxer);

    p->slave = demux_open(demuxer->stream, &params, demuxer->global);
    if (!p->slave)
        return -1;

    // So that we don't miss initial packets of delayed subtitle streams.
    demux_set_stream_autoselect(p->slave, true);

    // With cache enabled, the stream can be seekable. This causes demux_lavf.c
    // (actually libavformat/mpegts.c) to seek sometimes when reading a packet.
    // It does this to seek back a bit in case the current file position points
    // into the middle of a packet.
    if (demuxer->stream->uncached_type != STREAMTYPE_CDDA) {
        demuxer->stream->seekable = false;

        // Can be seekable even if the stream isn't.
        demuxer->seekable = true;

        demuxer->rel_seeks = true;
    }

    add_dvd_streams(demuxer);
    add_streams(demuxer);
    add_stream_chapters(demuxer);

    return 0;
}

static void d_close(demuxer_t *demuxer)
{
    struct priv *p = demuxer->priv;
    free_demuxer(p->slave);
}

static int d_control(demuxer_t *demuxer, int cmd, void *arg)
{
    struct priv *p = demuxer->priv;

    switch (cmd) {
    case DEMUXER_CTRL_GET_TIME_LENGTH: {
        double len;
        if (stream_control(demuxer->stream, STREAM_CTRL_GET_TIME_LENGTH, &len) < 1)
            break;
        *(double *)arg = len;
        return DEMUXER_CTRL_OK;
    }
    case DEMUXER_CTRL_RESYNC:
        demux_flush(p->slave);
        break; // relay to slave demuxer
    case DEMUXER_CTRL_SWITCHED_TRACKS:
        reselect_streams(demuxer);
        return DEMUXER_CTRL_OK;
    case DEMUXER_CTRL_GET_NAV_EVENT:
        return stream_control(demuxer->stream, STREAM_CTRL_GET_NAV_EVENT, arg)
               == STREAM_OK ? DEMUXER_CTRL_OK : DEMUXER_CTRL_DONTKNOW;
    }
    return demux_control(p->slave, cmd, arg);
}

const demuxer_desc_t demuxer_desc_disc = {
    .name = "disc",
    .desc = "CD/DVD/BD wrapper",
    .fill_buffer = d_fill_buffer,
    .open = d_open,
    .close = d_close,
    .seek = d_seek,
    .control = d_control,
};
