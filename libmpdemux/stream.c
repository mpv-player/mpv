
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <signal.h>

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"

#include "stream.h"
#include "demuxer.h"

extern int verbose; // defined in mplayer.c

#ifdef HAVE_VCD

#ifdef __FreeBSD__
#include "vcd_read_fbsd.h" 
#elif defined(__NetBSD__)
#include "vcd_read_nbsd.h" 
#else
#include "vcd_read.h"
#endif

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
  case STREAMTYPE_PLAYLIST:
#ifdef STREAMING
    if( s->streaming_ctrl!=NULL ) {
	    len=s->streaming_ctrl->streaming_read(s->fd,s->buffer,STREAM_BUFFER_SIZE, s->streaming_ctrl);break;
    } else {
      len=read(s->fd,s->buffer,STREAM_BUFFER_SIZE);break;
    }
#else
    len=read(s->fd,s->buffer,STREAM_BUFFER_SIZE);break;
#endif
#ifdef HAVE_VCD
  case STREAMTYPE_VCD:
#ifdef VCD_CACHE
    len=vcd_cache_read(s->fd,s->buffer);break;
#else
    len=vcd_read(s->fd,s->buffer);break;
#endif
#endif
#ifdef USE_DVDNAV
  case STREAMTYPE_DVDNAV: {
    dvdnav_stream_read((dvdnav_priv_t*)s->priv,s->buffer,&len);
    if (len==0) return 0; // this was an event, so repeat the read
    break;
  }
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
#ifdef USE_TV
  case STREAMTYPE_TV:
  {
    len = 0;
    break;
  }
#endif
  case STREAMTYPE_DS:
    len = demux_read_data((demux_stream_t*)s->priv,s->buffer,STREAM_BUFFER_SIZE);
    break;
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

//  if(verbose>=3) printf("seek_long to 0x%X\n",(unsigned int)pos);

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

if(verbose>=3){
#ifdef _LARGEFILE_SOURCE
  printf("s->pos=%llX  newpos=%llX  new_bufpos=%llX  buflen=%X  \n",
    (long long)s->pos,(long long)newpos,(long long)pos,s->buf_len);
#else
  printf("s->pos=%X  newpos=%X  new_bufpos=%X  buflen=%X  \n",
    (unsigned int)s->pos,newpos,pos,s->buf_len);
#endif
}

  pos-=newpos;

if(newpos==0 || newpos!=s->pos){
  switch(s->type){
  case STREAMTYPE_FILE:
    s->pos=newpos; // real seek
    if(lseek(s->fd,s->pos,SEEK_SET)<0) s->eof=1;
    break;
#ifdef HAVE_VCD
  case STREAMTYPE_VCD:
    s->pos=newpos; // real seek
#ifdef VCD_CACHE
    vcd_cache_seek(s->pos/VCD_SECTOR_DATA);
#else
    vcd_set_msf(s->pos/VCD_SECTOR_DATA);
#endif
    break;
#endif
#ifdef USE_DVDNAV
  case STREAMTYPE_DVDNAV: {
    if (newpos==0) {
      if (dvdnav_stream_reset((dvdnav_priv_t*)s->priv))
        s->pos=0;
    }
    if(newpos!=s->pos){
      mp_msg(MSGT_STREAM,MSGL_INFO,"Cannot seek in DVDNAV streams yet!\n");
      return 1;
    }
    break;
  }
#endif
#ifdef USE_DVDREAD
  case STREAMTYPE_DVD:
    s->pos=newpos; // real seek
    dvd_seek(s->priv,s->pos/2048);
    break;
#endif
  case STREAMTYPE_STREAM:
    //s->pos=newpos; // real seek
    // Some streaming protocol allow to seek backward and forward
    // A function call that return -1 can tell that the protocol
    // doesn't support seeking.
#ifdef STREAMING
    if( s->streaming_ctrl!=NULL ) {
      if( s->streaming_ctrl->streaming_seek( s->fd, pos, s->streaming_ctrl )<0 ) {
        mp_msg(MSGT_STREAM,MSGL_INFO,"Stream not seekable!\n");
        return 1;
      }
    } 
#else
    if(newpos<s->pos){
      mp_msg(MSGT_STREAM,MSGL_INFO,"Cannot seek backward in linear streams!\n");
      return 1;
    }
    while(s->pos<newpos){
      if(stream_fill_buffer(s)<=0) break; // EOF
    }
#endif
    break;
#ifdef USE_TV
  case STREAMTYPE_TV:
    s->pos=newpos; /* no sense */
    break;
#endif
  }
//   putchar('.');fflush(stdout);
//} else {
//   putchar('%');fflush(stdout);
}

  stream_fill_buffer(s);
  if(pos>=0 && pos<=s->buf_len){
    s->buf_pos=pos; // byte position in sector
    return 1;
  }
  
//  if(pos==s->buf_len) printf("XXX Seek to last byte of file -> EOF\n");
  
#ifdef _LARGEFILE_SOURCE
  mp_msg(MSGT_STREAM,MSGL_V,"stream_seek: WARNING! Can't seek to 0x%llX !\n",(long long)(pos+newpos));
#else
  mp_msg(MSGT_STREAM,MSGL_V,"stream_seek: WARNING! Can't seek to 0x%X !\n",(pos+newpos));
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

stream_t* new_memory_stream(unsigned char* data,int len){
  stream_t *s=malloc(sizeof(stream_t)+len);
  s->fd=-1;
  s->type=STREAMTYPE_MEMORY;
  s->buf_pos=0; s->buf_len=len;
  s->start_pos=0; s->end_pos=len;
  stream_reset(s);
  s->pos=len;
  memcpy(s->buffer,data,len);
  return s;
}

stream_t* new_stream(int fd,int type){
  stream_t *s=malloc(sizeof(stream_t));
  if(s==NULL) return NULL;
  memset(s,0,sizeof(stream_t));
  
  s->fd=fd;
  s->type=type;
  s->buf_pos=s->buf_len=0;
  s->start_pos=s->end_pos=0;
  s->priv=NULL;
  s->cache_pid=0;
  stream_reset(s);
  return s;
}

void free_stream(stream_t *s){
//  printf("\n*** free_stream() called ***\n");
  if(s->cache_pid) {
//    kill(s->cache_pid,SIGTERM);
    kill(s->cache_pid,SIGKILL);
    waitpid(s->cache_pid,NULL,0);
  }
  if(s->priv) free(s->priv);
  free(s);
}

stream_t* new_ds_stream(demux_stream_t *ds) {
  stream_t* s = new_stream(-1,STREAMTYPE_DS);
  s->priv = ds;
  return s;
}
