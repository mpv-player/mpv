//=================== DEMUXER v2.5 =========================

#define MAX_PACKS 2048
#define MAX_PACK_BYTES 0x400000

typedef struct demux_packet_st {
  int len;
  float pts;
  int pos;  // pozicio indexben (AVI) ill. fileban (MPG)
  unsigned char* buffer;
  struct demux_packet_st* next;
} demux_packet_t;

inline demux_packet_t* new_demux_packet(int len){
  demux_packet_t* dp=malloc(sizeof(demux_packet_t));
  dp->len=len;
  dp->buffer=malloc(len);
  dp->next=NULL;
  dp->pts=0;
  dp->pos=0;
  return dp;
}

inline void free_demux_packet(demux_packet_t* dp){
  free(dp->buffer);
  free(dp);
}

typedef struct {
  int buffer_pos;          // current buffer position
  int buffer_size;         // current buffer size
  unsigned char* buffer;   // current buffer
  float pts;               // current buffer's pts
  int eof;                 // end of demuxed stream? (true if all buffer empty)
  int pos;                 // position in the input stream (file)
  int dpos;                // position in the demuxed stream
//---------------
  int packs;              // number of packets in buffer
  int bytes;              // total bytes of packets in buffer
  demux_packet_t *first;  // read to current buffer from here
  demux_packet_t *last;   // append new packets from input stream to here
  int id;                 // stream ID  (for multiple audio/video streams)
  int type;               // stream type (currently used only for audio)
  struct demuxer_st *demuxer; // parent demuxer structure (stream handler)
// ---- asf -----
  demux_packet_t *asf_packet;  // read asf fragments here
  int asf_seq;
// ---- stream header ----
  void* sh;
} demux_stream_t;

demux_stream_t* new_demuxer_stream(struct demuxer_st *demuxer,int id){
  demux_stream_t* ds=malloc(sizeof(demux_stream_t));
  ds->buffer_pos=ds->buffer_size=0;
  ds->buffer=NULL;
  ds->pts=0;
  ds->eof=0;
  ds->pos=0;
  ds->dpos=0;
//---------------
  ds->packs=0;
  ds->bytes=0;
  ds->first=ds->last=NULL;
  ds->id=id;
  ds->type=-1;
  ds->demuxer=demuxer;
//----------------
  ds->asf_seq=-1;
  ds->asf_packet=NULL;
//----------------
  ds->sh=NULL;
  return ds;
}

#define DEMUXER_TYPE_UNKNOWN 0
#define DEMUXER_TYPE_MPEG_ES 1
#define DEMUXER_TYPE_MPEG_PS 2
#define DEMUXER_TYPE_AVI 3
#define DEMUXER_TYPE_AVI_NI 4
#define DEMUXER_TYPE_AVI_NINI 5
#define DEMUXER_TYPE_ASF 6

#define DEMUXER_TIME_NONE 0
#define DEMUXER_TIME_PTS 1
#define DEMUXER_TIME_FILE 2
#define DEMUXER_TIME_BPS 3

typedef struct demuxer_st {
  stream_t *stream;
  int synced;  // stream synced (used by mpeg)
  int filepos; // input stream current pos.
  int endpos;  // input stream end pos. (return EOF fi filepos>endpos)
  int type;    // mpeg system stream, mpeg elementary s., avi raw, avi indexed
//  int time_src;// time source (pts/file/bps)
  demux_stream_t *audio;
  demux_stream_t *video;
} demuxer_t;

demuxer_t* new_demuxer(stream_t *stream,int type,int a_id,int v_id){
  demuxer_t *d=malloc(sizeof(demuxer_t));
  d->stream=stream;
  d->synced=0;
  d->filepos=0;
  d->audio=new_demuxer_stream(d,a_id);
  d->video=new_demuxer_stream(d,v_id);
  d->type=type;
  return d;
}

static void ds_add_packet(demux_stream_t *ds,demux_packet_t* dp){
//    demux_packet_t* dp=new_demux_packet(len);
//    stream_read(stream,dp->buffer,len);
//    dp->pts=pts; //(float)pts/90000.0f;
//    dp->pos=pos;
    // append packet to DS stream:
    ++ds->packs;
    ds->bytes+=dp->len;
    if(ds->last){
      // next packet in stream
      ds->last->next=dp;
      ds->last=dp;
    } else {
      // first packet in stream
      ds->first=ds->last=dp;
    }
    if(verbose>=2)
      printf("DEMUX: Append packet to %s, len=%d  pts=%5.3f  pos=%d  [packs: A=%d V=%d]\n",
        (ds==ds->demuxer->audio)?"d_audio":"d_video",
        dp->len,dp->pts,dp->pos,ds->demuxer->audio->packs,ds->demuxer->video->packs);
}

static void ds_read_packet(demux_stream_t *ds,stream_t *stream,int len,float pts,int pos){
    demux_packet_t* dp=new_demux_packet(len);
    stream_read(stream,dp->buffer,len);
    dp->pts=pts; //(float)pts/90000.0f;
    dp->pos=pos;
    // append packet to DS stream:
    ds_add_packet(ds,dp);
}


// return value:
//     0 = EOF or no stream found or invalid type
//     1 = successfully read a packet
int demux_mpg_es_fill_buffer(demuxer_t *demux);
int demux_mpg_fill_buffer(demuxer_t *demux);
int demux_avi_fill_buffer(demuxer_t *demux);
int demux_avi_fill_buffer_ni(demuxer_t *demux,demux_stream_t *ds);
int demux_avi_fill_buffer_nini(demuxer_t *demux,demux_stream_t *ds);
int demux_asf_fill_buffer(demuxer_t *demux);

int demux_fill_buffer(demuxer_t *demux,demux_stream_t *ds){
  // Note: parameter 'ds' can be NULL!
  switch(demux->type){
    case DEMUXER_TYPE_MPEG_ES: return demux_mpg_es_fill_buffer(demux);
    case DEMUXER_TYPE_MPEG_PS: return demux_mpg_fill_buffer(demux);
    case DEMUXER_TYPE_AVI: return demux_avi_fill_buffer(demux);
    case DEMUXER_TYPE_AVI_NI: return demux_avi_fill_buffer_ni(demux,ds);
    case DEMUXER_TYPE_AVI_NINI: return demux_avi_fill_buffer_nini(demux,ds);
    case DEMUXER_TYPE_ASF: return demux_asf_fill_buffer(demux);
  }
  return 0;
}

// return value:
//     0 = EOF
//     1 = succesfull
inline static int ds_fill_buffer(demux_stream_t *ds){
  demuxer_t *demux=ds->demuxer;
  if(ds->buffer) free(ds->buffer);
  if(verbose>2){
    if(ds==demux->audio) printf("ds_fill_buffer(d_audio) called\n");else
    if(ds==demux->video) printf("ds_fill_buffer(d_video) called\n");else
                         printf("ds_fill_buffer(unknown 0x%X) called\n",ds);
  }
  while(1){
    if(ds->packs){
      demux_packet_t *p=ds->first;
      // copy useful data:
      ds->buffer=p->buffer;
      ds->buffer_pos=0;
      ds->buffer_size=p->len;
      ds->pos=p->pos;
      ds->dpos+=p->len; // !!!
      ds->pts=p->pts;
      // free packet:
      ds->bytes-=p->len;
      ds->first=p->next;
      if(!ds->first) ds->last=NULL;
      free(p);
      --ds->packs;
      return 1; //ds->buffer_size;
    }
    if(demux->audio->packs>=MAX_PACKS || demux->audio->bytes>=MAX_PACK_BYTES){
      printf("\nDEMUXER: Too many (%d in %d bytes) audio packets in the buffer!\n",demux->audio->packs,demux->audio->bytes);
      printf("(maybe you play a non-interleaved stream/file or audio codec failed)\n");
      break;
    }
    if(demux->video->packs>=MAX_PACKS || demux->video->bytes>=MAX_PACK_BYTES){
      printf("\nDEMUXER: Too many (%d in %d bytes) video packets in the buffer!\n",demux->video->packs,demux->video->bytes);
      printf("(maybe you play a non-interleaved stream/file or video codec failed)\n");
      break;
    }
    if(!demux_fill_buffer(demux,ds)) break; // EOF
  }
  ds->buffer_pos=ds->buffer_size=0;
  ds->buffer=NULL;
  if(verbose) printf("ds_fill_buffer: EOF reached (stream: %s)  \n",ds==demux->audio?"audio":"video");
  ds->eof=1;
  return 0;
}

inline int ds_tell(demux_stream_t *ds){
  return (ds->dpos-ds->buffer_size)+ds->buffer_pos;
}

int demux_read_data(demux_stream_t *ds,char* mem,int len){
int x;
int bytes=0;
while(len>0){
  x=ds->buffer_size-ds->buffer_pos;
  if(x==0){
    if(!ds_fill_buffer(ds)) return bytes;
  } else {
    if(x>len) x=len;
    if(mem) memcpy(mem+bytes,&ds->buffer[ds->buffer_pos],x);
    bytes+=x;len-=x;ds->buffer_pos+=x;
  }
}
return bytes;
}


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

void ds_free_packs(demux_stream_t *ds){
  demux_packet_t *dp=ds->first;
  while(dp){
    demux_packet_t *dn=dp->next;
    free(dp->buffer);
    free(dp);
    dp=dn;
  }
  if(ds->asf_packet){
    // free unfinished .asf fragments:
    free(ds->asf_packet->buffer);
    free(ds->asf_packet);
    ds->asf_packet=NULL;
  }
  ds->first=ds->last=NULL;
  ds->packs=0; // !!!!!
  ds->bytes=0;
  if(ds->buffer) free(ds->buffer);
  ds->buffer=NULL;
  ds->buffer_pos=ds->buffer_size;
  ds->pts=0;
}

int ds_get_packet(demux_stream_t *ds,char **start){
    while(1){
        int len;
        if(ds->buffer_pos>=ds->buffer_size){
          if(!ds_fill_buffer(ds)){
            // EOF
            *start = NULL;
            return -1;
          }
        }
        len=ds->buffer_size-ds->buffer_pos;
        *start = &ds->buffer[ds->buffer_pos];
        ds->buffer_pos+=len;
        return len;
    }
}
