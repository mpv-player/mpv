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

#include "osdep/semaphore.h"

#ifdef MP_SEMAPHORE_EMULATION

#include <unistd.h>
#include <poll.h>
#include <limits.h>
#include <sys/time.h>
#include <errno.h>

#include <common/common.h>
#include "io.h"
#include "timer.h"

int mp_sem_init(mp_sem_t *sem, int pshared, unsigned int value)
{
    if (pshared) {
        errno = ENOSYS;
        return -1;
    }
    if (value > INT_MAX) {
        errno = EINVAL;
        return -1;
    }
    if (mp_make_wakeup_pipe(sem->wakeup_pipe) < 0)
        return -1;
    sem->count = 0;
    pthread_mutex_init(&sem->lock, NULL);
    return 0;
}

int mp_sem_wait(mp_sem_t *sem)
{
    return mp_sem_timedwait(sem, -1);
}

int mp_sem_trywait(mp_sem_t *sem)
{
    int r = -1;
    pthread_mutex_lock(&sem->lock);
    if (sem->count == 0) {
        char buf[1024];
        ssize_t s = read(sem->wakeup_pipe[0], buf, sizeof(buf));
        if (s > 0 && s <= INT_MAX - sem->count) // can't handle overflows correctly
            sem->count += s;
    }
    if (sem->count > 0) {
        sem->count -= 1;
        r = 0;
    }
    pthread_mutex_unlock(&sem->lock);
    if (r < 0)
        errno = EAGAIN;
    return r;
}

int mp_sem_timedwait(mp_sem_t *sem, int64_t until)
{
    while (1) {
        if (!mp_sem_trywait(sem))
            return 0;

        int timeout = 0;
        if (until == -1) {
            timeout = -1;
        } else if (until >= 0) {
            timeout = (until - mp_time_ns()) / MP_TIME_MS_TO_NS(1);
            timeout = MPCLAMP(timeout, 0, INT_MAX);
        } else {
            assert(false && "Invalid mp_time value!");
        }

        struct pollfd fd = {.fd = sem->wakeup_pipe[0], .events = POLLIN};
        int r = poll(&fd, 1, timeout);
        if (r < 0)
            return -1;
        if (r == 0) {
            errno = ETIMEDOUT;
            return -1;
        }
    }
}

int mp_sem_post(mp_sem_t *sem)
{
    if (write(sem->wakeup_pipe[1], &(char){0}, 1) == 1)
        return 0;
    // Actually we can't handle overflow fully correctly, because we can't
    // check sem->count atomically, while still being AS-safe.
    errno = EOVERFLOW;
    return -1;
}

int mp_sem_destroy(mp_sem_t *sem)
{
    close(sem->wakeup_pipe[0]);
    close(sem->wakeup_pipe[1]);
    pthread_mutex_destroy(&sem->lock);
    return 0;
}

#endif
