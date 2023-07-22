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
#include "stream_bstr.h"

static int open2(stream_t *stream, const struct stream_open_args *args)
{
    // Initial data
    bstr data = bstr0(stream->url);
    bool use_hex = bstr_eatstart0(&data, "hex://");
    if (!use_hex)
        bstr_eatstart0(&data, "memory://");

    if (args->special_arg)
        data = *(bstr *)args->special_arg;

    data = bstrdup(stream, data);

    if (use_hex && !bstr_decode_hex(stream, data, &data)) {
        MP_FATAL(stream, "Invalid data.\n");
        return STREAM_ERROR;
    }

    return stream_bstr_open2(stream, args, data);
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
