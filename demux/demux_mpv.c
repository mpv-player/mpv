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

#include "demux.h"

#include <common/playlist.h>
#include <stream/stream.h>

static int open_mpv(struct demuxer *demuxer, enum demux_check check)
{
    if (check != DEMUX_CHECK_REQUEST)
        return -1;

    struct stream *s = demuxer->stream;
    if (!s->info || strcmp(s->info->name, "mpv"))
        return -1;

    demuxer->playlist = talloc_zero(demuxer, struct playlist);
    mp_url_unescape_inplace(s->path);
    playlist_append_file(demuxer->playlist, s->path);
    playlist_set_stream_flags(demuxer->playlist, demuxer->stream_origin);
    demuxer->fully_read = true;
    demux_close_stream(demuxer);

    return 0;
}

const struct demuxer_desc demuxer_desc_mpv = {
    .name = "mpv",
    .desc = "mpv protocol",
    .open = open_mpv,
};
