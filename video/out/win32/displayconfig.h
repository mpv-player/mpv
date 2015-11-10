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

#ifndef MP_WIN32_DISPLAYCONFIG_H_
#define MP_WIN32_DISPLAYCONFIG_H_

#include <wchar.h>

// Given a GDI monitor device name, get the precise refresh rate using the
// Windows 7 DisplayConfig API. Returns 0.0 on failure.
double mp_w32_displayconfig_get_refresh_rate(const wchar_t *device);

#endif
