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
    // This contains each DVD sub stream, or NULL. Needed because
    struct sh_stream *dvd_subs[32];
};

static void reselect_streams(demuxer_t *demuxer)
{
    struct priv *p = demuxer->priv;
    for (int n = 0; n < MPMIN(p->slave->num_streams, p->num_streams); n++) {
        if (p->streams[n]) {
            demuxer_select_track(p->slave, p->slave->streams[n],
                demuxer_stream_is_selected(demuxer, p->streams[n]));
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
            float cmatrix[3][4];
            mp_get_yuv2rgb_coeffs(&csp, cmatrix);

            char *s = talloc_strdup(sh, "");
            s = talloc_asprintf_append(s, "palette: ");
            for (int i = 0; i < 16; i++) {
                int color = info.palette[i];
                int c[3] = {(color >> 16) & 0xff, (color >> 8) & 0xff, color & 0xff};
                mp_map_int_color(cmatrix, 8, c);
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
        assert(p->num_streams == n); // directly mapped
        MP_TARRAY_APPEND(p, p->streams, p->num_streams, sh);
        // Copy all stream fields that might be relevant
        sh->codec = talloc_strdup(sh, src->codec);
        sh->format = src->format;
        sh->lav_headers = src->lav_headers;
        if (sh && src->video) {
            double ar;
            if (stream_control(demuxer->stream, STREAM_CTRL_GET_ASPECT_RATIO, &ar)
                                == STREAM_OK)
                src->video->aspect = ar;
        }
        if (sh && src->audio)
            sh->audio = src->audio;
    }
    reselect_streams(demuxer);
}

static void d_seek(demuxer_t *demuxer, float rel_seek_secs, int flags)
{
    struct priv *p = demuxer->priv;

    if (demuxer->stream->uncached_type == STREAMTYPE_CDDA) {
        demux_seek(p->slave, rel_seek_secs, flags);
        return;
    }

    double pts;
    if (flags & SEEK_ABSOLUTE)
        pts = 0.0f;
    else {
        if (demuxer->stream_pts == MP_NOPTS_VALUE)
            return;
        pts = demuxer->stream_pts;
    }

    if (flags & SEEK_FACTOR) {
        double tmp = 0;
        stream_control(demuxer->stream, STREAM_CTRL_GET_TIME_LENGTH, &tmp);
        pts += tmp * rel_seek_secs;
    } else {
        pts += rel_seek_secs;
    }

    stream_control(demuxer->stream, STREAM_CTRL_SEEK_TO_TIME, &pts);
    demux_control(p->slave, DEMUXER_CTRL_RESYNC, NULL);
}

static int d_fill_buffer(demuxer_t *demuxer)
{
    struct priv *p = demuxer->priv;

    struct demux_packet *pkt = demux_read_any_packet(p->slave);
    if (!pkt)
        return 0;

    add_streams(demuxer);
    if (pkt->stream >= p->num_streams) { // out of memory?
        abort();
        talloc_free(pkt);
        return 0;
    }

    struct sh_stream *sh = p->streams[pkt->stream];

    // Use only one stream for stream_pts, otherwise PTS might be jumpy.
    if (sh->type == STREAM_VIDEO) {
        double pts;
        if (stream_control(demuxer->stream, STREAM_CTRL_GET_CURRENT_TIME, &pts) > 0)
            pkt->stream_pts = pts;
    }

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
        demuxer_add_chapter(demuxer, bstr0(""), p * 1e9, 0, 0);
    }
}

static int d_open(demuxer_t *demuxer, enum demux_check check)
{
    struct priv *p = demuxer->priv = talloc_zero(demuxer, struct priv);

    if (check != DEMUX_CHECK_FORCE)
        return -1;

    char *demux = "+lavf";
    if (demuxer->stream->uncached_type == STREAMTYPE_CDDA)
        demux = "+rawaudio";

    p->slave = demux_open(demuxer->stream, demux, NULL, demuxer->global);
    if (!p->slave)
        return -1;

    // Incorrect, but fixes some behavior
    demuxer->ts_resets_possible = false;
    // Doesn't work, because stream_pts is a "guess".
    demuxer->accurate_seek = false;
    // Can be seekable even if the stream isn't.
    demuxer->seekable = true;

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
    case DEMUXER_CTRL_SWITCHED_TRACKS:
        reselect_streams(demuxer);
        return DEMUXER_CTRL_OK;
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
    .type = DEMUXER_TYPE_DISC,
};
