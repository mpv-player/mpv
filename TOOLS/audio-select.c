// This small util discovers your audio driver's behaviour

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/soundcard.h>

#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
                     
#define OUTBURST 128

// Returns current time in microseconds
unsigned int GetTimer(){
  struct timeval tv;
  struct timezone tz;
//  float s;
  gettimeofday(&tv,&tz);
//  s=tv.tv_usec;s*=0.000001;s+=tv.tv_sec;
  return (tv.tv_sec*1000000+tv.tv_usec);
}  

static unsigned char a_buffer[OUTBURST];

int main(){
  int audio_buffer_size;
  int r;
  int xxx=1024;
  int audio_fd;
  char *dsp="/dev/dsp";
  unsigned int t1,t2,t3;

  audio_fd=open(dsp, O_WRONLY);
  if(audio_fd<0){
    printf("Can't open audio device %s\n",dsp);
    return 1;
  }
  
  r=AFMT_S16_LE;ioctl (audio_fd, SNDCTL_DSP_SETFMT, &r);
  r=1; ioctl (audio_fd, SNDCTL_DSP_STEREO, &r);
  r=44100; if(ioctl (audio_fd, SNDCTL_DSP_SPEED, &r)==-1)
      printf("audio_setup: your card doesn't support %d Hz samplerate\n",r);

  t3=t1=GetTimer();

while(xxx>0){
    audio_buffer_size=0;
    while(audio_buffer_size<0x100000){
      fd_set rfds;
      struct timeval tv;
      FD_ZERO(&rfds); FD_SET(audio_fd,&rfds);
      tv.tv_sec=0; tv.tv_usec = 0;
      if(!select(audio_fd+1, NULL, &rfds, NULL, &tv)) break;
      r=write(audio_fd,a_buffer,OUTBURST);
      if(r<0) printf("Error writting to device\n"); else
      if(r==0) printf("EOF writting to device???\n"); else
      audio_buffer_size+=r;
    }
    t2=GetTimer();
    if(audio_buffer_size>0){
      printf("%6d bytes written in %5d us (wait %5d us)\n",audio_buffer_size,t2-t1,t1-t3);
      --xxx;
      t3=t2;
    }
    t1=t2;
}

close(audio_fd);

return 0;
}

