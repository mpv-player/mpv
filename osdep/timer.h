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

#ifndef MPLAYER_TIMER_H
#define MPLAYER_TIMER_H

#include <inttypes.h>

// Initialize timer, must be called at least once at start.
void mp_time_init(void);

// Return time in microseconds. Never wraps. Never returns 0 or negative values.
int64_t mp_time_us(void);

// Return time in seconds. Can have down to 1 microsecond resolution, but will
// be much worse when casted to float.
double mp_time_sec(void);

// Provided by OS specific functions (timer-linux.c)
void mp_raw_time_init(void);
uint64_t mp_raw_time_us(void);

// Sleep in microseconds.
void mp_sleep_us(int64_t us);

#define MP_START_TIME 10000000

// Return the amount of time that has passed since the last call, in
// microseconds. *t is used to calculate the time that has passed by storing
// the current time in it. If *t is 0, the call will return 0. (So that the
// first call will return 0, instead of the absolute current time.)
int64_t mp_time_relative_us(int64_t *t);

// Add a time in seconds to the given time in microseconds, and return it.
// Takes care of possible overflows. Never returns a negative or 0 time.
int64_t mp_add_timeout(int64_t time_us, double timeout_sec);

// Convert the mp time in microseconds to a timespec using CLOCK_REALTIME.
struct timespec mp_time_us_to_timespec(int64_t time_us);

// Convert the relative timeout in seconds to a timespec.
// The timespec is absolute, using CLOCK_REALTIME.
struct timespec mp_rel_time_to_timespec(double timeout_sec);

#endif /* MPLAYER_TIMER_H */
