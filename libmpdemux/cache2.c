#include "config.h"

#ifdef USE_STREAM_CACHE

// Initial draft of my new cache system...
// Note it runs in 2 processes (using fork()), but doesn't requires locking!!
// TODO: seeking, data consistency checking

#define READ_USLEEP_TIME 10000
#define FILL_USLEEP_TIME 50000
#define PREFILL_SLEEP_TIME 200

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>

#include "../linux/shmem.h"

#include "mp_msg.h"

#include "stream.h"

int stream_fill_buffer(stream_t *s);
int stream_seek_long(stream_t *s,off_t pos);


typedef struct {
  // constats:
  unsigned char *buffer;      // base pointer of the alllocated buffer memory
  int buffer_size; // size of the alllocated buffer memory
  int sector_size; // size of a single sector (2048/2324)
  int back_size;   // we should keep back_size amount of old bytes for backward seek
  int fill_limit;  // we should fill buffer only if space>=fill_limit
  int prefill;	   // we should fill min prefill bytes if cache gets empty
  // filler's pointers:
  int eof;
  off_t min_filepos; // buffer contain only a part of the file, from min-max pos
  off_t max_filepos;
  off_t offset;      // filepos <-> bufferpos  offset value (filepos of the buffer's first byte)
  // reader's pointers:
  off_t read_filepos;
  // commands/locking:
//  int seek_lock;   // 1 if we will seek/reset buffer, 2 if we are ready for cmd
//  int fifo_flag;  // 1 if we should use FIFO to notice cache about buffer reads.
  // callback
  stream_t* stream;
} cache_vars_t;

static int min_fill=0;
static int sleep_flag=0;

int cache_fill_status=0;

void cache_stats(cache_vars_t* s){
  int newb=s->max_filepos-s->read_filepos; // new bytes in the buffer
  printf("0x%06X  [0x%06X]  0x%06X   ",s->min_filepos,s->read_filepos,s->max_filepos);
  printf("%3d %%  (%3d%%)\n",100*newb/s->buffer_size,100*min_fill/s->buffer_size);
}

int cache_read(cache_vars_t* s,unsigned char* buf,int size){
  int total=0;
  while(size>0){
    int pos,newb,len;

  //printf("CACHE2_READ: 0x%X <= 0x%X <= 0x%X  \n",s->min_filepos,s->read_filepos,s->max_filepos);
    
    if(s->read_filepos>=s->max_filepos || s->read_filepos<s->min_filepos){
	// eof?
	if(s->eof) break;
	// waiting for buffer fill...
	usleep(READ_USLEEP_TIME); // 10ms
	continue; // try again...
    }

    newb=s->max_filepos-s->read_filepos; // new bytes in the buffer
    if(newb<min_fill) min_fill=newb; // statistics...

//    printf("*** newb: %d bytes ***\n",newb);

    pos=s->read_filepos - s->offset;
    if(pos<0) pos+=s->buffer_size; else
    if(pos>=s->buffer_size) pos-=s->buffer_size;

    if(newb>s->buffer_size-pos) newb=s->buffer_size-pos; // handle wrap...
    if(newb>size) newb=size;
    
    // check:
    if(s->read_filepos<s->min_filepos) mp_msg(MSGT_CACHE,MSGL_ERR,"Ehh. s->read_filepos<s->min_filepos !!! Report bug...\n");
    
    // len=write(mem,newb)
    //printf("Buffer read: %d bytes\n",newb);
    memcpy(buf,&s->buffer[pos],newb);
    buf+=newb;
    len=newb;
    // ...
    
    s->read_filepos+=len;
    size-=len;
    total+=len;
    
  }
  cache_fill_status=100*(s->max_filepos-s->read_filepos)/s->buffer_size;
  return total;
}

int cache_fill(cache_vars_t* s){
  int back,back2,newb,space,len,pos,endpos;
  off_t read=s->read_filepos;
  
  if(read<s->min_filepos || read>s->max_filepos){
      // seek...
      mp_msg(MSGT_CACHE,MSGL_DBG2,"Out of boundaries... seeking to 0x%X  \n",read);
      s->offset= // FIXME!?
      s->min_filepos=s->max_filepos=read; // drop cache content :(
      if(s->stream->eof) stream_reset(s->stream);
      stream_seek(s->stream,read);
      mp_msg(MSGT_CACHE,MSGL_DBG2,"Seek done. new pos: 0x%X  \n",(int)stream_tell(s->stream));
  }
  
  // calc number of back-bytes:
  back=read - s->min_filepos;
  if(back<0) back=0; // strange...
  if(back>s->back_size) back=s->back_size;
  
  // calc number of new bytes:
  newb=s->max_filepos - read;
  if(newb<0) newb=0; // strange...

  // calc free buffer space:
  space=s->buffer_size - (newb+back);
  
  // calc bufferpos:
  pos=s->max_filepos - s->offset;
  if(pos>=s->buffer_size) pos-=s->buffer_size; // wrap-around
  
  if(space<s->fill_limit){
//    printf("Buffer is full (%d bytes free, limit: %d)\n",space,s->fill_limit);
    return 0; // no fill...
  }

//  printf("### read=0x%X  back=%d  newb=%d  space=%d  pos=%d\n",read,back,newb,space,pos);
     
  // reduce space if needed:
  if(space>s->buffer_size-pos) space=s->buffer_size-pos;
  
//  if(space>32768) space=32768; // limit one-time block size
  if(space>4*s->sector_size) space=4*s->sector_size;
  
//  if(s->seek_lock) return 0; // FIXME

#if 1
  // back+newb+space <= buffer_size
  back2=s->buffer_size-(space+newb); // max back size
  if(s->min_filepos<(read-back2)) s->min_filepos=read-back2;
#else
  s->min_filepos=read-back; // avoid seeking-back to temp area...
#endif
  
  // ....
  //printf("Buffer fill: %d bytes of %d\n",space,s->buffer_size);
  //len=stream_fill_buffer(s->stream);
  //memcpy(&s->buffer[pos],s->stream->buffer,len); // avoid this extra copy!
  // ....
  len=stream_read(s->stream,&s->buffer[pos],space);
  if(!len) s->eof=1;
  
  s->max_filepos+=len;
  if(pos+len>=s->buffer_size){
      // wrap...
      s->offset+=s->buffer_size;
  }
  
  return len;
  
}

cache_vars_t* cache_init(int size,int sector){
  int num;
  cache_vars_t* s=shmem_alloc(sizeof(cache_vars_t));
  memset(s,0,sizeof(cache_vars_t));
  num=size/sector;
  s->buffer_size=num*sector;
  s->sector_size=sector;
  s->buffer=shmem_alloc(s->buffer_size);
  s->fill_limit=8*sector;
  s->back_size=size/2;
  s->prefill=size/20; // default: 5%
  return s;
}

static void exit_sighandler(int x){
  // close stream
  exit(0);
}

int stream_enable_cache(stream_t *stream,int size,int min,int prefill){
  int ss=(stream->type==STREAMTYPE_VCD)?VCD_SECTOR_DATA:STREAM_BUFFER_SIZE;
  cache_vars_t* s;
  if(size<32*1024) size=32*1024; // 32kb min
  s=cache_init(size,ss);
  stream->cache_data=s;
  s->stream=stream; // callback
  s->prefill=size*prefill;
  
  if((stream->cache_pid=fork())){
    // wait until cache is filled at least prefill_init %
    mp_msg(MSGT_CACHE,MSGL_V,"CACHE_PRE_INIT: %d [%d] %d  pre:%d  eof:%d  \n",
	s->min_filepos,s->read_filepos,s->max_filepos,min,s->eof);
    while(s->read_filepos<s->min_filepos || s->max_filepos-s->read_filepos<min){
	mp_msg(MSGT_CACHE,MSGL_STATUS,"\rCache fill: %5.2f%% (%d bytes)    ",
	    100.0*(float)(s->max_filepos-s->read_filepos)/(float)(s->buffer_size),
	    s->max_filepos-s->read_filepos
	);
	if(s->eof) break; // file is smaller than prefill size
	if(mpdemux_check_interrupt(PREFILL_SLEEP_TIME))
	  return 0;
    }
    return 1; // parent exits
  }
  
// cache thread mainloop:
  signal(SIGTERM,exit_sighandler); // kill
  while(1){
    if(!cache_fill(s)){
	 usleep(FILL_USLEEP_TIME); // idle
    }
//	 cache_stats(s->cache_data);
  }
}

int cache_stream_fill_buffer(stream_t *s){
  int len;
  if(s->eof){ s->buf_pos=s->buf_len=0; return 0; }
  if(!s->cache_pid) return stream_fill_buffer(s);

//  cache_stats(s->cache_data);

  if(s->pos!=((cache_vars_t*)s->cache_data)->read_filepos) mp_msg(MSGT_CACHE,MSGL_ERR,"!!! read_filepos differs!!! report this bug...\n");

  len=cache_read(s->cache_data,s->buffer, ((cache_vars_t*)s->cache_data)->sector_size);
  //printf("cache_stream_fill_buffer->read -> %d\n",len);

  if(len<=0){ s->eof=1; s->buf_pos=s->buf_len=0; return 0; }
  s->buf_pos=0;
  s->buf_len=len;
  s->pos+=len;
//  printf("[%d]",len);fflush(stdout);
  return len;

}

int cache_stream_seek_long(stream_t *stream,off_t pos){
  cache_vars_t* s;
  off_t newpos;
  if(!stream->cache_pid) return stream_seek_long(stream,pos);
  
  s=stream->cache_data;
//  s->seek_lock=1;
  
  mp_msg(MSGT_CACHE,MSGL_DBG2,"CACHE2_SEEK: 0x%X <= 0x%X (0x%X) <= 0x%X  \n",s->min_filepos,(int)pos,s->read_filepos,s->max_filepos);

  newpos=pos/s->sector_size; newpos*=s->sector_size; // align
  stream->pos=s->read_filepos=newpos;
  s->eof=0; // !!!!!!!

  cache_stream_fill_buffer(stream);

  pos-=newpos;
  if(pos>=0 && pos<=stream->buf_len){
    stream->buf_pos=pos; // byte position in sector
    return 1;
  }

//  stream->buf_pos=stream->buf_len=0;
//  return 1;

#ifdef _LARGEFILE_SOURCE
  mp_msg(MSGT_CACHE,MSGL_V,"cache_stream_seek: WARNING! Can't seek to 0x%llX !\n",(long long)(pos+newpos));
#else
  mp_msg(MSGT_CACHE,MSGL_V,"cache_stream_seek: WARNING! Can't seek to 0x%X !\n",(pos+newpos));
#endif
  return 0;
}

#endif
