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

#include <stdlib.h>
#include <time.h>
#include <limits.h>
#include <assert.h>

#include "common/common.h"
#include "common/msg.h"
#include "threads.h"
#include "timer.h"

static uint64_t raw_time_offset;
static mp_once timer_init_once = MP_STATIC_ONCE_INITIALIZER;

static void do_timer_init(void)
{
    mp_raw_time_init();
    raw_time_offset = mp_raw_time_ns();
    assert(raw_time_offset > 0);
}

void mp_time_init(void)
{
    mp_exec_once(&timer_init_once, do_timer_init);
}

int64_t mp_time_ns(void)
{
    return mp_time_ns_from_raw_time(mp_raw_time_ns());
}

int64_t mp_time_ns_from_raw_time(uint64_t raw_time)
{
    return raw_time - raw_time_offset;
}

double mp_time_sec(void)
{
    return mp_time_ns() / 1e9;
}

int64_t mp_time_ns_add(int64_t time_ns, double timeout_sec)
{
    assert(time_ns > 0); // mp_time_ns() returns strictly positive values
    double t = MPCLAMP(timeout_sec * 1e9, -0x1p63, 0x1p63);
    int64_t ti = t == 0x1p63 ? INT64_MAX : (int64_t)t;
    if (ti > INT64_MAX - time_ns)
        return INT64_MAX;
    if (ti <= -time_ns)
        return 1;
    return time_ns + ti;
}
