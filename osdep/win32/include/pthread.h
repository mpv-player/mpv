/* Copyright (C) 2017 the mpv developers
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef MP_WRAP_PTHREAD_H_
#define MP_WRAP_PTHREAD_H_

#include <windows.h>

#include <sys/types.h>

// Note: all pthread functions are mangled to make static linking easier.
#define pthread_once m_pthread_once
#define pthread_mutex_destroy m_pthread_mutex_destroy
#define pthread_mutex_init m_pthread_mutex_init
#define pthread_mutex_lock m_pthread_mutex_lock
#define pthread_mutex_unlock m_pthread_mutex_unlock
#define pthread_cond_timedwait m_pthread_cond_timedwait
#define pthread_cond_wait m_pthread_cond_wait
#define pthread_exit m_pthread_exit
#define pthread_join m_pthread_join
#define pthread_detach m_pthread_detach
#define pthread_create m_pthread_create
#define pthread_set_name_np m_pthread_set_name_np

#define pthread_once_t INIT_ONCE
#define PTHREAD_ONCE_INIT INIT_ONCE_STATIC_INIT

int pthread_once(pthread_once_t *once_control, void (*init_routine)(void));

typedef struct {
    char use_cs;
    union {
        SRWLOCK srw;
        CRITICAL_SECTION cs;
    } lock;
} pthread_mutex_t;

// Assume SRWLOCK_INIT is {0} so we can easily remain C89-compatible.
#define PTHREAD_MUTEX_INITIALIZER {0}

#define pthread_mutexattr_t int
#define pthread_mutexattr_destroy(attr) (void)0
#define pthread_mutexattr_init(attr) (*(attr) = 0)
#define pthread_mutexattr_settype(attr, type) (*(attr) = (type))
#define PTHREAD_MUTEX_RECURSIVE 1
#define PTHREAD_MUTEX_ERRORCHECK 2 // unsupported

int pthread_mutex_destroy(pthread_mutex_t *mutex);
int pthread_mutex_init(pthread_mutex_t *restrict mutex,
                       const pthread_mutexattr_t *restrict attr);

int pthread_mutex_lock(pthread_mutex_t *mutex);
int pthread_mutex_unlock(pthread_mutex_t *mutex);

#define pthread_cond_t CONDITION_VARIABLE
#define pthread_condattr_t int

#define PTHREAD_COND_INITIALIZER CONDITION_VARIABLE_INIT

#define pthread_cond_init(cond, attr) InitializeConditionVariable(cond)
#define pthread_cond_destroy(c) (void)0
#define pthread_cond_broadcast(cond) WakeAllConditionVariable(cond)
#define pthread_cond_signal(cond) WakeConditionVariable(cond)

int pthread_cond_timedwait(pthread_cond_t *restrict cond,
                           pthread_mutex_t *restrict mutex,
                           const struct timespec *restrict abstime);
int pthread_cond_wait(pthread_cond_t *restrict cond,
                      pthread_mutex_t *restrict mutex);

#define pthread_t DWORD

#define pthread_equal(a, b) ((a) == (b))
#define pthread_self() (GetCurrentThreadId())

void pthread_exit(void *retval);
int pthread_join(pthread_t thread, void **retval);
int pthread_detach(pthread_t thread);

#define pthread_attr_t int

int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                   void *(*start_routine) (void *), void *arg);

void pthread_set_name_np(pthread_t thread, const char *name);

#endif
