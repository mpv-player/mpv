
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include "config.h"

#include "stream.h"

extern int verbose; // defined in mplayer.c

#ifdef __FreeBSD__
#include "vcd_read_fbsd.c" 
#else
#include "vcd_read.c"
#endif

#ifdef USE_DVDREAD
int dvd_read_sector(void* d,void* p2);
void dvd_seek(void* d,off_t pos);
#endif

//=================== STREAMER =========================

int stream_fill_buffer(stream_t *s){
  int len;
  if(s->eof){ s->buf_pos=s->buf_len=0; return 0; }
  switch(s->type){
  case STREAMTYPE_FILE:
  case STREAMTYPE_STREAM:
    len=read(s->fd,s->buffer,STREAM_BUFFER_SIZE);break;
  case STREAMTYPE_VCD:
#ifdef VCD_CACHE
    len=vcd_cache_read(s->fd,s->buffer);break;
#else
    len=vcd_read(s->fd,s->buffer);break;
#endif
#ifdef USE_DVDREAD
  case STREAMTYPE_DVD: {
    off_t pos=dvd_read_sector(s->priv,s->buffer);
    if(pos>=0){
	len=2048; // full sector
	s->pos=2048*pos-len;
    } else len=-1; // error
    break;
  }
#endif
  default: len=0;
  }
  if(len<=0){ s->eof=1; s->buf_pos=s->buf_len=0; return 0; }
  s->buf_pos=0;
  s->buf_len=len;
  s->pos+=len;
//  printf("[%d]",len);fflush(stdout);
  return len;
}

int stream_seek_long(stream_t *s,off_t pos){
off_t newpos;

//  if(verbose>=3) printf("seek to 0x%X\n",(unsigned int)pos);

if(verbose>=3){
#ifdef _LARGEFILE_SOURCE
  printf("s->pos=%llX  newpos=%llX  new_bufpos=%llX  buflen=%X  \n",
    (long long)s->pos,(long long)newpos,(long long)pos,s->buf_len);
#else
  printf("s->pos=%X  newpos=%X  new_bufpos=%X  buflen=%X  \n",
    (unsigned int)s->pos,newpos,pos,s->buf_len);
#endif
}

  s->buf_pos=s->buf_len=0;

  switch(s->type){
  case STREAMTYPE_FILE:
  case STREAMTYPE_STREAM:
#ifdef _LARGEFILE_SOURCE
    newpos=pos&(~((long long)STREAM_BUFFER_SIZE-1));break;
#else
    newpos=pos&(~(STREAM_BUFFER_SIZE-1));break;
#endif
  case STREAMTYPE_VCD:
    newpos=(pos/VCD_SECTOR_DATA)*VCD_SECTOR_DATA;break;
  case STREAMTYPE_DVD:
    newpos=pos/2048; newpos*=2048; break;
  }

  pos-=newpos;

if(newpos==0 || newpos!=s->pos){
  switch(s->type){
  case STREAMTYPE_FILE:
    s->pos=newpos; // real seek
    if(lseek(s->fd,s->pos,SEEK_SET)<0) s->eof=1;
    break;
  case STREAMTYPE_VCD:
    s->pos=newpos; // real seek
#ifdef VCD_CACHE
    vcd_cache_seek(s->pos/VCD_SECTOR_DATA);
#else
    vcd_set_msf(s->pos/VCD_SECTOR_DATA);
#endif
    break;
#ifdef USE_DVDREAD
  case STREAMTYPE_DVD:
    s->pos=newpos; // real seek
    dvd_seek(s->priv,s->pos/2048);
    break;
#endif
  case STREAMTYPE_STREAM:
    //s->pos=newpos; // real seek
    if(newpos<s->pos){
      printf("Cannot seek backward in linear streams!\n");
      return 1;
    }
    while(s->pos<newpos){
      if(stream_fill_buffer(s)<=0) break; // EOF
    }
    break;
  }
//   putchar('.');fflush(stdout);
//} else {
//   putchar('%');fflush(stdout);
}

  stream_fill_buffer(s);
  if(pos>=0 && pos<s->buf_len){
    s->buf_pos=pos; // byte position in sector
    return 1;
  }
#ifdef _LARGEFILE_SOURCE
  if(verbose) printf("stream_seek: WARNING! Can't seek to 0x%llX !\n",(long long)(pos+newpos));
#else
  if(verbose) printf("stream_seek: WARNING! Can't seek to 0x%X !\n",(pos+newpos));
#endif
  return 0;
}


void stream_reset(stream_t *s){
  if(s->eof){
    s->pos=0; //ftell(f);
//    s->buf_pos=s->buf_len=0;
    s->eof=0;
  }
  //stream_seek(s,0);
}

stream_t* new_stream(int fd,int type){
  stream_t *s=malloc(sizeof(stream_t));
  s->fd=fd;
  s->type=type;
  s->buf_pos=s->buf_len=0;
  s->start_pos=s->end_pos=0;
  stream_reset(s);
  return s;
}

void free_stream(stream_t *s){
  free(s);
}


