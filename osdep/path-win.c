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

#include <windows.h>
#include <shlobj.h>

#include "osdep/path.h"
#include "osdep/io.h"
#include "options/path.h"

// Warning: do not use PATH_MAX. Cygwin messed it up.

static char *mp_get_win_exe_dir(void *talloc_ctx)
{
    wchar_t w_exedir[MAX_PATH + 1] = {0};

    int len = (int)GetModuleFileNameW(NULL, w_exedir, MAX_PATH);
    int imax = 0;
    for (int i = 0; i < len; i++) {
        if (w_exedir[i] == '\\') {
            w_exedir[i] = '/';
            imax = i;
        }
    }

    w_exedir[imax] = '\0';

    return mp_to_utf8(talloc_ctx, w_exedir);
}

static char *mp_get_win_exe_subdir(void *talloc_ctx)
{
    return talloc_asprintf(talloc_ctx, "%s/mpv", mp_get_win_exe_dir(talloc_ctx));
}

static char *mp_get_win_app_dir(void *talloc_ctx)
{
    wchar_t w_appdir[MAX_PATH + 1] = {0};

    if (SHGetFolderPathW(NULL, CSIDL_APPDATA|CSIDL_FLAG_CREATE, NULL,
        SHGFP_TYPE_CURRENT, w_appdir) != S_OK)
        return NULL;

    return talloc_asprintf(talloc_ctx, "%s/mpv", mp_to_utf8(talloc_ctx, w_appdir));
}

const char *mp_get_platform_path_win(void *talloc_ctx, const char *type)
{
    if (strcmp(type, "home") == 0)
        return mp_get_win_app_dir(talloc_ctx);
    if (strcmp(type, "old_home") == 0)
        return mp_get_win_exe_dir(talloc_ctx);
    // Not really true, but serves as a way to return a lowest-priority dir.
    if (strcmp(type, "global") == 0)
        return mp_get_win_exe_subdir(talloc_ctx);
    return NULL;
}
