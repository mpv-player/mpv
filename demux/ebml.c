/*
 * native ebml reader for the Matroska demuxer
 * new parser copyright (c) 2010 Uoti Urpala
 * copyright (c) 2004 Aurelien Jacobs <aurel@gnuage.org>
 * based on the one written by Ronald Bultje for gstreamer
 *
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>
#include <stddef.h>
#include <assert.h>

#include <libavutil/intfloat.h>
#include <libavutil/common.h>
#include "talloc.h"
#include "ebml.h"
#include "stream/stream.h"
#include "common/msg.h"

// Whether the id is a known Matroska level 1 element (allowed as element on
// global file level, after the level 0 MATROSKA_ID_SEGMENT).
// This (intentionally) doesn't include "global" elements.
bool ebml_is_mkv_level1_id(uint32_t id)
{
    switch (id) {
    case MATROSKA_ID_SEEKHEAD:
    case MATROSKA_ID_INFO:
    case MATROSKA_ID_CLUSTER:
    case MATROSKA_ID_TRACKS:
    case MATROSKA_ID_CUES:
    case MATROSKA_ID_ATTACHMENTS:
    case MATROSKA_ID_CHAPTERS:
    case MATROSKA_ID_TAGS:
        return true;
    default:
        return false;
    }
}

/*
 * Read: the element content data ID.
 * Return: the ID.
 */
uint32_t ebml_read_id(stream_t *s)
{
    int i, len_mask = 0x80;
    uint32_t id;

    for (i = 0, id = stream_read_char(s); i < 4 && !(id & len_mask); i++)
        len_mask >>= 1;
    if (i >= 4)
        return EBML_ID_INVALID;
    while (i--)
        id = (id << 8) | stream_read_char(s);
    return id;
}

/*
 * Read a variable length unsigned int.
 */
uint64_t ebml_read_vlen_uint(bstr *buffer)
{
    int i, j, num_ffs = 0, len_mask = 0x80;
    uint64_t num;

    if (buffer->len == 0)
        return EBML_UINT_INVALID;

    for (i = 0, num = buffer->start[0]; i < 8 && !(num & len_mask); i++)
        len_mask >>= 1;
    if (i >= 8)
        return EBML_UINT_INVALID;
    j = i + 1;
    if ((int) (num &= (len_mask - 1)) == len_mask - 1)
        num_ffs++;
    if (j > buffer->len)
        return EBML_UINT_INVALID;
    for (int n = 0; n < i; n++) {
        num = (num << 8) | buffer->start[n + 1];
        if ((num & 0xFF) == 0xFF)
            num_ffs++;
    }
    if (j == num_ffs)
        return EBML_UINT_INVALID;
    buffer->start += j;
    buffer->len -= j;
    return num;
}

/*
 * Read a variable length signed int.
 */
int64_t ebml_read_vlen_int(bstr *buffer)
{
    uint64_t unum;
    int l;

    /* read as unsigned number first */
    size_t len = buffer->len;
    unum = ebml_read_vlen_uint(buffer);
    if (unum == EBML_UINT_INVALID)
        return EBML_INT_INVALID;
    l = len - buffer->len;

    return unum - ((1 << ((7 * l) - 1)) - 1);
}

/*
 * Read: element content length.
 */
uint64_t ebml_read_length(stream_t *s)
{
    int i, j, num_ffs = 0, len_mask = 0x80;
    uint64_t len;

    for (i = 0, len = stream_read_char(s); i < 8 && !(len & len_mask); i++)
        len_mask >>= 1;
    if (i >= 8)
        return EBML_UINT_INVALID;
    j = i + 1;
    if ((int) (len &= (len_mask - 1)) == len_mask - 1)
        num_ffs++;
    while (i--) {
        len = (len << 8) | stream_read_char(s);
        if ((len & 0xFF) == 0xFF)
            num_ffs++;
    }
    if (j == num_ffs)
        return EBML_UINT_INVALID;
    if (len >= 1ULL<<63)   // Can happen if stream_read_char returns EOF
        return EBML_UINT_INVALID;
    return len;
}

/*
 * Read the next element as an unsigned int.
 */
uint64_t ebml_read_uint(stream_t *s)
{
    uint64_t len, value = 0;

    len = ebml_read_length(s);
    if (len == EBML_UINT_INVALID || len < 1 || len > 8)
        return EBML_UINT_INVALID;

    while (len--)
        value = (value << 8) | stream_read_char(s);

    return value;
}

/*
 * Read the next element as a signed int.
 */
int64_t ebml_read_int(stream_t *s)
{
    uint64_t value = 0;
    uint64_t len;
    int l;

    len = ebml_read_length(s);
    if (len == EBML_UINT_INVALID || len < 1 || len > 8)
        return EBML_INT_INVALID;

    len--;
    l = stream_read_char(s);
    if (l & 0x80)
        value = -1;
    value = (value << 8) | l;
    while (len--)
        value = (value << 8) | stream_read_char(s);

    return (int64_t)value; // assume complement of 2
}

/*
 * Skip the current element.
 * end: the end of the parent element or -1 (for robust error handling)
 */
int ebml_read_skip(struct mp_log *log, int64_t end, stream_t *s)
{
    uint64_t len;

    int64_t pos = stream_tell(s);

    len = ebml_read_length(s);
    if (len == EBML_UINT_INVALID)
        goto invalid;

    int64_t pos2 = stream_tell(s);
    if (len >= INT64_MAX - pos2 || (end > 0 && pos2 + len > end))
        goto invalid;

    if (!stream_skip(s, len))
        goto invalid;

    return 0;

invalid:
    mp_err(log, "Invalid EBML length at position %"PRId64"\n", pos);
    stream_seek(s, pos);
    return 1;
}

/*
 * Skip to (probable) next cluster (MATROSKA_ID_CLUSTER) element start position.
 */
int ebml_resync_cluster(struct mp_log *log, stream_t *s)
{
    int64_t pos = stream_tell(s);
    uint32_t last_4_bytes = 0;
    if (!s->eof) {
        mp_err(log, "Corrupt file detected. "
               "Trying to resync starting from position %"PRId64"...\n", pos);
    }
    while (!s->eof) {
        // Assumes MATROSKA_ID_CLUSTER is 4 bytes, with no 0 bytes.
        if (last_4_bytes == MATROSKA_ID_CLUSTER) {
            mp_err(log, "Cluster found at %"PRId64".\n", pos - 4);
            stream_seek(s, pos - 4);
            return 0;
        }
        last_4_bytes = (last_4_bytes << 8) | stream_read_char(s);
        pos++;
    }
    return -1;
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

static uint32_t ebml_parse_id(uint8_t *data, size_t data_len, int *length)
{
    *length = -1;
    uint8_t *end = data + data_len;
    if (data == end)
        return EBML_ID_INVALID;
    int len = 1;
    uint32_t id = *data++;
    for (int len_mask = 0x80; !(id & len_mask); len_mask >>= 1) {
        len++;
        if (len > 4)
            return EBML_ID_INVALID;
    }
    *length = len;
    while (--len && data < end)
        id = (id << 8) | *data++;
    return id;
}

static uint64_t ebml_parse_length(uint8_t *data, size_t data_len, int *length)
{
    *length = -1;
    uint8_t *end = data + data_len;
    if (data == end)
        return -1;
    uint64_t r = *data++;
    int len = 1;
    int len_mask;
    for (len_mask = 0x80; !(r & len_mask); len_mask >>= 1) {
        len++;
        if (len > 8)
            return -1;
    }
    r &= len_mask - 1;

    int num_allones = 0;
    if (r == len_mask - 1)
        num_allones++;
    for (int i = 1; i < len; i++) {
        if (data == end)
            return -1;
        if (*data == 255)
            num_allones++;
        r = (r << 8) | *data++;
    }
    // According to Matroska specs this means "unknown length"
    // Could be supported if there are any actual files using it
    if (num_allones == len)
        return -1;
    *length = len;
    return r;
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
    uint64_t r = 0;
    if (*data & 0x80)
        r = -1;
    while (length--)
        r = (r << 8) | *data++;
    return (int64_t)r; // assume complement of 2
}

static double ebml_parse_float(uint8_t *data, int length)
{
    assert(length == 4 || length == 8);
    uint64_t i = ebml_parse_uint(data, length);
    if (length == 4)
        return av_int2float(i);
    else
        return av_int2double(i);
}


// target must be initialized to zero
static void ebml_parse_element(struct ebml_parse_ctx *ctx, void *target,
                               uint8_t *data, int size,
                               const struct ebml_elem_desc *type, int level)
{
    assert(type->type == EBML_TYPE_SUBELEMENTS);
    assert(level < 8);
    MP_DBG(ctx, "%.*sParsing element %s\n", level, "       ", type->name);

    char *s = target;
    uint8_t *end = data + size;
    uint8_t *p = data;
    int num_elems[MAX_EBML_SUBELEMENTS] = {0};
    while (p < end) {
        uint8_t *startp = p;
        int len;
        uint32_t id = ebml_parse_id(p, end - p, &len);
        if (len > end - p)
            goto past_end_error;
        if (len < 0) {
            MP_DBG(ctx, "Error parsing subelement id\n");
            goto other_error;
        }
        p += len;
        uint64_t length = ebml_parse_length(p, end - p, &len);
        if (len > end - p)
            goto past_end_error;
        if (len < 0) {
            MP_DBG(ctx, "Error parsing subelement length\n");
            goto other_error;
        }
        p += len;

        int field_idx = -1;
        for (int i = 0; i < type->field_count; i++)
            if (type->fields[i].id == id) {
                field_idx = i;
                num_elems[i]++;
                if (num_elems[i] >= 0x70000000) {
                    MP_ERR(ctx, "Too many EBML subelements.\n");
                    goto other_error;
                }
                break;
            }

        if (length > end - p) {
            if (field_idx >= 0 && type->fields[field_idx].desc->type
                != EBML_TYPE_SUBELEMENTS) {
                MP_DBG(ctx, "Subelement content goes "
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
        MP_DBG(ctx, "Subelement headers go past end of containing element\n");
    other_error:
        ctx->has_errors = true;
        end = startp;
        break;
    }

    for (int i = 0; i < type->field_count; i++)
        if (num_elems[i] && type->fields[i].multiple) {
            char *ptr = s + type->fields[i].offset;
            switch (type->fields[i].desc->type) {
            case EBML_TYPE_SUBELEMENTS: {
                size_t max = 1000000000 / type->fields[i].desc->size;
                if (num_elems[i] > max) {
                    MP_ERR(ctx, "Too many subelements.\n");
                    num_elems[i] = max;
                }
                int sz = num_elems[i] * type->fields[i].desc->size;
                *(generic_struct **) ptr = talloc_zero_size(ctx->talloc_ctx, sz);
                break;
            }
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
        uint32_t id = ebml_parse_id(data, end - data, &len);
        if (len < 0 || len > end - data) {
            MP_DBG(ctx, "Error parsing subelement\n");
            break;
        }
        data += len;
        uint64_t length = ebml_parse_length(data, end - data, &len);
        if (len < 0 || len > end - data) {
            MP_DBG(ctx, "Error parsing subelement length\n");
            break;
        }
        data += len;
        if (length > end - data) {
            // Try to parse what is possible from inside this partial element
            length = end - data;
            MP_DBG(ctx, "Next subelement content goes "
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
                MP_DBG(ctx, "%.*sIgnoring Void element "
                       "size: %"PRIu64"\n", level+1, "        ", length);
            else if (id == 0xbf)
                MP_DBG(ctx, "%.*sIgnoring CRC-32 "
                       "element size: %"PRIu64"\n", level+1, "        ",
                       length);
            else
                MP_DBG(ctx, "Ignoring unrecognized "
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
            MP_ERR(ctx, "Too many subelems?\n");
            ctx->has_errors = true;
            data += length;
            continue;
        }
        if (*countptr > 0 && !multiple) {
            MP_DBG(ctx, "Another subelement of type "
                   "%x %s (size: %"PRIu64"). Only one allowed. Ignoring.\n",
                   id, ed->name, length);
            ctx->has_errors = true;
            data += length;
            continue;
        }
        MP_DBG(ctx, "%.*sParsing %x %s size: %"PRIu64
               " value: ", level+1, "        ", id, ed->name, length);

        char *fieldptr = s + fd->offset;
        switch (ed->type) {
        case EBML_TYPE_SUBELEMENTS:
            MP_DBG(ctx, "subelements\n");
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
                MP_DBG(ctx, "uint invalid length %"PRIu64"\n", length);
                goto error;
            }
            *uintptr = ebml_parse_uint(data, length);
            MP_DBG(ctx, "uint %"PRIu64"\n", *uintptr);
            break;

        case EBML_TYPE_SINT:;
            int64_t *sintptr;
            GETPTR(sintptr, int64_t);
            if (length < 1 || length > 8) {
                MP_DBG(ctx, "sint invalid length %"PRIu64"\n", length);
                goto error;
            }
            *sintptr = ebml_parse_sint(data, length);
            MP_DBG(ctx, "sint %"PRId64"\n", *sintptr);
            break;

        case EBML_TYPE_FLOAT:;
            double *floatptr;
            GETPTR(floatptr, double);
            if (length != 4 && length != 8) {
                MP_DBG(ctx, "float invalid length %"PRIu64"\n", length);
                goto error;
            }
            *floatptr = ebml_parse_float(data, length);
            MP_DBG(ctx, "float %f\n", *floatptr);
            break;

        case EBML_TYPE_STR:
        case EBML_TYPE_BINARY:;
            if (length > 0x80000000) {
                MP_ERR(ctx, "Not reading overly long EBML element.\n");
                break;
            }
            struct bstr *strptr;
            GETPTR(strptr, struct bstr);
            strptr->start = data;
            strptr->len = length;
            if (ed->type == EBML_TYPE_STR)
                MP_DBG(ctx, "string \"%.*s\"\n", BSTR_P(*strptr));
            else
                MP_DBG(ctx, "binary %zd bytes\n", strptr->len);
            break;

        case EBML_TYPE_EBML_ID:;
            uint32_t *idptr;
            GETPTR(idptr, uint32_t);
            *idptr = ebml_parse_id(data, end - data, &len);
            if (len != length) {
                MP_DBG(ctx, "ebml_id broken value\n");
                goto error;
            }
            MP_DBG(ctx, "ebml_id %x\n", (unsigned)*idptr);
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
    int msglevel = ctx->no_error_messages ? MSGL_DEBUG : MSGL_WARN;
    uint64_t length = ebml_read_length(s);
    if (s->eof) {
        MP_MSG(ctx, msglevel, "Unexpected end of file "
                   "- partial or corrupt file?\n");
        return -1;
    }
    if (length > 1000000000) {
        MP_MSG(ctx, msglevel, "Refusing to read element over 100 MB in size\n");
        return -1;
    }
    ctx->talloc_ctx = talloc_size(NULL, length);
    int read_len = stream_read(s, ctx->talloc_ctx, length);
    if (read_len < length)
        MP_MSG(ctx, msglevel, "Unexpected end of file - partial or corrupt file?\n");
    ebml_parse_element(ctx, target, ctx->talloc_ctx, read_len, desc, 0);
    if (ctx->has_errors)
        MP_MSG(ctx, msglevel, "Error parsing element %s\n", desc->name);
    return 0;
}
