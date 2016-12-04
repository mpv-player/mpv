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

#include <archive.h>
#include <archive_entry.h>

#include "common/common.h"
#include "common/playlist.h"
#include "stream/stream.h"
#include "demux.h"

#include "stream/stream_libarchive.h"

static int cmp_filename(const void *a, const void *b)
{
    return strcmp(*(char **)a, *(char **)b);
}

static int open_file(struct demuxer *demuxer, enum demux_check check)
{
    if (!demuxer->access_references)
        return -1;

    int flags = 0;
    int probe_size = STREAM_BUFFER_SIZE;
    if (check <= DEMUX_CHECK_REQUEST) {
        flags |= MP_ARCHIVE_FLAG_UNSAFE;
        probe_size *= 100;
    }

    bstr probe = stream_peek(demuxer->stream, probe_size);
    if (probe.len == 0)
        return -1;
    struct stream *probe_stream = open_memory_stream(probe.start, probe.len);
    struct mp_archive *mpa = mp_archive_new(mp_null_log, probe_stream, flags);
    bool ok = !!mpa;
    free_stream(probe_stream);
    mp_archive_free(mpa);
    if (!ok)
        return -1;

    mpa = mp_archive_new(demuxer->log, demuxer->stream, flags);
    if (!mpa)
        return -1;

    struct playlist *pl = talloc_zero(demuxer, struct playlist);
    demuxer->playlist = pl;

    // make it load archive://
    pl->disable_safety = true;

    char *prefix = mp_url_escape(mpa, demuxer->stream->url, "~|");

    char **files = NULL;
    int num_files = 0;

    while (mp_archive_next_entry(mpa)) {
        // stream_libarchive.c does the real work
        char *f = talloc_asprintf(mpa, "archive://%s|%s", prefix,
                                  mpa->entry_filename);
        MP_TARRAY_APPEND(mpa, files, num_files, f);
    }

    if (files)
        qsort(files, num_files, sizeof(files[0]), cmp_filename);

    for (int n = 0; n < num_files; n++)
        playlist_add_file(pl, files[n]);

    demuxer->filetype = "archive";
    demuxer->fully_read = true;

    mp_archive_free(mpa);

    return 0;
}

const struct demuxer_desc demuxer_desc_libarchive = {
    .name = "libarchive",
    .desc = "libarchive wrapper",
    .open = open_file,
};
