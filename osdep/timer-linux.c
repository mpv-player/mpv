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
#ifdef __BEOS__
#define usleep(t) snooze(t)
#endif
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include "config.h"
#include "timer.h"

const char *timer_name =
#ifdef HAVE_NANOSLEEP
    "nanosleep()";
#else
    "usleep()";
#endif

int usec_sleep(int usec_delay)
{
#ifdef HAVE_NANOSLEEP
    struct timespec ts;
    ts.tv_sec  =  usec_delay / 1000000;
    ts.tv_nsec = (usec_delay % 1000000) * 1000;
    return nanosleep(&ts, NULL);
#else
    return usleep(usec_delay);
#endif
}

// Returns current time in microseconds
unsigned int GetTimer(void)
{
    struct timeval tv;
    //float s;
    gettimeofday(&tv,NULL);
    //s = tv.tv_usec; s *= 0.000001; s += tv.tv_sec;
    return tv.tv_sec * 1000000 + tv.tv_usec;
}

// Returns current time in milliseconds
unsigned int GetTimerMS(void)
{
    struct timeval tv;
    //float s;
    gettimeofday(&tv,NULL);
    //s = tv.tv_usec; s *= 0.000001; s += tv.tv_sec;
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static unsigned int RelativeTime = 0;

// Returns time spent between now and last call in seconds
float GetRelativeTime(void)
{
    unsigned int t,r;
    t = GetTimer();
    //t *= 16; printf("time = %ud\n", t);
    r = t - RelativeTime;
    RelativeTime = t;
    return (float) r * 0.000001F;
}

// Initialize timer, must be called at least once at start
void InitTimer(void)
{
    GetRelativeTime();
}


#if 0
#include <stdio.h>
int main(void)
{
    float t = 0;
    InitTimer();
    while (1) {
        t += GetRelativeTime();
        printf("time = %10.6f\r", t);
        fflush(stdout); }
}
#endif
