//=================== DEMUXER v2.5 =========================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"
#include "../cfgparser.h"

#include "stream.h"
#include "demuxer.h"
#include "stheader.h"
#include "mf.h"

#include "../libvo/fastmemcpy.h"

void free_demuxer_stream(demux_stream_t *ds){
    ds_free_packs(ds);
    free(ds);
}

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
  ds->ss_mul=ds->ss_div=1;
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
  d->seekable=1;
  d->synced=0;
  d->filepos=0;
  d->audio=new_demuxer_stream(d,a_id);
  d->video=new_demuxer_stream(d,v_id);
  d->sub=new_demuxer_stream(d,s_id);
  d->type=type;
  stream_reset(stream);
  stream_seek(stream,stream->start_pos);
  return d;
}

sh_audio_t *get_sh_audio(demuxer_t *demuxer, int id)
{
    if(id > MAX_A_STREAMS-1 || id < 0)
    {
	mp_msg(MSGT_DEMUXER,MSGL_WARN,"Requested audio stream id overflow (%d > %d)\n",
	    id, MAX_A_STREAMS);
	return NULL;
    }
    return demuxer->a_streams[id];
}

sh_audio_t* new_sh_audio(demuxer_t *demuxer,int id){
    if(id > MAX_A_STREAMS-1 || id < 0)
    {
	mp_msg(MSGT_DEMUXER,MSGL_WARN,"Requested audio stream id overflow (%d > %d)\n",
	    id, MAX_A_STREAMS);
	return NULL;
    }
    if(demuxer->a_streams[id]){
        mp_msg(MSGT_DEMUXER,MSGL_WARN,MSGTR_AudioStreamRedefined,id);
    } else {
        mp_msg(MSGT_DEMUXER,MSGL_V,"==> Found audio stream: %d\n",id);
        demuxer->a_streams[id]=malloc(sizeof(sh_audio_t));
        memset(demuxer->a_streams[id],0,sizeof(sh_audio_t));
    }
    return demuxer->a_streams[id];
}

void free_sh_audio(sh_audio_t* sh){
    if(sh->a_in_buffer) free(sh->a_in_buffer);
    if(sh->a_buffer) free(sh->a_buffer);
    if(sh->wf) free(sh->wf);
    free(sh);
}

sh_video_t *get_sh_video(demuxer_t *demuxer, int id)
{
    if(id > MAX_V_STREAMS-1 || id < 0)
    {
	mp_msg(MSGT_DEMUXER,MSGL_WARN,"Requested video stream id overflow (%d > %d)\n",
	    id, MAX_V_STREAMS);
	return NULL;
    }
    return demuxer->v_streams[id];
}

sh_video_t* new_sh_video(demuxer_t *demuxer,int id){
    if(id > MAX_V_STREAMS-1 || id < 0)
    {
	mp_msg(MSGT_DEMUXER,MSGL_WARN,"Requested video stream id overflow (%d > %d)\n",
	    id, MAX_V_STREAMS);
	return NULL;
    }
    if(demuxer->v_streams[id]){
        mp_msg(MSGT_DEMUXER,MSGL_WARN,MSGTR_VideoStreamRedefined,id);
    } else {
        mp_msg(MSGT_DEMUXER,MSGL_V,"==> Found video stream: %d\n",id);
        demuxer->v_streams[id]=malloc(sizeof(sh_video_t));
        memset(demuxer->v_streams[id],0,sizeof(sh_video_t));
    }
    return demuxer->v_streams[id];
}

void free_sh_video(sh_video_t* sh){
    if(sh->our_out_buffer) free(sh->our_out_buffer);
    if(sh->bih) free(sh->bih);
    free(sh);
}

void free_demuxer(demuxer_t *demuxer){
    int i;
    // free streams:
    for(i=0;i<256;i++){
	if(demuxer->a_streams[i]) free_sh_audio(demuxer->a_streams[i]);
	if(demuxer->v_streams[i]) free_sh_video(demuxer->v_streams[i]);
    }
    //if(sh_audio) free_sh_audio(sh_audio);
    //if(sh_video) free_sh_video(sh_video);
    // free demuxers:
    free_demuxer_stream(demuxer->audio);
    free_demuxer_stream(demuxer->video);
    if(demuxer->info) {
      for(i=0;demuxer->info[i] != NULL; i++)
	free(demuxer->info[i]);
      free(demuxer->info);
    }
    free(demuxer);
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
    mp_dbg(MSGT_DEMUXER,MSGL_DBG2,"DEMUX: Append packet to %s, len=%d  pts=%5.3f  pos=%u  [packs: A=%d V=%d]\n",
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
int demux_mf_fill_buffer( demuxer_t *demux);
int demux_roq_fill_buffer(demuxer_t *demux);
int demux_film_fill_buffer(demuxer_t *demux);
int demux_fli_fill_buffer(demuxer_t *demux);
int demux_mpg_es_fill_buffer(demuxer_t *demux);
int demux_mpg_fill_buffer(demuxer_t *demux);
int demux_avi_fill_buffer(demuxer_t *demux);
int demux_avi_fill_buffer_ni(demuxer_t *demux,demux_stream_t *ds);
int demux_avi_fill_buffer_nini(demuxer_t *demux,demux_stream_t *ds);
int demux_asf_fill_buffer(demuxer_t *demux);
int demux_mov_fill_buffer(demuxer_t *demux,demux_stream_t* ds);
int demux_vivo_fill_buffer(demuxer_t *demux);
int demux_real_fill_buffer(demuxer_t *demuxer);
int demux_nuv_fill_buffer(demuxer_t *demux);
#ifdef USE_TV
#include "tv.h"
extern tvi_handle_t *tv_handler;
extern int tv_param_on;

extern int demux_tv_fill_buffer(demuxer_t *demux, tvi_handle_t *tvh);
extern int demux_open_tv(demuxer_t *demuxer, tvi_handle_t *tvh);
#endif
int demux_y4m_fill_buffer(demuxer_t *demux);
int demux_audio_fill_buffer(demux_stream_t *ds);
extern int demux_demuxers_fill_buffer(demuxer_t *demux,demux_stream_t *ds);

int demux_fill_buffer(demuxer_t *demux,demux_stream_t *ds){
  // Note: parameter 'ds' can be NULL!
//  printf("demux->type=%d\n",demux->type);
  switch(demux->type){
    case DEMUXER_TYPE_MF: return demux_mf_fill_buffer(demux);
    case DEMUXER_TYPE_ROQ: return demux_roq_fill_buffer(demux);
    case DEMUXER_TYPE_FILM: return demux_film_fill_buffer(demux);
    case DEMUXER_TYPE_FLI: return demux_fli_fill_buffer(demux);
    case DEMUXER_TYPE_MPEG_ES: return demux_mpg_es_fill_buffer(demux);
    case DEMUXER_TYPE_MPEG_PS: return demux_mpg_fill_buffer(demux);
    case DEMUXER_TYPE_AVI: return demux_avi_fill_buffer(demux);
    case DEMUXER_TYPE_AVI_NI: return demux_avi_fill_buffer_ni(demux,ds);
    case DEMUXER_TYPE_AVI_NINI: return demux_avi_fill_buffer_nini(demux,ds);
    case DEMUXER_TYPE_ASF: return demux_asf_fill_buffer(demux);
    case DEMUXER_TYPE_MOV: return demux_mov_fill_buffer(demux,ds);
    case DEMUXER_TYPE_VIVO: return demux_vivo_fill_buffer(demux);
    case DEMUXER_TYPE_REAL: return demux_real_fill_buffer(demux);
    case DEMUXER_TYPE_NUV: return demux_nuv_fill_buffer(demux);
#ifdef USE_TV
    case DEMUXER_TYPE_TV: return demux_tv_fill_buffer(demux, tv_handler);
#endif
    case DEMUXER_TYPE_Y4M: return demux_y4m_fill_buffer(demux);
    case DEMUXER_TYPE_AUDIO: return demux_audio_fill_buffer(ds);
    case DEMUXER_TYPE_DEMUXERS: return demux_demuxers_fill_buffer(demux,ds);
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
    if(ds==demux->audio) mp_dbg(MSGT_DEMUXER,MSGL_DBG3,"ds_fill_buffer(d_audio) called\n");else
    if(ds==demux->video) mp_dbg(MSGT_DEMUXER,MSGL_DBG3,"ds_fill_buffer(d_video) called\n");else
                         mp_dbg(MSGT_DEMUXER,MSGL_DBG3,"ds_fill_buffer(unknown 0x%X) called\n",(unsigned int)ds);
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
      mp_msg(MSGT_DEMUXER,MSGL_ERR,MSGTR_TooManyAudioInBuffer,demux->audio->packs,demux->audio->bytes);
      mp_msg(MSGT_DEMUXER,MSGL_HINT,MSGTR_MaybeNI);
      break;
    }
    if(demux->video->packs>=MAX_PACKS || demux->video->bytes>=MAX_PACK_BYTES){
      mp_msg(MSGT_DEMUXER,MSGL_ERR,MSGTR_TooManyVideoInBuffer,demux->video->packs,demux->video->bytes);
      mp_msg(MSGT_DEMUXER,MSGL_HINT,MSGTR_MaybeNI);
      break;
    }
    if(!demux_fill_buffer(demux,ds)){
       mp_dbg(MSGT_DEMUXER,MSGL_DBG2,"ds_fill_buffer()->demux_fill_buffer() failed\n");
       break; // EOF
    }
  }
  ds->buffer_pos=ds->buffer_size=0;
  ds->buffer=NULL;
  mp_msg(MSGT_DEMUXER,MSGL_V,"ds_fill_buffer: EOF reached (stream: %s)  \n",ds==demux->audio?"audio":"video");
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
extern int num_elementary_packets1B6;
extern int num_mp3audio_packets;

// commandline options, flags:
//extern int seek_to_byte;
extern int force_ni;
extern int pts_from_bps;

extern int audio_id;
extern int video_id;
extern int dvdsub_id;

int asf_check_header(demuxer_t *demuxer);
int read_asf_header(demuxer_t *demuxer);
demux_stream_t* demux_avi_select_stream(demuxer_t *demux,unsigned int id);
demuxer_t* demux_open_avi(demuxer_t* demuxer);
int mov_check_file(demuxer_t* demuxer);
int mov_read_header(demuxer_t* demuxer);
int demux_open_fli(demuxer_t* demuxer);
int demux_open_mf(demuxer_t* demuxer);
int demux_open_film(demuxer_t* demuxer);
int demux_open_roq(demuxer_t* demuxer);

extern int vivo_check_file(demuxer_t *demuxer);
extern void demux_open_vivo(demuxer_t *demuxer);
extern int y4m_check_file(demuxer_t *demuxer);
extern void demux_open_y4m(demuxer_t *demuxer);
extern int roq_check_file(demuxer_t *demuxer);

extern int real_check_file(demuxer_t *demuxer);
extern void demux_open_real(demuxer_t *demuxer);

extern int nuv_check_file(demuxer_t *demuxer);
extern void demux_open_nuv(demuxer_t *demuxer);
extern int demux_audio_open(demuxer_t* demuxer);


static demuxer_t* demux_open_stream(stream_t *stream,int file_format,int audio_id,int video_id,int dvdsub_id){

//int file_format=(*file_format_ptr);

demuxer_t *demuxer=NULL;

demux_stream_t *d_audio=NULL;
demux_stream_t *d_video=NULL;

sh_audio_t *sh_audio=NULL;
sh_video_t *sh_video=NULL;

//printf("demux_open(%p,%d,%d,%d,%d)  \n",stream,file_format,audio_id,video_id,dvdsub_id);

if ( mf_support )
 {
  mp_msg(MSGT_DEMUXER,MSGL_INFO,"forced mf.\n");
  file_format=DEMUXER_TYPE_MF;
 }

#ifdef USE_TV
//=============== Try to open as TV-input: =================
if((tv_param_on == 1) &&
    (file_format==DEMUXER_TYPE_UNKNOWN || file_format==DEMUXER_TYPE_TV)){
  demuxer=new_demuxer(stream,DEMUXER_TYPE_TV,audio_id,video_id,dvdsub_id);
  mp_msg(MSGT_DEMUXER,MSGL_INFO,"Detected TV! ;-)\n");
  file_format=DEMUXER_TYPE_TV;
}
#endif

//=============== Try to open as AVI file: =================
if(file_format==DEMUXER_TYPE_UNKNOWN || file_format==DEMUXER_TYPE_AVI){
  demuxer=new_demuxer(stream,DEMUXER_TYPE_AVI,audio_id,video_id,dvdsub_id);
  { //---- RIFF header:
    int id=stream_read_dword_le(demuxer->stream); // "RIFF"
    if(id==mmioFOURCC('R','I','F','F')){
      stream_read_dword_le(demuxer->stream); //filesize
      id=stream_read_dword_le(demuxer->stream); // "AVI "
      if(id==formtypeAVI){ 
        mp_msg(MSGT_DEMUXER,MSGL_INFO,MSGTR_DetectedAVIfile);
        file_format=DEMUXER_TYPE_AVI;
      } else {
        free_demuxer(demuxer);
        demuxer = NULL;
      }	
    }
  }
}
//=============== Try to open as Y4M file: =================
if(file_format==DEMUXER_TYPE_UNKNOWN || file_format==DEMUXER_TYPE_Y4M){
  demuxer=new_demuxer(stream,DEMUXER_TYPE_Y4M,audio_id,video_id,dvdsub_id);
  if(y4m_check_file(demuxer)){
      mp_msg(MSGT_DEMUXER,MSGL_INFO,"Detected YUV4MPEG2 file format!\n");
      file_format=DEMUXER_TYPE_Y4M;
  } else {
      free_demuxer(demuxer);
      demuxer = NULL;
  }
}
//=============== Try to open as ASF file: =================
if(file_format==DEMUXER_TYPE_UNKNOWN || file_format==DEMUXER_TYPE_ASF){
  demuxer=new_demuxer(stream,DEMUXER_TYPE_ASF,audio_id,video_id,dvdsub_id);
  if(asf_check_header(demuxer)){
      mp_msg(MSGT_DEMUXER,MSGL_INFO,MSGTR_DetectedASFfile);
      file_format=DEMUXER_TYPE_ASF;
  } else {
      free_demuxer(demuxer);
      demuxer = NULL;
  }
}
//=============== Try to open as NUV file: =================
if(file_format==DEMUXER_TYPE_UNKNOWN || file_format==DEMUXER_TYPE_NUV){
  demuxer=new_demuxer(stream,DEMUXER_TYPE_NUV,audio_id,video_id,dvdsub_id);
  if(nuv_check_file(demuxer)){
      mp_msg(MSGT_DEMUXER,MSGL_INFO,"Detected NuppelVideo file format!\n");
      file_format=DEMUXER_TYPE_NUV;
  } else {
      free_demuxer(demuxer);
      demuxer = NULL;
  }
}
//=============== Try to open as MOV file: =================
if(file_format==DEMUXER_TYPE_UNKNOWN || file_format==DEMUXER_TYPE_MOV){
  demuxer=new_demuxer(stream,DEMUXER_TYPE_MOV,audio_id,video_id,dvdsub_id);
  if(mov_check_file(demuxer)){
      mp_msg(MSGT_DEMUXER,MSGL_INFO,MSGTR_DetectedQTMOVfile);
      file_format=DEMUXER_TYPE_MOV;
  } else {
      free_demuxer(demuxer);
      demuxer = NULL;
  }
}
//=============== Try to open as VIVO file: =================
if(file_format==DEMUXER_TYPE_UNKNOWN || file_format==DEMUXER_TYPE_VIVO){
  demuxer=new_demuxer(stream,DEMUXER_TYPE_VIVO,audio_id,video_id,dvdsub_id);
  if(vivo_check_file(demuxer)){
      mp_msg(MSGT_DEMUXER,MSGL_INFO,"Detected VIVO file format!\n");
      file_format=DEMUXER_TYPE_VIVO;
  } else {
    free_demuxer(demuxer);
    demuxer = NULL;
  }
}
//=============== Try to open as REAL file: =================
if(file_format==DEMUXER_TYPE_UNKNOWN || file_format==DEMUXER_TYPE_REAL){
  demuxer=new_demuxer(stream,DEMUXER_TYPE_REAL,audio_id,video_id,dvdsub_id);
  if(real_check_file(demuxer)){
      mp_msg(MSGT_DEMUXER,MSGL_INFO,MSGTR_DetectedREALfile);
      file_format=DEMUXER_TYPE_REAL;
  } else {
      free_demuxer(demuxer);
      demuxer = NULL;
  }
}
//=============== Try to open as FLI file: =================
if(file_format==DEMUXER_TYPE_UNKNOWN || file_format==DEMUXER_TYPE_FLI){
  demuxer=new_demuxer(stream,DEMUXER_TYPE_FLI,audio_id,video_id,dvdsub_id);
  {
    int id;
    stream_seek(demuxer->stream, 4);
    id=stream_read_word_le(demuxer->stream);
    // check for the FLI file magic number
    if((id==0xAF11) || (id==0xAF12)){ 
      mp_msg(MSGT_DEMUXER,MSGL_INFO,MSGTR_DetectedFLIfile);
      file_format=DEMUXER_TYPE_FLI;
    } else {
      free_demuxer(demuxer);
      demuxer = NULL;
    }
  }
}
//=============== Try to open as FILM file: =================
if(file_format==DEMUXER_TYPE_UNKNOWN || file_format==DEMUXER_TYPE_FILM){
  demuxer=new_demuxer(stream,DEMUXER_TYPE_FILM,audio_id,video_id,dvdsub_id);
  {
    int signature=stream_read_fourcc(demuxer->stream);
    // check for the FLI file magic number
    if(signature==mmioFOURCC('F', 'I', 'L', 'M')){ 
      mp_msg(MSGT_DEMUXER,MSGL_INFO,MSGTR_DetectedFILMfile);
      file_format=DEMUXER_TYPE_FILM;
    } else {
      free_demuxer(demuxer);
      demuxer = NULL;
    }
  }
}
//=============== Try to open as RoQ file: =================
if(file_format==DEMUXER_TYPE_UNKNOWN || file_format==DEMUXER_TYPE_ROQ){
  demuxer=new_demuxer(stream,DEMUXER_TYPE_ROQ,audio_id,video_id,dvdsub_id);
  if(roq_check_file(demuxer)){
      mp_msg(MSGT_DEMUXER,MSGL_INFO,MSGTR_DetectedROQfile);
      file_format=DEMUXER_TYPE_ROQ;
  } else {
      free_demuxer(demuxer);
      demuxer = NULL;
  }
}
//=============== Try to open as MPEG-PS file: =================
if(file_format==DEMUXER_TYPE_UNKNOWN || file_format==DEMUXER_TYPE_MPEG_PS){
 int pes=1;
 int tmp;
 off_t tmppos;
 while(pes>=0){
  demuxer=new_demuxer(stream,DEMUXER_TYPE_MPEG_PS,audio_id,video_id,dvdsub_id);
  
  // try to pre-detect PES:
  tmppos=stream_tell(demuxer->stream);
  tmp=stream_read_dword(demuxer->stream);
  if(tmp==0x1E0 || tmp==0x1C0){
      tmp=stream_read_word(demuxer->stream);
      if(tmp>1 && tmp<=2048) pes=0; // demuxer->synced=3; // PES...
  }
  stream_seek(demuxer->stream,tmppos);
  
  if(!pes) demuxer->synced=3; // hack!

  num_elementary_packets100=0;
  num_elementary_packets101=0;
  num_elementary_packets1B6=0;
  num_elementary_packetsPES=0;
  num_mp3audio_packets=0;

  if(ds_fill_buffer(demuxer->video)){
    if(!pes)
      mp_msg(MSGT_DEMUXER,MSGL_INFO,MSGTR_DetectedMPEGPESfile);
    else
      mp_msg(MSGT_DEMUXER,MSGL_INFO,MSGTR_DetectedMPEGPSfile);
    file_format=DEMUXER_TYPE_MPEG_PS;
  } else {
    printf("MPEG packet stats: p100: %d  p101: %d  PES: %d  MP3: %d \n",
	num_elementary_packets100,num_elementary_packets101,num_elementary_packetsPES,num_mp3audio_packets);
    if(num_mp3audio_packets>50 && num_mp3audio_packets>2*num_elementary_packets100)
	break; // it's .MP3
    // some hack to get meaningfull error messages to our unhappy users:
    if(num_elementary_packets100>=2 && num_elementary_packets101>=2 &&
       abs(num_elementary_packets101+8-num_elementary_packets100)<16){
      if(num_elementary_packetsPES>=4 && num_elementary_packetsPES>=num_elementary_packets100-4){
        --pes;continue; // tricky...
      }
      file_format=DEMUXER_TYPE_MPEG_ES; //  <-- hack is here :)
    } else {
      if(demuxer->synced==2)
        mp_msg(MSGT_DEMUXER,MSGL_ERR,"MPEG: " MSGTR_MissingVideoStreamBug);
      else
        mp_msg(MSGT_DEMUXER,MSGL_V,"Not MPEG System Stream format... (maybe Transport Stream?)\n");
      free_demuxer(demuxer);
      demuxer = NULL;
    }
  }
  break;
 }
}
//=============== Try to open as MPEG-ES file: =================
if(file_format==DEMUXER_TYPE_MPEG_ES){ // little hack, see above!
  demuxer=new_demuxer(stream,DEMUXER_TYPE_MPEG_ES,audio_id,video_id,dvdsub_id);
  if(!ds_fill_buffer(demuxer->video)){
    mp_msg(MSGT_DEMUXER,MSGL_ERR,MSGTR_InvalidMPEGES);
    file_format=DEMUXER_TYPE_UNKNOWN;
    free_demuxer(demuxer);
    demuxer = NULL;
  } else {
    mp_msg(MSGT_DEMUXER,MSGL_INFO,MSGTR_DetectedMPEGESfile);
  }
}
//=============== Try to open as multi file: =================
if(file_format==DEMUXER_TYPE_UNKNOWN || file_format==DEMUXER_TYPE_MF){
  demuxer=new_demuxer(stream,DEMUXER_TYPE_MF,audio_id,video_id,dvdsub_id);
  {
   if ( mf_support )
    {
     file_format=DEMUXER_TYPE_MF;
     mp_msg( MSGT_DEMUXER,MSGL_INFO,"[demuxer] mf support.\n" );
    }
  }
}
//=============== Try to open as audio file: =================
if(file_format==DEMUXER_TYPE_UNKNOWN || file_format==DEMUXER_TYPE_AUDIO){
  demuxer=new_demuxer(stream,DEMUXER_TYPE_AUDIO,audio_id,video_id,dvdsub_id);
  if(demux_audio_open(demuxer)){
    mp_msg(MSGT_DEMUXER,MSGL_INFO,"Detected audio file\n");
    file_format=DEMUXER_TYPE_AUDIO;
  } else {
    free_demuxer(demuxer);
    demuxer = NULL;
  }
}
//=============== Unknown, exiting... ===========================
if(file_format==DEMUXER_TYPE_UNKNOWN || demuxer == NULL){
  mp_msg(MSGT_DEMUXER,MSGL_ERR,MSGTR_FormatNotRecognized);
  return NULL;
//  GUI_MSG( mplUnknowFileType )
}
//====== File format recognized, set up these for compatibility: =========
d_audio=demuxer->audio;
d_video=demuxer->video;
//d_dvdsub=demuxer->sub;

demuxer->file_format=file_format;

switch(file_format){
 case DEMUXER_TYPE_MF: {
  if (!demux_open_mf(demuxer)) return NULL;
  break;
 }
 case DEMUXER_TYPE_FLI: {
  if (!demux_open_fli(demuxer)) return NULL;
  break;
 }
 case DEMUXER_TYPE_FILM: {
  if (!demux_open_film(demuxer)) return NULL;
  break;
 }
 case DEMUXER_TYPE_ROQ: {
  if (!demux_open_roq(demuxer)) return NULL;
  break;
 }
 case DEMUXER_TYPE_MOV: {
  if(!mov_read_header(demuxer)) return NULL;
//  sh_video=d_video->sh;if(sh_video) sh_video->ds=d_video;
//  sh_audio=d_audio->sh;if(sh_audio) sh_audio->ds=d_audio;
  break;
 }
 case DEMUXER_TYPE_AVI: {
  return (demuxer_t*) demux_open_avi(demuxer);
//  break;
 }
 case DEMUXER_TYPE_NUV: {
  demux_open_nuv(demuxer);
  break;
 }
 case DEMUXER_TYPE_VIVO: {
  demux_open_vivo(demuxer);
  break;
 }
 case DEMUXER_TYPE_Y4M: {
  demux_open_y4m(demuxer);
  break;
 }
 case DEMUXER_TYPE_REAL: {
  demux_open_real(demuxer);
  break;
 }
 case DEMUXER_TYPE_ASF: {
  //---- ASF header:
  read_asf_header(demuxer);
  stream_reset(demuxer->stream);
  stream_seek(demuxer->stream,demuxer->movi_start);
//  demuxer->idx_pos=0;
//  demuxer->endpos=avi_header.movi_end;
  if(d_video->id != -2) {
    if(!ds_fill_buffer(d_video)){
      mp_msg(MSGT_DEMUXER,MSGL_WARN,"ASF: " MSGTR_MissingVideoStream);
      sh_video=NULL;
      //printf("ASF: missing video stream!? contact the author, it may be a bug :(\n");
      //GUI_MSG( mplASFErrorMissingVideoStream )
    } else {
      sh_video=d_video->sh;sh_video->ds=d_video;
      sh_video->fps=1000.0f; sh_video->frametime=0.001f; // 1ms
      mp_msg(MSGT_DEMUXER,MSGL_INFO,"VIDEO:  [%.4s]  %ldx%ld  %dbpp\n",
        (char *)&sh_video->bih->biCompression,
        sh_video->bih->biWidth,
        sh_video->bih->biHeight,
        sh_video->bih->biBitCount);
      //      sh_video->i_bps=10*asf_packetsize; // FIXME!
    }
  }
  if(d_audio->id!=-2){
    mp_msg(MSGT_DEMUXER,MSGL_V,"ASF: Searching for audio stream (id:%d)\n",d_audio->id);
    if(!ds_fill_buffer(d_audio)){
      mp_msg(MSGT_DEMUXER,MSGL_INFO,"ASF: " MSGTR_MissingAudioStream);
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
//  if(demuxer->stream->type!=STREAMTYPE_VCD) demuxer->movi_start=0; // for VCD

  if(audio_id!=-2) {
   if(!ds_fill_buffer(d_audio)){
    mp_msg(MSGT_DEMUXER,MSGL_INFO,"MPEG: " MSGTR_MissingAudioStream);
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
#ifdef USE_TV
 case DEMUXER_TYPE_TV: {
    if (!demux_open_tv(demuxer, tv_handler))
    {
	tv_uninit(tv_handler);
	return(NULL);
    }
    break;
 }
#endif
} // switch(file_format)
pts_from_bps=0; // !!!
return demuxer;
}

static char* audio_stream = NULL;
static char* sub_stream = NULL;
static int demuxer_type = 0, audio_demuxer_type = 0, sub_demuxer_type = 0;
extern m_config_t* mconfig;

demuxer_t* demux_open(stream_t *vs,int file_format,int audio_id,int video_id,int dvdsub_id){
  stream_t *as = NULL,*ss = NULL;
  demuxer_t *vd,*ad = NULL,*sd = NULL;
  int afmt = 0,sfmt = 0;

  if(audio_stream) {
    as = open_stream(audio_stream,0,&afmt);
    if(!as) {
      mp_msg(MSGT_DEMUXER,MSGL_ERR,"Can't open audio stream: %s\n",audio_stream);
      return NULL;
    }
  }
  if(sub_stream) {
    ss = open_stream(sub_stream,0,&sfmt);
    if(!ss) {
      mp_msg(MSGT_DEMUXER,MSGL_ERR,"Can't open subtitles stream: %s\n",sub_stream);
      return NULL;
    }
  }

  vd = demux_open_stream(vs,demuxer_type ? demuxer_type : file_format,audio_stream ? -2 : audio_id,video_id, sub_stream ? -2 : dvdsub_id);
  if(!vd)
    return NULL;
  if(as) {
    ad = demux_open_stream(as,audio_demuxer_type ? audio_demuxer_type : afmt,audio_id,-2,-2);
    if(!ad)
      mp_msg(MSGT_DEMUXER,MSGL_WARN,"Failed to open audio demuxer: %s\n",audio_stream);
    else if(ad->audio->sh && ((sh_audio_t*)ad->audio->sh)->format == 0x55) // MP3
      m_config_set_flag(mconfig,"hr-mp3-seek",1); // Enable high res seeking
  }
  if(ss) {
    sd = demux_open_stream(ss,sub_demuxer_type ? sub_demuxer_type : sfmt,-2,-2,dvdsub_id);
    if(!sd)
      mp_msg(MSGT_DEMUXER,MSGL_WARN,"Failed to open subtitles demuxer: %s\n",sub_stream);
  }

  if(ad && sd)
    return new_demuxers_demuxer(vd,ad,sd);
  else if(ad)
    return new_demuxers_demuxer(vd,ad,vd);
  else if(sd)
    return new_demuxers_demuxer(vd,vd,sd);
  else
    return vd;
}


int demux_seek_avi(demuxer_t *demuxer,float rel_seek_secs,int flags);
int demux_seek_asf(demuxer_t *demuxer,float rel_seek_secs,int flags);
int demux_seek_mpg(demuxer_t *demuxer,float rel_seek_secs,int flags);
int demux_seek_y4m(demuxer_t *demuxer,float rel_seek_secs,int flags);
int demux_seek_fli(demuxer_t *demuxer,float rel_seek_secs,int flags);
int demux_seek_mf(demuxer_t *demuxer,float rel_seek_secs,int flags);
int demux_seek_nuv(demuxer_t *demuxer,float rel_seek_secs,int flags);
void demux_seek_mov(demuxer_t *demuxer,float pts,int flags);
int demux_seek_real(demuxer_t *demuxer,float rel_seek_secs,int flags);
extern void demux_audio_seek(demuxer_t *demuxer,float rel_seek_secs,int flags);
extern void demux_demuxers_seek(demuxer_t *demuxer,float rel_seek_secs,int flags);

int demux_seek(demuxer_t *demuxer,float rel_seek_secs,int flags){
    demux_stream_t *d_audio=demuxer->audio;
    demux_stream_t *d_video=demuxer->video;
    sh_audio_t *sh_audio=d_audio->sh;
    sh_video_t *sh_video=d_video->sh;

if(!demuxer->seekable){
    if(demuxer->file_format==DEMUXER_TYPE_AVI)
	mp_msg(MSGT_SEEK,MSGL_WARN,MSGTR_CantSeekRawAVI);
#ifdef USE_TV
    else if (demuxer->file_format==DEMUXER_TYPE_TV)
	mp_msg(MSGT_SEEK,MSGL_WARN,"TV input isn't seekable! (probarly seeking will be for changing channels ;)\n");
#endif
    else
	mp_msg(MSGT_SEEK,MSGL_WARN,MSGTR_CantSeekFile);
    return 0;
}

    // clear demux buffers:
    if(sh_audio){ ds_free_packs(d_audio);sh_audio->a_buffer_len=0;}
    ds_free_packs(d_video);
    
    demuxer->stream->eof=0; // clear eof flag
    demuxer->video->eof=0;
    demuxer->audio->eof=0;

#if 0
    if(sh_audio) sh_audio->timer=sh_video->timer;
#else
    if(sh_audio) sh_audio->timer=0;
    if(sh_video) sh_video->timer=0; // !!!!!!
#endif

switch(demuxer->file_format){

  case DEMUXER_TYPE_AVI:
      demux_seek_avi(demuxer,rel_seek_secs,flags);  break;

  case DEMUXER_TYPE_ASF:
      demux_seek_asf(demuxer,rel_seek_secs,flags);  break;
  
  case DEMUXER_TYPE_MPEG_ES:
  case DEMUXER_TYPE_MPEG_PS:
      demux_seek_mpg(demuxer,rel_seek_secs,flags);  break;

  case DEMUXER_TYPE_MOV:
      demux_seek_mov(demuxer,rel_seek_secs,flags);  break;

  case DEMUXER_TYPE_REAL:
      demux_seek_real(demuxer,rel_seek_secs,flags);  break;

  case DEMUXER_TYPE_Y4M:
      demux_seek_y4m(demuxer,rel_seek_secs,flags);  break;

  case DEMUXER_TYPE_MF:
      demux_seek_mf(demuxer,rel_seek_secs,flags);  break;
      
  case DEMUXER_TYPE_FLI:
      demux_seek_fli(demuxer,rel_seek_secs,flags);  break;
  case DEMUXER_TYPE_NUV:
      demux_seek_nuv(demuxer,rel_seek_secs,flags);  break;
  case DEMUXER_TYPE_AUDIO:
      demux_audio_seek(demuxer,rel_seek_secs,flags);  break;
 case DEMUXER_TYPE_DEMUXERS:
      demux_demuxers_seek(demuxer,rel_seek_secs,flags);  break;


} // switch(demuxer->file_format)

return 1;
}

int demux_info_add(demuxer_t *demuxer, char *opt, char *param)
{
    char **info = demuxer->info;
    int n = 0;


    for(n = 0; info && info[2*n] != NULL; n++) 
      {
	if(!strcasecmp(opt,info[2*n]))
	  {
	    mp_msg(MSGT_DEMUX, MSGL_WARN, "Demuxer info %s already present\n!",opt);
	    return 0;
	  }
      }
    
    info = demuxer->info = (char**)realloc(info,(2*(n+2))*sizeof(char*));
    info[2*n] = strdup(opt);
    info[2*n+1] = strdup(param);
    memset(&info[2*(n+1)],0,2*sizeof(char*));

    return 1;
}

int demux_info_print(demuxer_t *demuxer)
{
    char **info = demuxer->info;
    int n;

    if(!info)
      return 0;

    mp_msg(MSGT_DEMUX, MSGL_INFO, "Clip info: \n");
    for(n = 0; info[2*n] != NULL ; n++)
      mp_msg(MSGT_DEMUX, MSGL_INFO, " %s: %s\n",info[2*n],info[2*n+1]);

    return 0;
}

char* demux_info_get(demuxer_t *demuxer, char *opt) {
  int i;
  char **info = demuxer->info;

  for(i = 0; info && info[2*i] != NULL; i++) {
    if(!strcasecmp(opt,info[2*i]))
      return info[2*i+1];
  }

  return NULL;
}

/******************* Options stuff **********************/

static config_t demuxer_opts[] = {
  { "audiofile", &audio_stream, CONF_TYPE_STRING, 0, 0, 0, NULL },
  { "subfile", &sub_stream, CONF_TYPE_STRING, 0, 0, 0, NULL },
  { "demuxer", &demuxer_type, CONF_TYPE_INT, CONF_RANGE, 1, DEMUXER_TYPE_MAX, NULL },
  { "audio-demuxer", &audio_demuxer_type, CONF_TYPE_INT, CONF_RANGE, 1, DEMUXER_TYPE_MAX, NULL },
  { "sub-demuxer", &sub_demuxer_type, CONF_TYPE_INT, CONF_RANGE, 1, DEMUXER_TYPE_MAX, NULL },
  { NULL, NULL, 0, 0, 0, 0, NULL}
};

void demuxer_register_options(m_config_t* cfg) {  
  m_config_register_options(cfg,demuxer_opts);
}

