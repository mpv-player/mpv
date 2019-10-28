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

#ifndef MPLAYER_SDL_COMMON_H
#define MPLAYER_SDL_COMMON_H

#include <stdint.h>
#include <stdbool.h>
#include <SDL.h>

#include "common/common.h"

struct vo;
struct mp_log;

struct vo_sdl_state {
    SDL_Window *window;
    Uint32 wakeup_event;
    bool screensaver_enabled;
    Uint32 pending_vo_events;

    // options
    int switch_mode;
};

int vo_sdl_init(struct vo *vo, int flags);
void vo_sdl_uninit(struct vo *vo);
int vo_sdl_control(struct vo *vo, int *events, uint32_t request, void *data);
int vo_sdl_config(struct vo *vo);
void vo_sdl_wakeup(struct vo *vo);
void vo_sdl_wait_events(struct vo *vo, int64_t until_time_us);

#endif /* MPLAYER_SDL_COMMON_H */
