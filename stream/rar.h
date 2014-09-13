/*****************************************************************************
 * rar.h: uncompressed RAR parser
 *****************************************************************************
 * Copyright (C) 2008-2010 Laurent Aimar
 * $Id: 4dea45925c2d8f319d692475bc0307fdd9f6cfe7 $
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

#ifndef MP_RAR_H
#define MP_RAR_H

#include <inttypes.h>
#include <sys/types.h>

typedef struct {
    char     *mrl;
    uint64_t offset;
    uint64_t size;
    uint64_t cummulated_size;
} rar_file_chunk_t;

typedef struct {
    char     *name;
    uint64_t size;
    bool     is_complete;

    int              chunk_count;
    rar_file_chunk_t **chunk;
    uint64_t         real_size;  /* Gathered size */

    // When actually reading the data
    struct mpv_global *global;
    struct mp_cancel *cancel;
    uint64_t i_pos;
    stream_t *s;
    rar_file_chunk_t *current_chunk;
} rar_file_t;

int  RarProbe(struct stream *);
void RarFileDelete(rar_file_t *);
int  RarParse(struct stream *, int *, rar_file_t ***);

int  RarSeek(rar_file_t *file, uint64_t position);
ssize_t RarRead(rar_file_t *file, void *data, size_t size);

#endif
