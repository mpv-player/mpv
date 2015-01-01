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
#include <pthread.h>
#include <semaphore.h>

#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <sys/time.h>

// We keep this around to avoid active waiting while handling static
// initializers.
static pthread_once_t init_cs_once = PTHREAD_ONCE_INIT;
static CRITICAL_SECTION init_cs;

static void init_init_cs(void)
{
    InitializeCriticalSection(&init_cs);
}

static void init_lock(void)
{
    pthread_once(&init_cs_once, init_init_cs);
    EnterCriticalSection(&init_cs);
}

static void init_unlock(void)
{
    LeaveCriticalSection(&init_cs);
}

int pthread_once(pthread_once_t *once_control, void (*init_routine)(void))
{
    BOOL pending;
    if (!InitOnceBeginInitialize(once_control, 0, &pending, NULL))
        abort();
    if (pending) {
        init_routine();
        InitOnceComplete(once_control, 0, NULL);
    }
    return 0;
}

int pthread_mutex_destroy(pthread_mutex_t *mutex)
{
    DeleteCriticalSection(&mutex->cs);
    return 0;
}

int pthread_mutex_init(pthread_mutex_t *restrict mutex,
                       const pthread_mutexattr_t *restrict attr)
{
    InitializeCriticalSection(&mutex->cs);
    return 0;
}

int pthread_mutex_lock(pthread_mutex_t *mutex)
{
    if (mutex->requires_init) {
        init_lock();
        if (mutex->requires_init)
            InitializeCriticalSection(&mutex->cs);
        _InterlockedAnd(&mutex->requires_init, 0);
        init_unlock();
    }
    EnterCriticalSection(&mutex->cs);
    return 0;
}

int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
    LeaveCriticalSection(&mutex->cs);
    return 0;
}

static int cond_wait(pthread_cond_t *restrict cond,
                     pthread_mutex_t *restrict mutex,
                     DWORD ms)
{
    return SleepConditionVariableCS(cond, &mutex->cs, ms) ? 0 : ETIMEDOUT;
}

int pthread_cond_timedwait(pthread_cond_t *restrict cond,
                           pthread_mutex_t *restrict mutex,
                           const struct timespec *restrict abstime)
{
    // mpv uses mingw's gettimeofday() as time source too.
    struct timeval tv;
    gettimeofday(&tv, NULL);
    DWORD timeout_ms = 0;
    if (abstime->tv_sec >= INT64_MAX / 10000) {
        timeout_ms = INFINITE;
    } else if (abstime->tv_sec >= tv.tv_sec) {
        long long msec = (abstime->tv_sec - tv.tv_sec) * 1000LL +
            abstime->tv_nsec / 1000LL / 1000LL - tv.tv_usec / 1000LL;
        if (msec > INT_MAX) {
            timeout_ms = INFINITE;
        } else if (msec > 0) {
            timeout_ms = msec;
        }
    }
    return cond_wait(cond, mutex, timeout_ms);
}

int pthread_cond_wait(pthread_cond_t *restrict cond,
                      pthread_mutex_t *restrict mutex)
{
    return cond_wait(cond, mutex, INFINITE);
}

struct m_thread_info {
    HANDLE handle;
    void *(*user_fn)(void *);
    void *user_arg;
    void *res;
};

// Assuming __thread maps to __declspec(thread)
static __thread struct m_thread_info *self;

pthread_t pthread_self(void)
{
    return (pthread_t){GetCurrentThreadId(), self};
}

void pthread_exit(void *retval)
{
    if (!self)
        abort(); // not started with pthread_create
    self->res = retval;
    if (!self->handle) {
        // detached case
        free(self);
        self = NULL;
    }
    ExitThread(0);
}

int pthread_join(pthread_t thread, void **retval)
{
    if (!thread.info)
        abort(); // not started with pthread_create
    HANDLE h = thread.info->handle;
    if (!h)
        abort(); // thread was detached
    WaitForSingleObject(h, INFINITE);
    CloseHandle(h);
    if (retval)
        *retval = thread.info->res;
    free(thread.info);
    return 0;
}

int pthread_detach(pthread_t thread)
{
    if (!pthread_equal(thread, pthread_self()))
        abort(); // restriction of this wrapper
    if (!thread.info)
        abort(); // not started with pthread_create
    if (!thread.info->handle)
        abort(); // already deatched
    CloseHandle(thread.info->handle);
    thread.info->handle = NULL;
    return 0;
}

static DWORD WINAPI run_thread(LPVOID lpParameter)
{
    struct m_thread_info *info = lpParameter;
    self = info;
    pthread_exit(info->user_fn(info->user_arg));
    abort(); // not reached
}

int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                   void *(*start_routine) (void *), void *arg)
{
    struct m_thread_info *info = calloc(1, sizeof(*info));
    if (!info)
        return EAGAIN;
    info->user_fn = start_routine;
    info->user_arg = arg;
    HANDLE h = CreateThread(NULL, 0, run_thread, info, CREATE_SUSPENDED, NULL);
    if (!h) {
        free(info);
        return EAGAIN;
    }
    info->handle = h;
    *thread = (pthread_t){GetThreadId(h), info};
    ResumeThread(h);
    return 0;
}

int sem_init(sem_t *sem, int pshared, unsigned int value)
{
    if (pshared)
        abort(); // unsupported
    pthread_mutex_init(&sem->lock, NULL);
    pthread_cond_init(&sem->wakeup, NULL);
    sem->value = value;
    return 0;
}

int sem_destroy(sem_t *sem)
{
    pthread_mutex_destroy(&sem->lock);
    pthread_cond_destroy(&sem->wakeup);
    return 0;
}

int sem_wait(sem_t *sem)
{
    pthread_mutex_lock(&sem->lock);
    while (!sem->value)
        pthread_cond_wait(&sem->wakeup, &sem->lock);
    sem->value -= 1;
    pthread_mutex_unlock(&sem->lock);
    return 0;
}

int sem_trywait(sem_t *sem)
{
    pthread_mutex_lock(&sem->lock);
    int r;
    if (sem->value > 0) {
        sem->value -= 1;
        r = 0;
    } else {
        errno = EAGAIN;
        r = -1;
    }
    pthread_mutex_unlock(&sem->lock);
    return r;
}

int sem_timedwait(sem_t *sem, const struct timespec *abs_timeout)
{
    pthread_mutex_lock(&sem->lock);
    while (!sem->value) {
        int err = pthread_cond_timedwait(&sem->wakeup, &sem->lock, abs_timeout);
        if (err) {
            pthread_mutex_unlock(&sem->lock);
            errno = err;
            return -1;
        }
    }
    sem->value -= 1;
    pthread_mutex_unlock(&sem->lock);
    return 0;
}

int sem_post(sem_t *sem)
{
    pthread_mutex_lock(&sem->lock);
    sem->value += 1;
    pthread_cond_broadcast(&sem->wakeup);
    pthread_mutex_unlock(&sem->lock);
    return 0;
}
