//=================== DEMUXER v2.5 =========================

#include <stdio.h>
#include <stdlib.h>

extern int verbose; // defined in mplayer.c

#include "stream.h"
#include "demuxer.h"

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

demuxer_t* new_demuxer(stream_t *stream,int type,int a_id,int v_id,int s_id){
  demuxer_t *d=malloc(sizeof(demuxer_t));
  memset(d,0,sizeof(demuxer_t));
  d->stream=stream;
  d->synced=0;
  d->filepos=0;
  d->audio=new_demuxer_stream(d,a_id);
  d->video=new_demuxer_stream(d,v_id);
  d->sub=new_demuxer_stream(d,s_id);
  d->type=type;
  return d;
}

void ds_add_packet(demux_stream_t *ds,demux_packet_t* dp){
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

void ds_read_packet(demux_stream_t *ds,stream_t *stream,int len,float pts,int pos){
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
int ds_fill_buffer(demux_stream_t *ds){
  demuxer_t *demux=ds->demuxer;
  if(ds->buffer) free(ds->buffer);
  if(verbose>2){
    if(ds==demux->audio) printf("ds_fill_buffer(d_audio) called\n");else
    if(ds==demux->video) printf("ds_fill_buffer(d_video) called\n");else
                         printf("ds_fill_buffer(unknown 0x%X) called\n",(unsigned int)ds);
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

int demux_read_data(demux_stream_t *ds,unsigned char* mem,int len){
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

int ds_get_packet(demux_stream_t *ds,unsigned char **start){
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

int ds_get_packet_sub(demux_stream_t *ds,unsigned char **start){
    while(1){
        int len;
        if(ds->buffer_pos>=ds->buffer_size){
          *start = NULL;
          if(!ds->packs) return -1; // no sub
          if(!ds_fill_buffer(ds)) return -1; // EOF
        }
        len=ds->buffer_size-ds->buffer_pos;
        *start = &ds->buffer[ds->buffer_pos];
        ds->buffer_pos+=len;
        return len;
    }
}

