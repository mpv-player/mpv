/*
 * Cocoa Event Handling
 *
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
 * with mplayer2.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MPLAYER_COCOA_EVENTS_H
#define MPLAYER_COCOA_EVENTS_H

#include "input/input.h"

void cocoa_events_init(struct input_ctx *ictx,
    void (*read_all_fd_events)(struct input_ctx *ictx, int time));
void cocoa_events_uninit(void);
void cocoa_events_read_all_events(struct input_ctx *ictx, int time);

#endif /* MPLAYER_COCOA_EVENTS_H */
