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

#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include "config.h"
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

#if defined(_POSIX_TIMERS) && _POSIX_TIMERS > 0 && defined(CLOCK_MONOTONIC)
uint64_t mp_raw_time_us(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts))
        abort();
    return ts.tv_sec * 1000000LL + ts.tv_nsec / 1000;
}
#else
uint64_t mp_raw_time_us(void)
{
    struct timeval tv;
    gettimeofday(&tv,NULL);
    return tv.tv_sec * 1000000LL + tv.tv_usec;
}
#endif

void mp_raw_time_init(void)
{
}
