/*
 * common SDL routines
 *
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef MPLAYER_SDL_COMMON_H
#define MPLAYER_SDL_COMMON_H

#include "config.h"
#ifdef CONFIG_SDL_SDL_H
#include <SDL/SDL.h>
#else
#include <SDL.h>
#endif

int vo_sdl_init(void);
void vo_sdl_uninit(void);
void vo_sdl_fullscreen(void);
int sdl_set_mode(int bpp, uint32_t flags);
int sdl_default_handle_event(SDL_Event *event);

#endif /* MPLAYER_SDL_COMMON_H */
