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

#include <stdlib.h>

#include "timer.h"

static uint64_t raw_time_offset;

void mp_time_init(void)
{
    mp_raw_time_init();
    srand(mp_raw_time_us());
    raw_time_offset = mp_raw_time_us();
    // Arbitrary additional offset to avoid confusing relative/absolute times.
    // Also,we rule that the timer never returns 0 (so default-initialized
    // time values will be always in the past).
    raw_time_offset -= 10000000;
}

int64_t mp_time_us(void)
{
    return mp_raw_time_us() - raw_time_offset;
}

double mp_time_sec(void)
{
    return mp_time_us() / (double)(1000 * 1000);
}

int usec_sleep(int usec_delay)
{
    mp_sleep_us(usec_delay);
    return 0;
}

#if 0
#include <stdio.h>

int main(void) {
    int c = 200;
    int64_t j, r, t = 0;

    mp_time_init();

    for (int i = 0; i < c; i++) {
        const int delay = rand() / (RAND_MAX / 1e5);
        r = mp_time_us();
        mp_sleep_us(delay);
        j = (mp_time_us() - r) - delay;
        printf("sleep time: sleep=%8i err=%5i\n", delay, (int)j);
        t += j;
    }
    fprintf(stderr, "average error:\t%i\n", (int)(t / c));

    return 0;
}
#endif
