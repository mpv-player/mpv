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

#include "stream_bstr.h"
#include "common/common.h"

struct priv {
    bstr data;
};

static int bs_fill_buffer(stream_t *s, void *buffer, int len)
{
    struct priv *p = s->priv;
    bstr data = p->data;
    if (s->pos < 0 || s->pos > data.len)
        return 0;
    len = MPMIN(len, data.len - s->pos);
    memcpy(buffer, data.start + s->pos, len);
    return len;
}

static int bs_seek(stream_t *s, int64_t newpos)
{
    return 1;
}

static int64_t bs_get_size(stream_t *s)
{
    struct priv *p = s->priv;
    return p->data.len;
}

int stream_bstr_open2(stream_t *s, const struct stream_open_args *args, bstr data)
{
    s->fill_buffer = bs_fill_buffer;
    s->seek = bs_seek;
    s->seekable = true;
    s->get_size = bs_get_size;

    struct priv *p = talloc_zero(s, struct priv);
    s->priv = p;

    ta_set_parent(data.start, p);
    p->data = data;

    return STREAM_OK;
}
