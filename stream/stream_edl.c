/*
 * Dummy stream implementation to enable demux_edl, which is in turn a
 * dummy demuxer implementation to enable tl_edl.
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

#include "stream.h"

static int s_open (struct stream *stream)
{
    stream->type = STREAMTYPE_EDL;
    stream->demuxer = "edl";

    return STREAM_OK;
}

const stream_info_t stream_info_edl = {
    .name = "edl",
    .open = s_open,
    .protocols = (const char*const[]){"edl", NULL},
};
