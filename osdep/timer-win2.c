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

#include <windows.h>
#include <sys/time.h>
#include <mmsystem.h>
#include <stdlib.h>
#include "timer.h"

#include "config.h"

static LARGE_INTEGER perf_freq;

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
    LARGE_INTEGER perf_count;
    QueryPerformanceCounter(&perf_count);

    // Convert QPC units (1/perf_freq seconds) to microseconds. This will work
    // without overflow because the QPC value is guaranteed not to roll-over
    // within 100 years, so perf_freq must be less than 2.9*10^9.
    return perf_count.QuadPart / perf_freq.QuadPart * 1000000 +
        perf_count.QuadPart % perf_freq.QuadPart * 1000000 / perf_freq.QuadPart;
}

void mp_raw_time_init(void)
{
    QueryPerformanceFrequency(&perf_freq);
#if !HAVE_UWP
    timeBeginPeriod(1); // request 1ms timer resolution
#endif
}
