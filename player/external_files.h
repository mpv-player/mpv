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

#ifndef MPLAYER_FIND_SUBFILES_H
#define MPLAYER_FIND_SUBFILES_H

#include <stdbool.h>

#include "common/common.h"

struct subfn {
    int type; // STREAM_SUB/STREAM_AUDIO/STREAM_VIDEO(coverart)
    int priority;
    char *fname;
    char *lang;
    enum track_flags flags;
};

struct mpv_global;
struct MPOpts;
struct subfn *find_external_files(struct mpv_global *global, const char *fname,
                                  struct MPOpts *opts);

bool mp_might_be_subtitle_file(const char *filename);
void mp_update_subtitle_exts(struct MPOpts *opts);

#endif /* MPLAYER_FINDFILES_H */
