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

#ifndef OSDEP_PATH_H
#define OSDEP_PATH_H

#define MAX_CONFIG_PATHS 32

struct mpv_global;

// Append paths starting at dirs[i]. The dirs array has place only for at most
// MAX_CONFIG_PATHS paths, but it's guaranteed that at least 4 paths can be
// added without checking for i>=MAX_CONFIG_PATHS.
// Return the new value of i.
int mp_add_win_config_dirs(struct mpv_global *global, char **dirs, int i);
int mp_add_macosx_bundle_dir(struct mpv_global *global, char **dirs, int i);

#endif
