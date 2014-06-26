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

#include "common/common.h"
#include "common/global.h"
#include "common/msg.h"
#include "options/options.h"
#include "options/path.h"
#include "talloc.h"
#include "osdep/io.h"
#include "osdep/path.h"

static int mp_add_xdg_config_dirs(struct mpv_global *global, char **dirs, int i)
{
    void *talloc_ctx = dirs;

    char *home = getenv("HOME");
    char *tmp = NULL;

    char *xdg_home = NULL;
    tmp = getenv("XDG_CONFIG_HOME");
    if (tmp && *tmp)
        xdg_home = talloc_asprintf(talloc_ctx, "%s/mpv", tmp);
    else if (home && *home)
        xdg_home = talloc_asprintf(talloc_ctx, "%s/.config/mpv", home);

    // Maintain compatibility with old ~/.mpv
    char *old_home = NULL;
    if (home && *home)
        old_home = talloc_asprintf(talloc_ctx, "%s/.mpv", home);

    // If the old ~/.mpv exists, and the XDG config dir doesn't, use the old
    // config dir only.
    if (mp_path_exists(xdg_home) || !mp_path_exists(old_home))
        dirs[i++] = xdg_home;
    dirs[i++] = old_home;

#if HAVE_COCOA
    i = mp_add_macosx_bundle_dir(global, dirs, i);
#endif

    tmp = getenv("XDG_CONFIG_DIRS");
    if (tmp && *tmp) {
        char *xdgdirs = talloc_strdup(talloc_ctx, tmp);
        while (xdgdirs) {
            char *dir = xdgdirs;

            xdgdirs = strchr(xdgdirs, ':');
            if (xdgdirs)
                *xdgdirs++ = 0;

            if (!dir[0])
                continue;

            dirs[i++] = talloc_asprintf(talloc_ctx, "%s%s", dir, "/mpv");

            if (i + 1 >= MAX_CONFIG_PATHS) {
                MP_WARN(global, "Too many config files, not reading any more\n");
                break;
            }
        }
    } else {
        dirs[i++] = MPLAYER_CONFDIR;
    }

    return i;
}

// Return NULL-terminated array of config directories, from highest to lowest
// priority
static char **mp_config_dirs(void *talloc_ctx, struct mpv_global *global)
{
    struct MPOpts *opts = global->opts;

    char **ret = talloc_zero_array(talloc_ctx, char*, MAX_CONFIG_PATHS);

    if (!opts->load_config)
        return ret;

    if (opts->force_configdir && opts->force_configdir[0]) {
        ret[0] = talloc_strdup(ret, opts->force_configdir);
        return ret;
    }

    const char *tmp = NULL;
    int i = 0;

    tmp = getenv("MPV_HOME");
    if (tmp && *tmp)
        ret[i++] = talloc_strdup(ret, tmp);

#if defined(_WIN32) && !defined(__CYGWIN__)
    i = mp_add_win_config_dirs(global, ret, i);
#else
    i = mp_add_xdg_config_dirs(global, ret, i);
#endif

    MP_VERBOSE(global, "search dirs:");
    for (char **c = ret; *c; c++)
        MP_VERBOSE(global, " %s", *c);
    MP_VERBOSE(global, "\n");

    return ret;
}

char *mp_find_config_file(void *talloc_ctx, struct mpv_global *global,
                          const char *filename)
{
    char *res = NULL;
    char **dirs = mp_config_dirs(NULL, global);
    for (int i = 0; dirs && dirs[i]; i++) {
        char *file = talloc_asprintf(talloc_ctx, "%s/%s", dirs[i], filename);

        if (mp_path_exists(file)) {
            res = file;
            break;
        }

        talloc_free(file);
    }
    talloc_free(dirs);

    MP_VERBOSE(global, "config path: '%s' -> '%s'\n", filename,
               res ? res : "(NULL)");
    return res;
}

char **mp_find_all_config_files(void *talloc_ctx, struct mpv_global *global,
                                const char *filename)
{
    char **ret = talloc_zero_array(talloc_ctx, char*, MAX_CONFIG_PATHS + 1);
    int num_ret = 0;

    char **dirs = mp_config_dirs(NULL, global);
    for (int i = 0; dirs && dirs[i]; i++) {
        char *file = talloc_asprintf(ret, "%s/%s", dirs[i], filename);

        if (!mp_path_exists(file) || num_ret >= MAX_CONFIG_PATHS)
            continue;

        ret[num_ret++] = file;
    }
    talloc_free(dirs);

    for (int n = 0; n < num_ret / 2; n++)
        MPSWAP(char*, ret[n], ret[num_ret - n - 1]);

    MP_VERBOSE(global, "config file: '%s'\n", filename);

    for (char** c = ret; *c; c++)
        MP_VERBOSE(global, "    -> '%s'\n", *c);

    return ret;
}

char *mp_get_user_path(void *talloc_ctx, struct mpv_global *global,
                       const char *path)
{
    if (!path)
        return NULL;
    char *res = NULL;
    bstr bpath = bstr0(path);
    if (bstr_eatstart0(&bpath, "~")) {
        // parse to "~" <prefix> "/" <rest>
        bstr prefix, rest;
        if (bstr_split_tok(bpath, "/", &prefix, &rest)) {
            const char *rest0 = rest.start; // ok in this case
            if (bstr_equals0(prefix, "~")) {
                res = mp_find_config_file(talloc_ctx, global, rest0);
            } else if (bstr_equals0(prefix, "")) {
                res = mp_path_join(talloc_ctx, bstr0(getenv("HOME")), rest);
            }
        }
    }
    if (!res)
        res = talloc_strdup(talloc_ctx, path);
    MP_VERBOSE(global, "user path: '%s' -> '%s'\n", path, res);
    return res;
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
    if (!split || !split[1] || strchr(split, '/'))
        return NULL;
    if (root)
        *root = (bstr){(char *)path, split - path};
    return (char *)split + 1;
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
    return path && mp_stat(path, &st) == 0;
}

bool mp_path_isdir(const char *path)
{
    struct stat st;
    return mp_stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

// Return false if it's considered a normal local filesystem path.
bool mp_is_url(bstr path)
{
    int proto = bstr_find0(path, "://");
    if (proto < 1)
        return false;
    // The protocol part must be alphanumeric, otherwise it's not an URL.
    for (int i = 0; i < proto; i++) {
        unsigned char c = path.start[i];
        if (!(c >= 'a' && c <= 'z') && !(c >= 'A' && c <= 'Z') &&
            !(c >= '0' && c <= '9') && c != '_')
            return false;
    }
    return true;
}

// Return the protocol part of path, e.g. "http" if path is "http://...".
// On success, out_url (if not NULL) is set to the part after the "://".
bstr mp_split_proto(bstr path, bstr *out_url)
{
    if (!mp_is_url(path))
        return (bstr){0};
    bstr r;
    bstr_split_tok(path, "://", &r, out_url ? out_url : &(bstr){0});
    return r;
}

void mp_mkdirp(const char *dir)
{
    char *path = talloc_strdup(NULL, dir);
    char *cdir = path + 1;

    while (cdir) {
        cdir = strchr(cdir, '/');
        if (cdir)
            *cdir = 0;

        mkdir(path, 0700);

        if (cdir)
            *cdir++ = '/';
    }

    talloc_free(path);
}

void mp_mk_config_dir(struct mpv_global *global, char *subdir)
{
    void *tmp = talloc_new(NULL);
    char *dir = mp_config_dirs(tmp, global)[0];

    if (dir) {
        dir = talloc_asprintf(tmp, "%s/%s", dir, subdir);
        mp_mkdirp(dir);
    }

    talloc_free(tmp);
}
