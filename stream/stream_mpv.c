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
    bstr proto = mp_split_proto(bstr0(st->path), NULL);
    if (proto.len) {
        if (!bstrcasecmp0(proto, "mpv")) {
            MP_ERR(st, "Nested mpv:// is not allowed.\n");
            return STREAM_NO_MATCH;
        }
        char **safe_protocols = stream_get_proto_list(true);
        bool safe = str_in_list(proto, safe_protocols);
        talloc_free(safe_protocols);
        if (!safe) {
            MP_ERR(st, "Unsafe protocol '%.*s' opened with mpv://.\n", BSTR_P(proto));
            return STREAM_NO_MATCH;
        }
    }

    st->demuxer = "mpv";
    return STREAM_OK;
}

const stream_info_t stream_info_mpv = {
    .name = "mpv",
    .open = open_mpv,
    .protocols = (const char*const[]){"mpv", NULL},
};
