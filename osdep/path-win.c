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

static void get_exe_dir(wchar_t path[MAX_PATH + 1])
{
    int len = (int)GetModuleFileNameW(NULL, path, MAX_PATH);
    int imax = 0;
    for (int i = 0; i < len; i++) {
        if (path[i] == '\\') {
            path[i] = '/';
            imax = i;
        }
    }

    path[imax] = '\0';
}

char *mp_get_win_config_dirs(void *talloc_ctx)
{
    wchar_t w_appdir[MAX_PATH + 1] = {0};
    wchar_t w_exedir[MAX_PATH + 1] = {0};

#ifndef __CYGWIN__
    if (SHGetFolderPathW(NULL, CSIDL_APPDATA|CSIDL_FLAG_CREATE, NULL,
        SHGFP_TYPE_CURRENT, w_appdir) != S_OK)
        w_appdir[0] = '\0';
#endif

    get_exe_dir(w_exedir);

    return talloc_asprintf(talloc_ctx, "%s:%s/mpv:",
                           mp_to_utf8(talloc_ctx, w_exedir),
                           mp_to_utf8(talloc_ctx, w_appdir));
}
