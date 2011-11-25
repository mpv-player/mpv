/*
 * This file is part of mplayer2.
 *
 * mplayer2 is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mplayer2 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mplayer2; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef MPLAYER_SCREENSHOT_H
#define MPLAYER_SCREENSHOT_H

#include <stdbool.h>

struct MPContext;
struct mp_image;

// Request a taking & saving a screenshot of the currently displayed frame.
// each_frame: If set, this toggles per-frame screenshots, exactly like the
//             screenshot slave command (MP_CMD_SCREENSHOT).
// full_window: If set, save the actual output window contents.
void screenshot_request(struct MPContext *mpctx, bool each_frame,
                        bool full_window);

// Save the screenshot contained in the image to disk.
// The image can be in any format supported by libswscale.
void screenshot_save(struct MPContext *mpctx, struct mp_image *image);

// Called by the playback core code when a new frame is displayed.
void screenshot_flip(struct MPContext *mpctx);

#endif /* MPLAYER_SCREENSHOT_H */
