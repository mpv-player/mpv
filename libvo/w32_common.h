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
#include <windows.h>

extern HWND vo_w32_window;
extern int vo_vm;

int vo_w32_init(void);
void vo_w32_uninit(void);
void vo_w32_ontop(void);
void vo_w32_border(void);
void vo_w32_fullscreen(void);
int vo_w32_check_events(void);
int vo_w32_config(uint32_t, uint32_t, uint32_t);
void destroyRenderingContext(void);
void w32_update_xinerama_info(void);
HDC vo_w32_get_dc(HWND wnd);
void vo_w32_release_dc(HWND wnd, HDC dc);

#endif /* MPLAYER_W32_COMMON_H */
