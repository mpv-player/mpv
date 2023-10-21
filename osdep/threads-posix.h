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

#pragma once

#include <errno.h>
#include <pthread.h>
#include <stdio.h>

#include "common/common.h"
#include "timer.h"

typedef pthread_cond_t  mp_cond;
typedef pthread_mutex_t mp_mutex;
typedef pthread_mutex_t mp_static_mutex;
typedef pthread_once_t  mp_once;
typedef pthread_t       mp_thread;

#define MP_STATIC_COND_INITIALIZER PTHREAD_COND_INITIALIZER
#define MP_STATIC_MUTEX_INITIALIZER PTHREAD_MUTEX_INITIALIZER
#define MP_STATIC_ONCE_INITIALIZER PTHREAD_ONCE_INIT

static inline int mp_mutex_init_type_internal(mp_mutex *mutex, enum mp_mutex_type mtype)
{
    int mutex_type;
    switch (mtype) {
    case MP_MUTEX_RECURSIVE:
        mutex_type = PTHREAD_MUTEX_RECURSIVE;
        break;
    case MP_MUTEX_NORMAL:
    default:
#ifndef NDEBUG
        mutex_type = PTHREAD_MUTEX_ERRORCHECK;
#else
        mutex_type = PTHREAD_MUTEX_DEFAULT;
#endif
        break;
    }

    int ret = 0;
    pthread_mutexattr_t attr;
    ret = pthread_mutexattr_init(&attr);
    if (ret != 0)
        return ret;

    pthread_mutexattr_settype(&attr, mutex_type);
    ret = pthread_mutex_init(mutex, &attr);
    pthread_mutexattr_destroy(&attr);
    return ret;
}

#define mp_mutex_destroy    pthread_mutex_destroy
#define mp_mutex_lock       pthread_mutex_lock
#define mp_mutex_trylock    pthread_mutex_trylock
#define mp_mutex_unlock     pthread_mutex_unlock

#define mp_cond_init(cond)  pthread_cond_init(cond, NULL)
#define mp_cond_destroy     pthread_cond_destroy
#define mp_cond_broadcast   pthread_cond_broadcast
#define mp_cond_signal      pthread_cond_signal
#define mp_cond_wait        pthread_cond_wait

static inline int mp_cond_timedwait(mp_cond *cond, mp_mutex *mutex, int64_t timeout)
{
    timeout = MPMAX(0, timeout);
    // consider anything above 1000 days as infinity
    if (timeout > MP_TIME_S_TO_NS(1000 * 24 * 60 * 60))
        return pthread_cond_wait(cond, mutex);

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec  += timeout / MP_TIME_S_TO_NS(1);
    ts.tv_nsec += timeout % MP_TIME_S_TO_NS(1);
    if (ts.tv_nsec >= MP_TIME_S_TO_NS(1)) {
        ts.tv_nsec -= MP_TIME_S_TO_NS(1);
        ts.tv_sec++;
    }

    return pthread_cond_timedwait(cond, mutex, &ts);
}

static inline int mp_cond_timedwait_until(mp_cond *cond, mp_mutex *mutex, int64_t until)
{
    return mp_cond_timedwait(cond, mutex, until - mp_time_ns());
}

#define mp_exec_once pthread_once

#define MP_THREAD_VOID void *
#define MP_THREAD_RETURN() return NULL

#define mp_thread_create(t, f, a) pthread_create(t, NULL, f, a)
#define mp_thread_join(t)         pthread_join(t, NULL)
#define mp_thread_self            pthread_self
#define mp_thread_equal           pthread_equal

static inline void mp_thread_set_name(const char *name)
{
#if HAVE_GLIBC_THREAD_NAME
    if (pthread_setname_np(pthread_self(), name) == ERANGE) {
        char tname[16] = {0}; // glibc-checked kernel limit
        strncpy(tname, name, sizeof(tname) - 1);
        pthread_setname_np(pthread_self(), tname);
    }
#elif HAVE_BSD_THREAD_NAME
    pthread_set_name_np(pthread_self(), name);
#elif HAVE_OSX_THREAD_NAME
    pthread_setname_np(name);
#endif
}

static inline int64_t mp_thread_cpu_time_ns(mp_thread thread)
{
#if defined(_POSIX_TIMERS) && _POSIX_TIMERS > 0 && defined(_POSIX_THREAD_CPUTIME)
    clockid_t id;
    struct timespec ts;
    if (pthread_getcpuclockid(thread, &id) == 0 && clock_gettime(id, &ts) == 0)
        return MP_TIME_S_TO_NS(ts.tv_sec) + ts.tv_nsec;
#endif
    return 0;
}
