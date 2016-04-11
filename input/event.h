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
#ifndef MP_INPUT_EVENT_H_
#define MP_INPUT_EVENT_H_

#include "misc/bstr.h"

struct input_ctx;

enum mp_dnd_action {
    DND_REPLACE,
    DND_APPEND,
};

// Enqueue files for playback after drag and drop
void mp_event_drop_files(struct input_ctx *ictx, int num_files, char **files,
                         enum mp_dnd_action append);

// Drop data in a specific format (identified by the mimetype).
// Returns <0 on error, ==0 if data was ok but empty, >0 on success.
int mp_event_drop_mime_data(struct input_ctx *ictx, const char *mime_type,
                            bstr data, enum mp_dnd_action append);

// Many drag & drop APIs support multiple mime types, and this function returns
// whether a type is preferred (higher integer score), or supported (scores
// below 0 indicate unsupported types).
int mp_event_get_mime_type_score(struct input_ctx *ictx, const char *mime_type);

#endif
