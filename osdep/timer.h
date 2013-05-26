/*
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
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

#endif /* MPLAYER_TIMER_H */
