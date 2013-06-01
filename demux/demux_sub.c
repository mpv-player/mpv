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

// Note: not a real demuxer. The frontend has its own code to open subtitle
//       code, and then creates a new dummy demuxer with new_sub_demuxer().
//       But eventually, all subtitles should be opened this way, and this
//       file can be removed.

#include "demux.h"

static int dummy_fill_buffer(struct demuxer *demuxer, struct demux_stream *ds)
{
    return 0;
}

const struct demuxer_desc demuxer_desc_sub = {
    .info = "External subtitles pseudo demuxer",
    .name = "sub",
    .shortdesc = "sub",
    .author = "",
    .comment = "",
    .type = DEMUXER_TYPE_SUB,
    .fill_buffer = dummy_fill_buffer,
};
