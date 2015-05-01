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

#include <string.h>
#include <pthread.h>

#include "options/path.h"
#include "path.h"

#include "config.h"

static pthread_once_t path_init_once = PTHREAD_ONCE_INIT;

static char mpv_home[512];
static char old_home[512];

static void path_init(void)
{
    char *home = getenv("HOME");
    char *xdg_dir = getenv("XDG_CONFIG_HOME");

    if (xdg_dir && xdg_dir[0]) {
        snprintf(mpv_home, sizeof(mpv_home), "%s/mpv", xdg_dir);
    } else if (home && home[0]) {
        snprintf(mpv_home, sizeof(mpv_home), "%s/.config/mpv", home);
    }

    // Maintain compatibility with old ~/.mpv
    if (home && home[0])
        snprintf(old_home, sizeof(old_home), "%s/.mpv", home);

    // If the old ~/.mpv exists, and the XDG config dir doesn't, use the old
    // config dir only.
    if (mp_path_exists(old_home) && !mp_path_exists(mpv_home)) {
        snprintf(mpv_home, sizeof(mpv_home), "%s", old_home);
        old_home[0] = '\0';
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
    return NULL;
}
