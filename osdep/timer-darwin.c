/*
 * Precise timer routines using Mach kernel-space timing.
 *
 * It reports to be accurate by ~20us, unless the task is preempted. 
 *
 * (C) 2003 Dan Christiansen
 *
 * Released into the public domain.
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#include <mach/mach_time.h>
#include <mach/mach.h>
#include <mach/clock.h>

#include "../config.h"
#include "../mp_msg.h"
#include "timer.h"

/* Utility macros for mach_timespec_t - it uses nsec rather than usec */

/* returns time from t1 to t2, in seconds (as float) */
#define diff_time(t1, t2)						\
  (((t2).tv_sec - (t1).tv_sec) +					\
   ((t2).tv_nsec - (t1).tv_nsec) / 1e9)

/* returns time from t1 to t2, in microseconds (as integer) */
#define udiff_time(t1, t2)						\
  (((t2).tv_sec - (t1).tv_sec) * 1000000 +				\
   ((t2).tv_nsec - (t1).tv_nsec) / 1000)

/* returns float value of t, in seconds */
#define time_to_float(t)						\
  ((t).tv_sec + (t).tv_nsec / 1e9)

/* returns integer value of t, in microseconds */
#define time_to_usec(t)							\
  ((t).tv_sec * 1000000 + (t).tv_nsec / 1000)

/* sets ts to the value of f, in seconds */
#define float_to_time(f, ts)						\
  do {									\
    (ts).tv_sec = (unsigned int)(f);					\
    (ts).tv_nsec = (int)(((f) - (ts).sec) / 1000000000.0);		\
  } while (0)

/* sets ts to the value of i, in microseconds */
#define usec_to_time(i, ts)						\
  do {									\
    (ts).tv_sec = (i) / 1000000;					\
    (ts).tv_nsec = (i) % 1000000 * 1000;				\
  } while (0)

#define time_uadd(i, ts)						\
  do {									\
    (ts).tv_sec += (i) / 1000000;					\
    (ts).tv_nsec += (i) % 1000000 * 1000;				\
    while ((ts).tv_nsec > 1000000000) {					\
      (ts).tv_sec++;							\
      (ts).tv_nsec -= 1000000000;					\
    }									\
  } while (0)


/* global variables */
static double relative_time, startup_time;
static double timebase_ratio;
static mach_port_t clock_port;


/* sleep usec_delay microseconds */
int usec_sleep(int usec_delay)
{
#if 0
  mach_timespec_t start_time, end_time;

  clock_get_time(clock_port, &start_time);

  end_time = start_time;
  time_uadd(usec_delay, end_time);

  clock_sleep(clock_port, TIME_ABSOLUTE, end_time, NULL);

  clock_get_time(clock_port, &end_time);

  return usec_delay - udiff_time(start_time, end_time);
#else
  usleep(usec_delay);
#endif
}


/* Returns current time in microseconds */
unsigned int GetTimer()
{
  return (unsigned int)((mach_absolute_time() * timebase_ratio - startup_time)
			* 1e6);
}  

/* Returns current time in milliseconds */
unsigned int GetTimerMS()
{
  return (unsigned int)(GetTimer() / 1000);
}

/* Returns time spent between now and last call in seconds */
float GetRelativeTime()
{
  double last_time;

  last_time = relative_time;
  
  relative_time = mach_absolute_time() * timebase_ratio;

  return (float)(relative_time-last_time);
}

/* Initialize timer, must be called at least once at start */
void InitTimer()
{
  struct mach_timebase_info timebase;

  /* get base for mach_absolute_time() */
  mach_timebase_info(&timebase);
  timebase_ratio = (double)timebase.numer / (double)timebase.denom 
    * (double)1e-9;
  
  /* get mach port for the clock */
  host_get_clock_service(mach_host_self(), REALTIME_CLOCK, &clock_port);
  
  /* prepare for GetRelativeTime() */
  relative_time = startup_time = 
    (double)(mach_absolute_time() * timebase_ratio);
}


#if 0
int main()
{
  const long delay = 0.001*1e6;
  const unsigned short attempts = 100;
  int i,j[attempts],t[attempts],r[attempts];
  double sqtotal;
  double total;
  
  InitTimer();

  for (i = 0; i < attempts; i++) {
    t[i] = j[i] = GetTimer();
    r[i] = usec_sleep(delay);
      j[i] = delay-(GetTimer() - j[i]);
      fflush(stdout);
  }

  for (i = 0; i < attempts; i++) {
    sqtotal += j[i]*j[i];
    total += j[i];
    printf("%2i=%0.06g  \tr: %9i\tj: %9i\tr - j:%9i\n",
	   i, t[i] / 1e6, r[i], j[i], r[i] - j[i]);
  }
  
  printf("attempts: %i\ttotal=%g\trms=%g\tavg=%g\n", attempts, total, 
	 sqrt(sqtotal/attempts),total/attempts);
  
  return 0;
}
#endif
