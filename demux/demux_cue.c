/*
 * This file is part of MPlayer.
 *
 * Original author: Uoti Urpala
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "mpvcore/bstr.h"
#include "demux.h"
#include "stream/stream.h"

// timeline/tl_cue.c
bool mp_probe_cue(struct bstr s);

#define PROBE_SIZE 512

static int try_open_file(struct demuxer *demuxer, enum demux_check check)
{
    struct stream *s = demuxer->stream;
    if (check >= DEMUX_CHECK_UNSAFE) {
        char buf[PROBE_SIZE];
        int len = stream_read(s, buf, sizeof(buf));
        if (len <= 0)
            return -1;
        if (!mp_probe_cue((struct bstr) { buf, len }))
            return -1;
        stream_seek(s, 0);
    }
    demuxer->file_contents = stream_read_complete(s, demuxer, 1000000);
    if (demuxer->file_contents.start == NULL)
        return -1;
    return 0;
}

const struct demuxer_desc demuxer_desc_cue = {
    .name = "cue",
    .desc = "CUE sheet",
    .type = DEMUXER_TYPE_CUE,
    .open = try_open_file,
};
