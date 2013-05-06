/*
 * Get path to config dir/file.
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

#ifndef MPLAYER_PATH_H
#define MPLAYER_PATH_H

#include <stdbool.h>
#include "core/bstr.h"


// Search for the input filename in several paths. These include user and global
// config locations by default. Some platforms may implement additional platform
// related lookups (i.e.: OSX inside an application bundle).
char *mp_find_config_file(const char *filename);

// Search for the input filename in the global configuration location.
char *mp_find_global_config_file(const char *filename);

// Search for the input filename in the user configuration location.
char *mp_find_user_config_file(const char *filename);

// Return pointer to filename part of path

char *mp_basename(const char *path);

/* Return struct bstr referencing directory part of path, or if that
 * would be empty, ".".
 */
struct bstr mp_dirname(const char *path);

/* Join two path components and return a newly allocated string
 * for the result. '/' is inserted between the components if needed.
 * If p2 is an absolute path then the value of p1 is ignored.
 */
char *mp_path_join(void *talloc_ctx, struct bstr p1, struct bstr p2);

char *mp_getcwd(void *talloc_ctx);

bool mp_path_exists(const char *path);
bool mp_path_isdir(const char *path);

#endif /* MPLAYER_PATH_H */
