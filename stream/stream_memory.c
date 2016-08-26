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
    switch(cmd) {
    case STREAM_CTRL_GET_SIZE:
        *(int64_t *)arg = p->data.len;
        return 1;
    case STREAM_CTRL_SET_CONTENTS: ;
        bstr *data = (bstr *)arg;
        talloc_free(p->data.start);
        p->data = bstrdup(s, *data);
        return 1;
    }
    return STREAM_UNSUPPORTED;
}

static int h_to_i(unsigned char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

static bool bstr_to_hex_inplace(bstr *h)
{
    if (h->len % 2)
        return false;
    for (int n = 0; n < h->len / 2; n++) {
        int hi = h_to_i(h->start[n * 2 + 0]);
        int lo = h_to_i(h->start[n * 2 + 1]);
        if (hi < 0 || lo < 0)
            return false;
        h->start[n] = (hi << 4) | lo;
    }
    h->len /= 2;
    return true;
}

static int open_f(stream_t *stream)
{
    stream->fill_buffer = fill_buffer;
    stream->seek = seek;
    stream->seekable = true;
    stream->control = control;
    stream->read_chunk = 1024 * 1024;
    stream->allow_caching = false;

    struct priv *p = talloc_zero(stream, struct priv);
    stream->priv = p;

    // Initial data
    bstr data = bstr0(stream->url);
    bool use_hex = bstr_eatstart0(&data, "hex://");
    if (!use_hex)
        bstr_eatstart0(&data, "memory://");
    stream_control(stream, STREAM_CTRL_SET_CONTENTS, &data);

    if (use_hex && !bstr_to_hex_inplace(&p->data)) {
        MP_FATAL(stream, "Invalid data.\n");
        return STREAM_ERROR;
    }

    return STREAM_OK;
}

const stream_info_t stream_info_memory = {
    .name = "memory",
    .open = open_f,
    .protocols = (const char*const[]){ "memory", "hex", NULL },
};
