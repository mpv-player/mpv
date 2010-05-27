/*
 * native ebml reader for the Matroska demuxer
 * copyright (c) 2004 Aurelien Jacobs <aurel@gnuage.org>
 * based on the one written by Ronald Bultje for gstreamer
 *
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

#include "config.h"

#include <stdlib.h>

#include "stream/stream.h"
#include "ebml.h"
#include "libavutil/common.h"
#include "mpbswap.h"
#include "libavutil/intfloat_readwrite.h"


#ifndef SIZE_MAX
#define SIZE_MAX ((size_t)-1)
#endif

/*
 * Read: the element content data ID.
 * Return: the ID.
 */
uint32_t ebml_read_id(stream_t *s, int *length)
{
    int i, len_mask = 0x80;
    uint32_t id;

    for (i = 0, id = stream_read_char(s); i < 4 && !(id & len_mask); i++)
        len_mask >>= 1;
    if (i >= 4)
        return EBML_ID_INVALID;
    if (length)
        *length = i + 1;
    while (i--)
        id = (id << 8) | stream_read_char(s);
    return id;
}

/*
 * Read a variable length unsigned int.
 */
uint64_t ebml_read_vlen_uint(uint8_t *buffer, int *length)
{
    int i, j, num_ffs = 0, len_mask = 0x80;
    uint64_t num;

    for (i = 0, num = *buffer++; i < 8 && !(num & len_mask); i++)
        len_mask >>= 1;
    if (i >= 8)
        return EBML_UINT_INVALID;
    j = i + 1;
    if (length)
        *length = j;
    if ((int) (num &= (len_mask - 1)) == len_mask - 1)
        num_ffs++;
    while (i--) {
        num = (num << 8) | *buffer++;
        if ((num & 0xFF) == 0xFF)
            num_ffs++;
    }
    if (j == num_ffs)
        return EBML_UINT_INVALID;
    return num;
}

/*
 * Read a variable length signed int.
 */
int64_t ebml_read_vlen_int(uint8_t *buffer, int *length)
{
    uint64_t unum;
    int l;

    /* read as unsigned number first */
    unum = ebml_read_vlen_uint(buffer, &l);
    if (unum == EBML_UINT_INVALID)
        return EBML_INT_INVALID;
    if (length)
        *length = l;

    return unum - ((1 << ((7 * l) - 1)) - 1);
}

/*
 * Read: element content length.
 */
uint64_t ebml_read_length(stream_t *s, int *length)
{
    int i, j, num_ffs = 0, len_mask = 0x80;
    uint64_t len;

    for (i = 0, len = stream_read_char(s); i < 8 && !(len & len_mask); i++)
        len_mask >>= 1;
    if (i >= 8)
        return EBML_UINT_INVALID;
    j = i + 1;
    if (length)
        *length = j;
    if ((int) (len &= (len_mask - 1)) == len_mask - 1)
        num_ffs++;
    while (i--) {
        len = (len << 8) | stream_read_char(s);
        if ((len & 0xFF) == 0xFF)
            num_ffs++;
    }
    if (j == num_ffs)
        return EBML_UINT_INVALID;
    return len;
}

/*
 * Read the next element as an unsigned int.
 */
uint64_t ebml_read_uint(stream_t *s, uint64_t *length)
{
    uint64_t len, value = 0;
    int l;

    len = ebml_read_length(s, &l);
    if (len == EBML_UINT_INVALID || len < 1 || len > 8)
        return EBML_UINT_INVALID;
    if (length)
        *length = len + l;

    while (len--)
        value = (value << 8) | stream_read_char(s);

    return value;
}

/*
 * Read the next element as a signed int.
 */
int64_t ebml_read_int(stream_t *s, uint64_t *length)
{
    int64_t value = 0;
    uint64_t len;
    int l;

    len = ebml_read_length(s, &l);
    if (len == EBML_UINT_INVALID || len < 1 || len > 8)
        return EBML_INT_INVALID;
    if (length)
        *length = len + l;

    len--;
    l = stream_read_char(s);
    if (l & 0x80)
        value = -1;
    value = (value << 8) | l;
    while (len--)
        value = (value << 8) | stream_read_char(s);

    return value;
}

/*
 * Read the next element as a float.
 */
long double ebml_read_float(stream_t *s, uint64_t *length)
{
    long double value;
    uint64_t len;
    int l;

    len = ebml_read_length(s, &l);
    switch (len) {
    case 4:
        value = av_int2flt(stream_read_dword(s));
        break;

    case 8:
        value = av_int2dbl(stream_read_qword(s));
        break;

    default:
        return EBML_FLOAT_INVALID;
    }

    if (length)
        *length = len + l;

    return value;
}

/*
 * Read the next element as an ASCII string.
 */
char *ebml_read_ascii(stream_t *s, uint64_t *length)
{
    uint64_t len;
    char *str;
    int l;

    len = ebml_read_length(s, &l);
    if (len == EBML_UINT_INVALID)
        return NULL;
    if (len > SIZE_MAX - 1)
        return NULL;
    if (length)
        *length = len + l;

    str = malloc(len + 1);
    if (stream_read(s, str, len) != (int) len) {
        free(str);
        return NULL;
    }
    str[len] = '\0';

    return str;
}

/*
 * Read the next element as a UTF-8 string.
 */
char *ebml_read_utf8(stream_t *s, uint64_t *length)
{
    return ebml_read_ascii(s, length);
}

/*
 * Skip the next element.
 */
int ebml_read_skip(stream_t *s, uint64_t *length)
{
    uint64_t len;
    int l;

    len = ebml_read_length(s, &l);
    if (len == EBML_UINT_INVALID)
        return 1;
    if (length)
        *length = len + l;

    stream_skip(s, len);

    return 0;
}

/*
 * Read the next element, but only the header. The contents
 * are supposed to be sub-elements which can be read separately.
 */
uint32_t ebml_read_master(stream_t *s, uint64_t *length)
{
    uint64_t len;
    uint32_t id;

    id = ebml_read_id(s, NULL);
    if (id == EBML_ID_INVALID)
        return id;

    len = ebml_read_length(s, NULL);
    if (len == EBML_UINT_INVALID)
        return EBML_ID_INVALID;
    if (length)
        *length = len;

    return id;
}


/*
 * Read an EBML header.
 */
char *ebml_read_header(stream_t *s, int *version)
{
    uint64_t length, l, num;
    uint32_t id;
    char *str = NULL;

    if (ebml_read_master(s, &length) != EBML_ID_HEADER)
        return 0;

    if (version)
        *version = 1;

    while (length > 0) {
        id = ebml_read_id(s, NULL);
        if (id == EBML_ID_INVALID)
            return NULL;
        length -= 2;

        switch (id) {
            /* is our read version uptodate? */
        case EBML_ID_EBMLREADVERSION:
            num = ebml_read_uint(s, &l);
            if (num != EBML_VERSION)
                return NULL;
            break;

            /* we only handle 8 byte lengths at max */
        case EBML_ID_EBMLMAXSIZELENGTH:
            num = ebml_read_uint(s, &l);
            if (num != sizeof(uint64_t))
                return NULL;
            break;

            /* we handle 4 byte IDs at max */
        case EBML_ID_EBMLMAXIDLENGTH:
            num = ebml_read_uint(s, &l);
            if (num != sizeof(uint32_t))
                return NULL;
            break;

        case EBML_ID_DOCTYPE:
            str = ebml_read_ascii(s, &l);
            if (str == NULL)
                return NULL;
            break;

        case EBML_ID_DOCTYPEREADVERSION:
            num = ebml_read_uint(s, &l);
            if (num == EBML_UINT_INVALID)
                return NULL;
            if (version)
                *version = num;
            break;

            /* we ignore these two, they don't tell us anything we care about */
        case EBML_ID_VOID:
        case EBML_ID_EBMLVERSION:
        case EBML_ID_DOCTYPEVERSION:
        default:
            if (ebml_read_skip(s, &l))
                return NULL;
            break;
        }
        length -= l;
    }

    return str;
}
