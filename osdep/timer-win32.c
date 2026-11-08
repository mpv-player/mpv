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

#include <windows.h>
#include <mmsystem.h>
#include <stdlib.h>
#include <versionhelpers.h>

#include <intrin.h>

#include "threads.h"
#include "timer.h"

#include "config.h"

static LARGE_INTEGER perf_freq;

// ms values
static int hires_max = 50;
static int hires_res = 1;

int mp_start_hires_timers(int wait_ms)
{
#if !HAVE_UWP
    // policy: request hires_res ms resolution if wait < hires_max ms
    if (wait_ms > 0 && wait_ms <= hires_max &&
        timeBeginPeriod(hires_res) == TIMERR_NOERROR)
    {
        return hires_res;
    }
#endif
    return 0;
}

void mp_end_hires_timers(int res_ms)
{
#if !HAVE_UWP
    if (res_ms > 0)
        timeEndPeriod(res_ms);
#endif
}

void mp_sleep_ns(int64_t ns)
{
    if (ns < 0)
        return;

    int hrt = mp_start_hires_timers(ns < 1e6 ? 1 : ns / 1e6);
    HANDLE timer = CreateWaitableTimerEx(NULL, NULL,
                                         CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                         TIMER_ALL_ACCESS);

    // CREATE_WAITABLE_TIMER_HIGH_RESOLUTION is supported in Windows 10 1803+,
    // retry without it.
    if (!timer)
        timer = CreateWaitableTimerEx(NULL, NULL, 0, TIMER_ALL_ACCESS);

    if (!timer)
        goto end;

    // Time is expected in 100 nanosecond intervals.
    // Negative values indicate relative time.
    LARGE_INTEGER time = (LARGE_INTEGER){ .QuadPart = -(ns / 100) };
    if (!SetWaitableTimer(timer, &time, 0, NULL, NULL, 0))
        goto end;

    if (WaitForSingleObject(timer, INFINITE) != WAIT_OBJECT_0)
        goto end;

end:
    if (timer)
        CloseHandle(timer);
    mp_end_hires_timers(hrt);
}

uint64_t mp_raw_time_ns(void)
{
    LARGE_INTEGER perf_count;
    QueryPerformanceCounter(&perf_count);

    // Convert QPC units (1/perf_freq seconds) to nanoseconds. This will work
    // without overflow because the QPC value is guaranteed not to roll-over
    // within 100 years, so perf_freq must be less than 2.9*10^9.
    return perf_count.QuadPart / perf_freq.QuadPart * UINT64_C(1000000000) +
        perf_count.QuadPart % perf_freq.QuadPart * UINT64_C(1000000000) / perf_freq.QuadPart;
}

void mp_raw_time_init(void)
{
    QueryPerformanceFrequency(&perf_freq);

#if !HAVE_UWP
    // allow (undocumented) control of all the High Res Timers parameters,
    // for easier experimentation and diagnostic of bug reports.
    const char *v;
    char *end;

    // 1..1000 ms max timetout for hires in ns (used in "perwait" mode)
    if ((v = getenv("MPV_HRT_MAX"))) {
        int64_t hmax = strtoll(v, &end, 10);
        if (*end == '\0' && hmax >= MP_TIME_MS_TO_NS(1) && hmax <= MP_TIME_MS_TO_NS(1000))
            hires_max = hmax / MP_TIME_MS_TO_NS(1);
    }

    // 1..15 ms hires resolution in ns, truncated to whole ms (not used in
    // "never" mode)
    if ((v = getenv("MPV_HRT_RES"))) {
        int64_t res = strtoll(v, &end, 10);
        if (*end == '\0' && res >= MP_TIME_MS_TO_NS(1) && res < MP_TIME_MS_TO_NS(16))
            hires_res = res / MP_TIME_MS_TO_NS(1);
    }

    // "always"/"never"/"perwait"  (or "auto" - same as unset)
    if (!(v = getenv("MPV_HRT")) || !strcmp(v, "auto"))
        v = IsWindows10OrGreater() ? "perwait" : "always";

    if (!strcmp(v, "perwait")) {
        // no-op, already per-wait
    } else if (!strcmp(v, "never")) {
        hires_max = 0;
    } else {  // "always" or unknown value
        hires_max = 0;
        timeBeginPeriod(hires_res);
    }
#endif
}

#if !HAVE_UWP && !defined(__aarch64__) && !defined(_M_ARM64)

static inline bool cpu_tsc_invariant(void)
{
    int cpu_info[4] = {0};
    __cpuid(cpu_info, 0x80000000);

    // Check if the extended CPUID leaf 0x80000007 is supported by the CPU
    if (cpu_info[0] >= 0x80000007) {
        __cpuid(cpu_info, 0x80000007);
        // Check if the 8th bit in EDX is set (TSC invariant)
        return (cpu_info[3] & (1 << 8)) != 0;
    }
    return false;
}

static double cpu_tsc_freq_ns(void)
{
    static double ticks_per_ns = 0;

    if (ticks_per_ns != 0)
        return ticks_per_ns;

    static mp_static_mutex mutex = MP_STATIC_MUTEX_INITIALIZER;
    mp_mutex_lock(&mutex);

    if (!cpu_tsc_invariant()) {
        ticks_per_ns = -1;
        goto done;
    }

    // Try to avoid context switches when comparing QPC to TSC.
    int previous_priority = GetThreadPriority(GetCurrentThread());
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

    static uint64_t initial = UINT64_C(-1);
    static uint64_t initial_time;

    // Remember the initial TSC and QPC values.
    if (initial == UINT64_C(-1)) {
        initial = __rdtsc();
        initial_time = mp_raw_time_ns();
    }

    uint64_t now = __rdtsc();
    uint64_t now_time = mp_raw_time_ns();

    SetThreadPriority(GetCurrentThread(), previous_priority);

    // If it has overflowed, try again.
    if (initial > now) {
        initial = UINT64_C(-1);
        goto done;
    }

    // Compute the frequency of the TSC between the initial time and now. To ensure
    // the accuracy of the result, we require at least 100 ms to have elapsed.
    uint64_t elapsed_time = now_time - initial_time;
    if (elapsed_time >= MP_TIME_MS_TO_NS(100))
        ticks_per_ns = (double)(now - initial) / elapsed_time;

done:
    mp_mutex_unlock(&mutex);
    return ticks_per_ns;
}

#endif

int64_t mp_thread_cpu_time_ns(mp_thread_id thread_id)
{
    int64_t thread_time = -1;

    HANDLE thread = OpenThread(THREAD_QUERY_INFORMATION, FALSE, thread_id);
    if (!thread)
        return thread_time;

#if !HAVE_UWP && !defined(__aarch64__) && !defined(_M_ARM64)

    // GetThreadTimes() is exactly what we want, but even though it returns
    // 100ns units, its real resolution is much worse, ~15.6ms in practice.
    // This is a known and documented issue at this point.
    // <https://devblogs.microsoft.com/oldnewthing/20161021-00/?p=94565>

    // Instead of using GetThreadTimes(), we use QueryThreadCycleTime() to get the
    // thread's clock cycle count and convert the result to elapsed time.
    // This works only on CPUs with an invariant TSC, which is the case for
    // x86_64/x86, but not for ARM.

    double tsc_ticks_per_ns = cpu_tsc_freq_ns();

    // If the TSC frequency is still being measured, return unsupported time
    // instead of falling back to GetThreadTimes() to avoid discontinuity in
    // thread times. This will only occur for the initial calls to this function.
    if (tsc_ticks_per_ns == 0)
        goto done;

    if (tsc_ticks_per_ns > 0) {
        ULONG64 cycle_time;
        if (QueryThreadCycleTime(thread, &cycle_time)) {
            thread_time = cycle_time / tsc_ticks_per_ns;
            goto done;
        }
    }

#endif

    FILETIME creation_time, exit_time, kernel_time, user_time;
    if (!GetThreadTimes(thread, &creation_time, &exit_time, &kernel_time, &user_time))
        goto done;

    ULARGE_INTEGER kernel_time_q;
    kernel_time_q.LowPart = kernel_time.dwLowDateTime;
    kernel_time_q.HighPart = kernel_time.dwHighDateTime;

    ULARGE_INTEGER user_time_q;
    user_time_q.LowPart = user_time.dwLowDateTime;
    user_time_q.HighPart = user_time.dwHighDateTime;

    thread_time = (kernel_time_q.QuadPart + user_time_q.QuadPart) * 100;

done:
    CloseHandle(thread);

    return thread_time;
}
