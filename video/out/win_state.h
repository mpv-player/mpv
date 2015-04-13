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

#ifndef MP_WIN_STATE_H_
#define MP_WIN_STATE_H_

#include "common/common.h"

struct vo;

enum {
    // By user settings, the window manager's chosen window position should
    // be overridden.
    VO_WIN_FORCE_POS = (1 << 0),
};

struct vo_win_geometry {
    // Bitfield of VO_WIN_* flags
    int flags;
    // Position & size of the window. In xinerama coordinates, i.e. they're
    // relative to the virtual desktop encompassing all screens, not the
    // current screen.
    struct mp_rect win;
    // Aspect ratio of the current monitor.
    // (calculated from screen size and options.)
    double monitor_par;
};

void vo_calc_window_geometry(struct vo *vo, const struct mp_rect *screen,
                             struct vo_win_geometry *out_geo);
void vo_apply_window_geometry(struct vo *vo, const struct vo_win_geometry *geo);

#endif
