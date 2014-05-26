/*
 * precise timer routines for Windows
 *
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

#include <windows.h>
#include <sys/time.h>
#include <mmsystem.h>
#include <stdlib.h>
#include "timer.h"

void mp_sleep_us(int64_t us)
{
    if (us < 0)
        return;
    // Sleep(0) won't sleep for one clocktick as the unix usleep
    // instead it will only make the thread ready
    // it may take some time until it actually starts to run again
    if (us < 1000)
        us = 1000;
    Sleep(us / 1000);
}

uint64_t mp_raw_time_us(void)
{
    struct timeval tv;
    gettimeofday(&tv,NULL);
    return tv.tv_sec * 1000000LL + tv.tv_usec;
}

static void restore_timer(void)
{
    // The MSDN documents that begin/end "must" be matched. This satisfies
    // this requirement.
    timeEndPeriod(1);
}

void mp_raw_time_init(void)
{
    timeBeginPeriod(1); // request 1ms timer resolution
    atexit(restore_timer);
}
