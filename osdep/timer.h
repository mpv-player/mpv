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

#ifndef MPLAYER_TIMER_H
#define MPLAYER_TIMER_H

#include <inttypes.h>

// Initialize timer, must be called at least once at start.
void mp_time_init(void);

// Return time in nanoseconds. Never wraps. Never returns negative values.
int64_t mp_time_ns(void);

// Return time in seconds. Can have down to 1 nanosecond resolution, but will
// be much worse when casted to float.
double mp_time_sec(void);

// Provided by OS specific functions (timer-linux.c)
void mp_raw_time_init(void);
// ensure this doesn't return 0
uint64_t mp_raw_time_ns(void);

// Sleep in nanoseconds.
void mp_sleep_ns(int64_t ns);

#ifdef _WIN32
// returns: timer resolution in ns if needed and started successfully, else 0
int64_t mp_start_hires_timers(int64_t wait_ns);

// call unconditionally with the return value of mp_start_hires_timers
void mp_end_hires_timers(int64_t resolution_ns);
#endif  /* _WIN32 */

// Converts time units to nanoseconds (int64_t)
#define MP_TIME_S_TO_NS(s) ((s) * INT64_C(1000000000))
#define MP_TIME_MS_TO_NS(ms) ((ms) * INT64_C(1000000))
#define MP_TIME_US_TO_NS(us) ((us) * INT64_C(1000))

// Converts nanoseconds to specified time unit (double)
#define MP_TIME_NS_TO_S(ns) ((ns) / (double)1000000000)
#define MP_TIME_NS_TO_MS(ns) ((ns) / (double)1000000)
#define MP_TIME_NS_TO_US(ns) ((ns) / (double)1000)

// Add a time in seconds to the given time in nanoseconds, and return it.
// Takes care of possible overflows. Never returns a negative or 0 time.
int64_t mp_time_ns_add(int64_t time_ns, double timeout_sec);

#endif /* MPLAYER_TIMER_H */
