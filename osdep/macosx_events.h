/*
 * Cocoa Application Event Handling
 *
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

#ifndef MACOSX_EVENTS_H
#define MACOSX_EVENTS_H
#include "input/keycodes.h"

struct input_ctx;

void cocoa_put_key(int keycode);
void cocoa_put_key_with_modifiers(int keycode, int modifiers);
void cocoa_put_key_event(void *event);

void cocoa_start_event_monitor(void);

void cocoa_init_apple_remote(void);
void cocoa_uninit_apple_remote(void);

void cocoa_init_media_keys(void);
void cocoa_uninit_media_keys(void);

void cocoa_set_input_context(struct input_ctx *input_context);

#endif
