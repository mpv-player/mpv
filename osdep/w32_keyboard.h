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

#ifndef MP_W32_KEYBOARD
#define MP_W32_KEYBOARD

#include <stdbool.h>

/* Convert a Windows virtual key code to an mpv key */
int mp_w32_vkey_to_mpkey(UINT vkey, bool extended);

/* Convert a WM_APPCOMMAND value to an mpv key */
int mp_w32_appcmd_to_mpkey(UINT appcmd);

#endif
