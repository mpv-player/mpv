//  AVI file parser for DEMUXER v2.9  by A'rpi/ESP-team

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"

#include "stream.h"
#include "demuxer.h"
#include "stheader.h"

#include "aviheader.h"

// Select ds from ID
demux_stream_t* demux_avi_select_stream(demuxer_t *demux,unsigned int id){
  int stream_id=avi_stream_id(id);

//  printf("demux_avi_select_stream(%d)  {a:%d/v:%d}\n",stream_id,
//       demux->audio->id,demux->video->id);

  if(demux->video->id==-1)
    if(demux->v_streams[stream_id])
        demux->video->id=stream_id;

  if(demux->audio->id==-1)
    if(demux->a_streams[stream_id])
        demux->audio->id=stream_id;

  if(stream_id==demux->audio->id){
      if(!demux->audio->sh){
        demux->audio->sh=demux->a_streams[stream_id];
        mp_msg(MSGT_DEMUX,MSGL_V,"Auto-selected AVI audio ID = %d\n",demux->audio->id);
      }
      return demux->audio;
  }
  if(stream_id==demux->video->id){
      if(!demux->video->sh){
        demux->video->sh=demux->v_streams[stream_id];
        mp_msg(MSGT_DEMUX,MSGL_V,"Auto-selected AVI video ID = %d\n",demux->video->id);
      }
      return demux->video;
  }
  if(id!=mmioFOURCC('J','U','N','K')){
     // unknown
     mp_msg(MSGT_DEMUX,MSGL_DBG2,"Unknown chunk: %.4s (%X)\n",(char *) &id,id);
     //abort();
  }
  return NULL;
}

static int valid_fourcc(unsigned int id){
    unsigned char* fcc=(unsigned char*)(&id);
#define FCC_CHR_CHECK(x) (x<48 || x>=96)
    if(FCC_CHR_CHECK(fcc[0])) return 0;
    if(FCC_CHR_CHECK(fcc[1])) return 0;
    if(FCC_CHR_CHECK(fcc[2])) return 0;
    if(FCC_CHR_CHECK(fcc[3])) return 0;
    return 1;
#undef FCC_CHR_CHECK
}

static int choose_chunk_len(unsigned int len1,unsigned int len2){
    // len1 has a bit more priority than len2. len1!=len2
    // Note: this is a first-idea-logic, may be wrong. comments welcomed.

    // prefer small frames rather than 0
    if(!len1) return (len2>0x80000) ? len1 : len2;
    if(!len2) return (len1>0x100000) ? len2 : len1;

    // choose the smaller value:
    return (len1<len2)? len1 : len2;
}

static int demux_avi_read_packet(demuxer_t *demux,unsigned int id,unsigned int len,int idxpos,int flags){
  avi_priv_t *priv=demux->priv;
  int skip;
  float pts=0;
  demux_stream_t *ds=demux_avi_select_stream(demux,id);
  
  mp_dbg(MSGT_DEMUX,MSGL_DBG3,"demux_avi.read_packet: %X\n",id);

  if(ds==demux->audio){

      if(priv->pts_corrected==0){
//          printf("\rYYY-A  A: %5.3f  V: %5.3f  \n",priv->avi_audio_pts,priv->avi_video_pts);
          if(priv->pts_has_video){
	      // we have video pts now
	      float delay=0;
	      if(((sh_audio_t*)(ds->sh))->wf->nAvgBytesPerSec)
	          delay=(float)priv->pts_corr_bytes/((sh_audio_t*)(ds->sh))->wf->nAvgBytesPerSec;
	      mp_msg(MSGT_DEMUX,MSGL_V,"XXX initial  v_pts=%5.3f  a_pos=%d (%5.3f) \n",priv->avi_audio_pts,priv->pts_corr_bytes,delay);
	      //priv->pts_correction=-priv->avi_audio_pts+delay;
	      priv->pts_correction=delay-priv->avi_audio_pts;
	      priv->avi_audio_pts+=priv->pts_correction;
	      priv->pts_corrected=1;
	  } else
          priv->pts_corr_bytes+=len;
      }
            pts=priv->avi_audio_pts; //+priv->pts_correction;
            priv->avi_audio_pts=0;
  } else 
  if(ds==demux->video){
     // video
     if(priv->skip_video_frames>0){
       // drop frame (seeking)
       --priv->skip_video_frames;
       ds=NULL;
//     } else {
//       pts=priv->avi_video_pts;
     }
     // ezt a 2 sort lehet hogy fell kell majd cserelni:
     //priv->avi_video_pts+=avi_pts_frametime;
     //priv->avi_video_pts+=(float)avi_header.video.dwScale/(float)avi_header.video.dwRate;
     //priv->avi_video_pts+=((sh_video_t*)ds->sh)->frametime;
// FIXME!!!
#if 1
//       printf("ds=0x%X\n",ds);
//       printf("packno=%d\n",ds->pack_no);
//    printf("### pack_no=%d\n",demux->video->pack_no+demux->video->packs);
       priv->avi_video_pts = (demux->video->pack_no+demux->video->packs) *
         (float)((sh_video_t*)demux->video->sh)->video.dwScale /
	 (float)((sh_video_t*)demux->video->sh)->video.dwRate;
#else
     priv->avi_video_pts+=(float)((sh_video_t*)(demux->video->sh))->video.dwScale/(float)((sh_video_t*)(demux->video->sh))->video.dwRate;
//     priv->avi_video_pts+=avi_video_ftime;
#endif
//          printf("\rYYY-V  A: %5.3f  V: %5.3f  \n",priv->avi_audio_pts,priv->avi_video_pts);
     priv->avi_audio_pts=priv->avi_video_pts+priv->pts_correction;
     priv->pts_has_video=1;

     pts=priv->avi_video_pts;

     //printf("read  pack_no: %d  pts %5.3f  \n",demux->video->pack_no+demux->video->packs,pts);

  }
  
//  len=stream_read_dword_le(demux->stream);
  skip=(len+1)&(~1); // total bytes in this chunk
  
  if(ds){
    mp_dbg(MSGT_DEMUX,MSGL_DBG2,"DEMUX_AVI: Read %d data bytes from packet %04X\n",len,id);
    ds_read_packet(ds,demux->stream,len,pts,idxpos,flags);
    skip-=len;
  }
  if(skip){
    mp_dbg(MSGT_DEMUX,MSGL_DBG2,"DEMUX_AVI: Skipping %d bytes from packet %04X\n",skip,id);
    stream_skip(demux->stream,skip);
  }
  return ds?1:0;
}

// return value:
//     0 = EOF or no stream found
//     1 = successfully read a packet
int demux_avi_fill_buffer(demuxer_t *demux){
avi_priv_t *priv=demux->priv;
unsigned int id=0;
unsigned int len;
int max_packs=128;
int ret=0;

do{
  int flags=0;
  AVIINDEXENTRY *idx=NULL;
#if 0
  demux->filepos=stream_tell(demux->stream);
  if(demux->filepos>=demux->movi_end){
          demux->stream->eof=1;
          return 0;
  }
  if(stream_eof(demux->stream)) return 0;
#endif
  if(priv->idx_size>0 && priv->idx_pos<priv->idx_size){
    off_t pos;
    
    //if(priv->idx_pos<0) printf("Fatal! idx_pos=%d\n",priv->idx_pos);
    
    idx=&((AVIINDEXENTRY *)priv->idx)[priv->idx_pos++];
    
    //printf("[%d]",priv->idx_pos);fflush(stdout);
    
    //stream_seek(demux->stream,idx.dwChunkOffset);
    //printf("IDX  pos=%X  idx.pos=%X  idx.size=%X  idx.flags=%X\n",demux->filepos,
    //  pos-4,idx->dwChunkLength,idx->dwFlags);
    if(idx->dwFlags&AVIIF_LIST){
      // LIST
      continue;
    }
    if(!demux_avi_select_stream(demux,idx->ckid)){
      mp_dbg(MSGT_DEMUX,MSGL_DBG3,"Skip chunk %.4s (0x%X)  \n",(char *)&idx->ckid,(unsigned int)idx->ckid);
      continue; // skip this chunk
    }

    pos = priv->idx_offset + (unsigned long)idx->dwChunkOffset;
    if((pos<demux->movi_start || pos>=demux->movi_end) && (demux->movi_end>demux->movi_start)){
      mp_msg(MSGT_DEMUX,MSGL_V,"ChunkOffset out of range!   idx=0x%X  \n",pos);
      continue;
    }
#if 0
    if(pos!=demux->filepos){
      mp_msg(MSGT_DEMUX,MSGL_V,"Warning! pos=0x%X  idx.pos=0x%X  diff=%d   \n",demux->filepos,pos,pos-demux->filepos);
    }
#endif
    stream_seek(demux->stream,pos);
    demux->filepos=stream_tell(demux->stream);
    id=stream_read_dword_le(demux->stream);
    if(stream_eof(demux->stream)) return 0; // EOF!
    
    if(id!=idx->ckid){
      mp_msg(MSGT_DEMUX,MSGL_V,"ChunkID mismatch! raw=%.4s idx=%.4s  \n",(char *)&id,(char *)&idx->ckid);
      if(valid_fourcc(idx->ckid))
          id=idx->ckid;	// use index if valid
      else
          if(!valid_fourcc(id)) continue; // drop chunk if both id and idx bad
    }
    len=stream_read_dword_le(demux->stream);
//    if((len&(~1))!=(idx->dwChunkLength&(~1))){
//    if((len)!=(idx->dwChunkLength)){
    if((len!=idx->dwChunkLength)&&((len+1)!=idx->dwChunkLength)){
      mp_msg(MSGT_DEMUX,MSGL_V,"ChunkSize mismatch! raw=%d idx=%ld  \n",len,idx->dwChunkLength);
      if(len>0x200000 && idx->dwChunkLength>0x200000) continue; // both values bad :(
      len=choose_chunk_len(idx->dwChunkLength,len);
    }
    if(idx->dwFlags&AVIIF_KEYFRAME) flags=1;
  } else {
    demux->filepos=stream_tell(demux->stream);
    if(demux->filepos>=demux->movi_end && demux->movi_end>demux->movi_start){
          demux->stream->eof=1;
          return 0;
    }
    id=stream_read_dword_le(demux->stream);
    len=stream_read_dword_le(demux->stream);
    if(stream_eof(demux->stream)) return 0; // EOF!
    
    if(id==mmioFOURCC('L','I','S','T') || id==mmioFOURCC('R', 'I', 'F', 'F')){
      id=stream_read_dword_le(demux->stream); // list or RIFF type
      continue;
    }
  }
  ret=demux_avi_read_packet(demux,id,len,priv->idx_pos-1,flags);
//      if(!ret && priv->skip_video_frames<=0)
//        if(--max_packs==0){
//          demux->stream->eof=1;
//          mp_msg(MSGT_DEMUX,MSGL_ERR,MSGTR_DoesntContainSelectedStream);
//          return 0;
//        }
} while(ret!=1);
  return 1;
}


// return value:
//     0 = EOF or no stream found
//     1 = successfully read a packet
int demux_avi_fill_buffer_ni(demuxer_t *demux,demux_stream_t* ds){
avi_priv_t *priv=demux->priv;
unsigned int id=0;
unsigned int len;
int max_packs=128;
int ret=0;

do{
  int flags=0;
  AVIINDEXENTRY *idx=NULL;
  int idx_pos=0;
  demux->filepos=stream_tell(demux->stream);
  
  if(ds==demux->video) idx_pos=priv->idx_pos_v++; else
  if(ds==demux->audio) idx_pos=priv->idx_pos_a++; else
                       idx_pos=priv->idx_pos++;
  
  if(priv->idx_size>0 && idx_pos<priv->idx_size){
    off_t pos;
    idx=&((AVIINDEXENTRY *)priv->idx)[idx_pos];
//    idx=&priv->idx[idx_pos];
    
    if(idx->dwFlags&AVIIF_LIST){
      // LIST
      continue;
    }
    if(ds && demux_avi_select_stream(demux,idx->ckid)!=ds){
      mp_dbg(MSGT_DEMUX,MSGL_DBG3,"Skip chunk %.4s (0x%X)  \n",(char *)&idx->ckid,(unsigned int)idx->ckid);
      continue; // skip this chunk
    }

    pos = priv->idx_offset+(unsigned long)idx->dwChunkOffset;
    if((pos<demux->movi_start || pos>=demux->movi_end) && (demux->movi_end>demux->movi_start)){
      mp_msg(MSGT_DEMUX,MSGL_V,"ChunkOffset out of range!  current=0x%X  idx=0x%X  \n",demux->filepos,pos);
      continue;
    }
#if 0
    if(pos!=demux->filepos){
      mp_msg(MSGT_DEMUX,MSGL_V,"Warning! pos=0x%X  idx.pos=0x%X  diff=%d   \n",demux->filepos,pos,pos-demux->filepos);
    }
#endif
    stream_seek(demux->stream,pos);

    id=stream_read_dword_le(demux->stream);

    if(stream_eof(demux->stream)) return 0;

    if(id!=idx->ckid){
      mp_msg(MSGT_DEMUX,MSGL_V,"ChunkID mismatch! raw=%.4s idx=%.4s  \n",(char *)&id,(char *)&idx->ckid);
      if(valid_fourcc(idx->ckid))
          id=idx->ckid;	// use index if valid
      else
          if(!valid_fourcc(id)) continue; // drop chunk if both id and idx bad
    }
    len=stream_read_dword_le(demux->stream);
//    if((len&(~1))!=(idx->dwChunkLength&(~1))){
//    if((len)!=(idx->dwChunkLength)){
    if((len!=idx->dwChunkLength)&&((len+1)!=idx->dwChunkLength)){
      mp_msg(MSGT_DEMUX,MSGL_V,"ChunkSize mismatch! raw=%d idx=%ld  \n",len,idx->dwChunkLength);
      if(len>0x200000 && idx->dwChunkLength>0x200000) continue; // both values bad :(
      len=choose_chunk_len(idx->dwChunkLength,len);
    }
    if(idx->dwFlags&AVIIF_KEYFRAME) flags=1;
  } else return 0;
  ret=demux_avi_read_packet(demux,id,len,idx_pos,flags);
//      if(!ret && priv->skip_video_frames<=0)
//        if(--max_packs==0){
//          demux->stream->eof=1;
//          mp_msg(MSGT_DEMUX,MSGL_ERR,MSGTR_DoesntContainSelectedStream);
//          return 0;
//        }
} while(ret!=1);
  return 1;
}


// return value:
//     0 = EOF or no stream found
//     1 = successfully read a packet
int demux_avi_fill_buffer_nini(demuxer_t *demux,demux_stream_t* ds){
avi_priv_t *priv=demux->priv;
unsigned int id=0;
unsigned int len;
int ret=0;
off_t *fpos=NULL;

  if(ds==demux->video) fpos=&priv->idx_pos_v; else
  if(ds==demux->audio) fpos=&priv->idx_pos_a; else
  return 0;

  stream_seek(demux->stream,fpos[0]);

do{

  demux->filepos=stream_tell(demux->stream);
  if(demux->filepos>=demux->movi_end && (demux->movi_end>demux->movi_start)){
          demux->stream->eof=1;
          return 0;
  }
  if(stream_eof(demux->stream)) return 0;

  id=stream_read_dword_le(demux->stream);
  len=stream_read_dword_le(demux->stream);
  if(id==mmioFOURCC('L','I','S','T')){
      id=stream_read_dword_le(demux->stream);      // list type
      continue;
  }
  
  if(id==mmioFOURCC('R','I','F','F')){
      printf("additional RIFF header...\n");
      id=stream_read_dword_le(demux->stream);      // "AVIX"
      continue;
  }
  
  if(ds==demux_avi_select_stream(demux,id)){
    // read it!
    ret=demux_avi_read_packet(demux,id,len,priv->idx_pos-1,0);
  } else {
    // skip it!
    int skip=(len+1)&(~1); // total bytes in this chunk
    stream_skip(demux->stream,skip);
  }
  
} while(ret!=1);
  fpos[0]=stream_tell(demux->stream);
  return 1;
}

//extern int audio_id;
//extern int video_id;
//extern int index_mode;  // -1=untouched  0=don't use index  1=use (geneate) index
//extern int force_ni;
//extern int pts_from_bps;

// AVI demuxer parameters:
int index_mode=-1;  // -1=untouched  0=don't use index  1=use (geneate) index
int force_ni=0;     // force non-interleaved AVI parsing

// PTS:  0=interleaved  1=BPS-based
#ifdef AVI_SYNC_BPS
int pts_from_bps=1;
#else
int pts_from_bps=0;
#endif

void read_avi_header(demuxer_t *demuxer,int index_mode);

demuxer_t* demux_open_avi(demuxer_t* demuxer){
    demux_stream_t *d_audio=demuxer->audio;
    demux_stream_t *d_video=demuxer->video;
    sh_audio_t *sh_audio=NULL;
    sh_video_t *sh_video=NULL;
    avi_priv_t* priv=malloc(sizeof(avi_priv_t));

  // priv struct:
  priv->avi_audio_pts=priv->avi_video_pts=0.0f;
  priv->pts_correction=0.0f;
  priv->skip_video_frames=0;
  priv->pts_corr_bytes=0;
  priv->pts_has_video=priv->pts_corrected=0;
  demuxer->priv=(void*)priv;

  //---- AVI header:
  read_avi_header(demuxer,(demuxer->stream->type!=STREAMTYPE_STREAM)?index_mode:-2);
  stream_reset(demuxer->stream);
  stream_seek(demuxer->stream,demuxer->movi_start);
  priv->idx_pos=0;
  priv->idx_pos_a=0;
  priv->idx_pos_v=0;
  if(priv->idx_size>1){
    // decide index format:
#if 1
    if((unsigned long)((AVIINDEXENTRY *)priv->idx)[0].dwChunkOffset<demuxer->movi_start ||
       (unsigned long)((AVIINDEXENTRY *)priv->idx)[1].dwChunkOffset<demuxer->movi_start)
      priv->idx_offset=demuxer->movi_start-4;
    else
      priv->idx_offset=0;
#else
    if((unsigned long)((AVIINDEXENTRY *)priv->idx)[0].dwChunkOffset<demuxer->movi_start)
      priv->idx_offset=demuxer->movi_start-4;
    else
      priv->idx_offset=0;
#endif
    mp_msg(MSGT_DEMUX,MSGL_V,"AVI index offset: 0x%X (movi=0x%X idx0=0x%X idx1=0x%X)\n",
	    (int)priv->idx_offset,(int)demuxer->movi_start,
	    (int)((AVIINDEXENTRY *)priv->idx)[0].dwChunkOffset,
	    (int)((AVIINDEXENTRY *)priv->idx)[1].dwChunkOffset);
  }
//  demuxer->endpos=avi_header.movi_end;
  
  if(priv->idx_size>0){
      // check that file is non-interleaved:
      int i;
      off_t a_pos=-1;
      off_t v_pos=-1;
      for(i=0;i<priv->idx_size;i++){
        AVIINDEXENTRY* idx=&((AVIINDEXENTRY *)priv->idx)[i];
        demux_stream_t* ds=demux_avi_select_stream(demuxer,idx->ckid);
        off_t pos = priv->idx_offset + (unsigned long)idx->dwChunkOffset;
        if(a_pos==-1 && ds==demuxer->audio){
          a_pos=pos;
          if(v_pos!=-1) break;
        }
        if(v_pos==-1 && ds==demuxer->video){
          v_pos=pos;
          if(a_pos!=-1) break;
        }
      }
      if(v_pos==-1){
        mp_msg(MSGT_DEMUX,MSGL_ERR,"AVI_NI: " MSGTR_MissingVideoStream);
	return NULL;
//        GUI_MSG( mplErrorAVINI )
      }
      if(a_pos==-1){
        mp_msg(MSGT_DEMUX,MSGL_INFO,"AVI_NI: " MSGTR_MissingAudioStream);
        sh_audio=NULL;
      } else {
        if(force_ni || abs(a_pos-v_pos)>0x100000){  // distance > 1MB
          mp_msg(MSGT_DEMUX,MSGL_INFO,MSGTR_NI_Message,force_ni?MSGTR_NI_Forced:MSGTR_NI_Detected);
          demuxer->type=DEMUXER_TYPE_AVI_NI; // HACK!!!!
	  pts_from_bps=1; // force BPS sync!
        }
      }
  } else {
      // no index
      if(force_ni){
          mp_msg(MSGT_DEMUX,MSGL_INFO,MSGTR_UsingNINI);
          demuxer->type=DEMUXER_TYPE_AVI_NINI; // HACK!!!!
	  priv->idx_pos_a=
	  priv->idx_pos_v=demuxer->movi_start;
	  pts_from_bps=1; // force BPS sync!
      }
      demuxer->seekable=0;
  }
  if(!ds_fill_buffer(d_video)){
    mp_msg(MSGT_DEMUX,MSGL_ERR,"AVI: " MSGTR_MissingVideoStreamBug);
    return NULL;
//    GUI_MSG( mplAVIErrorMissingVideoStream )
  }
  sh_video=d_video->sh;sh_video->ds=d_video;
  if(d_audio->id!=-2){
    mp_msg(MSGT_DEMUX,MSGL_V,"AVI: Searching for audio stream (id:%d)\n",d_audio->id);
    if(!priv->audio_streams || !ds_fill_buffer(d_audio)){
      mp_msg(MSGT_DEMUX,MSGL_INFO,"AVI: " MSGTR_MissingAudioStream);
      sh_audio=NULL;
    } else {
      sh_audio=d_audio->sh;sh_audio->ds=d_audio;
      sh_audio->format=sh_audio->wf->wFormatTag;
    }
  }
  // calc. FPS:
  sh_video->fps=(float)sh_video->video.dwRate/(float)sh_video->video.dwScale;
  sh_video->frametime=(float)sh_video->video.dwScale/(float)sh_video->video.dwRate;
  // calculating video bitrate:
  sh_video->i_bps=demuxer->movi_end-demuxer->movi_start-priv->idx_size*8;
  if(sh_audio) sh_video->i_bps-=sh_audio->audio.dwLength;
  mp_msg(MSGT_DEMUX,MSGL_V,"AVI video length=%lu\n",(unsigned long)sh_video->i_bps);
  sh_video->i_bps=((float)sh_video->i_bps/(float)sh_video->video.dwLength)*sh_video->fps;
  mp_msg(MSGT_DEMUX,MSGL_INFO,"VIDEO:  [%.4s]  %ldx%ld  %dbpp  %4.2f fps  %5.1f kbps (%4.1f kbyte/s)\n",
    (char *)&sh_video->bih->biCompression,
    sh_video->bih->biWidth,
    sh_video->bih->biHeight,
    sh_video->bih->biBitCount,
    sh_video->fps,
    sh_video->i_bps*0.008f,
    sh_video->i_bps/1024.0f );

  return demuxer;
  
}

//extern float initial_pts_delay;

void demux_seek_avi(demuxer_t *demuxer,float rel_seek_secs,int flags){
    avi_priv_t *priv=demuxer->priv;
    demux_stream_t *d_audio=demuxer->audio;
    demux_stream_t *d_video=demuxer->video;
    sh_audio_t *sh_audio=d_audio->sh;
    sh_video_t *sh_video=d_video->sh;
    float skip_audio_secs=0;

  //FIXME: OFF_T - Didn't check AVI case yet (avi files can't be >2G anyway?)
  //================= seek in AVI ==========================
    int rel_seek_frames=rel_seek_secs*sh_video->fps;
    int video_chunk_pos=d_video->pos;
    int i;

      if(flags&1){
	// seek absolute
	video_chunk_pos=0;
      }
      
      if(flags&2){
        // float 0..1
	int total=sh_video->video.dwLength;
	if(total<=1){
	    // bad video header, try to get it from audio
	    if(sh_audio) total=sh_video->fps*sh_audio->audio.dwLength/sh_audio->wf->nAvgBytesPerSec;
	    if(total<=1){
              mp_msg(MSGT_SEEK,MSGL_WARN,MSGTR_CouldntDetFNo);
	      total=0;
	    }
	}
	rel_seek_frames=rel_seek_secs*total;
      }
    
      priv->skip_video_frames=0;
      priv->avi_audio_pts=0;

// ------------ STEP 1: find nearest video keyframe chunk ------------
      // find nearest video keyframe chunk pos:
      if(rel_seek_frames>0){
        // seek forward
        while(video_chunk_pos<priv->idx_size){
          int id=((AVIINDEXENTRY *)priv->idx)[video_chunk_pos].ckid;
          if(avi_stream_id(id)==d_video->id){  // video frame
            if((--rel_seek_frames)<0 && ((AVIINDEXENTRY *)priv->idx)[video_chunk_pos].dwFlags&AVIIF_KEYFRAME) break;
          }
          ++video_chunk_pos;
        }
      } else {
        // seek backward
        while(video_chunk_pos>0){
          int id=((AVIINDEXENTRY *)priv->idx)[video_chunk_pos].ckid;
          if(avi_stream_id(id)==d_video->id){  // video frame
            if((++rel_seek_frames)>0 && ((AVIINDEXENTRY *)priv->idx)[video_chunk_pos].dwFlags&AVIIF_KEYFRAME) break;
          }
          --video_chunk_pos;
        }
      }
      priv->idx_pos_a=priv->idx_pos_v=priv->idx_pos=video_chunk_pos;

      // re-calc video pts:
      d_video->pack_no=0;
      for(i=0;i<video_chunk_pos;i++){
          int id=((AVIINDEXENTRY *)priv->idx)[i].ckid;
          if(avi_stream_id(id)==d_video->id) ++d_video->pack_no;
      }
      sh_video->num_frames=sh_video->num_frames_decoded=d_video->pack_no;
      priv->avi_video_pts=d_video->pack_no*(float)sh_video->video.dwScale/(float)sh_video->video.dwRate;
      d_video->pos=video_chunk_pos;
      
      mp_msg(MSGT_SEEK,MSGL_DBG2,"V_SEEK:  pack=%d  pts=%5.3f  chunk=%d  \n",d_video->pack_no,priv->avi_video_pts,video_chunk_pos);

// ------------ STEP 2: seek audio, find the right chunk & pos ------------

      d_audio->pack_no=0;
      d_audio->dpos=0;

      if(sh_audio){
        int i;
//        int apos=0;
        int last=0;
        int len=0;
	int skip_audio_bytes=0;
	int curr_audio_pos=-1;
	int audio_chunk_pos=-1;
	int chunk_max=(demuxer->type==DEMUXER_TYPE_AVI)?video_chunk_pos:priv->idx_size;
	
	if(sh_audio->audio.dwSampleSize){
	    // constant rate audio stream
#if 0
	    int align;
	    curr_audio_pos=(priv->avi_video_pts) * sh_audio->wf->nAvgBytesPerSec;
	    if(curr_audio_pos<0)curr_audio_pos=0;
	    align=sh_audio->audio.dwSampleSize;
	    if(sh_audio->wf->nBlockAlign>align) align=sh_audio->wf->nBlockAlign;
	    curr_audio_pos/=align;
	    curr_audio_pos*=align;
#else
	    curr_audio_pos=(priv->avi_video_pts)*(float)sh_audio->audio.dwRate/(float)sh_audio->audio.dwScale;
	    curr_audio_pos-=sh_audio->audio.dwStart;
	    curr_audio_pos*=sh_audio->audio.dwSampleSize;
#endif

        // find audio chunk pos:
          for(i=0;i<chunk_max;i++){
            int id=((AVIINDEXENTRY *)priv->idx)[i].ckid;
            if(avi_stream_id(id)==d_audio->id){
                len=((AVIINDEXENTRY *)priv->idx)[i].dwChunkLength;
                ++d_audio->pack_no;
                if(d_audio->dpos<=curr_audio_pos && curr_audio_pos<(d_audio->dpos+len)){
                  break;
                }
                d_audio->dpos+=len;
            }
          }
	  audio_chunk_pos=i;
	  skip_audio_bytes=curr_audio_pos-d_audio->dpos;

          mp_msg(MSGT_SEEK,MSGL_V,"SEEK: i=%d (max:%d) dpos=%d (wanted:%d)  \n",
	      i,chunk_max,(int)d_audio->dpos,curr_audio_pos);
	      
	} else {
	    // VBR audio
	    int chunks=(priv->avi_video_pts)*(float)sh_audio->audio.dwRate/(float)sh_audio->audio.dwScale;
	    audio_chunk_pos=0;
	    
        // find audio chunk pos:
          for(i=0;i<priv->idx_size && chunks>0;i++){
            int id=((AVIINDEXENTRY *)priv->idx)[i].ckid;
            if(avi_stream_id(id)==d_audio->id){
                len=((AVIINDEXENTRY *)priv->idx)[i].dwChunkLength;
		if(i>chunk_max){
		  skip_audio_bytes+=len;
		} else {
		  ++d_audio->pack_no;
                  d_audio->dpos+=len;
		  audio_chunk_pos=i;
		}
		--chunks;
            }
          }
	  //if(audio_chunk_pos>chunk_max) audio_chunk_pos=chunk_max;
	  
//	  printf("VBR seek: %5.3f -> chunk_no %d -> chunk_idx %d + skip %d  \n",
//	      priv->avi_video_pts, audio_chunk_pos, );
	
	}
	
	// Now we have:
	//      audio_chunk_pos = chunk no in index table (it's <=chunk_max)
	//      skip_audio_bytes = bytes to be skipped after chunk seek
	//      d-audio->pack_no = chunk_no in stream at audio_chunk_pos
	//      d_audio->dpos = bytepos in stream at audio_chunk_pos
	// let's seek!
	
          // update stream position:
          d_audio->pos=audio_chunk_pos;
//          d_audio->dpos=apos;
//	  d_audio->pts=initial_pts_delay+(float)apos/(float)sh_audio->wf->nAvgBytesPerSec;
	
	if(demuxer->type==DEMUXER_TYPE_AVI){
	  // interleaved stream:
	  if(audio_chunk_pos<video_chunk_pos){
            // calc priv->skip_video_frames & adjust video pts counter:
	    for(i=audio_chunk_pos;i<video_chunk_pos;i++){
              int id=((AVIINDEXENTRY *)priv->idx)[i].ckid;
              if(avi_stream_id(id)==d_video->id) ++priv->skip_video_frames;
            }
            // requires for correct audio pts calculation (demuxer):
            priv->avi_video_pts-=priv->skip_video_frames*(float)sh_video->video.dwScale/(float)sh_video->video.dwRate;
	    priv->idx_pos_a=priv->idx_pos_v=priv->idx_pos=audio_chunk_pos;
	  }
	} else {
	    // non-interleaved stream:
	    priv->idx_pos_a=audio_chunk_pos;
	    priv->idx_pos_v=video_chunk_pos;
	    priv->idx_pos=(audio_chunk_pos<video_chunk_pos)?audio_chunk_pos:video_chunk_pos;
	}
	


          mp_msg(MSGT_SEEK,MSGL_V,"SEEK: idx=%d  (a:%d v:%d)  v.skip=%d  a.skip=%d/%4.3f  \n",
            (int)priv->idx_pos,audio_chunk_pos,video_chunk_pos,
            (int)priv->skip_video_frames,skip_audio_bytes,skip_audio_secs);

          if(skip_audio_bytes){
            demux_read_data(d_audio,NULL,skip_audio_bytes);
            //d_audio->pts=0; // PTS is outdated because of the raw data skipping
          }
	  resync_audio_stream(sh_audio);

//          sh_audio->timer=-skip_audio_secs;

      }
	d_video->pts=priv->avi_video_pts; // OSD

}



