// Precise timer routines for LINUX  (C) LGB & A'rpi/ASTRAL

#include <unistd.h>
#ifdef __BEOS__
#define usleep(t) snooze(t)
#endif
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include "config.h"

const char timer_name[] =
#ifdef HAVE_NANOSLEEP
  "nanosleep()";
#else
  "usleep()";
#endif

int usec_sleep(int usec_delay)
{
#ifdef HAVE_NANOSLEEP
    struct timespec ts;
    ts.tv_sec  =  usec_delay / 1000000;
    ts.tv_nsec = (usec_delay % 1000000) * 1000;
    return nanosleep(&ts, NULL);
#else
    return usleep(usec_delay);
#endif
}

// Returns current time in microseconds
unsigned int GetTimer(void){
  struct timeval tv;
  gettimeofday(&tv,NULL);
  return tv.tv_sec * 1000000 + tv.tv_usec;
}  

// Returns current time in milliseconds
unsigned int GetTimerMS(void){
  struct timeval tv;
  gettimeofday(&tv,NULL);
  return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}  

// Initialize timer, must be called at least once at start
void InitTimer(void){
}
