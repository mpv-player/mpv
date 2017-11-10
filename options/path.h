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

struct mpv_global;

// Search for the input filename in several paths. These include user and global
// config locations by default. Some platforms may implement additional platform
// related lookups (i.e.: OSX inside an application bundle).
char *mp_find_config_file(void *talloc_ctx, struct mpv_global *global,
                          const char *filename);

// Like mp_find_config_file(), but search only the local writable user config
// dir. Also, this returns a result even if the file does not exist. Calling
// it with filename="" is equivalent to retrieving the user config dir.
char *mp_find_user_config_file(void *talloc_ctx, struct mpv_global *global,
                               const char *filename);

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

// Return pointer to filename part of path

char *mp_basename(const char *path);

/* Return file extension, excluding the '.'. If root is not NULL, set it to the
 * part of the path without extension. So: path == root + "." + extension
 * Don't consider it a file extension if the only '.' is the first character.
 * Return NULL if no extension and don't set *root in this case.
 */
char *mp_splitext(const char *path, bstr *root);

/* Return struct bstr referencing directory part of path, or if that
 * would be empty, ".".
 */
struct bstr mp_dirname(const char *path);

void mp_path_strip_trailing_separator(char *path);

/* Join two path components and return a newly allocated string
 * for the result. '/' is inserted between the components if needed.
 * If p2 is an absolute path then the value of p1 is ignored.
 */
char *mp_path_join(void *talloc_ctx, const char *p1, const char *p2);
char *mp_path_join_bstr(void *talloc_ctx, struct bstr p1, struct bstr p2);

char *mp_getcwd(void *talloc_ctx);

bool mp_path_exists(const char *path);
bool mp_path_isdir(const char *path);

bool mp_is_url(bstr path);

bstr mp_split_proto(bstr path, bstr *out_url);

void mp_mkdirp(const char *dir);
void mp_mk_config_dir(struct mpv_global *global, char *subdir);

#endif /* MPLAYER_PATH_H */
