/*
 * precise timer routines for Linux/UNIX
 * copyright (C) LGB & A'rpi/ASTRAL
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

#include <stdlib.h>
#include <time.h>
#include "timer.h"

void mp_sleep_us(int64_t us)
{
    if (us < 0)
        return;
    struct timespec ts;
    ts.tv_sec  =  us / 1000000;
    ts.tv_nsec = (us % 1000000) * 1000;
    nanosleep(&ts, NULL);
}

uint64_t mp_raw_time_ns(void)
{
    struct timespec tp = {0};
#if defined(CLOCK_MONOTONIC_RAW)
    clock_gettime(CLOCK_MONOTONIC_RAW, &tp);
#else
    timespec_get(&tp, TIME_UTC);
#endif
    return tp.tv_sec * UINT64_C(1000000000) + tp.tv_nsec;
}

void mp_raw_time_init(void)
{
}
