// Precise timer routines for WINDOWS

#include <windows.h>
#include <mmsystem.h>
#include "timer.h"

const char *timer_name = "Windows native";

// Returns current time in microseconds
unsigned int GetTimer(){
  return timeGetTime() * 1000;
}

// Returns current time in milliseconds
unsigned int GetTimerMS(){
  return timeGetTime() ;
}

int usec_sleep(int usec_delay){
  // Sleep(0) won't sleep for one clocktick as the unix usleep 
  // instead it will only make the thread ready
  // it may take some time until it actually starts to run again
  if(usec_delay<1000)usec_delay=1000;  
  Sleep( usec_delay/1000);
  return 0;
}

static DWORD RelativeTime = 0;

float GetRelativeTime(){
  DWORD t, r;
  t = GetTimer();
  r = t - RelativeTime;
  RelativeTime = t;
  return (float) r *0.000001F;
}

void InitTimer(){
  GetRelativeTime();
}
