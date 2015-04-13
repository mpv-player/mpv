/* Permission to use, copy, modify, and/or distribute this software for any
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

#include "osdep/io.h"

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
    return mp_sem_timedwait(sem, NULL);
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

int mp_sem_timedwait(mp_sem_t *sem, const struct timespec *abs_timeout)
{
    while (1) {
        if (!mp_sem_trywait(sem))
            return 0;

        int timeout_ms = -1;
        if (abs_timeout) {
            timeout_ms = 0;

            // OSX does not provide clock_gettime() either.
            struct timeval tv;
            gettimeofday(&tv, NULL);

            if (abs_timeout->tv_sec >= tv.tv_sec) {
                long long msec = (abs_timeout->tv_sec - tv.tv_sec) * 1000LL +
                    abs_timeout->tv_nsec / 1000LL / 1000LL - tv.tv_usec / 1000LL;
                if (msec > INT_MAX)
                    msec = INT_MAX;
                if (msec < 0)
                    msec = 0;
                timeout_ms = msec;
            }
        }
        struct pollfd fd = {.fd = sem->wakeup_pipe[0], .events = POLLIN};
        int r = poll(&fd, 1, timeout_ms);
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
