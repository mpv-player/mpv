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

#include "mpvcore/mp_common.h"
#include "mpvcore/options.h"
#include "mpvcore/mp_msg.h"
#include "mpvcore/playlist.h"
#include "mpvcore/path.h"
#include "stream/stream.h"
#include "demux.h"

#define PROBE_SIZE (8 * 1024)

struct pl_parser {
    struct stream *s;
    char buffer[8 * 1024];
    int utf16;
    struct playlist *pl;
    bool probing;
};

static char *pl_get_line0(struct pl_parser *p)
{
    char *res = stream_read_line(p->s, p->buffer, sizeof(p->buffer), p->utf16);
    if (res) {
        int len = strlen(res);
        if (len > 0 && res[len - 1] == '\n')
            res[len - 1] = '\0';
    }
    return res;
}

static bstr pl_get_line(struct pl_parser *p)
{
    return bstr0(pl_get_line0(p));
}

static void pl_add(struct pl_parser *p, bstr entry)
{
    char *s = bstrto0(NULL, entry);
    playlist_add_file(p->pl, s);
    talloc_free(s);
}

static bool pl_eof(struct pl_parser *p)
{
    return p->s->eof;
}

static int parse_m3u(struct pl_parser *p)
{
    bstr line = bstr_strip(pl_get_line(p));
    if (!bstr_equals0(line, "#EXTM3U"))
        return -1;
    if (p->probing)
        return 0;
    while (!pl_eof(p)) {
        line = bstr_strip(pl_get_line(p));
        if (line.len == 0 || bstr_startswith0(line, "#"))
            continue;
        pl_add(p, line);
    }
    return 0;
}

static int parse_ref_init(struct pl_parser *p)
{
    bstr line = bstr_strip(pl_get_line(p));
    if (!bstr_equals0(line, "[Reference]"))
        return -1;
    while (!pl_eof(p)) {
        line = bstr_strip(pl_get_line(p));
        if (bstr_case_startswith(line, bstr0("Ref"))) {
            bstr_split_tok(line, "=", &(bstr){0}, &line);
            if (line.len)
                pl_add(p, line);
        }
    }
    return 0;
}

static int parse_mov_rtsptext(struct pl_parser *p)
{
    bstr line = pl_get_line(p);
    if (!bstr_eatstart(&line, bstr0("RTSPtext")))
        return -1;
    if (p->probing)
        return 0;
    line = bstr_strip(line);
    do {
        if (bstr_case_startswith(line, bstr0("rtsp://"))) {
            pl_add(p, line);
            return 0;
        }
    } while (!pl_eof(p) && (line = bstr_strip(pl_get_line(p))).len);
    return -1;
}

static int parse_pls(struct pl_parser *p)
{
    bstr line = {0};
    while (!line.len && !pl_eof(p))
        line = bstr_strip(pl_get_line(p));
    if (bstrcasecmp0(line, "[playlist]") != 0)
        return -1;
    if (p->probing)
        return 0;
    while (!pl_eof(p)) {
        line = bstr_strip(pl_get_line(p));
        bstr key, value;
        if (bstr_split_tok(line, "=", &key, &value) &&
            bstr_case_startswith(key, bstr0("File")))
        {
            pl_add(p, value);
        }
    }
    return 0;
}

struct pl_format {
    const char *name;
    int (*parse)(struct pl_parser *p);
};

static const struct pl_format formats[] = {
    {"m3u", parse_m3u},
    {"ini", parse_ref_init},
    {"mov", parse_mov_rtsptext},
    {"pls", parse_pls},
};

static const struct pl_format *probe_pl(struct pl_parser *p, bool force)
{
    int64_t start = stream_tell(p->s);
    for (int n = 0; n < MP_ARRAY_SIZE(formats); n++) {
        const struct pl_format *fmt = &formats[n];
        stream_seek(p->s, start);
        if (fmt->parse(p) >= 0)
            return fmt;
    }
    return NULL;
}

static int open_file(struct demuxer *demuxer, enum demux_check check)
{
    bool force = check < DEMUX_CHECK_UNSAFE || check == DEMUX_CHECK_REQUEST;

    struct pl_parser *p = talloc_zero(NULL, struct pl_parser);
    p->pl = talloc_zero(p, struct playlist);

    bstr probe_buf = stream_peek(demuxer->stream, PROBE_SIZE);
    p->s = open_memory_stream(probe_buf.start, probe_buf.len);
    p->utf16 = stream_skip_bom(p->s);
    p->probing = true;
    const struct pl_format *fmt = probe_pl(p, force);
    free_stream(p->s);
    playlist_clear(p->pl);
    if (!fmt) {
        talloc_free(p);
        return -1;
    }

    p->probing = false;
    p->s = demuxer->stream;
    p->utf16 = stream_skip_bom(p->s);
    bool ok = fmt->parse(p) >= 0;
    if (ok)
        playlist_add_base_path(p->pl, mp_dirname(demuxer->filename));
    demuxer->playlist = talloc_steal(demuxer, p->pl);
    demuxer->filetype = fmt->name;
    talloc_free(p);
    return ok ? 0 : -1;
}

const struct demuxer_desc demuxer_desc_playlist = {
    .name = "playlist",
    .desc = "Playlist file",
    .open = open_file,
};
