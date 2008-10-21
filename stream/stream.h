#ifndef MPLAYER_STREAM_H
#define MPLAYER_STREAM_H

#include "mp_msg.h"
#include <string.h>
#include <inttypes.h>
#include <sys/types.h>

#define STREAMTYPE_DUMMY -1    // for placeholders, when the actual reading is handled in the demuxer
#define STREAMTYPE_FILE 0      // read from seekable file
#define STREAMTYPE_VCD  1      // raw mode-2 CDROM reading, 2324 bytes/sector
#define STREAMTYPE_STREAM 2    // same as FILE but no seeking (for net/stdin)
#define STREAMTYPE_DVD  3      // libdvdread
#define STREAMTYPE_MEMORY  4   // read data from memory area
#define STREAMTYPE_PLAYLIST 6  // FIXME!!! same as STREAMTYPE_FILE now
#define STREAMTYPE_DS   8      // read from a demuxer stream
#define STREAMTYPE_DVDNAV 9    // we cannot safely "seek" in this...
#define STREAMTYPE_CDDA 10     // raw audio CD reader
#define STREAMTYPE_SMB 11      // smb:// url, using libsmbclient (samba)
#define STREAMTYPE_VCDBINCUE 12      // vcd directly from bin/cue files
#define STREAMTYPE_DVB 13
#define STREAMTYPE_VSTREAM 14
#define STREAMTYPE_SDP 15
#define STREAMTYPE_PVR 16
#define STREAMTYPE_TV 17
#define STREAMTYPE_MF 18
#define STREAMTYPE_RADIO 19

#define STREAM_BUFFER_SIZE 2048

#define VCD_SECTOR_SIZE 2352
#define VCD_SECTOR_OFFS 24
#define VCD_SECTOR_DATA 2324

/// atm it will always use mode == STREAM_READ
/// streams that use the new api should check the mode at open
#define STREAM_READ  0
#define STREAM_WRITE 1
/// Seek flags, if not mannualy set and s->seek isn't NULL
/// STREAM_SEEK is automaticly set
#define STREAM_SEEK_BW  2
#define STREAM_SEEK_FW  4
#define STREAM_SEEK  (STREAM_SEEK_BW|STREAM_SEEK_FW)

//////////// Open return code
#define STREAM_REDIRECTED -2
/// This can't open the requested protocol (used by stream wich have a
/// * protocol when they don't know the requested protocol)
#define STREAM_UNSUPPORTED -1
#define STREAM_ERROR 0
#define STREAM_OK    1

#define MAX_STREAM_PROTOCOLS 10

#define STREAM_CTRL_RESET 0
#define STREAM_CTRL_GET_TIME_LENGTH 1
#define STREAM_CTRL_SEEK_TO_CHAPTER 2
#define STREAM_CTRL_GET_CURRENT_CHAPTER 3
#define STREAM_CTRL_GET_NUM_CHAPTERS 4
#define STREAM_CTRL_GET_CURRENT_TIME 5
#define STREAM_CTRL_SEEK_TO_TIME 6
#define STREAM_CTRL_GET_SIZE 7
#define STREAM_CTRL_GET_ASPECT_RATIO 8
#define STREAM_CTRL_GET_NUM_ANGLES 9
#define STREAM_CTRL_GET_ANGLE 10
#define STREAM_CTRL_SET_ANGLE 11


#ifdef CONFIG_NETWORK
#include "network.h"
#endif

struct stream_st;
typedef struct stream_info_st {
  const char *info;
  const char *name;
  const char *author;
  const char *comment;
  /// mode isn't used atm (ie always READ) but it shouldn't be ignored
  /// opts is at least in it's defaults settings and may have been
  /// altered by url parsing if enabled and the options string parsing.
  int (*open)(struct stream_st* st, int mode, void* opts, int* file_format);
  const char* protocols[MAX_STREAM_PROTOCOLS];
  const void* opts;
  int opts_url; /* If this is 1 we will parse the url as an option string
		 * too. Otherwise options are only parsed from the
		 * options string given to open_stream_plugin */
} stream_info_t;

typedef struct stream_st {
  // Read
  int (*fill_buffer)(struct stream_st *s, char* buffer, int max_len);
  // Write
  int (*write_buffer)(struct stream_st *s, char* buffer, int len);
  // Seek
  int (*seek)(struct stream_st *s,off_t pos);
  // Control
  // Will be later used to let streams like dvd and cdda report
  // their structure (ie tracks, chapters, etc)
  int (*control)(struct stream_st *s,int cmd,void* arg);
  // Close
  void (*close)(struct stream_st *s);

  int fd;   // file descriptor, see man open(2)
  int type; // see STREAMTYPE_*
  int flags;
  int sector_size; // sector size (seek will be aligned on this size if non 0)
  unsigned int buf_pos,buf_len;
  off_t pos,start_pos,end_pos;
  int eof;
  int mode; //STREAM_READ or STREAM_WRITE
  unsigned int cache_pid;
  void* cache_data;
  void* priv; // used for DVD, TV, RTSP etc
  char* url;  // strdup() of filename/url
#ifdef CONFIG_NETWORK
  streaming_ctrl_t *streaming_ctrl;
#endif
  unsigned char buffer[STREAM_BUFFER_SIZE>VCD_SECTOR_SIZE?STREAM_BUFFER_SIZE:VCD_SECTOR_SIZE];
} stream_t;

#ifdef CONFIG_STREAM_CACHE
int stream_enable_cache(stream_t *stream,int size,int min,int prefill);
int cache_stream_fill_buffer(stream_t *s);
int cache_stream_seek_long(stream_t *s,off_t pos);
#else
// no cache, define wrappers:
int stream_fill_buffer(stream_t *s);
int stream_seek_long(stream_t *s,off_t pos);
#define cache_stream_fill_buffer(x) stream_fill_buffer(x)
#define cache_stream_seek_long(x,y) stream_seek_long(x,y)
#define stream_enable_cache(x,y,z,w) 1
#endif
void fixup_network_stream_cache(stream_t *stream);
int stream_write_buffer(stream_t *s, unsigned char *buf, int len);

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

inline static uint64_t stream_read_qword_le(stream_t *s){
  uint64_t y;
  y = stream_read_dword_le(s);
  y|=(uint64_t)stream_read_dword_le(s)<<32;
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

inline static unsigned char* stream_read_line(stream_t *s,unsigned char* mem, int max) {
  int len;
  unsigned char* end,*ptr = mem;;
  do {
    len = s->buf_len-s->buf_pos;
    // try to fill the buffer
    if(len <= 0 &&
       (!cache_stream_fill_buffer(s) || 
        (len = s->buf_len-s->buf_pos) <= 0)) break;
    end = (unsigned char*) memchr((void*)(s->buffer+s->buf_pos),'\n',len);
    if(end) len = end - (s->buffer+s->buf_pos) + 1;
    if(len > 0 && max > 1) {
      int l = len > max-1 ? max-1 : len;
      memcpy(ptr,s->buffer+s->buf_pos,l);
      max -= l;
      ptr += l;
    }
    s->buf_pos += len;
  } while(!end);
  if(s->eof && ptr == mem) return NULL;
  if(max > 0) ptr[0] = 0;
  return mem;
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
  if( (len<0 && (s->flags & STREAM_SEEK_BW)) || (len>2*STREAM_BUFFER_SIZE && (s->flags & STREAM_SEEK_FW)) ) {
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
int stream_control(stream_t *s, int cmd, void *arg);
stream_t* new_stream(int fd,int type);
void free_stream(stream_t *s);
stream_t* new_memory_stream(unsigned char* data,int len);
stream_t* open_stream(char* filename,char** options,int* file_format);
stream_t* open_stream_full(char* filename,int mode, char** options, int* file_format);
stream_t* open_output_stream(char* filename,char** options);
/// Set the callback to be used by libstream to check for user
/// interruption during long blocking operations (cache filling, etc).
void stream_set_interrupt_callback(int (*cb)(int));
/// Call the interrupt checking callback if there is one.
int stream_check_interrupt(int time);

extern int dvd_title;
extern int dvd_chapter;
extern int dvd_last_chapter;
extern int dvd_angle;

extern char * audio_stream;

typedef struct {
 int id; // 0 - 31 mpeg; 128 - 159 ac3; 160 - 191 pcm
 int language; 
 int type;
 int channels;
} stream_language_t;

#endif /* MPLAYER_STREAM_H */
