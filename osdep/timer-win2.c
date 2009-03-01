/*
 * precise timer routines for Windows
 *
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <windows.h>
#include <mmsystem.h>
#include "timer.h"

const char *timer_name = "Windows native";

// Returns current time in microseconds
unsigned int GetTimer(void)
{
  return timeGetTime() * 1000;
}

// Returns current time in milliseconds
unsigned int GetTimerMS(void)
{
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

float GetRelativeTime(void)
{
  DWORD t, r;
  t = GetTimer();
  r = t - RelativeTime;
  RelativeTime = t;
  return (float) r *0.000001F;
}

void InitTimer(void)
{
  GetRelativeTime();
}
