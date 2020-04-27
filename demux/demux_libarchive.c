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
#include "options/m_config.h"
#include "stream/stream.h"
#include "misc/natural_sort.h"
#include "demux.h"

#include "stream/stream_libarchive.h"

struct demux_libarchive_opts {
    int rar_list_all_volumes;
};

static int cmp_filename(const void *a, const void *b)
{
    return mp_natural_sort_cmp(*(char **)a, *(char **)b);
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

    void *probe = ta_alloc_size(NULL, probe_size);
    if (!probe)
        return -1;
    int probe_got = stream_read_peek(demuxer->stream, probe, probe_size);
    struct stream *probe_stream =
        stream_memory_open(demuxer->global, probe, probe_got);
    struct mp_archive *mpa = mp_archive_new(mp_null_log, probe_stream, flags, 0);
    bool ok = !!mpa;
    free_stream(probe_stream);
    mp_archive_free(mpa);
    ta_free(probe);
    if (!ok)
        return -1;

    struct demux_libarchive_opts *opts =
        mp_get_config_group(demuxer, demuxer->global, demuxer->desc->options);

    if (!opts->rar_list_all_volumes)
        flags |= MP_ARCHIVE_FLAG_NO_VOLUMES;

    mpa = mp_archive_new(demuxer->log, demuxer->stream, flags, 0);
    if (!mpa)
        return -1;

    struct playlist *pl = talloc_zero(demuxer, struct playlist);
    demuxer->playlist = pl;

    char *prefix = mp_url_escape(mpa, demuxer->stream->url, "~|");

    char **files = NULL;
    int num_files = 0;

    while (mp_archive_next_entry(mpa)) {
        // stream_libarchive.c does the real work
        char *f = talloc_asprintf(mpa, "archive://%s|/%s", prefix,
                                  mpa->entry_filename);
        MP_TARRAY_APPEND(mpa, files, num_files, f);
    }

    if (files)
        qsort(files, num_files, sizeof(files[0]), cmp_filename);

    for (int n = 0; n < num_files; n++)
        playlist_add_file(pl, files[n]);

    playlist_set_stream_flags(pl, demuxer->stream_origin);

    demuxer->filetype = "archive";
    demuxer->fully_read = true;

    mp_archive_free(mpa);
    demux_close_stream(demuxer);

    return 0;
}

#define OPT_BASE_STRUCT struct demux_libarchive_opts

const struct demuxer_desc demuxer_desc_libarchive = {
    .name = "libarchive",
    .desc = "libarchive wrapper",
    .open = open_file,
    .options = &(const struct m_sub_options){
        .opts = (const struct m_option[]) {
            {"rar-list-all-volumes", OPT_FLAG(rar_list_all_volumes)},
            {0}
        },
        .size = sizeof(OPT_BASE_STRUCT),
    },
};
