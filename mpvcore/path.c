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
#include "mpvcore/mp_msg.h"
#include "mpvcore/path.h"
#include "talloc.h"
#include "osdep/io.h"

#if defined(_WIN32) && !defined(__CYGWIN__)
#include <windows.h>
#include <shlobj.h>
#endif

#ifdef CONFIG_MACOSX_BUNDLE
#include "osdep/macosx_bundle.h"
#endif

#define SUPPORT_OLD_CONFIG 1
#define ALWAYS_LOCAL_APPDATA 1

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

typedef enum {Config, Cache} config_type;

static inline char *mpv_home(void *talloc_ctx, const config_type type) {
    char *mpvhome = getenv("MPV_HOME");
    if (mpvhome)
        switch (type) {
        case Config:
            return talloc_strdup(talloc_ctx, mpvhome)
            break;
        case Cache:
            return mp_path_join(talloc_ctx, bstr0(mpvhome), bstr0("cache"));
            break;
        }

    return NULL;
}

#if !defined(_WIN32) || defined(__CYGWIN__)
static inline struct bstr find_config_dir(void *talloc_ctx, const config_type type) {
    char *confdir = mpv_home(talloc_ctx, type);
    if (confdir)
        return confdir;

    char *homedir = getenv("HOME");

    const char *xdg_env =
        type == Config ? "XDG_CONFIG_HOME" :
        type == Cache  ? "XDG_CACHE_HOME"  : NULL;

    /* first, we discover the new config dir's path */
    char *tmp = talloc_new(NULL);
    struct bstr ret = bstr0(NULL);

    /* spec requires that the paths on XDG_* envvars are absolute or ignored */
    if ((confdir = getenv(xdg_env)) != NULL && confdir[0] == '/') {
        mkdir(confdir, 0777);
        confdir = mp_path_join(tmp, bstr0(confdir), bstr0("mpv"));
    } else {
        if (homedir == NULL)
            goto exit;
        switch (type) {
        case Config:
            confdir = mp_path_join(tmp, bstr0(homedir), bstr0(".config"));
            break;
        case Cache:
            confdir = mp_path_join(tmp, bstr0(homedir), bstr0(".cache"));
            break;
        }
        mkdir(confdir, 0777);
        confdir = mp_path_join(tmp, bstr0(confdir), bstr0("mpv"));
    }

#if SUPPORT_OLD_CONFIG
    /* check for the old config dir -- we only accept it if it's a real dir */
    char *olddir = mp_path_join(tmp, bstr0(homedir), bstr0(".mpv"));
    struct stat st;
    if (lstat(olddir, &st) == 0 && S_ISDIR(st.st_mode)) {
        static int warned = 0;
        if (!warned++)
            mp_msg(MSGT_GLOBAL, MSGL_WARN,
                   "The default config directory changed. "
                   "Migrate to the new directory with: mv %s %s\n",
                   olddir, confdir);
        confdir = olddir;
    }
#endif

    ret = bstr0(talloc_strdup(talloc_ctx, confdir));
exit:
    talloc_free(tmp);
    return ret;
}

#else /* windows version */

static inline struct bstr find_config_dir(void *talloc_ctx, config_type type) {
    char *confdir = mpv_home(talloc_ctx, type);
    if (confdir)
        return confdir;

    char *tmp = talloc_new(NULL);

    /* get the exe's path */
    /* windows xp bug: exename might not be 0-terminated; give the buffer an extra 0 wchar */
    wchar_t exename[MAX_PATH+1] = {0};
    GetModuleFileNameW(NULL, exename, MAX_PATH);
    struct bstr exedir = mp_dirname(mp_to_utf8(tmp, exename));
    confdir = mp_path_join(tmp, exedir, bstr0("mpv"));

    /* check if we have an exe-local confdir */
    if (!(ALWAYS_LOCAL_APPDATA && type == Cache) &&
        mp_path_exists(confdir) && mp_path_isdir(confdir)) {
        if (type == Cache) {
            confdir = mp_path_join(talloc_ctx, bstr0(confdir), bstr0("cache"));
        } else {
            confdir = talloc_strdup(talloc_ctx, confdir);
        }
    } else {
        wchar_t appdata[MAX_PATH];
        DWORD flags =
            type == Config ? CSIDL_APPDATA       :
            type == Cache  ? CSIDL_LOCAL_APPDATA : 0;

        if (SUCCEEDED(SHGetFolderPathW(NULL,
                                       flags|CSIDL_FLAG_CREATE,
                                       NULL,
                                       SHGFP_TYPE_CURRENT,
                                       appdata))) {
            char *u8appdata = mp_to_utf8(tmp, appdata);

            confdir = mp_path_join(talloc_ctx, bstr0(u8appdata), bstr0("mpv"));
        } else {
            confdir = NULL;
        }
    }
    talloc_free(tmp);
    return bstr0(confdir);
}

#endif

char *mp_find_user_config_file(const char *filename)
{
    static struct bstr config_dir = {0};
    if (config_dir.len == 0)
        config_dir = find_config_dir(NULL, Config);

    char *buf = NULL;
    if (filename) {
        buf = mp_path_join(NULL, config_dir, bstr0(filename));
    } else {
        buf = bstrto0(NULL, config_dir);
    }

    mp_msg(MSGT_GLOBAL, MSGL_V, "mp_find_user_config_file('%s') -> '%s'\n", filename, buf);
    return buf;
}


char *mp_find_user_cache_file(const char *filename)
{
    static struct bstr cache_dir;
    if (cache_dir.len == 0)
        cache_dir = find_config_dir(NULL, Cache);

    char *buf = NULL;
    if (filename) {
        buf = mp_path_join(NULL, cache_dir, bstr0(filename));
    } else {
        buf = bstrto0(NULL, cache_dir);
    }

    mp_msg(MSGT_GLOBAL, MSGL_V, "mp_find_user_cache_file('%s') -> '%s'\n", filename, buf);
    return buf;
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

char *mp_splitext(const char *path, bstr *root)
{
    assert(path);
    const char *split = strrchr(path, '.');
    if (!split)
        split = path + strlen(path);
    if (root)
        *root = (bstr){.start = (char *)path, .len = path - split};
    return (char *)split;
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
