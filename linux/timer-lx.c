// Precise timer routines for LINUX  (C) LGB & A'rpi/ASTRAL

#include <unistd.h>
#include <sys/time.h>

// Returns current time in microseconds
unsigned int GetTimer(){
  struct timeval tv;
  struct timezone tz;
//  float s;
  gettimeofday(&tv,&tz);
//  s=tv.tv_usec;s*=0.000001;s+=tv.tv_sec;
  return (tv.tv_sec*1000000+tv.tv_usec);
}  

static unsigned int RelativeTime=0;

// Returns time spent between now and last call in seconds
float GetRelativeTime(){
unsigned int t,r;
  t=GetTimer();
//  t*=16;printf("time=%ud\n",t);
  r=t-RelativeTime;
  RelativeTime=t;
  return (float)r * 0.000001F;
}

// Initialize timer, must be called at least once at start
void InitTimer(){
  GetRelativeTime();
}


#if 0
void main(){
  float t=0;
  InitTimer();
  while(1){ t+=GetRelativeTime();printf("time= %10.6f\r",t);fflush(stdout); }
}
#endif

