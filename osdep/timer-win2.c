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
#include <sys/time.h>
#include <mmsystem.h>
#include <stdlib.h>
#include <versionhelpers.h>

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

void mp_sleep_us(int64_t us)
{
    if (us < 0)
        return;
    // Sleep(0) won't sleep for one clocktick as the unix usleep
    // instead it will only make the thread ready
    // it may take some time until it actually starts to run again
    if (us < 1000)
        us = 1000;
    int hrt = mp_start_hires_timers(us / 1000);
    Sleep(us / 1000);
    mp_end_hires_timers(hrt);
}

uint64_t mp_raw_time_us(void)
{
    LARGE_INTEGER perf_count;
    QueryPerformanceCounter(&perf_count);

    // Convert QPC units (1/perf_freq seconds) to microseconds. This will work
    // without overflow because the QPC value is guaranteed not to roll-over
    // within 100 years, so perf_freq must be less than 2.9*10^9.
    return perf_count.QuadPart / perf_freq.QuadPart * 1000000 +
        perf_count.QuadPart % perf_freq.QuadPart * 1000000 / perf_freq.QuadPart;
}

void mp_raw_time_init(void)
{
    QueryPerformanceFrequency(&perf_freq);

#if !HAVE_UWP
    // allow (undocumented) control of all the High Res Timers parameters,
    // for easier experimentation and diagnostic of bug reports.
    const char *v;

    // 1..1000 ms max timetout for hires (used in "perwait" mode)
    if ((v = getenv("MPV_HRT_MAX"))) {
        int hmax = atoi(v);
        if (hmax >= 1 && hmax <= 1000)
            hires_max = hmax;
    }

    // 1..15 ms hires resolution (not used in "never" mode)
    if ((v = getenv("MPV_HRT_RES"))) {
        int res = atoi(v);
        if (res >= 1 && res <= 15)
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
        hires_max = 0;
        timeBeginPeriod(hires_res);
    }
#endif
}
