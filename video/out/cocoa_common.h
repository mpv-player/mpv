/*
 * Cocoa OpenGL Backend
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

#ifndef MPLAYER_COCOA_COMMON_H
#define MPLAYER_COCOA_COMMON_H

#include <stdbool.h>
#include <stdint.h>
#include <OpenGL/OpenGL.h>

struct vo;
struct vo_cocoa_state;

void vo_cocoa_init(struct vo *vo);
void vo_cocoa_uninit(struct vo *vo);

int vo_cocoa_config_window(struct vo *vo);

int vo_cocoa_control(struct vo *vo, int *events, int request, void *arg);

void vo_cocoa_swap_buffers(struct vo *vo);
void vo_cocoa_set_opengl_ctx(struct vo *vo, CGLContextObj ctx);

#endif /* MPLAYER_COCOA_COMMON_H */
