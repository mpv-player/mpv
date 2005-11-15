
#define DUMP_PCM

// gcc test.c -I.. -L. -lMP3 -lm -o test1 -O4

#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#include <sys/time.h>

#include "mp3lib/mp3.h"
#include "config.h"

#include "cpudetect.h"
extern CpuCaps gCpuCaps;

static inline unsigned int GetTimer(){
  struct timeval tv;
  struct timezone tz;
//  float s;
  gettimeofday(&tv,&tz);
//  s=tv.tv_usec;s*=0.000001;s+=tv.tv_sec;
  return (tv.tv_sec*1000000+tv.tv_usec);
}  

static FILE* mp3file=NULL;

int mplayer_audio_read(char *buf,int size){
    return fread(buf,1,size,mp3file);
}

#define BUFFLEN 4608
static unsigned char buffer[BUFFLEN];

int main(int argc,char* argv[]){
  int len;
  int total=0;
  unsigned int time1;
  float length;
#ifdef DUMP_PCM
  FILE *f=NULL;
  f=fopen("test.pcm","wb");
#endif
  
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
  
  time1=GetTimer();
  while((len=MP3_DecodeFrame(buffer,-1))>0 && total<2000000){
      total+=len;
      // play it
#ifdef DUMP_PCM
      fwrite(buffer,len,1,f);
#endif
      //putchar('.');fflush(stdout);
  }
  time1=GetTimer()-time1;
  length=(float)total/(float)(MP3_samplerate*MP3_channels*2);
  printf("\nDecoding time: %8.6f\n",(float)time1*0.000001f);
  printf("Uncompressed size: %d bytes  (%8.3f secs)\n",total,length);
  printf("CPU usage at normal playback: %5.2f %%\n",time1*0.0001f/length);
  
  fclose(mp3file);
  
}
