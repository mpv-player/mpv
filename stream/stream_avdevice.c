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

#include "config.h"

#include "stream.h"

static int open_f(stream_t *stream)
{
    stream->type = STREAMTYPE_AVDEVICE;
    stream->demuxer = "lavf";

    return STREAM_OK;
}

const stream_info_t stream_info_avdevice = {
    .name = "avdevice",
    .open = open_f,
    .protocols = (const char*const[]){ "avdevice", "av", NULL },
};
