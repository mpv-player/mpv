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

#include "common/common.h"

struct vo;

int vo_w32_init(struct vo *vo);
void vo_w32_uninit(struct vo *vo);
int vo_w32_control(struct vo *vo, int *events, int request, void *arg);
int vo_w32_config(struct vo *vo, uint32_t);
HWND vo_w32_hwnd(struct vo *vo);

#endif /* MPLAYER_W32_COMMON_H */
