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
#include <pthread.h>

#include "options/path.h"
#include "path.h"

#include "config.h"

static pthread_once_t path_init_once = PTHREAD_ONCE_INIT;

static char mpv_home[512];
static char old_home[512];

static void copy_paths(char *default_path, const int size, char *compat_path)
{
    // If the compat. dir exists, and the proper dir doesn't, use the compat.
    // config dir only.
    if (mp_path_exists(compat_path) && !mp_path_exists(default_path)) {
        snprintf(default_path, size, "%s", compat_path);
        compat_path[0] = '\0';
    }

    snprintf(mpv_home, sizeof(mpv_home), "%s", default_path);
    snprintf(old_home, sizeof(old_home), "%s", compat_path);
}

static void path_init(void)
{
    char xdg_home[512];
    char dot_home[512];
    char *home = getenv("HOME");
    char *xdg_dir = getenv("XDG_CONFIG_HOME");

    if (home && home[0])
        snprintf(dot_home, sizeof(dot_home), "%s/.mpv", home);

    if (xdg_dir && xdg_dir[0]) {
        snprintf(xdg_home, sizeof(xdg_home), "%s/mpv", xdg_dir);
    } else if (home && home[0]) {
        snprintf(xdg_home, sizeof(xdg_home), "%s/.config/mpv", home);
    }

    if (HAVE_XDG) {
        copy_paths(xdg_home, sizeof(xdg_home), dot_home);
    } else {
        copy_paths(dot_home, sizeof(dot_home), xdg_home);
    }
}

const char *mp_get_platform_path_unix(void *talloc_ctx, const char *type)
{
    pthread_once(&path_init_once, path_init);
    if (strcmp(type, "home") == 0)
        return mpv_home;
    if (strcmp(type, "old_home") == 0)
        return old_home;
    if (strcmp(type, "global") == 0)
        return MPV_CONFDIR;
    if (strcmp(type, "desktop") == 0)
        return getenv("HOME");
    return NULL;
}
