/*
 * strnlen wrapper
 *
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

#ifndef MP_OSDEP_STRNLEN
#define MP_OSDEP_STRNLEN

#ifdef __ANDROID__
// strnlen is broken on current android ndk, see https://code.google.com/p/android/issues/detail?id=74741
#include "osdep/android/strnlen.h"
#define strnlen freebsd_strnlen
#endif

#endif
