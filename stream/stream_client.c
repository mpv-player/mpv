/*
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

#include <libavutil/common.h>

#include "stream.h"
#include "libmpv/client.h"

static int open_f(stream_t *stream) {
    struct stream_client_info info = *get_stream_client_info();
    stream->fill_buffer = info.fill_buffer;
    stream->write_buffer = info.write_buffer;
    stream->close = info.close;
    stream->seek = info.seek;
    stream->seekable = info.seek != NULL;
    stream->control = info.control;
    stream->read_chunk = 1024 * 1024;

    stream->priv = info.userdata;

    return STREAM_OK;
}

const stream_info_t stream_info_client = {
    .name = "client",
    .open = open_f,
    .protocols = (const char*const[]){ "client", NULL },
};
