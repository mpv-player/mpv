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

#ifndef MP_OSDEP_UTILS_H_
#define MP_OSDEP_UTILS_H_

typedef enum {
    MP_PLATFORM_UNKNOWN,
    MP_PLATFORM_WIN32,
    MP_PLATFORM_UWP,
    MP_PLATFORM_MACOS,
    MP_PLATFORM_BSD_LINUX,
    MP_PLATFORM_ANDROID,
    MP_PLATFORM_TVOS
} mp_target_platform;

// Array for enum to string conversion
static const char *const platform_enum_to_str[] = {
    "unknown",
    "win32",
    "uwp",
    "macos",
    "bsd/linux",
    "android",
    "tvos"
};

// Return an enum describing the target platform mpv has been built for
mp_target_platform get_target_platform(void);

#endif
