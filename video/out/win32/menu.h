/*
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

#ifndef MP_WIN32_MENU_H
#define MP_WIN32_MENU_H

#include <windows.h>

struct mpv_node;
struct menu_ctx;

struct menu_ctx *mp_win32_menu_init(void);
void mp_win32_menu_uninit(struct menu_ctx *ctx);
void mp_win32_menu_show(struct menu_ctx *ctx, HWND hwnd);
void mp_win32_menu_update(struct menu_ctx *ctx, struct mpv_node *data);
const char* mp_win32_menu_get_cmd(struct menu_ctx *ctx, UINT id);

#endif
