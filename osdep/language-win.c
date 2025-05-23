/*
 * User language lookup for win32
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

#include <windows.h>

#include "misc/language.h"
#include "misc/mp_assert.h"
#include "osdep/io.h"


static void apppend_langs(char ***ret, size_t *ret_count, wchar_t *buf, ULONG count)
{
    for (ULONG pos = 0, i = 0; i < count; ++i) {
        mp_assert(buf[pos]);
        char *item = mp_to_utf8(NULL, buf + pos);
        MP_TARRAY_APPEND(NULL, *ret, *ret_count, item);
        talloc_steal(*ret, item);
    }
}

char **mp_get_user_langs(void)
{
    char **ret = NULL;
    size_t ret_count = 0;
    wchar_t *buf = NULL;
    ULONG size, count;

    if (!GetUserPreferredUILanguages(MUI_LANGUAGE_NAME, &count, NULL, &size))
        goto done;

    MP_TARRAY_GROW(NULL, buf, size);
    if (GetUserPreferredUILanguages(MUI_LANGUAGE_NAME, &count, buf, &size))
        apppend_langs(&ret, &ret_count, buf, count);

    size = 0;
    if (!GetSystemPreferredUILanguages(MUI_LANGUAGE_NAME, &count, NULL, &size))
        goto done;

    MP_TARRAY_GROW(NULL, buf, size);
    if (GetSystemPreferredUILanguages(MUI_LANGUAGE_NAME, &count, buf, &size))
        apppend_langs(&ret, &ret_count, buf, count);

done:
    if (ret_count)
        MP_TARRAY_APPEND(NULL, ret, ret_count, NULL);

    talloc_free(buf);
    return ret;
}
