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

#include "common.h"

#include "misc/json.h"
#include "mpv_talloc.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    void *tmp = talloc_new(NULL);
    char *s = talloc_array_ptrtype(tmp, s, size + 1);
    memcpy(s, data, size);
    s[size] = '\0';

    json_skip_whitespace(&s);

    struct mpv_node res;
    if (!json_parse(tmp, &res, &s, MAX_JSON_DEPTH)) {
        char *d = talloc_strdup(tmp, "");
        json_write(&d, &res);

        d[0] = '\0';
        json_write_pretty(&d, &res);
    }

    talloc_free(tmp);

    return 0;
}
