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

#include "common/global.h"
#include "common/msg.h"
#include "options/options.h"
#include "options/path.h"
#include "talloc.h"
#include "osdep/io.h"
#include "osdep/path.h"

#define STRNULL(s) ((s) ? (s) : "(NULL)")



static char *mp_append_all(void* talloc_ctx, const char *_dirs,
                           const char *suffix)
{
    char *ret = "";
    char *dirs = talloc_strdup(talloc_ctx, _dirs);

    while (dirs) {
        char *res = dirs;

        dirs = strchr(dirs, ':');
        if (dirs)
            *dirs++ = 0;

        if (!*res)
        {
            res = NULL;
            continue;
        }

        ret = talloc_asprintf(talloc_ctx, "%s:%s%s", ret, res, suffix);
    }

    return ret;
}

//Return colon separated list of config directories
static const char *config_dirs = NULL;

static const char *mp_config_dirs(struct mpv_global *global)
{
    if (global->opts->force_configdir && global->opts->force_configdir[0])
        return global->opts->force_configdir;

    if (config_dirs)
        return config_dirs;

    void *talloc_ctx = talloc_new(NULL);
    const char *tmp = NULL;
    char *ret = "";

    tmp = getenv("MPV_HOME");
    if (tmp)
        ret = talloc_asprintf(talloc_ctx, "%s%s:", ret, tmp);

#ifdef _WIN32
    tmp = mp_get_win_config_dir(talloc_ctx);
    ret = talloc_asprintf(talloc_ctx, "%s%s:", ret, tmp);
#endif

    tmp = getenv("XDG_CONFIG_HOME");
    if (tmp)
        ret = talloc_asprintf(talloc_ctx, "%s%s/mpv:", ret, tmp);
    else
        ret = talloc_asprintf(talloc_ctx, "%s%s/.config/mpv:", ret, getenv("HOME"));

//Backwards compatibility
    ret = talloc_asprintf(talloc_ctx, "%s%s/.mpv:", ret, getenv("HOME"));

#if HAVE_COCOA
    tmp = mp_get_macosx_bundle_dir(talloc_ctx);
    ret = talloc_asprintf(talloc_ctx, "%s%s:", ret, tmp);
#endif

    tmp = getenv("XDG_CONFIG_DIRS");
    if (tmp)
        ret = talloc_asprintf(talloc_ctx, "%s%s:", ret,
                              mp_append_all(talloc_ctx, tmp, "/mpv"));
    else
        ret = talloc_asprintf(talloc_ctx, "%s%s:", ret, MPLAYER_CONFDIR);

    config_dirs = strdup(ret);
    talloc_free(talloc_ctx);

    MP_VERBOSE(global, "search dirs: %s\n", config_dirs);

    return config_dirs;
}



char *mp_find_config_file(void *talloc_ctx, struct mpv_global *global,
                          const char *filename)
{
    struct MPOpts *opts = global->opts;

    void *tmp = talloc_new(NULL);
    char *res = NULL;
    if (opts->load_config) {
        char *dirs = talloc_strdup(tmp, mp_config_dirs(global));

        while (dirs) {
            char *dir = dirs;

            dirs = strchr(dirs, ':');
            if (dirs)
                *dirs++ = 0;

            if (!*dir)
                continue;

            dir = talloc_asprintf(tmp, "%s/%s", dir, filename);

            if (mp_path_exists(dir)) {
                res = talloc_strdup(talloc_ctx, dir);
                break;
            }
        }
    }

    talloc_free(tmp);

    MP_VERBOSE(global, "config path: '%s' -> '%s'\n", STRNULL(filename),
               STRNULL(res));
    return res;
}
char **mp_find_all_config_files(void *talloc_ctx, struct mpv_global *global,
                                const char *filename)
{
    struct MPOpts *opts = global->opts;
    //Pray that there's less than 31 config files
    char **front = (char**)talloc_zero_size(talloc_ctx, sizeof(char*) * 32);
    char **ret = front + 31;

    if (opts->load_config) {
        char *dirs = talloc_strdup(talloc_ctx, mp_config_dirs(global));

        while (dirs) {
            char* res = dirs;

            dirs = strchr(dirs, ':');
            if (dirs)
                *dirs++ = 0;

            if (!*res)
                continue;

            res = talloc_asprintf(talloc_ctx, "%s/%s", res, filename);

            if (!mp_path_exists(res))
                continue;

            *(--ret) = res;

            if (front == ret)
            {
                MP_WARN(global, "Too many config files, not reading any more\n");
                break;
            }
        }
    }

    MP_VERBOSE(global, "config file: '%s'\n", STRNULL(filename));

    char **c;
    for (c = ret; *c; c++)
        MP_VERBOSE(global, "    -> '%s'\n", STRNULL(*c));

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
    return mp_stat(path, &st) == 0;
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

void mp_mk_config_dir(struct mpv_global *global, char *subdir)
{
    void *tmp = talloc_new(NULL);
    char *dirs = talloc_strdup(tmp, mp_config_dirs(global));
    char *end = strchr(dirs, ':');

    if (end)
        *end = 0;

    mkdir(dirs, 0777);
    dirs = mp_path_join(tmp, bstr0(dirs), bstr0(subdir));
    mkdir(dirs, 0777);

    talloc_free(tmp);
}
