#ifndef __STREAM_H
#define __STREAM_H

#include "mp_msg.h"
#include <inttypes.h>
#include <sys/types.h>

#define STREAM_BUFFER_SIZE 2048

#define STREAMTYPE_FILE 0
#define STREAMTYPE_VCD  1
#define STREAMTYPE_STREAM 2    // same as FILE but no seeking (for stdin)
#define STREAMTYPE_DVD  3
#define STREAMTYPE_MEMORY  4
#define STREAMTYPE_TV	5
#define STREAMTYPE_PLAYLIST 6
#define STREAMTYPE_MF   7
#define STREAMTYPE_DS   8
#define STREAMTYPE_DVDNAV 9

#define VCD_SECTOR_SIZE 2352
#define VCD_SECTOR_OFFS 24
#define VCD_SECTOR_DATA 2324

#ifdef STREAMING
#include "network.h"
#endif

int vcd_seek_to_track(int fd,int track);
void vcd_read_toc(int fd);

#ifdef VCD_CACHE
void vcd_cache_init(int s);
#endif

typedef struct {
  int fd;
  off_t pos;
  int eof;
  int type; // 0=file 1=VCD
  unsigned int buf_pos,buf_len;
  off_t start_pos,end_pos;
  unsigned int cache_pid;
  void* cache_data;
  void* priv; // used for DVD
  unsigned char buffer[STREAM_BUFFER_SIZE>VCD_SECTOR_SIZE?STREAM_BUFFER_SIZE:VCD_SECTOR_SIZE];
#ifdef STREAMING
  streaming_ctrl_t *streaming_ctrl;
#endif
} stream_t;

#ifdef USE_STREAM_CACHE
int stream_enable_cache(stream_t *stream,int size,int min,int prefill);
#else
// no cache
#define cache_stream_fill_buffer(x) stream_fill_buffer(x)
#define cache_stream_seek_long(x,y) stream_seek_long(x,y)
#define stream_enable_cache(x,y,z,w) 1
#endif

int cache_stream_fill_buffer(stream_t *s);
int cache_stream_seek_long(stream_t *s,off_t pos);

#include <string.h>

inline static int stream_read_char(stream_t *s){
  return (s->buf_pos<s->buf_len)?s->buffer[s->buf_pos++]:
    (cache_stream_fill_buffer(s)?s->buffer[s->buf_pos++]:-256);
//  if(s->buf_pos<s->buf_len) return s->buffer[s->buf_pos++];
//  stream_fill_buffer(s);
//  if(s->buf_pos<s->buf_len) return s->buffer[s->buf_pos++];
//  return 0; // EOF
}

inline static unsigned int stream_read_word(stream_t *s){
  int x,y;
  x=stream_read_char(s);
  y=stream_read_char(s);
  return (x<<8)|y;
}

inline static unsigned int stream_read_dword(stream_t *s){
  unsigned int y;
  y=stream_read_char(s);
  y=(y<<8)|stream_read_char(s);
  y=(y<<8)|stream_read_char(s);
  y=(y<<8)|stream_read_char(s);
  return y;
}

#define stream_read_fourcc stream_read_dword_le

inline static unsigned int stream_read_word_le(stream_t *s){
  int x,y;
  x=stream_read_char(s);
  y=stream_read_char(s);
  return (y<<8)|x;
}

inline static unsigned int stream_read_dword_le(stream_t *s){
  unsigned int y;
  y=stream_read_char(s);
  y|=stream_read_char(s)<<8;
  y|=stream_read_char(s)<<16;
  y|=stream_read_char(s)<<24;
  return y;
}

inline static uint64_t stream_read_qword(stream_t *s){
  uint64_t y;
  y = stream_read_char(s);
  y=(y<<8)|stream_read_char(s);
  y=(y<<8)|stream_read_char(s);
  y=(y<<8)|stream_read_char(s);
  y=(y<<8)|stream_read_char(s);
  y=(y<<8)|stream_read_char(s);
  y=(y<<8)|stream_read_char(s);
  y=(y<<8)|stream_read_char(s);
  return y;
}

inline static unsigned int stream_read_int24(stream_t *s){
  unsigned int y;
  y = stream_read_char(s);
  y=(y<<8)|stream_read_char(s);
  y=(y<<8)|stream_read_char(s);
  return y;
}

inline static int stream_read(stream_t *s,char* mem,int total){
  int len=total;
  while(len>0){
    int x;
    x=s->buf_len-s->buf_pos;
    if(x==0){
      if(!cache_stream_fill_buffer(s)) return total-len; // EOF
      x=s->buf_len-s->buf_pos;
    }
    if(s->buf_pos>s->buf_len) mp_msg(MSGT_DEMUX, MSGL_WARN, "stream_read: WARNING! s->buf_pos>s->buf_len\n");
    if(x>len) x=len;
    memcpy(mem,&s->buffer[s->buf_pos],x);
    s->buf_pos+=x; mem+=x; len-=x;
  }
  return total;
}

inline static int stream_eof(stream_t *s){
  return s->eof;
}

inline static off_t stream_tell(stream_t *s){
  return s->pos+s->buf_pos-s->buf_len;
}

inline static int stream_seek(stream_t *s,off_t pos){

  mp_dbg(MSGT_DEMUX, MSGL_DBG3, "seek to 0x%qX\n",(long long)pos);

  if(pos<s->pos){
    off_t x=pos-(s->pos-s->buf_len);
    if(x>=0){
      s->buf_pos=x;
//      putchar('*');fflush(stdout);
      return 1;
    }
  }
  
  return cache_stream_seek_long(s,pos);
}

inline static int stream_skip(stream_t *s,off_t len){
  if(len<0 || (len>2*STREAM_BUFFER_SIZE && s->type!=STREAMTYPE_STREAM)){
    // negative or big skip!
    return stream_seek(s,stream_tell(s)+len);
  }
  while(len>0){
    int x=s->buf_len-s->buf_pos;
    if(x==0){
      if(!cache_stream_fill_buffer(s)) return 0; // EOF
      x=s->buf_len-s->buf_pos;
    }
    if(x>len) x=len;
    //memcpy(mem,&s->buf[s->buf_pos],x);
    s->buf_pos+=x; len-=x;
  }
  return 1;
}

void stream_reset(stream_t *s);
stream_t* new_stream(int fd,int type);
void free_stream(stream_t *s);
stream_t* new_memory_stream(unsigned char* data,int len);
stream_t* open_stream(char* filename,int vcd_track,int* file_format);

//#ifdef USE_DVDREAD
struct config;
extern int dvd_title;
extern int dvd_chapter;
extern int dvd_last_chapter;
extern int dvd_angle;
extern int dvd_nav;
int dvd_parse_chapter_range(struct config*, const char*);
//#endif

#ifdef USE_DVDREAD

#include <dvdread/dvd_reader.h>
#include <dvdread/ifo_types.h>
#include <dvdread/ifo_read.h>
#include <dvdread/nav_read.h>

typedef struct {
 int id; // 0 - 31 mpeg; 128 - 159 ac3; 160 - 191 pcm
 int language; 
} stream_language_t;

typedef struct {
  dvd_reader_t *dvd;
  dvd_file_t *title;
  ifo_handle_t *vmg_file;
  tt_srpt_t *tt_srpt;
  ifo_handle_t *vts_file;
  vts_ptt_srpt_t *vts_ptt_srpt;
  pgc_t *cur_pgc;
//
  int cur_cell;
  int last_cell;
  int cur_pack;
  int cell_last_pack;
// Navi:
  int packs_left;
  dsi_t dsi_pack;
  int angle_seek;
// audio datas
  int nr_of_channels;
  stream_language_t audio_streams[32];
// subtitles
  int nr_of_subtitles;
  stream_language_t subtitles[32];
} dvd_priv_t;

int dvd_aid_from_lang(stream_t *stream, unsigned char* lang);
int dvd_sid_from_lang(stream_t *stream, unsigned char* lang);

#endif
							    
#endif // __STREAM_H
