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

#ifndef MPLAYER_FIND_SUBFILES_H
#define MPLAYER_FIND_SUBFILES_H

#include <stdbool.h>

struct subfn {
    int type; // STREAM_SUB/STREAM_AUDIO
    int priority;
    char *fname;
    char *lang;
};

struct mpv_global;
struct subfn *find_external_files(struct mpv_global *global, const char *fname);

bool mp_might_be_subtitle_file(const char *filename);

#endif /* MPLAYER_FINDFILES_H */
