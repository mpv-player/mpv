/*
 * Precise timer routines using Mach timing
 *
 * Copyright (c) 2003-2004, Dan Villiom Podlaski Christiansen
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
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
