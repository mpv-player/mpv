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
#include <process.h>
#include <windows.h>

#include "common/common.h"
#include "timer.h"

typedef struct {
    char use_cs;
    union {
        CRITICAL_SECTION cs;
        SRWLOCK srw;
    };
} mp_mutex;

typedef CONDITION_VARIABLE mp_cond;
typedef INIT_ONCE          mp_once;
typedef mp_mutex           mp_static_mutex;
typedef HANDLE             mp_thread;
typedef DWORD              mp_thread_id;

#define MP_STATIC_COND_INITIALIZER CONDITION_VARIABLE_INIT
#define MP_STATIC_MUTEX_INITIALIZER (mp_mutex){ .srw = SRWLOCK_INIT }
#define MP_STATIC_ONCE_INITIALIZER INIT_ONCE_STATIC_INIT

static inline int mp_mutex_init_type_internal(mp_mutex *mutex, enum mp_mutex_type mtype)
{
    mutex->use_cs = mtype == MP_MUTEX_RECURSIVE;
    if (mutex->use_cs)
        return !InitializeCriticalSectionEx(&mutex->cs, 0, 0);
    InitializeSRWLock(&mutex->srw);
    return 0;
}

static inline int mp_mutex_destroy(mp_mutex *mutex)
{
    if (mutex->use_cs)
        DeleteCriticalSection(&mutex->cs);
    return 0;
}

static inline int mp_mutex_lock(mp_mutex *mutex)
{
    if (mutex->use_cs) {
        EnterCriticalSection(&mutex->cs);
    } else {
        AcquireSRWLockExclusive(&mutex->srw);
    }
    return 0;
}

static inline int mp_mutex_trylock(mp_mutex *mutex)
{
    if (mutex->use_cs)
        return !TryEnterCriticalSection(&mutex->cs);
    return !TryAcquireSRWLockExclusive(&mutex->srw);
}

static inline int mp_mutex_unlock(mp_mutex *mutex)
{
    if (mutex->use_cs) {
        LeaveCriticalSection(&mutex->cs);
    } else {
        ReleaseSRWLockExclusive(&mutex->srw);
    }
    return 0;
}

static inline int mp_cond_init(mp_cond *cond)
{
    InitializeConditionVariable(cond);
    return 0;
}

static inline int mp_cond_destroy(mp_cond *cond)
{
    // condition variables are not destroyed
    (void) cond;
    return 0;
}

static inline int mp_cond_broadcast(mp_cond *cond)
{
    WakeAllConditionVariable(cond);
    return 0;
}

static inline int mp_cond_signal(mp_cond *cond)
{
    WakeConditionVariable(cond);
    return 0;
}

static inline int mp_cond_timedwait(mp_cond *cond, mp_mutex *mutex, int64_t timeout)
{
    timeout = MPCLAMP(timeout, 0, MP_TIME_MS_TO_NS(INFINITE)) / MP_TIME_MS_TO_NS(1);

    int ret = 0;
    int hrt = mp_start_hires_timers(timeout);
    BOOL bRet;

    if (mutex->use_cs) {
        bRet = SleepConditionVariableCS(cond, &mutex->cs, timeout);
    } else {
        bRet = SleepConditionVariableSRW(cond, &mutex->srw, timeout, 0);
    }
    if (bRet == FALSE)
        ret = GetLastError() == ERROR_TIMEOUT ? ETIMEDOUT : EINVAL;

    mp_end_hires_timers(hrt);
    return ret;
}

static inline int mp_cond_wait(mp_cond *cond, mp_mutex *mutex)
{
    return mp_cond_timedwait(cond, mutex, MP_TIME_MS_TO_NS(INFINITE));
}

static inline int mp_cond_timedwait_until(mp_cond *cond, mp_mutex *mutex, int64_t until)
{
    return mp_cond_timedwait(cond, mutex, until - mp_time_ns());
}

static inline int mp_exec_once(mp_once *once, void (*init_routine)(void))
{
    BOOL pending;

    if (!InitOnceBeginInitialize(once, 0, &pending, NULL))
        abort();

    if (pending) {
        init_routine();
        InitOnceComplete(once, 0, NULL);
    }

    return 0;
}

#define MP_THREAD_VOID unsigned __stdcall
#define MP_THREAD_RETURN() return 0

static inline int mp_thread_create(mp_thread *thread,
                                   MP_THREAD_VOID (*fun)(void *),
                                   void *__restrict arg)
{
    *thread = (HANDLE) _beginthreadex(NULL, 0, fun, arg, 0, NULL);
    return *thread ? 0 : -1;
}

static inline int mp_thread_join(mp_thread thread)
{
    DWORD ret = WaitForSingleObject(thread, INFINITE);
    if (ret != WAIT_OBJECT_0)
        return ret == WAIT_ABANDONED ? EINVAL : EDEADLK;
    CloseHandle(thread);
    return 0;
}

static inline int mp_thread_join_id(mp_thread_id id)
{
    mp_thread thread = OpenThread(SYNCHRONIZE, FALSE, id);
    if (!thread)
        return ESRCH;
    int ret = mp_thread_join(thread);
    if (ret)
        CloseHandle(thread);
    return ret;
}

static inline int mp_thread_detach(mp_thread thread)
{
    return CloseHandle(thread) ? 0 : EINVAL;
}

#define mp_thread_current_id GetCurrentThreadId
#define mp_thread_id_equal(a, b) ((a) == (b))
#define mp_thread_get_id(thread) GetThreadId(thread)

wchar_t *mp_from_utf8(void *talloc_ctx, const char *s);
static inline void mp_thread_set_name(const char *name)
{
    HRESULT (WINAPI *pSetThreadDescription)(HANDLE, PCWSTR);
#if !HAVE_UWP
    HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
    if (!kernel32)
        return;
    pSetThreadDescription = (void *) GetProcAddress(kernel32, "SetThreadDescription");
    if (!pSetThreadDescription)
        return;
#else
    WINBASEAPI HRESULT WINAPI
    SetThreadDescription(HANDLE hThread, PCWSTR lpThreadDescription);
    pSetThreadDescription = &SetThreadDescription;
#endif
    wchar_t *wname = mp_from_utf8(NULL, name);
    pSetThreadDescription(GetCurrentThread(), wname);
    talloc_free(wname);
}

static inline int64_t mp_thread_cpu_time_ns(mp_thread_id thread)
{
    (void) thread;
    return 0;
}
