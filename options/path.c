/*
 * This file is part of mpv.
 *
 * Get path to config dir/file.
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

#include "misc/path_utils.h"
#include "common/common.h"
#include "common/global.h"
#include "common/msg.h"
#include "options/options.h"
#include "options/path.h"
#include "mpv_talloc.h"
#include "osdep/io.h"
#include "osdep/path.h"
#include "misc/ctype.h"

// In order of decreasing priority: the first has highest priority.
static const mp_get_platform_path_cb path_resolvers[] = {
#if HAVE_COCOA
    mp_get_platform_path_mac,
#endif
#if HAVE_DARWIN
    mp_get_platform_path_darwin,
#elif !defined(_WIN32) || defined(__CYGWIN__)
    mp_get_platform_path_unix,
#endif
#if HAVE_UWP
    mp_get_platform_path_uwp,
#elif defined(_WIN32)
    mp_get_platform_path_win,
#endif
};

// from highest (most preferred) to lowest priority
static const char *const config_dirs[] = {
    "home",
    "old_home",
    "osxbundle",
    "exe_dir",
    "global",
};

// Return a platform specific path using a path type as defined in osdep/path.h.
// Keep in mind that the only way to free the return value is freeing talloc_ctx
// (or its children), as this function can return a statically allocated string.
static const char *mp_get_platform_path(void *talloc_ctx,
                                        struct mpv_global *global,
                                        const char *type)
{
    assert(talloc_ctx);

    if (global->configdir) {
        // Return NULL for all platform paths if --no-config is passed
        if (!global->configdir[0])
            return NULL;

        // force all others to NULL, only first returns the path
        for (int n = 0; n < MP_ARRAY_SIZE(config_dirs); n++) {
            if (strcmp(config_dirs[n], type) == 0)
                return (n == 0) ? global->configdir : NULL;
        }
    }

    // Return the native config path if the platform doesn't support the
    // type we are trying to fetch.
    const char *fallback_type = NULL;
    if (!strcmp(type, "cache") || !strcmp(type, "state"))
        fallback_type = "home";

    for (int n = 0; n < MP_ARRAY_SIZE(path_resolvers); n++) {
        const char *path = path_resolvers[n](talloc_ctx, type);
        if (path && path[0])
            return path;
    }

    if (fallback_type) {
        assert(strcmp(fallback_type, type) != 0);
        return mp_get_platform_path(talloc_ctx, global, fallback_type);
    }
    return NULL;
}

void mp_init_paths(struct mpv_global *global, struct MPOpts *opts)
{
    TA_FREEP(&global->configdir);

    const char *force_configdir = getenv("MPV_HOME");
    if (opts->force_configdir && opts->force_configdir[0])
        force_configdir = opts->force_configdir;
    if (!opts->load_config)
        force_configdir = "";

    global->configdir = talloc_strdup(global, force_configdir);
}

char *mp_find_user_file(void *talloc_ctx, struct mpv_global *global,
                        const char *type, const char *filename)
{
    void *tmp = talloc_new(NULL);
    char *res = (char *)mp_get_platform_path(tmp, global, type);
    if (res)
        res = mp_path_join(talloc_ctx, res, filename);
    talloc_free(tmp);
    MP_DBG(global, "%s path: '%s' -> '%s'\n", type, filename, res ? res : "-");
    return res;
}

static char **mp_find_all_config_files_limited(void *talloc_ctx,
                                               struct mpv_global *global,
                                               int max_files,
                                               const char *filename)
{
    char **ret = talloc_array(talloc_ctx, char*, 2); // 2 preallocated
    int num_ret = 0;

    for (int i = 0; i < MP_ARRAY_SIZE(config_dirs); i++) {
        const char *dir = mp_get_platform_path(ret, global, config_dirs[i]);
        bstr s = bstr0(filename);
        while (dir && num_ret < max_files && s.len) {
            bstr fn;
            bstr_split_tok(s, "|", &fn, &s);

            char *file = mp_path_join_bstr(ret, bstr0(dir), fn);
            if (mp_path_exists(file)) {
                MP_DBG(global, "config path: '%.*s' -> '%s'\n",
                        BSTR_P(fn), file);
                MP_TARRAY_APPEND(NULL, ret, num_ret, file);
            } else {
                MP_DBG(global, "config path: '%.*s' -/-> '%s'\n",
                        BSTR_P(fn), file);
            }
        }
    }

    MP_TARRAY_GROW(NULL, ret, num_ret);
    ret[num_ret] = NULL;

    for (int n = 0; n < num_ret / 2; n++)
        MPSWAP(char*, ret[n], ret[num_ret - n - 1]);
    return ret;
}

char **mp_find_all_config_files(void *talloc_ctx, struct mpv_global *global,
                                const char *filename)
{
    return mp_find_all_config_files_limited(talloc_ctx, global, 64, filename);
}

char *mp_find_config_file(void *talloc_ctx, struct mpv_global *global,
                          const char *filename)
{
    char **l = mp_find_all_config_files_limited(talloc_ctx, global, 1, filename);
    char *r = l && l[0] ? talloc_steal(talloc_ctx, l[0]) : NULL;
    talloc_free(l);
    return r;
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
                if (!res) {
                    void *tmp = talloc_new(NULL);
                    const char *p = mp_get_platform_path(tmp, global, "home");
                    res = mp_path_join_bstr(talloc_ctx, bstr0(p), rest);
                    talloc_free(tmp);
                }
            } else if (bstr_equals0(prefix, "")) {
                char *home = getenv("HOME");
                if (!home)
                    home = getenv("USERPROFILE");
                res = mp_path_join_bstr(talloc_ctx, bstr0(home), rest);
            } else if (bstr_eatstart0(&prefix, "~")) {
                void *tmp = talloc_new(NULL);
                char type[80];
                snprintf(type, sizeof(type), "%.*s", BSTR_P(prefix));
                const char *p = mp_get_platform_path(tmp, global, type);
                res = mp_path_join_bstr(talloc_ctx, bstr0(p), rest);
                talloc_free(tmp);
            }
        }
    }
    if (!res)
        res = talloc_strdup(talloc_ctx, path);
    MP_DBG(global, "user path: '%s' -> '%s'\n", path, res);
    return res;
}

void mp_mk_user_dir(struct mpv_global *global, const char *type, char *subdir)
{
    char *dir = mp_find_user_file(NULL, global, type, subdir);
    if (dir)
        mp_mkdirp(dir);
    talloc_free(dir);
}
