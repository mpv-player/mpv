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

#include <stdlib.h>

#include "common/common.h"
#include "stream_bstr.h"

static int open2(stream_t *stream, const struct stream_open_args *args)
{
    bstr data = bstr0(stream->url);
    bstr_eatstart0(&data, "env://");

    char *name = bstrdup0(stream, data);
    char *val = getenv(name);
    talloc_free(name);

    if (!val) {
        MP_FATAL(stream, "Could not read from environment variable '%.*s'\n",
                 BSTR_P(data));
        return STREAM_ERROR;
    }

    data = bstrdup(stream, bstr0(val));
    return stream_bstr_open2(stream, args, data);
}

const stream_info_t stream_info_env = {
    .name = "env",
    .open2 = open2,
    .protocols = (const char*const[]){ "env", NULL },
    .stream_origin = STREAM_ORIGIN_DIRECT,
};
