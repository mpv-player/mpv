/*
 * Semi-precise timer routines using CoreFoundation
 *
 * (C) 2003 Dan Christiansen
 *
 * Released into the public domain.
 */

#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include "../config.h"

#ifdef MACOSX
# include <CoreFoundation/CFRunLoop.h>
#endif

/* Rather than using CF timers, we simply store the absolute time 
 * CFAbsoluteTime == double */
static CFAbsoluteTime relative_time;

int usec_sleep(int usec_delay)
{
  CFRunLoopRunInMode(kCFRunLoopDefaultMode,  usec_delay / 1000000.0, false);
}


// Returns current time in microseconds
unsigned int GetTimer(){
  return (unsigned int)(CFAbsoluteTimeGetCurrent() * 1000000);
}  

// Returns current time in milliseconds
unsigned int GetTimerMS(){
  return (unsigned int)(CFAbsoluteTimeGetCurrent() * 1000);
}

// Returns time spent between now and last call in seconds
float GetRelativeTime(){
  CFAbsoluteTime last_time = relative_time;
  relative_time = CFAbsoluteTimeGetCurrent();
  return (float)(relative_time - last_time);
}

// Initialize timer, must be called at least once at start
void InitTimer(){
  GetRelativeTime();
}

#if 0
int main() {
  int i;

  for (i = 0; i < 20; i++) {
    printf("CF relative time:\t%f\n", GetRelativeTime());
    usec_sleep(1000000);
    printf("usleep relative time:\t%f\n", GetRelativeTime());
    usleep(1000000);
  }
}
#endif


