/*
 * Get path to config dir/file.
 *
 * Return Values:
 *   Returns the pointer to the ALLOCATED buffer containing the
 *   zero terminated path string. This buffer has to be FREED
 *   by the caller.
 *
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

#define MAX_CONFIG_PATHS 32

static const char *mp_get_forced_home(void *talloc_ctx, const char *type)
{
    return strcmp(type, "home") == 0 ? getenv("MPV_HOME") : NULL;
}

// In order of increasing priority: the first hiz has highest priority.
static const mp_get_platform_path_cb path_resolvers[] = {
    mp_get_forced_home,
#if HAVE_COCOA
    mp_get_platform_path_osx,
#endif
#if !defined(_WIN32) || defined(__CYGWIN__)
    mp_get_platform_path_unix,
#endif
#if defined(_WIN32)
    mp_get_platform_path_win,
#endif
};

static const char *mp_get_platform_path(void *talloc_ctx, const char *type)
{
    for (int n = 0; n < MP_ARRAY_SIZE(path_resolvers); n++) {
        const char *path = path_resolvers[n](talloc_ctx, type);
        if (path && path[0])
            return path;
    }
    return NULL;
}

// Return NULL-terminated array of config directories, from highest to lowest
// priority
static char **mp_config_dirs(void *talloc_ctx, struct mpv_global *global)
{
    struct MPOpts *opts = global->opts;

    char **ret = talloc_zero_array(talloc_ctx, char*, MAX_CONFIG_PATHS + 1);
    int num_ret = 0;

    if (!opts->load_config)
        return ret;

    if (opts->force_configdir && opts->force_configdir[0]) {
        ret[0] = talloc_strdup(ret, opts->force_configdir);
        return ret;
    }

    // from highest (most preferred) to lowest priority
    static const char *const configdirs[] = {
        "home",
        "old_home",
        "osxbundle",
        "global",
    };

    for (int n = 0; n < MP_ARRAY_SIZE(configdirs); n++) {
        const char *path = mp_get_platform_path(ret, configdirs[n]);
        if (path && path[0] && num_ret < MAX_CONFIG_PATHS)
            ret[num_ret++] = (char *)path;
    }

    MP_VERBOSE(global, "search dirs:");
    for (int n = 0; n < num_ret; n++)
        MP_VERBOSE(global, " %s", ret[n]);
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
        bstr s = bstr0(filename);
        while (s.len) {
            bstr fn;
            bstr_split_tok(s, "|", &fn, &s);

            char *file = talloc_asprintf(ret, "%s/%.*s", dirs[i], BSTR_P(fn));
            if (mp_path_exists(file) && num_ret < MAX_CONFIG_PATHS)
                ret[num_ret++] = file;
        }
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
    if ((p2.len >= 2 && p2.start[1] == ':')
        || p2.start[0] == '\\' || p2.start[0] == '/')
#else
    if (p2.start[0] == '/')
#endif
        return bstrdup0(talloc_ctx, p2);   // absolute path

    bool have_separator;
    int endchar1 = p1.start[p1.len - 1];
#if HAVE_DOS_PATHS
    have_separator = endchar1 == '/' || endchar1 == '\\'
                     || (p1.len == 2 && endchar1 == ':'); // "X:" only
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
    return path && stat(path, &st) == 0;
}

bool mp_path_isdir(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
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
