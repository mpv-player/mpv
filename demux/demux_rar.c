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

#include "common/common.h"
#include "common/playlist.h"
#include "stream/stream.h"
#include "stream/rar.h"
#include "demux.h"

static int open_file(struct demuxer *demuxer, enum demux_check check)
{
    if (!demuxer->access_references)
        return -1;

    if (RarProbe(demuxer->stream))
        return -1;

    int count;
    rar_file_t **files;
    if (RarParse(demuxer->stream, &count, &files))
        return -1;

    void *tmp = talloc_new(NULL);
    talloc_steal(tmp, files);

    struct playlist *pl = talloc_zero(demuxer, struct playlist);
    demuxer->playlist = pl;

    // make it load rar://
    pl->disable_safety = true;

    char *prefix = mp_url_escape(tmp, demuxer->stream->url, "~|");
    for (int n = 0; n < count; n++) {
        // stream_rar.c does the real work
        playlist_add_file(pl,
                talloc_asprintf(tmp, "rar://%s|%s", prefix, files[n]->name));
        RarFileDelete(files[n]);
    }

    demuxer->filetype = "rar";
    demuxer->fully_read = true;

    talloc_free(tmp);
    return 0;
}

const struct demuxer_desc demuxer_desc_rar = {
    .name = "rar",
    .desc = "Rar archive file",
    .open = open_file,
};
