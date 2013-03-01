/*
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

#ifndef MPLAYER_W32_COMMON_H
#define MPLAYER_W32_COMMON_H

#include <stdint.h>
#include <stdbool.h>
#include <windows.h>

struct vo_w32_state {
    HWND window;

    // last non-fullscreen extends (updated only on fullscreen or on initialization)
    int prev_width;
    int prev_height;
    int prev_x;
    int prev_y;

    // whether the window position and size were intialized
    bool window_bounds_initialized;

    bool current_fs;

    int window_x;
    int window_y;

    // video size
    uint32_t o_dwidth;
    uint32_t o_dheight;

    int event_flags;
    int mon_cnt;
    int mon_id;
};

struct vo;

int vo_w32_init(struct vo *vo);
void vo_w32_uninit(struct vo *vo);
void vo_w32_ontop(struct vo *vo);
void vo_w32_border(struct vo *vo);
void vo_w32_fullscreen(struct vo *vo);
int vo_w32_check_events(struct vo *vo);
int vo_w32_config(struct vo *vo, uint32_t, uint32_t, uint32_t);
void w32_update_xinerama_info(struct vo *vo);

#endif /* MPLAYER_W32_COMMON_H */
