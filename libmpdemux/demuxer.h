#ifndef __DEMUXER_H
#define __DEMUXER_H 1

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
#define DEMUXER_TYPE_BMP 19
#define DEMUXER_TYPE_RAWAUDIO 20
// This should always match the higest demuxer type number.
// Unless you want to disallow users to force the demuxer to some types
#define DEMUXER_TYPE_MIN 0
#define DEMUXER_TYPE_MAX 20

#define DEMUXER_TYPE_DEMUXERS (1<<16)
// A virtual demuxer type for the network code
#define DEMUXER_TYPE_PLAYLIST (2<<16)


#define DEMUXER_TIME_NONE 0
#define DEMUXER_TIME_PTS 1
#define DEMUXER_TIME_FILE 2
#define DEMUXER_TIME_BPS 3


// Holds one packet/frame/whatever
typedef struct demux_packet_st {
  int len;
  float pts;
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
  float pts;               // current buffer's pts
  int pts_bytes;           // number of bytes read after last pts stamp
  int eof;                 // end of demuxed stream? (true if all buffer empty)
  off_t pos;                 // position in the input stream (file)
  off_t dpos;                // position in the demuxed stream
  int pack_no;		   // serial number of packet
  int block_no;            // number of <=block_size length blocks (for VBR mp3)
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
// ---- avi -----
  unsigned int block_size;
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

typedef struct demuxer_st {
  off_t filepos; // input stream current pos.
  off_t movi_start;
  off_t movi_end;
  stream_t *stream;
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
  char s_streams[32];   // dvd subtitles (flag)
  
  void* priv;  // fileformat-dependent data
  char** info;
} demuxer_t;

inline static demux_packet_t* new_demux_packet(int len){
  demux_packet_t* dp=malloc(sizeof(demux_packet_t));
  dp->len=len;
  dp->buffer=len?malloc(len):NULL;
  dp->next=NULL;
  dp->pts=0;
  dp->pos=0;
  dp->flags=0;
  dp->refcount=1;
  dp->master=NULL;
  return dp;
}

inline static demux_packet_t* clone_demux_packet(demux_packet_t* pack){
  demux_packet_t* dp=malloc(sizeof(demux_packet_t));
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

demux_stream_t* new_demuxer_stream(struct demuxer_st *demuxer,int id);
demuxer_t* new_demuxer(stream_t *stream,int type,int a_id,int v_id,int s_id);
void free_demuxer_stream(demux_stream_t *ds);
void free_demuxer(demuxer_t *demuxer);

void ds_add_packet(demux_stream_t *ds,demux_packet_t* dp);
void ds_read_packet(demux_stream_t *ds,stream_t *stream,int len,float pts,off_t pos,int flags);

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

#if 1
#define demux_getc(ds) (\
     (ds->buffer_pos<ds->buffer_size) ? ds->buffer[ds->buffer_pos++] \
     :((!ds_fill_buffer(ds))? (-1) : ds->buffer[ds->buffer_pos++] ) )
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
int ds_get_packet_sub(demux_stream_t *ds,unsigned char **start);
float ds_get_next_pts(demux_stream_t *ds);

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

demuxer_t* demux_open(stream_t *stream,int file_format,int aid,int vid,int sid);
int demux_seek(demuxer_t *demuxer,float rel_seek_secs,int flags);
demuxer_t*  new_demuxers_demuxer(demuxer_t* vd, demuxer_t* ad, demuxer_t* sd);

// AVI demuxer params:
extern int index_mode;  // -1=untouched  0=don't use index  1=use (geneate) index
extern int force_ni;
extern int pts_from_bps;

int demux_info_add(demuxer_t *demuxer, char *opt, char *param);
char* demux_info_get(demuxer_t *demuxer, char *opt);
int demux_info_print(demuxer_t *demuxer);

#endif
