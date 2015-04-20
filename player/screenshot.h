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

#ifndef MPLAYER_SCREENSHOT_H
#define MPLAYER_SCREENSHOT_H

#include <stdbool.h>

struct MPContext;

// One time initialization at program start.
void screenshot_init(struct MPContext *mpctx);

// Request a taking & saving a screenshot of the currently displayed frame.
// mode: 0: -, 1: save the actual output window contents, 2: with subtitles.
// each_frame: If set, this toggles per-frame screenshots, exactly like the
//             screenshot slave command (MP_CMD_SCREENSHOT).
// osd: show status on OSD
void screenshot_request(struct MPContext *mpctx, int mode, bool each_frame,
                        bool osd);

// filename: where to store the screenshot; doesn't try to find an alternate
//           name if the file already exists
// mode, osd: same as in screenshot_request()
void screenshot_to_file(struct MPContext *mpctx, const char *filename, int mode,
                        bool osd);

// mode is the same as in screenshot_request()
struct mp_image *screenshot_get_rgb(struct MPContext *mpctx, int mode);

// Called by the playback core code when a new frame is displayed.
void screenshot_flip(struct MPContext *mpctx);

#endif /* MPLAYER_SCREENSHOT_H */
