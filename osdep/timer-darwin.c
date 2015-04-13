/*
 * Precise timer routines using Mach timing
 *
 * Copyright (c) 2003-2004, Dan Villiom Podlaski Christiansen
 *
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

#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <sys/time.h>
#include <mach/mach_time.h>

#include "config.h"
#include "common/msg.h"
#include "timer.h"

static double timebase_ratio;

void mp_sleep_us(int64_t us)
{
    uint64_t deadline = us / 1e6 / timebase_ratio + mach_absolute_time();

    mach_wait_until(deadline);
}

uint64_t mp_raw_time_us(void)
{
    return mach_absolute_time() * timebase_ratio * 1e6;
}

void mp_raw_time_init(void)
{
    struct mach_timebase_info timebase;

    mach_timebase_info(&timebase);
    timebase_ratio = (double)timebase.numer / (double)timebase.denom * 1e-9;
}
