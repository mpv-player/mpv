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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "numcores.h"

#ifdef _SC_NPROCESSORS_ONLN
#define SCNAME _SC_NPROCESSORS_ONLN
#elif defined _SC_NPROC_ONLN
#define SCNAME _SC_NPROC_ONLN
#elif defined _SC_CRAY_NCPU
#define SCNAME _SC_CRAY_NCPU
#endif

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
int default_thread_count(void)
{
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    int count = info.dwNumberOfProcessors;
    if (count > 0)
        return count;
    return -1;
}

#elif defined SCNAME

int default_thread_count(void)
{
    long nprocs = sysconf(SCNAME);
    if (nprocs < 1)
        return -1;
    return nprocs;
}

#else

int default_thread_count(void)
{
    return -2;
}

#endif
