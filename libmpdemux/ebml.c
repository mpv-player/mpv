/*
 * native ebml reader for the Matroska demuxer
 * new parser copyright (c) 2010 Uoti Urpala
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
#include <stdbool.h>
#include <inttypes.h>
#include <stddef.h>
#include <assert.h>

#include <libavutil/intfloat_readwrite.h>
#include <libavutil/common.h>
#include "talloc.h"
#include "ebml.h"
#include "stream/stream.h"
#include "mpbswap.h"
#include "mp_msg.h"

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
double ebml_read_float(stream_t *s, uint64_t *length)
{
    double value;
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

    str = (char *) malloc(len + 1);
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



#define EVALARGS(F, ...) F(__VA_ARGS__)
#define E(str, N, type) const struct ebml_elem_desc ebml_ ## N ## _desc = { str, type };
#define E_SN(str, count, N) const struct ebml_elem_desc ebml_ ## N ## _desc = { str, EBML_TYPE_SUBELEMENTS, sizeof(struct ebml_ ## N), count, (const struct ebml_field_desc[]){
#define E_S(str, count) EVALARGS(E_SN, str, count, N)
#define FN(id, name, multiple, N) { id, multiple, offsetof(struct ebml_ ## N, name), offsetof(struct ebml_ ## N, n_ ## name), &ebml_##name##_desc},
#define F(id, name, multiple) EVALARGS(FN, id, name, multiple, N)
#include "ebml_defs.c"
#undef EVALARGS
#undef SN
#undef S
#undef FN
#undef F

// Used to read/write pointers to different struct types
struct generic;
#define generic_struct struct generic

static uint32_t ebml_parse_id(uint8_t *data, int *length)
{
    int len = 1;
    uint32_t id = *data++;
    for (int len_mask = 0x80; !(id & len_mask); len_mask >>= 1) {
        len++;
        if (len > 4) {
            *length = -1;
            return EBML_ID_INVALID;
        }
    }
    *length = len;
    while (--len)
        id = (id << 8) | *data++;
    return id;
}

static uint64_t parse_vlen(uint8_t *data, int *length, bool is_length)
{
    uint64_t r = *data++;
    int len = 1;
    int len_mask;
    for (len_mask = 0x80; !(r & len_mask); len_mask >>= 1) {
        len++;
        if (len > 8) {
            *length = -1;
            return -1;
        }
    }
    r &= len_mask - 1;

    int num_allones = 0;
    if (r == len_mask - 1)
        num_allones++;
    for (int i = 1; i < len; i++) {
        if (*data == 255)
            num_allones++;
        r = (r << 8) | *data++;
    }
    if (is_length && num_allones == len) {
        // According to Matroska specs this means "unknown length"
        // Could be supported if there are any actual files using it
        *length = -1;
        return -1;
    }
    *length = len;
    return r;
}

static uint64_t ebml_parse_length(uint8_t *data, int *length)
{
    return parse_vlen(data, length, true);
}

static uint64_t ebml_parse_uint(uint8_t *data, int length)
{
    assert(length >= 1 && length <= 8);
    uint64_t r = 0;
    while (length--)
        r = (r << 8) + *data++;
    return r;
}

static int64_t ebml_parse_sint(uint8_t *data, int length)
{
    assert(length >=1 && length <= 8);
    int64_t r = 0;
    if (*data & 0x80)
        r = -1;
    while (length--)
        r = (r << 8) | *data++;
    return r;
}

static double ebml_parse_float(uint8_t *data, int length)
{
    assert(length == 4 || length == 8);
    uint64_t i = ebml_parse_uint(data, length);
    if (length == 4)
        return av_int2flt(i);
    else
        return av_int2dbl(i);
}


// target must be initialized to zero
static void ebml_parse_element(struct ebml_parse_ctx *ctx, void *target,
                               uint8_t *data, int size,
                               const struct ebml_elem_desc *type, int level)
{
    assert(type->type == EBML_TYPE_SUBELEMENTS);
    assert(level < 8);
    mp_msg(MSGT_DEMUX, MSGL_DBG2, "%.*s[mkv] Parsing element %s\n",
           level, "       ", type->name);

    char *s = target;
    int len;
    uint8_t *end = data + size;
    uint8_t *p = data;
    int num_elems[MAX_EBML_SUBELEMENTS] = {};
    while (p < end) {
        uint8_t *startp = p;
        uint32_t id = ebml_parse_id(p, &len);
        if (len > end - p)
            goto past_end_error;
        if (len < 0) {
            mp_msg(MSGT_DEMUX, MSGL_DBG2, "[mkv] Error parsing subelement "
                   "id\n");
            goto other_error;
        }
        p += len;
        uint64_t length = ebml_parse_length(p, &len);
        if (len > end - p)
            goto past_end_error;
        if (len < 0) {
            mp_msg(MSGT_DEMUX, MSGL_DBG2, "[mkv] Error parsing subelement "
                   "length\n");
            goto other_error;
        }
        p += len;

        int field_idx = -1;
        for (int i = 0; i < type->field_count; i++)
            if (type->fields[i].id == id) {
                field_idx = i;
                num_elems[i]++;
                break;
            }

        if (length > end - p) {
            if (field_idx >= 0 && type->fields[field_idx].desc->type
                != EBML_TYPE_SUBELEMENTS) {
                mp_msg(MSGT_DEMUX, MSGL_DBG2, "[mkv] Subelement content goes "
                       "past end of containing element\n");
                goto other_error;
            }
            // Try to parse what is possible from inside this partial element
            ctx->has_errors = true;
            length = end - p;
        }
        p += length;

        continue;

    past_end_error:
        mp_msg(MSGT_DEMUX, MSGL_DBG2, "[mkv] Subelement headers go "
               "past end of containing element\n");
    other_error:
        ctx->has_errors = true;
        end = startp;
        break;
    }

    for (int i = 0; i < type->field_count; i++)
        if (num_elems[i] && type->fields[i].multiple) {
            char *ptr = s + type->fields[i].offset;
            switch (type->fields[i].desc->type) {
            case EBML_TYPE_SUBELEMENTS:
                num_elems[i] = FFMIN(num_elems[i],
                                     1000000000 / type->fields[i].desc->size);
                int size = num_elems[i] * type->fields[i].desc->size;
                *(generic_struct **) ptr = talloc_zero_size(ctx->talloc_ctx,
                                                            size);
                break;
            case EBML_TYPE_UINT:
                *(uint64_t **) ptr = talloc_zero_array(ctx->talloc_ctx,
                                                       uint64_t, num_elems[i]);
                break;
            case EBML_TYPE_SINT:
                *(int64_t **) ptr = talloc_zero_array(ctx->talloc_ctx,
                                                      int64_t, num_elems[i]);
                break;
            case EBML_TYPE_FLOAT:
                *(double **) ptr = talloc_zero_array(ctx->talloc_ctx,
                                                     double, num_elems[i]);
                break;
            case EBML_TYPE_STR:
            case EBML_TYPE_BINARY:
                *(struct bstr **) ptr = talloc_zero_array(ctx->talloc_ctx,
                                                          struct bstr,
                                                          num_elems[i]);
                break;
            case EBML_TYPE_EBML_ID:
                *(int32_t **) ptr = talloc_zero_array(ctx->talloc_ctx,
                                                      uint32_t, num_elems[i]);
                break;
            default:
                abort();
            }
        }

    while (data < end) {
        int len;
        uint32_t id = ebml_parse_id(data, &len);
        assert(len >= 0 && len <= end - data);
        data += len;
        uint64_t length = ebml_parse_length(data, &len);
        assert(len >= 0 && len <= end - data);
        data += len;
        if (length > end - data) {
            // Try to parse what is possible from inside this partial element
            length = end - data;
            mp_msg(MSGT_DEMUX, MSGL_DBG2, "[mkv] Next subelement content goes "
                   "past end of containing element, will be truncated\n");
        }
        int field_idx = -1;
        for (int i = 0; i < type->field_count; i++)
            if (type->fields[i].id == id) {
                field_idx = i;
                break;
            }
        if (field_idx < 0) {
            if (id == 0xec)
                mp_msg(MSGT_DEMUX, MSGL_DBG2, "%.*s[mkv] Ignoring Void element "
                       "size: %"PRIu64"\n", level+1, "        ", length);
            else if (id == 0xbf)
                mp_msg(MSGT_DEMUX, MSGL_DBG2, "%.*s[mkv] Ignoring CRC-32 "
                       "element size: %"PRIu64"\n", level+1, "        ",
                       length);
            else
                mp_msg(MSGT_DEMUX, MSGL_DBG2, "[mkv] Ignoring unrecognized "
                       "subelement. ID: %x size: %"PRIu64"\n", id, length);
            data += length;
            continue;
        }
        const struct ebml_field_desc *fd = &type->fields[field_idx];
        const struct ebml_elem_desc *ed = fd->desc;
        bool multiple = fd->multiple;
        int *countptr = (int *) (s + fd->count_offset);
        if (*countptr >= num_elems[field_idx]) {
            // Shouldn't happen with on any sane file without bugs
            mp_msg(MSGT_DEMUX, MSGL_ERR, "[mkv] Too many subelems?\n");
            ctx->has_errors = true;
            data += length;
            continue;
        }
        if (*countptr > 0 && !multiple) {
            mp_msg(MSGT_DEMUX, MSGL_DBG2, "[mkv] Another subelement of type "
                   "%x %s (size: %"PRIu64"). Only one allowed. Ignoring.\n",
                   id, ed->name, length);
            ctx->has_errors = true;
            data += length;
            continue;
        }
        mp_msg(MSGT_DEMUX, MSGL_DBG2, "%.*s[mkv] Parsing %x %s size: %"PRIu64
               " value: ", level+1, "        ", id, ed->name, length);

        char *fieldptr = s + fd->offset;
        switch (ed->type) {
        case EBML_TYPE_SUBELEMENTS:
            mp_msg(MSGT_DEMUX, MSGL_DBG2, "subelements\n");
            char *subelptr;
            if (multiple) {
                char *array_start = (char *) *(generic_struct **) fieldptr;
                subelptr = array_start + *countptr * ed->size;
            } else
                subelptr = fieldptr;
            ebml_parse_element(ctx, subelptr, data, length, ed, level + 1);
            break;

        case EBML_TYPE_UINT:;
            uint64_t *uintptr;
#define GETPTR(subelptr, fieldtype)                                     \
            if (multiple)                                               \
                subelptr = *(fieldtype **) fieldptr + *countptr;        \
            else                                                        \
                subelptr = (fieldtype *) fieldptr
            GETPTR(uintptr, uint64_t);
            if (length < 1 || length > 8) {
                mp_msg(MSGT_DEMUX, MSGL_DBG2, "uint invalid length %"PRIu64
                       "\n", length);
                goto error;
            }
            *uintptr = ebml_parse_uint(data, length);
            mp_msg(MSGT_DEMUX, MSGL_DBG2, "uint %"PRIu64"\n", *uintptr);
            break;

        case EBML_TYPE_SINT:;
            int64_t *sintptr;
            GETPTR(sintptr, int64_t);
            if (length < 1 || length > 8) {
                mp_msg(MSGT_DEMUX, MSGL_DBG2, "sint invalid length %"PRIu64
                       "\n", length);
                goto error;
            }
            *sintptr = ebml_parse_sint(data, length);
            mp_msg(MSGT_DEMUX, MSGL_DBG2, "sint %"PRId64"\n", *sintptr);
            break;

        case EBML_TYPE_FLOAT:;
            double *floatptr;
            GETPTR(floatptr, double);
            if (length != 4 && length != 8) {
                mp_msg(MSGT_DEMUX, MSGL_DBG2, "float invalid length %"PRIu64
                       "\n", length);
                goto error;
            }
            *floatptr = ebml_parse_float(data, length);
            mp_msg(MSGT_DEMUX, MSGL_DBG2, "float %f\n", *floatptr);
            break;

        case EBML_TYPE_STR:
        case EBML_TYPE_BINARY:;
            struct bstr *strptr;
            GETPTR(strptr, struct bstr);
            strptr->start = data;
            strptr->len = length;
            if (ed->type == EBML_TYPE_STR)
                mp_msg(MSGT_DEMUX, MSGL_DBG2, "string \"%.*s\"\n",
                       strptr->len, strptr->start);
            else
                mp_msg(MSGT_DEMUX, MSGL_DBG2, "binary %d bytes\n",
                       strptr->len);
            break;

        case EBML_TYPE_EBML_ID:;
            uint32_t *idptr;
            GETPTR(idptr, uint32_t);
            *idptr = ebml_parse_id(data, &len);
            if (len != length) {
                mp_msg(MSGT_DEMUX, MSGL_DBG2, "ebml_id broken value\n");
                goto error;
            }
            mp_msg(MSGT_DEMUX, MSGL_DBG2, "ebml_id %x\n", (unsigned)*idptr);
            break;
        default:
            abort();
        }
        *countptr += 1;
    error:
        data += length;
    }
}

// target must be initialized to zero
int ebml_read_element(struct stream *s, struct ebml_parse_ctx *ctx,
                      void *target, const struct ebml_elem_desc *desc)
{
    ctx->has_errors = false;
    int msglevel = ctx->no_error_messages ? MSGL_DBG2 : MSGL_WARN;
    uint64_t length = ebml_read_length(s, &ctx->bytes_read);
    if (s->eof) {
        mp_msg(MSGT_DEMUX, msglevel, "[mkv] Unexpected end of file "
                   "- partial or corrupt file?\n");
        return -1;
    }
    if (length > 1000000000) {
        mp_msg(MSGT_DEMUX, msglevel, "[mkv] Refusing to read element over "
               "100 MB in size\n");
        return -1;
    }
    ctx->talloc_ctx = talloc_size(NULL, length + 8);
    int read_len = stream_read(s, ctx->talloc_ctx, length);
    ctx->bytes_read += read_len;
    if (read_len < length)
        mp_msg(MSGT_DEMUX, msglevel, "[mkv] Unexpected end of file "
               "- partial or corrupt file?\n");
    ebml_parse_element(ctx, target, ctx->talloc_ctx, read_len, desc, 0);
    if (ctx->has_errors)
        mp_msg(MSGT_DEMUX, msglevel, "[mkv] Error parsing element %s\n",
               desc->name);
    return 0;
}
