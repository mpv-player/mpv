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

#include "misc/bstr.h"
#include "stream/stream.h"
#include "demux.h"

static int try_open_file(struct demuxer *demux, enum demux_check check)
{
    if (!bstr_startswith0(bstr0(demux->filename), "null://") &&
        check != DEMUX_CHECK_REQUEST)
        return -1;
    demux->seekable = true;
    return 0;
}

const struct demuxer_desc demuxer_desc_null = {
    .name = "null",
    .desc = "null demuxer",
    .open = try_open_file,
};
