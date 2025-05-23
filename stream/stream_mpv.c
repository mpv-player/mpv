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

#include "stream.h"

#include <misc/path_utils.h>
#include <player/core.h>

static int open_mpv(stream_t *st)
{
    st->demuxer = "mpv";
    return STREAM_OK;
}

const stream_info_t stream_info_mpv = {
    .name = "mpv",
    .open = open_mpv,
    .stream_origin = STREAM_ORIGIN_NET,
    .protocols = (const char*const[]){"mpv", NULL},
};
