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

#include "threads.h"
#include "timer.h"

int mpthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex,
                            int64_t abstime)
{
    struct timespec ts = mp_time_us_to_timespec(abstime);
    return pthread_cond_timedwait(cond, mutex, &ts);
}

int mpthread_cond_timedwait_rel(pthread_cond_t *cond, pthread_mutex_t *mutex,
                                double s)
{
    return mpthread_cond_timedwait(cond, mutex, mp_add_timeout(mp_time_us(), s));
}

int mpthread_mutex_init_recursive(pthread_mutex_t *mutex)
{
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    int r = pthread_mutex_init(mutex, &attr);
    pthread_mutexattr_destroy(&attr);
    return r;
}
