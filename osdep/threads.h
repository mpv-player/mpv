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

#ifndef MP_OSDEP_THREADS_H_
#define MP_OSDEP_THREADS_H_

#include <pthread.h>
#include <inttypes.h>

// Call pthread_cond_timedwait() with an absolute timeout using the same
// time source/unit as mp_time_us() (microseconds).
int mpthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex,
                            int64_t abstime);

// Wait by a relative amount of time in seconds.
int mpthread_cond_timedwait_rel(pthread_cond_t *cond, pthread_mutex_t *mutex,
                                double seconds);

// Helper to reduce boiler plate.
int mpthread_mutex_init_recursive(pthread_mutex_t *mutex);

// Set thread name (for debuggers).
void mpthread_set_name(const char *name);

#endif
