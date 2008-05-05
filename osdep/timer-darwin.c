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
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <sys/time.h>
#include <mach/mach_time.h>

#include "config.h"
#include "mp_msg.h"
#include "timer.h"

/* global variables */
static double relative_time;
static double timebase_ratio;

const char *timer_name = "Darwin accurate";



/* the core sleep function, uses floats and is used in MPlayer G2 */
float sleep_accurate(float time_frame)
{
	uint64_t deadline = time_frame / timebase_ratio + mach_absolute_time();
	
	mach_wait_until(deadline);
	
	return (mach_absolute_time() - deadline) * timebase_ratio;
}

/* wrapper for MPlayer G1 */
int usec_sleep(int usec_delay)
{
  return sleep_accurate(usec_delay / 1e6) * 1e6;
}


/* current time in microseconds */
unsigned int GetTimer()
{
  return (unsigned int)(uint64_t)(mach_absolute_time() * timebase_ratio * 1e6);
}

/* current time in milliseconds */
unsigned int GetTimerMS()
{
  return (unsigned int)(uint64_t)(mach_absolute_time() * timebase_ratio * 1e3);
}

/* time spent between now and last call in seconds */
float GetRelativeTime()
{
  double last_time = relative_time;
  
  if (!relative_time)
    InitTimer();
  
  relative_time = mach_absolute_time() * timebase_ratio;

  return (float)(relative_time-last_time);
}

/* initialize timer, must be called at least once at start */
void InitTimer()
{
  struct mach_timebase_info timebase;

  mach_timebase_info(&timebase);
  timebase_ratio = (double)timebase.numer / (double)timebase.denom 
    * (double)1e-9;
    
  relative_time = (double)(mach_absolute_time() * timebase_ratio);
}

#if 0
#include <stdio.h>

int main(void) {
  int i,j, r, c = 200;
  long long t = 0;
  
  InitTimer();

  for (i = 0; i < c; i++) {
    const int delay = rand() / (RAND_MAX / 1e5);
    j = GetTimer();
#if 1
    r = usec_sleep(delay);
#else
    r = sleep_accurate(delay / 1e6) * 1e6;
#endif
    j = (GetTimer() - j) - delay;
    printf("sleep time:%8i %5i (%i)\n", delay, j, j - r);
    t += j - r;
  }
  fprintf(stderr, "average error:\t%lli\n", t / c);

  return 0;
}
#endif
