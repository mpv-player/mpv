/*
 * Get path to config dir/file.
 *
 * Return Values:
 *   Returns the pointer to the ALLOCATED buffer containing the
 *   zero terminated path string. This buffer has to be FREED
 *   by the caller.
 *
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include "config.h"
#include "core/mp_msg.h"
#include "core/path.h"
#include "talloc.h"
#include "osdep/io.h"

#if defined(__MINGW32__)
#include <windows.h>
#elif defined(__CYGWIN__)
#include <windows.h>
#include <sys/cygwin.h>
#endif

#ifdef CONFIG_MACOSX_BUNDLE
#include "osdep/macosx_bundle.h"
#endif


typedef char *(*lookup_fun)(const char *);
static const lookup_fun config_lookup_functions[] = {
    mp_find_user_config_file,
#ifdef CONFIG_MACOSX_BUNDLE
    get_bundled_path,
#endif
    mp_find_global_config_file,
    NULL
};

char *mp_find_config_file(const char *filename)
{
    for (int i = 0; config_lookup_functions[i] != NULL; i++) {
        char *path = config_lookup_functions[i](filename);
        if (!path) continue;

        if (mp_path_exists(path))
            return path;

        talloc_free(path);
    }
    return NULL;
}

char *mp_find_user_config_file(const char *filename)
{
    char *homedir = NULL, *buff = NULL;
#ifdef __MINGW32__
    static char *config_dir = "mpv";
#else
    static char *config_dir = ".mpv";
#endif
#if defined(__MINGW32__) || defined(__CYGWIN__)
    char exedir[260];
#endif
    if ((homedir = getenv("MPV_HOME")) != NULL) {
        config_dir = "";
    } else if ((homedir = getenv("HOME")) == NULL) {
#if defined(__MINGW32__) || defined(__CYGWIN__)
    /* Hack to get fonts etc. loaded outside of Cygwin environment. */
        int i, imax = 0;
        int len = (int)GetModuleFileNameA(NULL, exedir, 260);
        for (i = 0; i < len; i++)
            if (exedir[i] == '\\') {
                exedir[i] = '/';
                imax = i;
            }
        exedir[imax] = '\0';
        homedir = exedir;
#else
        return NULL;
#endif
    }

    if (filename) {
        char * temp = mp_path_join(NULL, bstr0(homedir), bstr0(config_dir));
        buff = mp_path_join(NULL, bstr0(temp), bstr0(filename));
        talloc_free(temp);
    } else {
        buff = mp_path_join(NULL, bstr0(homedir), bstr0(config_dir));
    }

    mp_msg(MSGT_GLOBAL, MSGL_V, "get_path('%s') -> '%s'\n", filename, buff);
    return buff;
}

char *mp_find_global_config_file(const char *filename)
{
    if (filename) {
        return mp_path_join(NULL, bstr0(MPLAYER_CONFDIR), bstr0(filename));
    } else {
        return talloc_strdup(NULL, MPLAYER_CONFDIR);
    }
}

char *mp_basename(const char *path)
{
    char *s;

#if HAVE_DOS_PATHS
    s = strrchr(path, '\\');
    if (s)
        path = s + 1;
    s = strrchr(path, ':');
    if (s)
        path = s + 1;
#endif
    s = strrchr(path, '/');
    return s ? s + 1 : (char *)path;
}

struct bstr mp_dirname(const char *path)
{
    struct bstr ret = {
        (uint8_t *)path, mp_basename(path) - path
    };
    if (ret.len == 0)
        return bstr0(".");
    return ret;
}

char *mp_path_join(void *talloc_ctx, struct bstr p1, struct bstr p2)
{
    if (p1.len == 0)
        return bstrdup0(talloc_ctx, p2);
    if (p2.len == 0)
        return bstrdup0(talloc_ctx, p1);

#if HAVE_DOS_PATHS
    if (p2.len >= 2 && p2.start[1] == ':'
        || p2.start[0] == '\\' || p2.start[0] == '/')
#else
    if (p2.start[0] == '/')
#endif
        return bstrdup0(talloc_ctx, p2);   // absolute path

    bool have_separator;
    int endchar1 = p1.start[p1.len - 1];
#if HAVE_DOS_PATHS
    have_separator = endchar1 == '/' || endchar1 == '\\'
                     || p1.len == 2 && endchar1 == ':'; // "X:" only
#else
    have_separator = endchar1 == '/';
#endif

    return talloc_asprintf(talloc_ctx, "%.*s%s%.*s", BSTR_P(p1),
                           have_separator ? "" : "/", BSTR_P(p2));
}

char *mp_getcwd(void *talloc_ctx)
{
    char *wd = talloc_array(talloc_ctx, char, 20);
    while (getcwd(wd, talloc_get_size(wd)) == NULL) {
        if (errno != ERANGE) {
            talloc_free(wd);
            return NULL;
        }
        wd = talloc_realloc(talloc_ctx, wd, char, talloc_get_size(wd) * 2);
    }
    return wd;
}

bool mp_path_exists(const char *path)
{
    struct stat st;
    return mp_stat(path, &st) == 0;
}

bool mp_path_isdir(const char *path)
{
    struct stat st;
    return mp_stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}
