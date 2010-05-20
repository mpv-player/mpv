/*
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef MPLAYER_EBML_H
#define MPLAYER_EBML_H

#include <inttypes.h>
#include <stddef.h>
#include <stdbool.h>

#include "stream/stream.h"
#include "bstr.h"


/* EBML version supported */
#define EBML_VERSION 1

enum ebml_elemtype {
    EBML_TYPE_SUBELEMENTS,
    EBML_TYPE_UINT,
    EBML_TYPE_SINT,
    EBML_TYPE_FLOAT,
    EBML_TYPE_STR,
    EBML_TYPE_BINARY,
    EBML_TYPE_EBML_ID,
};

struct ebml_field_desc {
    uint32_t id;
    bool multiple;
    int offset;
    int count_offset;
    const struct ebml_elem_desc *desc;
};

struct ebml_elem_desc {
    char *name;
    enum ebml_elemtype type;
    int size;
    int field_count;
    const struct ebml_field_desc *fields;
};

struct ebml_parse_ctx {
    void *talloc_ctx;
    int bytes_read;
    bool has_errors;
    bool no_error_messages;
};

#include "ebml_types.h"

#define EBML_ID_INVALID 0xffffffff


/* matroska track types */
#define MATROSKA_TRACK_VIDEO    0x01 /* rectangle-shaped pictures aka video */
#define MATROSKA_TRACK_AUDIO    0x02 /* anything you can hear */
#define MATROSKA_TRACK_COMPLEX  0x03 /* audio+video in same track used by DV */
#define MATROSKA_TRACK_LOGO     0x10 /* overlay-pictures displayed over video*/
#define MATROSKA_TRACK_SUBTITLE 0x11 /* text-subtitles */
#define MATROSKA_TRACK_CONTROL  0x20 /* control-codes for menu or other stuff*/

/* matroska subtitle types */
#define MATROSKA_SUBTYPE_UNKNOWN   0
#define MATROSKA_SUBTYPE_TEXT      1
#define MATROSKA_SUBTYPE_SSA       2
#define MATROSKA_SUBTYPE_VOBSUB    3

#ifndef UINT64_MAX
#define UINT64_MAX 18446744073709551615ULL
#endif

#ifndef INT64_MAX
#define INT64_MAX 9223372036854775807LL
#endif

#define EBML_UINT_INVALID   UINT64_MAX
#define EBML_INT_INVALID    INT64_MAX
#define EBML_FLOAT_INVALID  -1000000000.0


uint32_t ebml_read_id (stream_t *s, int *length);
uint64_t ebml_read_vlen_uint (uint8_t *buffer, int *length);
int64_t ebml_read_vlen_int (uint8_t *buffer, int *length);
uint64_t ebml_read_length (stream_t *s, int *length);
uint64_t ebml_read_uint (stream_t *s, uint64_t *length);
int64_t ebml_read_int (stream_t *s, uint64_t *length);
double ebml_read_float (stream_t *s, uint64_t *length);
char *ebml_read_ascii (stream_t *s, uint64_t *length);
char *ebml_read_utf8 (stream_t *s, uint64_t *length);
int ebml_read_skip (stream_t *s, uint64_t *length);
uint32_t ebml_read_master (stream_t *s, uint64_t *length);

int ebml_read_element(struct stream *s, struct ebml_parse_ctx *ctx,
                      void *target, const struct ebml_elem_desc *desc);

#endif /* MPLAYER_EBML_H */
