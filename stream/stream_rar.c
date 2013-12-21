// Major parts based on:
/*****************************************************************************
 * access.c: uncompressed RAR access
 *****************************************************************************
 * Copyright (C) 2008-2010 Laurent Aimar
 * $Id: dcd973529e0029abe326d31f8d58cd13bbcc276c $
 *
 * Author: Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "config.h"

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "osdep/io.h"

#include "common/msg.h"
#include "stream.h"
#include "options/m_option.h"
#include "rar.h"

/*
This works as follows:

- stream_open() with file01.rar
    - is opened as normal file (stream_file.c or others) first
    - stream_info_rar_filter stream filter applies
    - leads to rar_filter_open()
    - if multi-part, opens file02.rar, file03.rar, etc. as actual streams
      (recursive opening is prevented with the STREAM_NO_FILTERS flag)
    - read accesses return a m3u playlist with entries like:
        rar://bla01.rar|subfile.mkv
      (one such entry for each file contained in the rar)
- stream_open() with the playlist entry, e.g. rar://bla01.rar|subfile.mkv
    - leads to rar_entry_open()
    - opens bla01.rar etc. again as actual streams
      (again, STREAM_NO_FILTERS to open the actual files)
    - read accesses go into subfile.mkv contained in the rar file(s)
*/

static int rar_entry_fill_buffer(stream_t *s, char *buffer, int max_len)
{
    rar_file_t *rar_file = s->priv;
    return RarRead(rar_file, buffer, max_len);
}

static int rar_entry_seek(stream_t *s, int64_t newpos)
{
    rar_file_t *rar_file = s->priv;
    return RarSeek(rar_file, newpos);
}

static void rar_entry_close(stream_t *s)
{
    rar_file_t *rar_file = s->priv;
    RarFileDelete(rar_file);
}

static int rar_entry_control(stream_t *s, int cmd, void *arg)
{
    rar_file_t *rar_file = s->priv;
    switch (cmd) {
    case STREAM_CTRL_GET_BASE_FILENAME:
        *(char **)arg = talloc_strdup(NULL, rar_file->s->url);
        return STREAM_OK;
    }
    return STREAM_UNSUPPORTED;
}

static int rar_entry_open(stream_t *stream, int mode)
{
    if (!strchr(stream->path, '|'))
        return STREAM_ERROR;

    char *base = talloc_strdup(stream, stream->path);
    char *name = strchr(base, '|');
    *name++ = '\0';
    mp_url_unescape_inplace(base);

    struct stream *rar =
        stream_create(base, STREAM_READ | STREAM_NO_FILTERS, stream->global);
    if (!rar)
        return STREAM_ERROR;

    int count;
    rar_file_t **files;
    if (RarProbe(rar) || RarParse(rar, &count, &files)) {
        free_stream(rar);
        return STREAM_ERROR;
    }

    rar_file_t *file = NULL;
    for (int i = 0; i < count; i++) {
        if (!file && strcmp(files[i]->name, name) == 0)
            file = files[i];
        else
            RarFileDelete(files[i]);
    }
    talloc_free(files);
    if (!file) {
        free_stream(rar);
        return STREAM_ERROR;
    }

    rar_file_chunk_t dummy = {
        .mrl = base,
    };
    file->current_chunk = &dummy;
    file->s = rar; // transfer ownership
    file->global = stream->global;
    RarSeek(file, 0);

    stream->priv = file;
    stream->end_pos = file->size;
    stream->fill_buffer = rar_entry_fill_buffer;
    stream->seek = rar_entry_seek;
    stream->close = rar_entry_close;
    stream->control = rar_entry_control;

    return STREAM_OK;
}

static int rar_filter_fill_buffer(stream_t *s, char *buffer, int max_len)
{
    struct stream *m = s->priv;
    return stream_read_partial(m, buffer, max_len);
}

static int rar_filter_seek(stream_t *s, int64_t newpos)
{
    struct stream *m = s->priv;
    return stream_seek(m, newpos);
}

static void rar_filter_close(stream_t *s)
{
    struct stream *m = s->priv;
    free_stream(m);
}

static int rar_filter_open(stream_t *stream, int mode)
{
    if (mode != STREAM_READ)
        return STREAM_UNSUPPORTED;

    struct stream *rar = stream->source;
    if (!rar)
        return STREAM_UNSUPPORTED;

    int count;
    rar_file_t **files;
    if (!rar || RarProbe(rar) || RarParse(rar, &count, &files))
        return STREAM_UNSUPPORTED;

    void *tmp = talloc_new(NULL);

    // Create a playlist containing all entries of the .rar file. The URLs
    // link to rar_entry_open().
    char *prefix = mp_url_escape(tmp, stream->url, "~|");
    char *pl = talloc_strdup(tmp, "#EXTM3U\n");
    for (int n = 0; n < count; n++) {
        pl = talloc_asprintf_append_buffer(pl, "rar://%s|%s\n",
                                           prefix, files[n]->name);
        RarFileDelete(files[n]);
    }
    talloc_free(files);

    struct stream *m = open_memory_stream(pl, strlen(pl));

    stream->priv = m;
    stream->end_pos = m->end_pos;
    stream->fill_buffer = rar_filter_fill_buffer;
    stream->seek = rar_filter_seek;
    stream->close = rar_filter_close;
    stream->safe_origin = true;

    talloc_free(tmp);
    return STREAM_OK;
}

const stream_info_t stream_info_rar_entry = {
    .name = "rar_entry",
    .open = rar_entry_open,
    .protocols = (const char*[]){ "rar", NULL },
};

const stream_info_t stream_info_rar_filter = {
    .name = "rar_filter",
    .open = rar_filter_open,
    .stream_filter = true,
};
