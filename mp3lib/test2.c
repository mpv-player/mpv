
//gcc test2.c -O2 -I.. -L. ../libvo/aclib.c -lMP3 -lm -o test2

#include <stdio.h>
#include <stdlib.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/soundcard.h>

#include "mp3lib/mp3.h"
#include "config.h"
#include "cpudetect.h"

static FILE* mp3file=NULL;

int mplayer_audio_read(char *buf,int size){
    return fread(buf,1,size,mp3file);
}

#define BUFFLEN 4608
static unsigned char buffer[BUFFLEN];


int main(int argc,char* argv[]){
  int len;
  int total=0;
  float length;
  int r;
  int audio_fd;
  
  mp3file=fopen((argc>1)?argv[1]:"test.mp3","rb");
  if(!mp3file){  printf("file not found\n");  exit(1); }
  
  GetCpuCaps(&gCpuCaps);

  // MPEG Audio:
#ifdef USE_FAKE_MONO
  MP3_Init(0);
#else
  MP3_Init();
#endif
  MP3_samplerate=MP3_channels=0;
  len=MP3_DecodeFrame(buffer,-1);
  
  audio_fd=open("/dev/dsp", O_WRONLY);
  if(audio_fd<0){  printf("Can't open audio device\n");exit(1); }
  r=AFMT_S16_LE;ioctl (audio_fd, SNDCTL_DSP_SETFMT, &r);
  r=MP3_channels-1;ioctl (audio_fd, SNDCTL_DSP_STEREO, &r);
  r=MP3_samplerate;ioctl (audio_fd, SNDCTL_DSP_SPEED, &r);
  printf("audio_setup: using %d Hz samplerate (requested: %d)\n",r,MP3_samplerate);
  
  while(1){
      int len2;
      if(len==0) len=MP3_DecodeFrame(buffer,-1);
      if(len<=0) break; // EOF
      
      // play it
      len2=write(audio_fd,buffer,len);
      if(len2<0) break; // ERROR?
      len-=len2; total+=len2;
      if(len>0){
          // this shouldn't happen...
          memcpy(buffer,buffer+len2,len);
          putchar('!');fflush(stdout);
      }
  }
  
  fclose(mp3file);
  
}
