/*
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

#include <string.h>

#include "misc/bstr.h"
#include "options/path.h"
#include "osdep/threads.h"
#include "path.h"

#include "config.h"

static mp_once path_init_once = MP_STATIC_ONCE_INITIALIZER;

#define CONF_MAX 512
static char mpv_home[CONF_MAX];
static char old_home[CONF_MAX];
static char mpv_cache[CONF_MAX];
static char old_cache[CONF_MAX];
static char mpv_desktop[CONF_MAX];
static char mpv_state[CONF_MAX];
#define MKPATH(BUF, ...) (snprintf((BUF), CONF_MAX, __VA_ARGS__) >= CONF_MAX)

static void path_init(void)
{
    char *home = getenv("HOME");
    char *xdg_cache = getenv("XDG_CACHE_HOME");
    char *xdg_config = getenv("XDG_CONFIG_HOME");
    char *xdg_state = getenv("XDG_STATE_HOME");

    bool err = false;
    if (xdg_config && xdg_config[0]) {
        err = err || MKPATH(mpv_home, "%s/mpv", xdg_config);
    } else if (home && home[0]) {
        err = err || MKPATH(mpv_home, "%s/.config/mpv", home);
    }

    // Maintain compatibility with old ~/.mpv
    if (home && home[0]) {
        err = err || MKPATH(old_home, "%s/.mpv", home);
        err = err || MKPATH(old_cache, "%s/.mpv/cache", home);
    }

    if (xdg_cache && xdg_cache[0]) {
        err = err || MKPATH(mpv_cache, "%s/mpv", xdg_cache);
    } else if (home && home[0]) {
        err = err || MKPATH(mpv_cache, "%s/.cache/mpv", home);
    }

    if (xdg_state && xdg_state[0]) {
        err = err || MKPATH(mpv_state, "%s/mpv", xdg_state);
    } else if (home && home[0]) {
        err = err || MKPATH(mpv_state, "%s/.local/state/mpv", home);
    }

    char xdg_user_dirs[CONF_MAX];
    if (xdg_config && xdg_config[0]) {
        err = err || MKPATH(xdg_user_dirs, "%s/user-dirs.dirs", xdg_config);
    } else if (home && home[0]) {
        err = err || MKPATH(xdg_user_dirs, "%s/.config/user-dirs.dirs", home);
    }

    // Attempt to read user-dirs for XDG_DESKTOP_DIR
    mpv_desktop[0] = '\0';
    if (mp_path_exists(xdg_user_dirs)) {
        char line[4096];
        FILE *user_dirs = fopen(xdg_user_dirs, "r");
        while (fgets(line, sizeof(line), user_dirs)) {
            bstr data = bstr0(line);
            if (bstr_eatstart0(&data, "XDG_DESKTOP_DIR=")) {
                bstr value = bstr_strip_linebreaks(data);
                if (bstr_eatstart0(&value, "\"") && bstr_eatend0(&value, "\"")) {
                    bool home_prefix = bstr_eatstart0(&value, "$HOME/");
                    if (home_prefix) {
                        err = err || MKPATH(mpv_desktop, "%s/%.*s", home, BSTR_P(value));
                    } else {
                        err = err || MKPATH(mpv_desktop, "%.*s", BSTR_P(value));
                    }
                }
                break;
            }
        }
    }

    if (!mpv_desktop[0])
        err = err || MKPATH(mpv_desktop, "%s/%s", home, "Desktop");

    // If the old ~/.mpv exists, and the XDG config dir doesn't, use the old
    // config dir only. Also do not use any other XDG directories.
    if (mp_path_exists(old_home) && !mp_path_exists(mpv_home)) {
        err = err || MKPATH(mpv_home, "%s", old_home);
        err = err || MKPATH(mpv_cache, "%s", old_cache);
        err = err || MKPATH(mpv_state, "%s", old_home);
        old_home[0] = '\0';
        old_cache[0] = '\0';
    }

    if (err) {
        fprintf(stderr, "Config dir exceeds %d bytes\n", CONF_MAX);
        abort();
    }
}

const char *mp_get_platform_path_unix(void *talloc_ctx, const char *type)
{
    mp_exec_once(&path_init_once, path_init);
    if (strcmp(type, "home") == 0)
        return mpv_home;
    if (strcmp(type, "old_home") == 0)
        return old_home;
    if (strcmp(type, "cache") == 0)
        return mpv_cache;
    if (strcmp(type, "state") == 0)
        return mpv_state;
    if (strcmp(type, "global") == 0)
        return MPV_CONFDIR;
    if (strcmp(type, "desktop") == 0)
        return mpv_desktop;
    return NULL;
}
