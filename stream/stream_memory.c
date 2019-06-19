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

#include <libavutil/common.h>

#include "common/common.h"
#include "stream.h"

struct priv {
    bstr data;
};

static int fill_buffer(stream_t *s, char* buffer, int len)
{
    struct priv *p = s->priv;
    bstr data = p->data;
    if (s->pos < 0 || s->pos > data.len)
        return 0;
    len = FFMIN(len, data.len - s->pos);
    memcpy(buffer, data.start + s->pos, len);
    return len;
}

static int seek(stream_t *s, int64_t newpos)
{
    return 1;
}

static int control(stream_t *s, int cmd, void *arg)
{
    struct priv *p = s->priv;
    if (cmd == STREAM_CTRL_GET_SIZE) {
        *(int64_t *)arg = p->data.len;
        return 1;
    }
    return STREAM_UNSUPPORTED;
}

static int open2(stream_t *stream, void *arg)
{
    stream->fill_buffer = fill_buffer;
    stream->seek = seek;
    stream->seekable = true;
    stream->control = control;
    stream->read_chunk = 1024 * 1024;

    struct priv *p = talloc_zero(stream, struct priv);
    stream->priv = p;

    // Initial data
    bstr data = bstr0(stream->url);
    bool use_hex = bstr_eatstart0(&data, "hex://");
    if (!use_hex)
        bstr_eatstart0(&data, "memory://");

    if (arg)
        data = *(bstr *)arg;

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

struct stream *stream_memory_open(struct mpv_global *global, void *data, int len)
{
    assert(len >= 0);

    struct stream *s = NULL;
    stream_create_instance(&stream_info_memory, "memory://",
                           STREAM_READ | STREAM_SILENT, NULL, global,
                           &(bstr){data, len}, &s);
    MP_HANDLE_OOM(s);
    return s;
}
