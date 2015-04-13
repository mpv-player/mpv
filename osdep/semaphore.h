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

#ifndef MP_SEMAPHORE_H_
#define MP_SEMAPHORE_H_

#include <sys/types.h>
#include <semaphore.h>

// OSX provides non-working empty stubs, so we emulate them.
// This should be AS-safe, but cancellation issues were ignored.
// sem_getvalue() is not provided.
// sem_post() won't always correctly return an error on overflow.
// Process-shared semantics are not provided.

#ifdef __APPLE__

#define MP_SEMAPHORE_EMULATION

#include <pthread.h>

#define MP_SEM_VALUE_MAX 4096

typedef struct {
    int wakeup_pipe[2];
    pthread_mutex_t lock;
    // protected by lock
    unsigned int count;
} mp_sem_t;

int mp_sem_init(mp_sem_t *sem, int pshared, unsigned int value);
int mp_sem_wait(mp_sem_t *sem);
int mp_sem_trywait(mp_sem_t *sem);
int mp_sem_timedwait(mp_sem_t *sem, const struct timespec *abs_timeout);
int mp_sem_post(mp_sem_t *sem);
int mp_sem_destroy(mp_sem_t *sem);

#undef sem_init
#undef sem_wait
#undef sem_trywait
#undef sem_timedwait
#undef sem_post
#undef sem_getvalue
#undef sem_destroy
#undef sem_getvalue
#undef sem_t
#undef SEM_VALUE_MAX

#define sem_init        mp_sem_init
#define sem_wait        mp_sem_wait
#define sem_trywait     mp_sem_trywait
#define sem_timedwait   mp_sem_timedwait
#define sem_post        mp_sem_post
#define sem_destroy     mp_sem_destroy
#define sem_t           mp_sem_t
#define SEM_VALUE_MAX   MP_SEM_VALUE_MAX

#define sem_getvalue    (void)

#endif

#endif
