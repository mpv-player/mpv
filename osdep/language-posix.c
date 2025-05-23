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
    bool has_c = false;

    // Prefer anything we get from LANGUAGE first
    for (const char *langList = getenv("LANGUAGE"); langList && *langList;) {
        size_t len = strcspn(langList, ":");
        MP_TARRAY_GROW(NULL, ret, nb);
        char *lang = talloc_strndup(ret, langList, len);
        for (int i = 0; i < len; i++) {
            if (lang[i] == '_')
                lang[i] = '-';
        }
        ret[nb++] = lang;
        langList += len;
        while (*langList == ':')
            langList++;
    }

    // Then, the language components of other relevant locale env vars
    for (int i = 0; list[i]; i++) {
        const char *envval = getenv(list[i]);
        if (envval && *envval) {
            size_t len = strcspn(envval, ".@");
            if (!strncmp("C", envval, len)) {
                has_c = true;
                continue;
            }

            MP_TARRAY_GROW(NULL, ret, nb);
            char *lang = talloc_strndup(ret, envval, len);
            for (int j = 0; j < len; j++) {
                if (lang[j] == '_')
                    lang[j] = '-';
            }

            ret[nb++] = lang;
        }
    }

    if (has_c && !nb) {
        MP_TARRAY_GROW(NULL, ret, nb);
        ret[nb++] = talloc_strdup(ret, "en");
    }

    // Null-terminate the list
    MP_TARRAY_APPEND(NULL, ret, nb, NULL);

    return ret;
}
