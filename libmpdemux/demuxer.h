#ifndef __DEMUXER_H
#define __DEMUXER_H 1

#ifdef USE_ASS
#include "libass/ass_types.h"
#endif

#define MAX_PACKS 4096
#ifdef HAVE_TV_BSDBT848
#define MAX_PACK_BYTES 0x2000000
#else
#define MAX_PACK_BYTES 0x800000
#endif

#define DEMUXER_TYPE_UNKNOWN 0
#define DEMUXER_TYPE_MPEG_ES 1
#define DEMUXER_TYPE_MPEG_PS 2
#define DEMUXER_TYPE_AVI 3
#define DEMUXER_TYPE_AVI_NI 4
#define DEMUXER_TYPE_AVI_NINI 5
#define DEMUXER_TYPE_ASF 6
#define DEMUXER_TYPE_MOV 7
#define DEMUXER_TYPE_VIVO 8
#define DEMUXER_TYPE_TV 9
#define DEMUXER_TYPE_FLI 10
#define DEMUXER_TYPE_REAL 11
#define DEMUXER_TYPE_Y4M 12
#define DEMUXER_TYPE_NUV 13
#define DEMUXER_TYPE_FILM 14
#define DEMUXER_TYPE_ROQ 15
#define DEMUXER_TYPE_MF 16
#define DEMUXER_TYPE_AUDIO 17
#define DEMUXER_TYPE_OGG 18
#define DEMUXER_TYPE_RAWAUDIO 20
#define DEMUXER_TYPE_RTP 21
#define DEMUXER_TYPE_RAWDV 22
#define DEMUXER_TYPE_PVA 23
#define DEMUXER_TYPE_SMJPEG 24
#define DEMUXER_TYPE_XMMS 25
#define DEMUXER_TYPE_RAWVIDEO 26
#define DEMUXER_TYPE_MPEG4_ES 27
#define DEMUXER_TYPE_GIF 28
#define DEMUXER_TYPE_MPEG_TS 29
#define DEMUXER_TYPE_H264_ES 30
#define DEMUXER_TYPE_MATROSKA 31
#define DEMUXER_TYPE_REALAUDIO 32
#define DEMUXER_TYPE_MPEG_TY 33
#define DEMUXER_TYPE_LMLM4 34
#define DEMUXER_TYPE_LAVF 35
#define DEMUXER_TYPE_NSV 36
#define DEMUXER_TYPE_VQF 37
#define DEMUXER_TYPE_AVS 38
#define DEMUXER_TYPE_AAC 39
#define DEMUXER_TYPE_MPC 40
#define DEMUXER_TYPE_MPEG_PES 41
#define DEMUXER_TYPE_MPEG_GXF 42
#define DEMUXER_TYPE_NUT 43

// This should always match the higest demuxer type number.
// Unless you want to disallow users to force the demuxer to some types
#define DEMUXER_TYPE_MIN 0
#define DEMUXER_TYPE_MAX 43

#define DEMUXER_TYPE_DEMUXERS (1<<16)
// A virtual demuxer type for the network code
#define DEMUXER_TYPE_PLAYLIST (2<<16)


#define MP_NOPTS_VALUE (-1LL<<63) //both int64_t and double should be able to represent this exactly


// DEMUXER control commands/answers
#define DEMUXER_CTRL_NOTIMPL -1
#define DEMUXER_CTRL_DONTKNOW 0
#define DEMUXER_CTRL_OK 1
#define DEMUXER_CTRL_GUESS 2
#define DEMUXER_CTRL_GET_TIME_LENGTH 10
#define DEMUXER_CTRL_GET_PERCENT_POS 11
#define DEMUXER_CTRL_SWITCH_AUDIO 12
#define DEMUXER_CTRL_RESYNC 13
#define DEMUXER_CTRL_SWITCH_VIDEO 14
#define DEMUXER_CTRL_IDENTIFY_PROGRAM 15

// Holds one packet/frame/whatever
typedef struct demux_packet_st {
  int len;
  double pts;
  double endpts;
  double stream_pts;
  off_t pos;  // position in index (AVI) or file (MPG)
  unsigned char* buffer;
  int flags; // keyframe, etc
  int refcount;   //refcounter for the master packet, if 0, buffer can be free()d
  struct demux_packet_st* master; //pointer to the master packet if this one is a cloned one
  struct demux_packet_st* next;
} demux_packet_t;

typedef struct {
  int buffer_pos;          // current buffer position
  int buffer_size;         // current buffer size
  unsigned char* buffer;   // current buffer, never free() it, always use free_demux_packet(buffer_ref);
  double pts;              // current buffer's pts
  int pts_bytes;           // number of bytes read after last pts stamp
  int eof;                 // end of demuxed stream? (true if all buffer empty)
  off_t pos;                 // position in the input stream (file)
  off_t dpos;                // position in the demuxed stream
  int pack_no;		   // serial number of packet
  int flags;               // flags of current packet (keyframe etc)
//---------------
  int packs;              // number of packets in buffer
  int bytes;              // total bytes of packets in buffer
  demux_packet_t *first;  // read to current buffer from here
  demux_packet_t *last;   // append new packets from input stream to here
  demux_packet_t *current;// needed for refcounting of the buffer
  int id;                 // stream ID  (for multiple audio/video streams)
  struct demuxer_st *demuxer; // parent demuxer structure (stream handler)
// ---- asf -----
  demux_packet_t *asf_packet;  // read asf fragments here
  int asf_seq;
// ---- mov -----
  unsigned int ss_mul,ss_div;
// ---- stream header ----
  void* sh;
} demux_stream_t;

typedef struct demuxer_info_st {
  char *name;
  char *author;
  char *encoder;
  char *comments;
  char *copyright;
} demuxer_info_t;

#define MAX_A_STREAMS 256
#define MAX_V_STREAMS 256
#define MAX_S_STREAMS 32

struct demuxer_st;

extern int correct_pts;

/**
 * Demuxer description structure
 */
typedef struct demuxers_desc_st {
  const char *info; ///< What is it (long name and/or description)
  const char *name; ///< Demuxer name, used with -demuxer switch
  const char *shortdesc; ///< Description printed at demuxer detection
  const char *author; ///< Demuxer author(s)
  const char *comment; ///< Comment, printed with -demuxer help

  int type; ///< DEMUXER_TYPE_xxx
  int safe_check; ///< If 1 detection is safe and fast, do it before file extension check

  /// Check if can demux the file, return DEMUXER_TYPE_xxx on success
  int (*check_file)(struct demuxer_st *demuxer); ///< Mandatory if safe_check == 1, else optional 
  /// Get packets from file, return 0 on eof
  int (*fill_buffer)(struct demuxer_st *demuxer, demux_stream_t *ds); ///< Mandatory
  /// Open the demuxer, return demuxer on success, NULL on failure
  struct demuxer_st* (*open)(struct demuxer_st *demuxer); ///< Optional
  /// Close the demuxer
  void (*close)(struct demuxer_st *demuxer); ///< Optional
  // Seek
  void (*seek)(struct demuxer_st *demuxer, float rel_seek_secs, float audio_delay, int flags); ///< Optional
  // Control
  int (*control)(struct demuxer_st *demuxer, int cmd, void *arg); ///< Optional
} demuxer_desc_t;

typedef struct demux_chapter_s
{
  uint64_t start, end;
  char* name;
} demux_chapter_t;

typedef struct demuxer_st {
  demuxer_desc_t *desc;  ///< Demuxer description structure
  off_t filepos; // input stream current pos.
  off_t movi_start;
  off_t movi_end;
  stream_t *stream;
  double stream_pts;       // current stream pts, if applicable (e.g. dvd)
  char *filename; ///< Needed by avs_check_file
  int synced;  // stream synced (used by mpeg)
  int type;    // demuxer type: mpeg PS, mpeg ES, avi, avi-ni, avi-nini, asf
  int file_format;  // file format: mpeg/avi/asf
  int seekable;  // flag
  //
  demux_stream_t *audio; // audio buffer/demuxer
  demux_stream_t *video; // video buffer/demuxer
  demux_stream_t *sub;   // dvd subtitle buffer/demuxer

  // stream headers:
  void* a_streams[MAX_A_STREAMS]; // audio streams (sh_audio_t)
  void* v_streams[MAX_V_STREAMS]; // video sterams (sh_video_t)
  void *s_streams[MAX_S_STREAMS];   // dvd subtitles (flag)

  demux_chapter_t* chapters;
  int num_chapters;
  
  void* priv;  // fileformat-dependent data
  char** info;
} demuxer_t;

typedef struct {
  int progid;        //program id
  int aid, vid, sid; //audio, video and subtitle id
} demux_program_t;

inline static demux_packet_t* new_demux_packet(int len){
  demux_packet_t* dp=(demux_packet_t*)malloc(sizeof(demux_packet_t));
  dp->len=len;
  dp->next=NULL;
  // still using 0 by default in case there is some code that uses 0 for both
  // unknown and a valid pts value
  dp->pts=correct_pts ? MP_NOPTS_VALUE : 0;
  dp->endpts=MP_NOPTS_VALUE;
  dp->stream_pts = MP_NOPTS_VALUE;
  dp->pos=0;
  dp->flags=0;
  dp->refcount=1;
  dp->master=NULL;
  dp->buffer=NULL;
  if (len > 0 && (dp->buffer = (unsigned char *)malloc(len + 8)))
    memset(dp->buffer + len, 0, 8);
  else
    dp->len = 0;
  return dp;
}

inline static void resize_demux_packet(demux_packet_t* dp, int len)
{
  if(len > 0)
  {
     dp->buffer=(unsigned char *)realloc(dp->buffer,len+8);
  }
  else
  {
     if(dp->buffer) free(dp->buffer);
     dp->buffer=NULL;
  }
  dp->len=len;
  if (dp->buffer)
     memset(dp->buffer + len, 0, 8);
  else
     dp->len = 0;
}

inline static demux_packet_t* clone_demux_packet(demux_packet_t* pack){
  demux_packet_t* dp=(demux_packet_t*)malloc(sizeof(demux_packet_t));
  while(pack->master) pack=pack->master; // find the master
  memcpy(dp,pack,sizeof(demux_packet_t));
  dp->next=NULL;
  dp->refcount=0;
  dp->master=pack;
  pack->refcount++;
  return dp;
}

inline static void free_demux_packet(demux_packet_t* dp){
  if (dp->master==NULL){  //dp is a master packet
    dp->refcount--;
    if (dp->refcount==0){
      if (dp->buffer) free(dp->buffer);
      free(dp);
    }
    return;
  }
  // dp is a clone:
  free_demux_packet(dp->master);
  free(dp);
}

#ifndef SIZE_MAX
#define SIZE_MAX ((size_t)-1)
#endif

static void *realloc_struct(void *ptr, size_t nmemb, size_t size) {
  if (nmemb > SIZE_MAX / size) {
    free(ptr);
    return NULL;
  }
  return realloc(ptr, nmemb * size);
}

demux_stream_t* new_demuxer_stream(struct demuxer_st *demuxer,int id);
demuxer_t* new_demuxer(stream_t *stream,int type,int a_id,int v_id,int s_id,char *filename);
void free_demuxer_stream(demux_stream_t *ds);
void free_demuxer(demuxer_t *demuxer);

void ds_add_packet(demux_stream_t *ds,demux_packet_t* dp);
void ds_read_packet(demux_stream_t *ds, stream_t *stream, int len, double pts, off_t pos, int flags);

int demux_fill_buffer(demuxer_t *demux,demux_stream_t *ds);
int ds_fill_buffer(demux_stream_t *ds);

inline static off_t ds_tell(demux_stream_t *ds){
  return (ds->dpos-ds->buffer_size)+ds->buffer_pos;
}

inline static int ds_tell_pts(demux_stream_t *ds){
  return (ds->pts_bytes-ds->buffer_size)+ds->buffer_pos;
}

int demux_read_data(demux_stream_t *ds,unsigned char* mem,int len);
int demux_read_data_pack(demux_stream_t *ds,unsigned char* mem,int len);
int demux_pattern_3(demux_stream_t *ds, unsigned char *mem, int maxlen,
                    int *read, uint32_t pattern);

#define demux_peekc(ds) (\
     (likely(ds->buffer_pos<ds->buffer_size)) ? ds->buffer[ds->buffer_pos] \
     :((unlikely(!ds_fill_buffer(ds)))? (-1) : ds->buffer[ds->buffer_pos] ) )
#if 1
#define demux_getc(ds) (\
     (likely(ds->buffer_pos<ds->buffer_size)) ? ds->buffer[ds->buffer_pos++] \
     :((unlikely(!ds_fill_buffer(ds)))? (-1) : ds->buffer[ds->buffer_pos++] ) )
#else
inline static int demux_getc(demux_stream_t *ds){
  if(ds->buffer_pos>=ds->buffer_size){
    if(!ds_fill_buffer(ds)){
//      printf("DEMUX_GETC: EOF reached!\n");
      return -1; // EOF
    }
  }
//  printf("[%02X]",ds->buffer[ds->buffer_pos]);
  return ds->buffer[ds->buffer_pos++];
}
#endif

void ds_free_packs(demux_stream_t *ds);
int ds_get_packet(demux_stream_t *ds,unsigned char **start);
int ds_get_packet_pts(demux_stream_t *ds, unsigned char **start, double *pts);
int ds_get_packet_sub(demux_stream_t *ds,unsigned char **start);
double ds_get_next_pts(demux_stream_t *ds);

// This is defined here because demux_stream_t ins't defined in stream.h
stream_t* new_ds_stream(demux_stream_t *ds);

static inline int avi_stream_id(unsigned int id){
  unsigned char *p=(unsigned char *)&id;
  unsigned char a,b;
#if WORDS_BIGENDIAN
  a=p[3]-'0'; b=p[2]-'0';
#else
  a=p[0]-'0'; b=p[1]-'0';
#endif
  if(a>9 || b>9) return 100; // invalid ID
  return a*10+b;
}

demuxer_t* demux_open(stream_t *stream,int file_format,int aid,int vid,int sid,char* filename);
int demux_seek(demuxer_t *demuxer,float rel_seek_secs,float audio_delay,int flags);
demuxer_t*  new_demuxers_demuxer(demuxer_t* vd, demuxer_t* ad, demuxer_t* sd);

// AVI demuxer params:
extern int index_mode;  // -1=untouched  0=don't use index  1=use (geneate) index
extern char *index_file_save, *index_file_load;
extern int force_ni;
extern int pts_from_bps;

extern int extension_parsing;

int demux_info_add(demuxer_t *demuxer, const char *opt, const char *param);
char* demux_info_get(demuxer_t *demuxer, char *opt);
int demux_info_print(demuxer_t *demuxer);
int demux_control(demuxer_t *demuxer, int cmd, void *arg);

#ifdef HAVE_OGGVORBIS
/* Found in demux_ogg.c */
int demux_ogg_num_subs(demuxer_t *demuxer);
int demux_ogg_sub_id(demuxer_t *demuxer, int index);
char *demux_ogg_sub_lang(demuxer_t *demuxer, int index);
#endif

#endif

extern int demuxer_get_current_time(demuxer_t *demuxer);
extern double demuxer_get_time_length(demuxer_t *demuxer);
extern int demuxer_get_percent_pos(demuxer_t *demuxer);
extern int demuxer_switch_audio(demuxer_t *demuxer, int index);
extern int demuxer_switch_video(demuxer_t *demuxer, int index);

extern int demuxer_type_by_filename(char* filename);

extern void demuxer_help(void);
extern int get_demuxer_type_from_name(char *demuxer_name, int *force);

int demuxer_add_chapter(demuxer_t* demuxer, const char* name, uint64_t start, uint64_t end);
int demuxer_seek_chapter(demuxer_t *demuxer, int chapter, int mode, float *seek_pts, int *num_chapters, char **chapter_name);

