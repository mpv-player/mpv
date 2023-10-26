/*
 * Cocoa Application Event Handling
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

#ifndef MACOSX_EVENTS_H
#define MACOSX_EVENTS_H
#include "input/keycodes.h"

struct input_ctx;
struct mpv_handle;

void cocoa_put_key(int keycode);
void cocoa_put_key_with_modifiers(int keycode, int modifiers);

void cocoa_init_media_keys(void);
void cocoa_uninit_media_keys(void);

void cocoa_set_input_context(struct input_ctx *input_context);
void cocoa_set_mpv_handle(struct mpv_handle *ctx);

#endif
