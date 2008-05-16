// Precise timer routines for LINUX  (C) LGB & A'rpi/ASTRAL

#include <unistd.h>
#ifdef __BEOS__
#define usleep(t) snooze(t)
#endif
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include "config.h"

const char *timer_name =
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
//  float s;
  gettimeofday(&tv,NULL);
//  s=tv.tv_usec;s*=0.000001;s+=tv.tv_sec;
  return tv.tv_sec * 1000000 + tv.tv_usec;
}  

// Returns current time in milliseconds
unsigned int GetTimerMS(void){
  struct timeval tv;
//  float s;
  gettimeofday(&tv,NULL);
//  s=tv.tv_usec;s*=0.000001;s+=tv.tv_sec;
  return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}  

static unsigned int RelativeTime=0;

// Returns time spent between now and last call in seconds
float GetRelativeTime(void){
unsigned int t,r;
  t=GetTimer();
//  t*=16;printf("time=%ud\n",t);
  r=t-RelativeTime;
  RelativeTime=t;
  return (float)r * 0.000001F;
}

// Initialize timer, must be called at least once at start
void InitTimer(void){
  GetRelativeTime();
}


#if 0
#include <stdio.h>
int main(void){
  float t=0;
  InitTimer();
  while(1){ t+=GetRelativeTime();printf("time= %10.6f\r",t);fflush(stdout); }
}
#endif

