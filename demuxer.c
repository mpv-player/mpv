//=================== DEMUXER v2.5 =========================

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>

extern int verbose; // defined in mplayer.c

#include "config.h"

#include "stream.h"
#include "demuxer.h"

#include "wine/mmreg.h"
#include "wine/avifmt.h"
#include "wine/vfw.h"

#include "codec-cfg.h"
#include "stheader.h"

demux_stream_t* new_demuxer_stream(struct demuxer_st *demuxer,int id){
  demux_stream_t* ds=malloc(sizeof(demux_stream_t));
  ds->buffer_pos=ds->buffer_size=0;
  ds->buffer=NULL;
  ds->pts=0;
  ds->pts_bytes=0;
  ds->eof=0;
  ds->pos=0;
  ds->dpos=0;
  ds->pack_no=0;
//---------------
  ds->packs=0;
  ds->bytes=0;
  ds->first=ds->last=NULL;
  ds->id=id;
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
  d->movi_start=stream->start_pos;
  d->movi_end=stream->end_pos;
  d->synced=0;
  d->filepos=0;
  d->audio=new_demuxer_stream(d,a_id);
  d->video=new_demuxer_stream(d,v_id);
  d->sub=new_demuxer_stream(d,s_id);
  d->type=type;
  return d;
}

sh_audio_t* new_sh_audio(demuxer_t *demuxer,int id){
    if(demuxer->a_streams[id]){
        printf("Warning! Audio stream header %d redefined!\n",id);
    } else {
        if(verbose) printf("==> Found audio stream: %d\n",id);
        demuxer->a_streams[id]=malloc(sizeof(sh_audio_t));
        memset(demuxer->a_streams[id],0,sizeof(sh_audio_t));
    }
    return demuxer->a_streams[id];
}

sh_video_t* new_sh_video(demuxer_t *demuxer,int id){
    if(demuxer->v_streams[id]){
        printf("Warning! video stream header %d redefined!\n",id);
    } else {
        if(verbose) printf("==> Found video stream: %d\n",id);
        demuxer->v_streams[id]=malloc(sizeof(sh_video_t));
        memset(demuxer->v_streams[id],0,sizeof(sh_video_t));
    }
    return demuxer->v_streams[id];
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
      printf("DEMUX: Append packet to %s, len=%d  pts=%5.3f  pos=%u  [packs: A=%d V=%d]\n",
        (ds==ds->demuxer->audio)?"d_audio":"d_video",
        dp->len,dp->pts,(unsigned int)dp->pos,ds->demuxer->audio->packs,ds->demuxer->video->packs);
}

void ds_read_packet(demux_stream_t *ds,stream_t *stream,int len,float pts,off_t pos,int flags){
    demux_packet_t* dp=new_demux_packet(len);
    stream_read(stream,dp->buffer,len);
    dp->pts=pts; //(float)pts/90000.0f;
    dp->pos=pos;
    dp->flags=flags;
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
//  printf("demux->type=%d\n",demux->type);
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
      ++ds->pack_no;
      if(p->pts){
        ds->pts=p->pts;
        ds->pts_bytes=0;
      }
      ds->pts_bytes+=p->len; // !!!
      ds->flags=p->flags;
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
    if(!demux_fill_buffer(demux,ds)){
       if(verbose) printf("ds_fill_buffer()->demux_fill_buffer() failed\n");
       break; // EOF
    }
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

int demux_read_data_pack(demux_stream_t *ds,unsigned char* mem,int len){
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
    return bytes; // stop at end of package! (for correct timestamping)
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
  ds->pts=0; ds->pts_bytes=0;
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

// ====================================================================

// feed-back from demuxers:
extern int num_elementary_packets100; // for MPEG-ES fileformat detection
extern int num_elementary_packets101;
extern int num_elementary_packetsPES;

// commandline options, flags:
extern int seek_to_byte;
extern int force_ni;
extern int pts_from_bps;

extern int audio_id;
extern int video_id;
extern int dvdsub_id;

void read_avi_header(demuxer_t *demuxer,int index_mode);
int asf_check_header(demuxer_t *demuxer);
int read_asf_header(demuxer_t *demuxer);
demux_stream_t* demux_avi_select_stream(demuxer_t *demux,unsigned int id);


demuxer_t* demux_open(stream_t *stream,int file_format,int audio_id,int video_id,int dvdsub_id){

//int file_format=(*file_format_ptr);

demuxer_t *demuxer=NULL;

demux_stream_t *d_audio=NULL;
demux_stream_t *d_video=NULL;

sh_audio_t *sh_audio=NULL;
sh_video_t *sh_video=NULL;

//=============== Try to open as AVI file: =================
if(file_format==DEMUXER_TYPE_UNKNOWN || file_format==DEMUXER_TYPE_AVI){
  stream_reset(stream);
  demuxer=new_demuxer(stream,DEMUXER_TYPE_AVI,audio_id,video_id,dvdsub_id);
  stream_seek(demuxer->stream,seek_to_byte);
  { //---- RIFF header:
    int id=stream_read_dword_le(demuxer->stream); // "RIFF"
    if(id==mmioFOURCC('R','I','F','F')){
      stream_read_dword_le(demuxer->stream); //filesize
      id=stream_read_dword_le(demuxer->stream); // "AVI "
      if(id==formtypeAVI){ 
        printf("Detected AVI file format!\n");
        file_format=DEMUXER_TYPE_AVI;
      }
    }
  }
}
//=============== Try to open as ASF file: =================
if(file_format==DEMUXER_TYPE_UNKNOWN || file_format==DEMUXER_TYPE_ASF){
  stream_reset(stream);
  demuxer=new_demuxer(stream,DEMUXER_TYPE_ASF,audio_id,video_id,dvdsub_id);
  stream_seek(demuxer->stream,seek_to_byte);
  if(asf_check_header(demuxer)){
      printf("Detected ASF file format!\n");
      file_format=DEMUXER_TYPE_ASF;
  }
}
//=============== Try to open as MPEG-PS file: =================
if(file_format==DEMUXER_TYPE_UNKNOWN || file_format==DEMUXER_TYPE_MPEG_PS){
 int pes=1;
 while(pes>=0){
  stream_reset(stream);
  demuxer=new_demuxer(stream,DEMUXER_TYPE_MPEG_PS,audio_id,video_id,dvdsub_id);
  stream_seek(demuxer->stream,seek_to_byte);
  if(!pes) demuxer->synced=1; // hack!
  if(ds_fill_buffer(demuxer->video)){
    if(!pes)
      printf("Detected MPEG-PES file format!\n");
    else
      printf("Detected MPEG-PS file format!\n");
    file_format=DEMUXER_TYPE_MPEG_PS;
  } else {
    // some hack to get meaningfull error messages to our unhappy users:
    if(num_elementary_packets100>=2 && num_elementary_packets101>=2 &&
       abs(num_elementary_packets101-num_elementary_packets100)<8){
      if(num_elementary_packetsPES>=4 && num_elementary_packetsPES>=num_elementary_packets100-4){
        --pes;continue; // tricky...
      }
      file_format=DEMUXER_TYPE_MPEG_ES; //  <-- hack is here :)
    } else {
      if(demuxer->synced==2)
        printf("Missing MPEG video stream!? contact the author, it may be a bug :(\n");
      else
        printf("Not MPEG System Stream format... (maybe Transport Stream?)\n");
    }
  }
  break;
 }
}
//=============== Try to open as MPEG-ES file: =================
if(file_format==DEMUXER_TYPE_MPEG_ES){ // little hack, see above!
  stream_reset(stream);
  demuxer=new_demuxer(stream,DEMUXER_TYPE_MPEG_ES,audio_id,video_id,dvdsub_id);
  stream_seek(demuxer->stream,seek_to_byte);
  if(!ds_fill_buffer(demuxer->video)){
    printf("Invalid MPEG-ES stream??? contact the author, it may be a bug :(\n");
    file_format=DEMUXER_TYPE_UNKNOWN;
  } else {
    printf("Detected MPEG-ES file format!\n");
  }
}
//=============== Try to open as MOV file: =================
#if 1
if(file_format==DEMUXER_TYPE_UNKNOWN || file_format==DEMUXER_TYPE_MOV){
  stream_reset(stream);
  demuxer=new_demuxer(stream,DEMUXER_TYPE_MOV,audio_id,video_id,dvdsub_id);
  stream_seek(demuxer->stream,seek_to_byte);
  if(mov_check_file(demuxer)){
      printf("Detected QuickTime/MOV file format!\n");
      file_format=DEMUXER_TYPE_MOV;
  }
}
#endif
//=============== Unknown, exiting... ===========================
if(file_format==DEMUXER_TYPE_UNKNOWN){
  fprintf(stderr,"============= Sorry, this file format not recognized/supported ===============\n");
  fprintf(stderr,"=== If this file is an AVI, ASF or MPEG stream, please contact the author! ===\n");
  return NULL;
//  GUI_MSG( mplUnknowFileType )
//  exit(1);
}
//====== File format recognized, set up these for compatibility: =========
d_audio=demuxer->audio;
d_video=demuxer->video;
//d_dvdsub=demuxer->sub;

demuxer->file_format=file_format;

switch(file_format){
 case DEMUXER_TYPE_MOV: {
  mov_read_header(demuxer);
  break;
 }
 case DEMUXER_TYPE_AVI: {
  return demux_open_avi(demuxer);
//  break;
 }
 case DEMUXER_TYPE_ASF: {
  //---- ASF header:
  read_asf_header(demuxer);
  stream_reset(demuxer->stream);
  stream_seek(demuxer->stream,demuxer->movi_start);
//  demuxer->idx_pos=0;
//  demuxer->endpos=avi_header.movi_end;
  if(!ds_fill_buffer(d_video)){
    printf("ASF: no video stream found!\n");
    sh_video=NULL;
    //printf("ASF: missing video stream!? contact the author, it may be a bug :(\n");
    //GUI_MSG( mplASFErrorMissingVideoStream )
    //exit(1);
  } else {
    sh_video=d_video->sh;sh_video->ds=d_video;
    sh_video->fps=1000.0f; sh_video->frametime=0.001f; // 1ms
    printf("VIDEO:  [%.4s]  %ldx%ld  %dbpp\n",
      (char *)&sh_video->bih->biCompression,
      sh_video->bih->biWidth,
      sh_video->bih->biHeight,
      sh_video->bih->biBitCount);
//      sh_video->i_bps=10*asf_packetsize; // FIXME!
  }
  if(audio_id!=-2){
    if(verbose) printf("ASF: Searching for audio stream (id:%d)\n",d_audio->id);
    if(!ds_fill_buffer(d_audio)){
      printf("ASF: No Audio stream found...  ->nosound\n");
      sh_audio=NULL;
    } else {
      sh_audio=d_audio->sh;sh_audio->ds=d_audio;
      sh_audio->format=sh_audio->wf->wFormatTag;
    }
  }
  break;
 }
 case DEMUXER_TYPE_MPEG_ES: {
   sh_audio=NULL;   // ES streams has no audio channel
   d_video->sh=new_sh_video(demuxer,0); // create dummy video stream header, id=0
   sh_video=d_video->sh;sh_video->ds=d_video;
   break;
 }
 case DEMUXER_TYPE_MPEG_PS: {
  sh_video=d_video->sh;sh_video->ds=d_video;
  if(audio_id!=-2) {
   if(!ds_fill_buffer(d_audio)){
    printf("MPEG: No Audio stream found...  ->nosound\n");
    sh_audio=NULL;
   } else {
    sh_audio=d_audio->sh;sh_audio->ds=d_audio;
    switch(d_audio->id & 0xE0){  // 1110 0000 b  (high 3 bit: type  low 5: id)
      case 0x00: sh_audio->format=0x50;break; // mpeg
      case 0xA0: sh_audio->format=0x10001;break;  // dvd pcm
      case 0x80: sh_audio->format=0x2000;break; // ac3
      default: sh_audio=NULL; // unknown type
    }
   }
  }
  break;
 }
} // switch(file_format)

return demuxer;
}
