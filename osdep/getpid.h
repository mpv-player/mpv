/*
 * getpid()/GetCurrentProcessId() wrapper for Windows
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

#ifndef MPLAYER_OSDEP_GETPID
#define MPLAYER_OSDEP_GETPID

#ifdef _WIN32

#include <windows.h>

// The POSIX getpid() is available on Linux and OSX, but not on Windows.
// So, on Windows, make getpid() actually call GetCurrentProcessId()

#undef getpid
#define getpid GetCurrentProcessId
// GetCurrentProcessId returns a DWORD aka an unsigned int, which
// should be compatible with pid_t.

#endif

#endif
