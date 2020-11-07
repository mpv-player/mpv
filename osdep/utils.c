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

#include "utils.h"
#include "config.h"

mp_target_platform get_target_platform(void)
{
#if HAVE_UWP
    return MP_PLATFORM_UWP;
#endif
#if HAVE_WIN32_DESKTOP
    return MP_PLATFORM_WIN32;
#endif
#if defined(TARGET_OS_TV) && TARGET_OS_TV
    return MP_PLATFORM_TVOS;
#elif defined(__APPLE__)
    return MP_PLATFORM_MACOS;
#endif
#if HAVE_ANDROID
    return MP_PLATFORM_ANDROID;
#elif HAVE_POSIX
    return MP_PLATFORM_BSD_LINUX;
#endif
    return MP_PLATFORM_UNKNOWN;
}
