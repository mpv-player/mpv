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
void vo_w32_config(struct vo *vo);
HWND vo_w32_hwnd(struct vo *vo);
void vo_w32_run_on_thread(struct vo *vo, void (*cb)(void *ctx), void *ctx);

#endif /* MPLAYER_W32_COMMON_H */
