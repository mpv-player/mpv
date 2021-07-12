/*
 * User language lookup for generic POSIX platforms
 *
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

#include "misc/language.h"
#include "mpv_talloc.h"

#include <stddef.h>

char **mp_get_user_langs(void)
{
    static const char *const list[] = {
        "LC_ALL",
        "LC_MESSAGES",
        "LANG",
        NULL
    };

    size_t nb = 0;
    char **ret = NULL;

    for (const char *langList = getenv("LANGUAGE"); langList && *langList;) {
        size_t len = strcspn(langList, ":");
        ret = talloc_realloc(NULL, ret, char*, (nb + 2));
        ret[nb++] = talloc_strndup(ret, langList, len);
        ret[nb] = NULL;
        langList += len;
        while (*langList == ':')
            langList++;
    }

    for (int i = 0; list[i]; i++) {
        const char *envval = getenv(list[i]);
        if (envval && *envval) {
            size_t len = strcspn(envval, ".@");
            ret = talloc_realloc(NULL, ret, char*, (nb + 2));
            ret[nb++] = talloc_strndup(ret, envval, len);
        }
    }

    ret[nb] = NULL;

    return ret;
}
