#ifdef USE_STREAM_CACHE
// gcc cache2.c ../linux/shmem.o -o cache2

// Initial draft of my new cache system...
// includes some simulation code, using usleep() to emulate limited bandwith
// TODO: seeking, data consistency checking

#define READ_SPEED 20
#define FILL_SPEED 10

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include "../linux/shmem.h"

#include "stream.h"

int stream_fill_buffer(stream_t *s);
int stream_seek_long(stream_t *s,off_t pos);


typedef struct {
  // constats:
  unsigned char *buffer;      // base pointer of the alllocated buffer memory
  int buffer_size;   // size of the alllocated buffer memory
  int sector_size;   // size of a single sector (2048/2324)
     // Note: buffer_size should be N*sector_size, where N is integer...
  int back_size;  // we should keep back_size amount of old bytes for backward seek
  int fill_limit; // we should fill buffer if space>fill_limit
  // reader's pointers:
  int read_filepos;
  // filler's pointers:
  int min_filepos; // buffer contain only a part of the file, from min-max pos
  int max_filepos;
  int offset;      // filepos <-> bufferpos  offset value (filepos of the buffer's first byte)
  // commands/locking:
  int cmd_lock;   // 1 if we will seek/reset buffer, 2 if we are ready for cmd
  int fifo_flag;  // 1 if we should use FIFO to notice cache about buffer reads.
  // callback
  stream_t* stream;
} cache_vars_t;

int min_fill=0;
int sleep_flag=0;

void cache_stats(cache_vars_t* s){
  int newb=s->max_filepos-s->read_filepos; // new bytes in the buffer
  printf("0x%06X  [0x%06X]  0x%06X   ",s->min_filepos,s->read_filepos,s->max_filepos);
  printf("%3d %%  (%3d%%)\n",100*newb/s->buffer_size,100*min_fill/s->buffer_size);
}

int cache_read(cache_vars_t* s,unsigned char* buf,int size){
  int total=0;
  while(size>0){
    int pos,newb,len;
    
    pos=s->read_filepos - s->offset;
    if(pos<0) pos+=s->buffer_size; else
    if(pos>=s->buffer_size) pos-=s->buffer_size;
    
    newb=s->max_filepos-s->read_filepos; // new bytes in the buffer

    if(newb<min_fill) min_fill=newb; // statistics...

    if(newb<=0){
	// waiting for buffer fill...
	usleep(10000); // 10ms
	continue;
    }
    
//    printf("*** newb: %d bytes ***\n",newb);
    
    if(newb>s->buffer_size-pos) newb=s->buffer_size-pos; // handle wrap...
    if(newb>size) newb=size;
    
    // len=write(mem,newb)
    //printf("Buffer read: %d bytes\n",newb);
    memcpy(buf,&s->buffer[pos],newb);
    buf+=newb;
    len=newb; //usleep(len*READ_SPEED*sleep_flag);
    // ...
    
    s->read_filepos+=len;
    size-=len;
    total+=len;
    
  }
  return total;
}

int cache_fill(cache_vars_t* s){
  int read,back,newb,space,len,pos,endpos;
  
  read=s->read_filepos;
  
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
  
  s->min_filepos=read-back; // avoid seeking-back to temp area...
  
  // ....
  //printf("Buffer fill: %d bytes of %d\n",space,s->buffer_size);
  //len=space; usleep(len*FILL_SPEED*sleep_flag);
  //len=stream_fill_buffer(s->stream);
  //memcpy(&s->buffer[pos],s->stream->buffer,len); // avoid this extra copy!
  // ....
  stream_read(s->stream,&s->buffer[pos],space); len=space;
  
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
  return s;
}

static void exit_sighandler(int x){
  // close stream
  exit(0);
}

void stream_enable_cache(stream_t *s,int size){
  int ss=(s->type==STREAMTYPE_VCD)?VCD_SECTOR_DATA:STREAM_BUFFER_SIZE;
  s->cache_data=cache_init(size,ss);
  ((cache_vars_t*)s->cache_data)->stream=s; // callback
  if((s->cache_pid=fork())) return; // parent exits
// cache thread mainloop:
  signal(SIGTERM,exit_sighandler); // kill
  while(1){
    if(!cache_fill(s->cache_data)){
	 usleep(50000); // idle
    }
	 cache_stats(s->cache_data);
  }
}

int cache_stream_fill_buffer(stream_t *s){
  int len;
  if(s->eof){ s->buf_pos=s->buf_len=0; return 0; }
  if(!s->cache_pid) return stream_fill_buffer(s);

  len=cache_read(s->cache_data,s->buffer, ((cache_vars_t*)s->cache_data)->sector_size);
  
  if(len<=0){ s->eof=1; s->buf_pos=s->buf_len=0; return 0; }
  s->buf_pos=0;
  s->buf_len=len;
  s->pos+=len;
//  printf("[%d]",len);fflush(stdout);
  return len;

}

int cache_stream_seek_long(stream_t *s,off_t pos){

  if(!s->cache_pid) return stream_seek_long(s,pos);

  printf("cache2 seek not implemented!!!!!!!!\n");
  return 0;
}

#endif
