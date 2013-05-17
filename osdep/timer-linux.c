/*
 * precise timer routines for Linux
 * copyright (C) LGB & A'rpi/ASTRAL
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
#ifdef HAVE_NANOSLEEP
    struct timespec ts;
    ts.tv_sec  =  us / 1000000;
    ts.tv_nsec = (us % 1000000) * 1000;
    nanosleep(&ts, NULL);
#else
    usleep(us);
#endif
}

uint64_t mp_raw_time_us(void)
{
    struct timeval tv;
    gettimeofday(&tv,NULL);
    return tv.tv_sec * 1000000LL + tv.tv_usec;
}

void mp_raw_time_init(void)
{
}
