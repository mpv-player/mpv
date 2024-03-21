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
#include <winternl.h>
#include <ntstatus.h>
#include <sys/time.h>
#include <mmsystem.h>
#include <stdlib.h>
#include <versionhelpers.h>

#include "timer.h"

#include "config.h"

static LARGE_INTEGER perf_freq;

static int64_t hires_max = MP_TIME_MS_TO_NS(50);
static int64_t hires_res = MP_TIME_MS_TO_NS(1);

// NtSetTimerResolution allows setting the timer resolution to less than 1 ms.
// Resolutions are specified in 100-ns units.
// If Set is TRUE, set the RequestedResolution. Otherwise, return to the previous resolution.
NTSTATUS NTAPI NtSetTimerResolution(ULONG RequestedResolution, BOOLEAN Set, PULONG ActualResolution);
// Acquire the valid timer resolution range.
NTSTATUS NTAPI NtQueryTimerResolution(PULONG MinimumResolution, PULONG MaximumResolution, PULONG ActualResolution);

int64_t mp_start_hires_timers(int64_t wait_ns)
{
#if !HAVE_UWP
    ULONG actual_res = 0;
    // policy: request hires_res resolution if wait < hires_max ns
    if (wait_ns > 0 && wait_ns <= hires_max &&
        NtSetTimerResolution(hires_res / 100, TRUE, &actual_res) == STATUS_SUCCESS)
    {
        return hires_res;
    }
#endif
    return 0;
}

void mp_end_hires_timers(int64_t res_ns)
{
#if !HAVE_UWP
    ULONG actual_res = 0;
    if (res_ns > 0)
        NtSetTimerResolution(res_ns / 100, FALSE, &actual_res);
#endif
}

void mp_sleep_ns(int64_t ns)
{
    if (ns < 0)
        return;

    int64_t hrt = mp_start_hires_timers(ns);

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x2
#endif

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
    ULONG min_res, max_res, actual_res;
    if (NtQueryTimerResolution(&min_res, &max_res, &actual_res) != STATUS_SUCCESS) {
        min_res = 156250;
        max_res = 10000;
    }

    // allow (undocumented) control of all the High Res Timers parameters,
    // for easier experimentation and diagnostic of bug reports.
    const char *v;
    char *end;

    // 1..1000 ms max timetout for hires (used in "perwait" mode)
    if ((v = getenv("MPV_HRT_MAX"))) {
        int64_t hmax = strtoll(v, &end, 10);
        if (*end == '\0' && hmax >= MP_TIME_MS_TO_NS(1) && hmax <= MP_TIME_MS_TO_NS(1000))
            hires_max = hmax;
    }

    // hires resolution clamped by the available resolution range (not used in "never" mode)
    if ((v = getenv("MPV_HRT_RES"))) {
        int64_t res = strtoll(v, &end, 10);
        if (*end == '\0' && res >= max_res * INT64_C(100) && res <= min_res * INT64_C(100))
            hires_res = res;
    }

    // "always"/"never"/"perwait"  (or "auto" - same as unset)
    if (!(v = getenv("MPV_HRT")) || !strcmp(v, "auto"))
        v = IsWindows10OrGreater() ? "perwait" : "always";

    if (!strcmp(v, "perwait")) {
        // no-op, already per-wait
    } else if (!strcmp(v, "never")) {
        hires_max = 0;
    } else {  // "always" or unknown value
        mp_start_hires_timers(hires_res);
        hires_max = 0;
    }
#endif
}
