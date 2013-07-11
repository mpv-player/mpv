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
 * with mpv; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#include "stream.h"
#include "demux/demux.h"

static int fill_buffer(stream_t *s, char *buffer, int max_len)
{
    return -1;
}

static int open_f(stream_t *stream, int mode, void *opts)
{
    if (mode != STREAM_READ)
        return STREAM_ERROR;

    stream->fill_buffer = fill_buffer;
    stream->type = STREAMTYPE_AVDEVICE;
    stream->demuxer = "lavf";

    return STREAM_OK;
}

const stream_info_t stream_info_avdevice = {
    .info = "FFmpeg libavdevice",
    .name = "avdevice",
    .author = "",
    .comment =
        "Force a libavformat/libavdevice demuxer with avdevice://demuxer:args",
    .open = open_f,
    .protocols = { "avdevice", "av", NULL },
};
