/*
 * Get path to config dir/file.
 *
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

#ifndef MPLAYER_PATH_H
#define MPLAYER_PATH_H

#include <stdbool.h>
#include "misc/bstr.h"
#include "misc/path_utils.h"

struct mpv_global;
struct MPOpts;

void mp_init_paths(struct mpv_global *global, struct MPOpts *opts);

// Search for the input filename in several paths. These include user and global
// config locations by default. Some platforms may implement additional platform
// related lookups (i.e.: macOS inside an application bundle).
char *mp_find_config_file(void *talloc_ctx, struct mpv_global *global,
                          const char *filename);

// Search for local writable user files within a specific kind of user dir
// as documented in osdep/path.h. This returns a result even if the file does
// not exist. Calling it with filename="" is equivalent to retrieving the path
// to the dir.
char *mp_find_user_file(void *talloc_ctx, struct mpv_global *global,
                        const char *type, const char *filename);

// Find all instances of the given config file. Paths are returned in order
// from lowest to highest priority. filename can contain multiple names
// separated with '|', with the first having highest priority.
char **mp_find_all_config_files(void *talloc_ctx, struct mpv_global *global,
                                const char *filename);

// Normally returns a talloc_strdup'ed copy of the path, except for special
// paths starting with '~'. Used to allow the user explicitly reference a
// file from the user's home or mpv config directory.
char *mp_get_user_path(void *talloc_ctx, struct mpv_global *global,
                       const char *path);

// Same as mp_get_user_path but also normalizes the path if it happens to be
// relative. Requires a talloc_ctx.
char *mp_normalize_user_path(void *talloc_ctx, struct mpv_global *global,
                             const char *path);

void mp_mk_user_dir(struct mpv_global *global, const char *type, char *subdir);

#endif /* MPLAYER_PATH_H */
