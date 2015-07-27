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
#include <pthread.h>

#include "osdep/path.h"
#include "osdep/io.h"
#include "options/path.h"

// Warning: do not use PATH_MAX. Cygwin messed it up.

static pthread_once_t path_init_once = PTHREAD_ONCE_INIT;

static char *portable_path;

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

static char *mp_get_win_exe_subdir(void *ta_ctx, const char *name)
{
    return talloc_asprintf(ta_ctx, "%s/%s", mp_get_win_exe_dir(ta_ctx), name);
}

static char *mp_get_win_shell_dir(void *talloc_ctx, int folder)
{
    wchar_t w_appdir[MAX_PATH + 1] = {0};

    if (SHGetFolderPathW(NULL, folder|CSIDL_FLAG_CREATE, NULL,
        SHGFP_TYPE_CURRENT, w_appdir) != S_OK)
        return NULL;

    return mp_to_utf8(talloc_ctx, w_appdir);
}

static char *mp_get_win_app_dir(void *talloc_ctx)
{
    char *path = mp_get_win_shell_dir(talloc_ctx, CSIDL_APPDATA);
    return path ? mp_path_join(talloc_ctx, path, "mpv") : NULL;
}


static void path_init(void)
{
    void *tmp = talloc_new(NULL);
    char *path = mp_get_win_exe_subdir(tmp, "portable_config");
    if (path && mp_path_exists(path))
        portable_path = talloc_strdup(NULL, path);
    talloc_free(tmp);
}

const char *mp_get_platform_path_win(void *talloc_ctx, const char *type)
{
    pthread_once(&path_init_once, path_init);
    if (portable_path) {
        if (strcmp(type, "home") == 0)
            return portable_path;
    } else {
        if (strcmp(type, "home") == 0)
            return mp_get_win_app_dir(talloc_ctx);
        if (strcmp(type, "old_home") == 0)
            return mp_get_win_exe_dir(talloc_ctx);
        // Not really true, but serves as a way to return a lowest-priority dir.
        if (strcmp(type, "global") == 0)
            return mp_get_win_exe_subdir(talloc_ctx, "mpv");
    }
    if (strcmp(type, "desktop") == 0)
        return mp_get_win_shell_dir(talloc_ctx, CSIDL_DESKTOPDIRECTORY);
    return NULL;
}
