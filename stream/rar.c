/*****************************************************************************
 * rar.c: uncompressed RAR parser
 *****************************************************************************
 * Copyright (C) 2008-2010 Laurent Aimar
 * $Id: f368245f4260f913f5c211e09b7dd511a96525e6 $
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#include <assert.h>
#include <limits.h>
#include <stdio.h>

#include <libavutil/intreadwrite.h>

#include "mpv_talloc.h"
#include "common/common.h"
#include "stream.h"
#include "rar.h"

static const uint8_t rar_marker[] = {
    0x52, 0x61, 0x72, 0x21, 0x1a, 0x07, 0x00
};
static const int rar_marker_size = sizeof(rar_marker);

void RarFileDelete(rar_file_t *file)
{
    for (int i = 0; i < file->chunk_count; i++) {
        free(file->chunk[i]->mrl);
        free(file->chunk[i]);
    }
    talloc_free(file->chunk);
    free(file->name);
    free_stream(file->s);
    free(file);
}

typedef struct {
    uint16_t crc;
    uint8_t  type;
    uint16_t flags;
    uint16_t size;
    uint32_t add_size;
} rar_block_t;

enum {
    RAR_BLOCK_MARKER = 0x72,
    RAR_BLOCK_ARCHIVE = 0x73,
    RAR_BLOCK_FILE = 0x74,
    RAR_BLOCK_SUBBLOCK = 0x7a,
    RAR_BLOCK_END = 0x7b,
};
enum {
    RAR_BLOCK_END_HAS_NEXT = 0x0001,
};
enum {
    RAR_BLOCK_FILE_HAS_PREVIOUS = 0x0001,
    RAR_BLOCK_FILE_HAS_NEXT     = 0x0002,
    RAR_BLOCK_FILE_HAS_HIGH     = 0x0100,
};

static int PeekBlock(struct stream *s, rar_block_t *hdr)
{
    bstr data = stream_peek(s, 11);
    const uint8_t *peek = (uint8_t *)data.start;
    int peek_size = data.len;

    if (peek_size < 7)
        return -1;

    hdr->crc   = AV_RL16(&peek[0]);
    hdr->type  = peek[2];
    hdr->flags = AV_RL16(&peek[3]);
    hdr->size  = AV_RL16(&peek[5]);
    hdr->add_size = 0;
    if ((hdr->flags & 0x8000) ||
        hdr->type == RAR_BLOCK_FILE ||
        hdr->type == RAR_BLOCK_SUBBLOCK) {
        if (peek_size < 11)
            return -1;
        hdr->add_size = AV_RL32(&peek[7]);
    }

    if (hdr->size < 7)
        return -1;
    return 0;
}
static int SkipBlock(struct stream *s, const rar_block_t *hdr)
{
    uint64_t size = (uint64_t)hdr->size + hdr->add_size;

    while (size > 0) {
        int skip = MPMIN(size, INT_MAX);
        if (!stream_skip(s, skip))
            return -1;

        size -= skip;
    }
    return 0;
}

static int IgnoreBlock(struct stream *s, int block)
{
    /* */
    rar_block_t bk;
    if (PeekBlock(s, &bk) || bk.type != block)
        return -1;
    return SkipBlock(s, &bk);
}

static int SkipEnd(struct stream *s, const rar_block_t *hdr)
{
    if (!(hdr->flags & RAR_BLOCK_END_HAS_NEXT))
        return -1;

    if (SkipBlock(s, hdr))
        return -1;

    /* Now, we need to look for a marker block,
     * It seems that there is garbage at EOF */
    for (;;) {
        bstr peek = stream_peek(s, rar_marker_size);

        if (peek.len < rar_marker_size)
            return -1;

        if (!memcmp(peek.start, rar_marker, rar_marker_size))
            break;

        if (!stream_skip(s, 1))
            return -1;
    }

    /* Skip marker and archive blocks */
    if (IgnoreBlock(s, RAR_BLOCK_MARKER))
        return -1;
    if (IgnoreBlock(s, RAR_BLOCK_ARCHIVE))
        return -1;

    return 0;
}

static int SkipFile(struct stream *s, int *count, rar_file_t ***file,
                    const rar_block_t *hdr, const char *volume_mrl)
{
    int min_size = 7+21;
    if (hdr->flags & RAR_BLOCK_FILE_HAS_HIGH)
        min_size += 8;
    if (hdr->size < (unsigned)min_size)
        return -1;

    bstr data = stream_peek(s, min_size);
    if (data.len < min_size)
        return -1;
    const uint8_t *peek = (uint8_t *)data.start;

    /* */
    uint32_t file_size_low = AV_RL32(&peek[7+4]);
    uint8_t  method = peek[7+18];
    uint16_t name_size = AV_RL16(&peek[7+19]);
    uint32_t file_size_high = 0;
    if (hdr->flags & RAR_BLOCK_FILE_HAS_HIGH)
        file_size_high = AV_RL32(&peek[7+29]);
    const uint64_t file_size = ((uint64_t)file_size_high << 32) | file_size_low;

    char *name = calloc(1, name_size + 1);
    if (!name)
        return -1;

    const int name_offset = (hdr->flags & RAR_BLOCK_FILE_HAS_HIGH) ? (7+33) : (7+25);
    if (name_offset + name_size <= hdr->size) {
        const int max_size = name_offset + name_size;
        bstr namedata = stream_peek(s, max_size);
        if (namedata.len < max_size) {
            free(name);
            return -1;
        }
        memcpy(name, &namedata.start[name_offset], name_size);
    }

    rar_file_t *current = NULL;
    if (method != 0x30) {
        MP_WARN(s, "Ignoring compressed file %s (method=0x%2.2x)\n", name, method);
        goto exit;
    }

    /* */
    if( *count > 0 )
        current = (*file)[*count - 1];

    if (current &&
        (current->is_complete ||
          strcmp(current->name, name) ||
          (hdr->flags & RAR_BLOCK_FILE_HAS_PREVIOUS) == 0))
        current = NULL;

    if (!current) {
        if (hdr->flags & RAR_BLOCK_FILE_HAS_PREVIOUS)
            goto exit;
        current = calloc(1, sizeof(*current));
        if (!current)
            goto exit;
        MP_TARRAY_APPEND(NULL, *file, *count, current);

        current->name = name;
        current->size = file_size;
        current->is_complete = false;
        current->real_size = 0;
        current->chunk_count = 0;
        current->chunk = NULL;

        name = NULL;
    }

    /* Append chunks */
    rar_file_chunk_t *chunk = malloc(sizeof(*chunk));
    if (chunk) {
        chunk->mrl = strdup(volume_mrl);
        chunk->offset = stream_tell(s) + hdr->size;
        chunk->size = hdr->add_size;
        chunk->cummulated_size = 0;
        if (current->chunk_count > 0) {
            rar_file_chunk_t *previous = current->chunk[current->chunk_count-1];

            chunk->cummulated_size += previous->cummulated_size +
                                      previous->size;
        }

        MP_TARRAY_APPEND(NULL, current->chunk, current->chunk_count, chunk);

        current->real_size += hdr->add_size;
    }
    if ((hdr->flags & RAR_BLOCK_FILE_HAS_NEXT) == 0)
        current->is_complete = true;

exit:
    /* */
    free(name);

    /* We stop on the first non empty file if we cannot seek */
    if (current) {
        if (!s->seekable && current->size > 0)
            return -1;
    }

    if (SkipBlock(s, hdr))
        return -1;
    return 0;
}

int RarProbe(struct stream *s)
{
    bstr peek = stream_peek(s, rar_marker_size);
    if (peek.len < rar_marker_size)
        return -1;
    if (memcmp(peek.start, rar_marker, rar_marker_size))
        return -1;
    return 0;
}

typedef struct {
    const char *match;
    const char *format;
    int start;
    int stop;
} rar_pattern_t;

static const rar_pattern_t *FindVolumePattern(const char *location)
{
    static const rar_pattern_t patterns[] = {
        { ".part1.rar",   "%s.part%.1d.rar", 2,   9 },
        { ".part01.rar",  "%s.part%.2d.rar", 2,  99, },
        { ".part001.rar", "%s.part%.3d.rar", 2, 999 },
        { ".rar",         "%s.%c%.2d",       0, 999 },
        { NULL, NULL, 0, 0 },
    };

    const size_t location_size = strlen(location);
    for (int i = 0; patterns[i].match != NULL; i++) {
        const size_t match_size = strlen(patterns[i].match);

        if (location_size < match_size)
            continue;
        if (!strcmp(&location[location_size - match_size], patterns[i].match))
            return &patterns[i];
    }
    return NULL;
}

int RarParse(struct stream *s, int *count, rar_file_t ***file)
{
    *count = 0;
    *file = NULL;

    const rar_pattern_t *pattern = FindVolumePattern(s->url);
    int volume_offset = 0;

    char *volume_mrl;
    if (asprintf(&volume_mrl, "%s", s->url) < 0)
        return -1;

    struct stream *vol = s;
    for (;;) {
        /* Skip marker & archive */
        if (IgnoreBlock(vol, RAR_BLOCK_MARKER) ||
            IgnoreBlock(vol, RAR_BLOCK_ARCHIVE)) {
            if (vol != s)
                free_stream(vol);
            free(volume_mrl);
            return -1;
        }

        /* */
        int has_next = -1;
        for (;;) {
            rar_block_t bk;
            int ret;

            if (PeekBlock(vol, &bk))
                break;

            switch(bk.type) {
            case RAR_BLOCK_END:
                ret = SkipEnd(vol, &bk);
                has_next = ret && (bk.flags & RAR_BLOCK_END_HAS_NEXT);
                break;
            case RAR_BLOCK_FILE:
                ret = SkipFile(vol, count, file, &bk, volume_mrl);
                break;
            default:
                ret = SkipBlock(vol, &bk);
                break;
            }
            if (ret)
                break;
        }
        if (has_next < 0 && *count > 0 && !(*file)[*count -1]->is_complete)
            has_next = 1;
        if (vol != s)
            free_stream(vol);

        if (!has_next || !pattern)
            goto done;

        /* Open next volume */
        const int volume_index = pattern->start + volume_offset++;
        if (volume_index > pattern->stop)
            goto done;

        char *volume_base;
        if (asprintf(&volume_base, "%.*s",
                     (int)(strlen(s->url) - strlen(pattern->match)), s->url) < 0) {
            goto done;
        }

        free(volume_mrl);
        if (pattern->start) {
            if (asprintf(&volume_mrl, pattern->format, volume_base, volume_index) < 0)
                volume_mrl = NULL;
        } else {
            if (asprintf(&volume_mrl, pattern->format, volume_base,
                         'r' + volume_index / 100, volume_index % 100) < 0)
                volume_mrl = NULL;
        }
        free(volume_base);

        if (!volume_mrl)
            goto done;

        vol = stream_create(volume_mrl, STREAM_READ, s->cancel, s->global);

        if (!vol)
            goto done;
    }

done:
    free(volume_mrl);
    if (*count == 0) {
        talloc_free(*file);
        return -1;
    }
    return 0;
}

int  RarSeek(rar_file_t *file, uint64_t position)
{
    if (position > file->real_size)
        position = file->real_size;

    /* Search the chunk */
    const rar_file_chunk_t *old_chunk = file->current_chunk;
    for (int i = 0; i < file->chunk_count; i++) {
        file->current_chunk = file->chunk[i];
        if (position < file->current_chunk->cummulated_size + file->current_chunk->size)
            break;
    }
    file->i_pos = position;

    const uint64_t offset = file->current_chunk->offset +
                            (position - file->current_chunk->cummulated_size);

    if (strcmp(old_chunk->mrl, file->current_chunk->mrl)) {
        if (file->s)
            free_stream(file->s);
        file->s = stream_create(file->current_chunk->mrl, STREAM_READ,
                                file->cancel, file->global);
    }
    return file->s ? stream_seek(file->s, offset) : 0;
}

ssize_t RarRead(rar_file_t *file, void *data, size_t size)
{
    size_t total = 0;
    while (total < size) {
        const uint64_t chunk_end = file->current_chunk->cummulated_size + file->current_chunk->size;
        int max = MPMIN(MPMIN((int64_t)(size - total), (int64_t)(chunk_end - file->i_pos)), INT_MAX);
        if (max <= 0)
            break;

        int r = stream_read(file->s, data, max);
        if (r <= 0)
            break;

        total += r;
        data = (char *)data + r;
        file->i_pos += r;
        if (file->i_pos >= chunk_end &&
            RarSeek(file, file->i_pos))
            break;
    }
    return total;

}
