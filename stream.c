
#include "vcd_read.c"

//=================== STREAMER =========================

#define STREAM_BUFFER_SIZE 4096

#define STREAMTYPE_FILE 0
#define STREAMTYPE_VCD  1

typedef struct {
  int fd;
  long pos;
  int eof;
  int type; // 0=file 1=VCD
  unsigned int buf_pos,buf_len;
  unsigned char buffer[STREAM_BUFFER_SIZE];
} stream_t;

int stream_fill_buffer(stream_t *s){
  int len;
  if(s->eof){ s->buf_pos=s->buf_len=0; return 0; }
  switch(s->type){
  case STREAMTYPE_FILE:
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

inline unsigned int stream_read_char(stream_t *s){
  return (s->buf_pos<s->buf_len)?s->buffer[s->buf_pos++]:
    (stream_fill_buffer(s)?s->buffer[s->buf_pos++]:0);
//  if(s->buf_pos<s->buf_len) return s->buffer[s->buf_pos++];
//  stream_fill_buffer(s);
//  if(s->buf_pos<s->buf_len) return s->buffer[s->buf_pos++];
//  return 0; // EOF
}

inline unsigned int stream_read_word(stream_t *s){
  int x,y;
  x=stream_read_char(s);
  y=stream_read_char(s);
  return (x<<8)|y;
}

inline unsigned int stream_read_dword(stream_t *s){
  unsigned int y;
  y=stream_read_char(s);
  y=(y<<8)|stream_read_char(s);
  y=(y<<8)|stream_read_char(s);
  y=(y<<8)|stream_read_char(s);
  return y;
}

inline unsigned int stream_read_word_le(stream_t *s){
  int x,y;
  x=stream_read_char(s);
  y=stream_read_char(s);
  return (y<<8)|x;
}

inline unsigned int stream_read_dword_le(stream_t *s){
  unsigned int y;
  y=stream_read_char(s);
  y|=stream_read_char(s)<<8;
  y|=stream_read_char(s)<<16;
  y|=stream_read_char(s)<<24;
  return y;
}

inline void stream_read(stream_t *s,char* mem,int len){
  while(len>0){
    int x;
    x=s->buf_len-s->buf_pos;
    if(x==0){
      if(!stream_fill_buffer(s)) return; // EOF
      x=s->buf_len-s->buf_pos;
    }
    if(s->buf_pos>s->buf_len) printf("stream_read: WARNING! s->buf_pos>s->buf_len\n");
    if(x>len) x=len;
    memcpy(mem,&s->buffer[s->buf_pos],x);
    s->buf_pos+=x; mem+=x; len-=x;
  }
}

inline int stream_eof(stream_t *s){
  return s->eof;
}

inline int stream_tell(stream_t *s){
  return s->pos+s->buf_pos-s->buf_len;
}

inline int stream_seek(stream_t *s,unsigned int pos){
unsigned int newpos;

  if(verbose>=3) printf("seek to 0x%X\n",pos);

  if(pos<s->pos){
    int x=pos-(s->pos-s->buf_len);
    if(x>=0){
      s->buf_pos=x;
//      putchar('*');fflush(stdout);
      return 1;
    }
  }

if(verbose>=3){
  printf("s->pos=%X  newpos=%X  new_bufpos=%X  buflen=%X  \n",
    s->pos,newpos,pos,s->buf_len);
}

  s->buf_pos=s->buf_len=0;

  switch(s->type){
  case STREAMTYPE_FILE:
    newpos=pos&(~4095);break;
  case STREAMTYPE_VCD:
    newpos=(pos/VCD_SECTOR_DATA)*VCD_SECTOR_DATA;break;
  }

  pos-=newpos;

if(newpos==0 || newpos!=s->pos){
  s->pos=newpos; // real seek
  switch(s->type){
  case STREAMTYPE_FILE:
    if(lseek(s->fd,s->pos,SEEK_SET)<0) s->eof=1;
    break;
  case STREAMTYPE_VCD:
#ifdef VCD_CACHE
    vcd_cache_seek(s->pos/VCD_SECTOR_DATA);
#else
    vcd_set_msf(s->pos/VCD_SECTOR_DATA);
#endif
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
  printf("stream_seek: WARNING! Can't seek to 0x%X !\n",pos+newpos);
  return 0;
}

inline void stream_skip(stream_t *s,int len){
  if(len<0 || len>2*STREAM_BUFFER_SIZE){
    // negative or big skip!
    stream_seek(s,stream_tell(s)+len);
    return;
  }
  while(len>0){
    int x=s->buf_len-s->buf_pos;
    if(x==0){
      if(!stream_fill_buffer(s)) return; // EOF
      x=s->buf_len-s->buf_pos;
    }
    if(x>len) x=len;
    //memcpy(mem,&s->buf[s->buf_pos],x);
    s->buf_pos+=x; len-=x;
  }
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
  stream_reset(s);
  return s;
}

void free_stream(stream_t *s){
  free(s);
}


