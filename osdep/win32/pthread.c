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

#include <pthread.h>
#include <semaphore.h>

#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <sys/time.h>
#include <assert.h>

#include "osdep/timer.h"  // mp_{start,end}_hires_timers

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
    if (mutex->use_cs)
        DeleteCriticalSection(&mutex->lock.cs);
    return 0;
}

int pthread_mutex_init(pthread_mutex_t *restrict mutex,
                       const pthread_mutexattr_t *restrict attr)
{
    mutex->use_cs = attr && (*attr & PTHREAD_MUTEX_RECURSIVE);
    if (mutex->use_cs) {
        InitializeCriticalSection(&mutex->lock.cs);
    } else {
        InitializeSRWLock(&mutex->lock.srw);
    }
    return 0;
}

int pthread_mutex_lock(pthread_mutex_t *mutex)
{
    if (mutex->use_cs) {
        EnterCriticalSection(&mutex->lock.cs);
    } else {
        AcquireSRWLockExclusive(&mutex->lock.srw);
    }
    return 0;
}

int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
    if (mutex->use_cs) {
        LeaveCriticalSection(&mutex->lock.cs);
    } else {
        ReleaseSRWLockExclusive(&mutex->lock.srw);
    }
    return 0;
}

static int cond_wait(pthread_cond_t *restrict cond,
                     pthread_mutex_t *restrict mutex,
                     DWORD ms)
{
    BOOL res;
    int hrt = mp_start_hires_timers(ms);
    if (mutex->use_cs) {
        res = SleepConditionVariableCS(cond, &mutex->lock.cs, ms);
    } else {
        res = SleepConditionVariableSRW(cond, &mutex->lock.srw, ms, 0);
    }
    mp_end_hires_timers(hrt);
    return res ? 0 : ETIMEDOUT;
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

static pthread_mutex_t pthread_table_lock = PTHREAD_MUTEX_INITIALIZER;
static struct m_thread_info *pthread_table;
size_t pthread_table_num;

struct m_thread_info {
    DWORD id;
    HANDLE handle;
    void *(*user_fn)(void *);
    void *user_arg;
    void *res;
};

static struct m_thread_info *find_thread_info(DWORD id)
{
    for (int n = 0; n < pthread_table_num; n++) {
        if (id == pthread_table[n].id)
            return &pthread_table[n];
    }
    return NULL;
}

static void remove_thread_info(struct m_thread_info *info)
{
    assert(pthread_table_num);
    assert(info >= &pthread_table[0] && info < &pthread_table[pthread_table_num]);

    pthread_table[info - pthread_table] = pthread_table[pthread_table_num - 1];
    pthread_table_num -= 1;

    // Avoid upsetting leak detectors.
    if (pthread_table_num == 0) {
        free(pthread_table);
        pthread_table = NULL;
    }
}

void pthread_exit(void *retval)
{
    pthread_mutex_lock(&pthread_table_lock);
    struct m_thread_info *info = find_thread_info(pthread_self());
    assert(info); // not started with pthread_create, or pthread_join() race
    info->res = retval;
    if (!info->handle)
        remove_thread_info(info); // detached case
    pthread_mutex_unlock(&pthread_table_lock);

    ExitThread(0);
}

int pthread_join(pthread_t thread, void **retval)
{
    pthread_mutex_lock(&pthread_table_lock);
    struct m_thread_info *info = find_thread_info(thread);
    assert(info); // not started with pthread_create, or pthread_join() race
    HANDLE h = info->handle;
    assert(h); // thread was detached
    pthread_mutex_unlock(&pthread_table_lock);

    WaitForSingleObject(h, INFINITE);

    pthread_mutex_lock(&pthread_table_lock);
    info = find_thread_info(thread);
    assert(info);
    assert(info->handle == h);
    CloseHandle(h);
    if (retval)
        *retval = info->res;
    remove_thread_info(info);
    pthread_mutex_unlock(&pthread_table_lock);

    return 0;
}

int pthread_detach(pthread_t thread)
{
    if (!pthread_equal(thread, pthread_self()))
        abort(); // restriction of this wrapper

    pthread_mutex_lock(&pthread_table_lock);
    struct m_thread_info *info = find_thread_info(thread);
    assert(info); // not started with pthread_create
    assert(info->handle); // already detached
    CloseHandle(info->handle);
    info->handle = NULL;
    pthread_mutex_unlock(&pthread_table_lock);

    return 0;
}

static DWORD WINAPI run_thread(LPVOID lpParameter)
{
    pthread_mutex_lock(&pthread_table_lock);
    struct m_thread_info *pinfo = find_thread_info(pthread_self());
    assert(pinfo);
    struct m_thread_info info = *pinfo;
    pthread_mutex_unlock(&pthread_table_lock);

    pthread_exit(info.user_fn(info.user_arg));
    abort(); // not reached
}

int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                   void *(*start_routine) (void *), void *arg)
{
    int res = 0;
    pthread_mutex_lock(&pthread_table_lock);
    void *nalloc =
        realloc(pthread_table, (pthread_table_num + 1) * sizeof(pthread_table[0]));
    if (!nalloc) {
        res = EAGAIN;
        goto done;
    }
    pthread_table = nalloc;
    pthread_table_num += 1;
    struct m_thread_info *info = &pthread_table[pthread_table_num - 1];
    *info = (struct m_thread_info) {
        .user_fn = start_routine,
        .user_arg = arg,
    };
    info->handle = CreateThread(NULL, 0, run_thread, NULL, CREATE_SUSPENDED,
                                &info->id);
    if (!info->handle) {
        remove_thread_info(info);
        res = EAGAIN;
        goto done;
    }
    *thread = info->id;
    ResumeThread(info->handle);
done:
    pthread_mutex_unlock(&pthread_table_lock);
    return res;
}

void pthread_set_name_np(pthread_t thread, const char *name)
{
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP) && defined(_PROCESSTHREADSAPI_H_)
    HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
    if (!kernel32)
        return;
    HRESULT (WINAPI *pSetThreadDescription)(HANDLE, PCWSTR) =
        (void*)GetProcAddress(kernel32, "SetThreadDescription");
    if (!pSetThreadDescription)
        return;

    HANDLE th = OpenThread(THREAD_SET_LIMITED_INFORMATION, FALSE, thread);
    if (!th)
        return;
    wchar_t wname[80];
    int wc = MultiByteToWideChar(CP_UTF8, 0, name, -1, wname,
                                 sizeof(wname) / sizeof(wchar_t) - 1);
    if (wc > 0) {
        wname[wc] = L'\0';
        pSetThreadDescription(th, wname);
    }
    CloseHandle(th);
#endif
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
