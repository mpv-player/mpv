// Precise timer routines for WINDOWS

#include <windows.h>
#include <mmsystem.h>
#include "timer.h"

// Returns current time in microseconds
unsigned int GetTimer(){
  return timeGetTime() * 1000;
}

// Returns current time in milliseconds
unsigned int GetTimerMS(){
  return timeGetTime() ;
}

int usec_sleep(int usec_delay){
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
