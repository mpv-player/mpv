/*
 * Path utility functions
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

#pragma once

#include <stdbool.h>
#include "misc/bstr.h"

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

// Return whether the path is absolute.
bool mp_path_is_absolute(struct bstr path);

char *mp_getcwd(void *talloc_ctx);

char *mp_normalize_path(void *talloc_ctx, const char *path);

bool mp_path_exists(const char *path);
bool mp_path_isdir(const char *path);

bool mp_is_url(bstr path);

bstr mp_split_proto(bstr path, bstr *out_url);

void mp_mkdirp(const char *dir);
