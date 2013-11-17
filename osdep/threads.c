/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <time.h>
#include <unistd.h>
#include <sys/time.h>

#include "threads.h"

static void get_pthread_time(struct timespec *out_ts)
{
#if defined(_POSIX_TIMERS) && _POSIX_TIMERS > 0
    clock_gettime(CLOCK_REALTIME, out_ts);
#else
    // OSX
    struct timeval tv;
    gettimeofday(&tv, NULL);
    out_ts->tv_sec = tv.tv_sec;
    out_ts->tv_nsec = tv.tv_usec * 1000UL;
#endif
}

static void timespec_add_seconds(struct timespec *ts, double seconds)
{
    unsigned long secs = (int)seconds;
    unsigned long nsecs = (seconds - secs) * 1000000000UL;
    if (nsecs + ts->tv_nsec >= 1000000000UL) {
        secs += 1;
        nsecs -= 1000000000UL;
    }
    ts->tv_sec += secs;
    ts->tv_nsec += nsecs;
}

// Call pthread_cond_timedwait() with a relative timeout in seconds
int mpthread_cond_timed_wait(pthread_cond_t *cond, pthread_mutex_t *mutex,
                             double timeout)
{
    struct timespec ts;
    get_pthread_time(&ts);
    timespec_add_seconds(&ts, timeout);
    return pthread_cond_timedwait(cond, mutex, &ts);
}
