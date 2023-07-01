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

#include "misc/language.h"
#include "mpv_talloc.h"
#include "osdep/io.h"

#include <windows.h>

char **mp_get_user_langs(void)
{
    size_t nb = 0;
    char **ret = NULL;
    ULONG got_count = 0;
    ULONG got_size = 0;
    if (!GetUserPreferredUILanguages(MUI_LANGUAGE_NAME, &got_count, NULL, &got_size) ||
        got_size == 0)
        return NULL;

    wchar_t *buf = talloc_array(NULL, wchar_t, got_size);

    if (!GetUserPreferredUILanguages(MUI_LANGUAGE_NAME, &got_count, buf, &got_size) ||
        got_size == 0)
        goto cleanup;

    for (ULONG pos = 0; buf[pos]; pos += wcslen(buf + pos) + 1) {
        ret = talloc_realloc(NULL, ret, char*, (nb + 2));
        ret[nb++] = mp_to_utf8(ret, buf + pos);
    }
    ret[nb] = NULL;

    if (!GetSystemPreferredUILanguages(MUI_LANGUAGE_NAME, &got_count, NULL, &got_size))
        goto cleanup;

    buf = talloc_realloc(NULL, buf, wchar_t, got_size);

    if (!GetSystemPreferredUILanguages(MUI_LANGUAGE_NAME, &got_count, buf, &got_size))
        goto cleanup;

    for (ULONG pos = 0; buf[pos]; pos += wcslen(buf + pos) + 1) {
        ret = talloc_realloc(NULL, ret, char*, (nb + 2));
        ret[nb++] = mp_to_utf8(ret, buf + pos);
    }
    ret[nb] = NULL;

cleanup:
    talloc_free(buf);
    return ret;
}
