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
#include "stream.h"

struct priv {
    bstr data;
};

static int fill_buffer(stream_t *s, void *buffer, int len)
{
    struct priv *p = s->priv;
    bstr data = p->data;
    if (s->pos < 0 || s->pos > data.len)
        return 0;
    len = MPMIN(len, data.len - s->pos);
    memcpy(buffer, data.start + s->pos, len);
    return len;
}

static int seek(stream_t *s, int64_t newpos)
{
    return 1;
}

static int64_t get_size(stream_t *s)
{
    struct priv *p = s->priv;
    return p->data.len;
}

static int open2(stream_t *stream, const struct stream_open_args *args)
{
    stream->fill_buffer = fill_buffer;
    stream->seek = seek;
    stream->seekable = true;
    stream->get_size = get_size;

    struct priv *p = talloc_zero(stream, struct priv);
    stream->priv = p;

    // Initial data
    bstr data = bstr0(stream->url);
    bool use_hex = bstr_eatstart0(&data, "hex://");
    if (!use_hex)
        bstr_eatstart0(&data, "memory://");

    if (args->special_arg)
        data = *(bstr *)args->special_arg;

    p->data = bstrdup(stream, data);

    if (use_hex && !bstr_decode_hex(stream, p->data, &p->data)) {
        MP_FATAL(stream, "Invalid data.\n");
        return STREAM_ERROR;
    }

    return STREAM_OK;
}

const stream_info_t stream_info_memory = {
    .name = "memory",
    .open2 = open2,
    .protocols = (const char*const[]){ "memory", "hex", NULL },
};

// The data is copied.
// Caller may need to set stream.stream_origin correctly.
struct stream *stream_memory_open(struct mpv_global *global, void *data, int len)
{
    assert(len >= 0);

    struct stream_open_args sargs = {
        .global = global,
        .url = "memory://",
        .flags = STREAM_READ | STREAM_SILENT | STREAM_ORIGIN_DIRECT,
        .sinfo = &stream_info_memory,
        .special_arg = &(bstr){data, len},
    };

    struct stream *s = NULL;
    stream_create_with_args(&sargs, &s);
    MP_HANDLE_OOM(s);
    return s;
}
