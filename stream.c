
#include <stdio.h>
#include <stdlib.h>

#include <sys/ioctl.h>
#include <unistd.h>

//#include <sys/types.h>
//#include <sys/stat.h>
//#include <fcntl.h>

#ifdef __FreeBSD__
#include <sys/cdio.h>
#include <sys/cdrio.h>
#else
#ifdef __sun
#include <sys/cdio.h>
#else
#include <linux/cdrom.h>
#endif
#endif

#include "stream.h"

extern int verbose; // defined in mplayer.c

#ifdef __FreeBSD__
#warning "VCD support under FreeBSD not implemented yet"
#include "vcd_read_fbsd.c" 
#else
#include "vcd_read.c"
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
  default: len=0;
  }
  if(len<=0){ s->eof=1; s->buf_pos=s->buf_len=0; return 0; }
  s->buf_pos=0;
  s->buf_len=len;
  s->pos+=len;
//  printf("[%d]",len);fflush(stdout);
  return len;
}

int stream_seek_long(stream_t *s,unsigned int pos){
unsigned int newpos;

//  if(verbose>=3) printf("seek to 0x%X\n",pos);

if(verbose>=3){
  printf("s->pos=%X  newpos=%X  new_bufpos=%X  buflen=%X  \n",
    (unsigned int)s->pos,newpos,pos,s->buf_len);
}

  s->buf_pos=s->buf_len=0;

  switch(s->type){
  case STREAMTYPE_FILE:
  case STREAMTYPE_STREAM:
    newpos=pos&(~(STREAM_BUFFER_SIZE-1));break;
  case STREAMTYPE_VCD:
    newpos=(pos/VCD_SECTOR_DATA)*VCD_SECTOR_DATA;break;
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
  if(verbose) printf("stream_seek: WARNING! Can't seek to 0x%X !\n",pos+newpos);
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


