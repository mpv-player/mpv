//  AVI file parser for DEMUXER v2.6  by A'rpi/ESP-team

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

extern int verbose; // defined in mplayer.c

#include "stream.h"
#include "demuxer.h"

#include "wine/mmreg.h"
#include "wine/avifmt.h"
#include "wine/vfw.h"

#include "codec-cfg.h"
#include "stheader.h"

//static float avi_pts_frametime=1.0f/25.0f;
float avi_audio_pts=0;
float avi_video_pts=0;
//float avi_video_ftime=0.04;
int skip_video_frames=0;

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
        if(verbose) printf("Auto-selected AVI audio ID = %d\n",demux->audio->id);
      }
      return demux->audio;
  }
  if(stream_id==demux->video->id){
      if(!demux->video->sh){
        demux->video->sh=demux->v_streams[stream_id];
        if(verbose) printf("Auto-selected AVI video ID = %d\n",demux->video->id);
      }
      return demux->video;
  }
  if(id!=mmioFOURCC('J','U','N','K')){
     // unknown
     if(verbose>=2) printf("Unknown chunk: %.4s (%X)\n",(char *) &id,id);
  }
  return NULL;
}

static float pts_correction=0.0;
static int pts_corrected=0;
static int pts_has_video=0;
static unsigned int pts_corr_bytes=0;

static int demux_avi_read_packet(demuxer_t *demux,unsigned int id,unsigned int len,int idxpos,int flags){
  int skip;
  float pts=0;
  demux_stream_t *ds=demux_avi_select_stream(demux,id);
  
  if(verbose>=3) printf("demux_avi.read_packet: %X\n",id);

  if(ds==demux->audio){

      if(pts_corrected==0){
//          printf("\rYYY-A  A: %5.3f  V: %5.3f  \n",avi_audio_pts,avi_video_pts);
          if(pts_has_video){
	      // we have video pts now
	      float delay=(float)pts_corr_bytes/((sh_audio_t*)(ds->sh))->wf->nAvgBytesPerSec;
	      printf("XXX initial  v_pts=%5.3f  a_pos=%d (%5.3f) \n",avi_audio_pts,pts_corr_bytes,delay);
	      //pts_correction=-avi_audio_pts+delay;
	      pts_correction=delay-avi_audio_pts;
	      pts_corrected=1;
	  } else
          pts_corr_bytes+=len;
      }

            pts=avi_audio_pts+pts_correction;
            avi_audio_pts=0;
  } else 
  if(ds==demux->video){
     // video
     if(skip_video_frames>0){
       // drop frame (seeking)
       --skip_video_frames;
       ds=NULL;
     } else {
       pts=avi_video_pts;
     }
     // ezt a 2 sort lehet hogy fell kell majd cserelni:
     //avi_video_pts+=avi_pts_frametime;
     //avi_video_pts+=(float)avi_header.video.dwScale/(float)avi_header.video.dwRate;
     //avi_video_pts+=((sh_video_t*)ds->sh)->frametime;
// FIXME!!!
#if 1
//       printf("ds=0x%X\n",ds);
//       printf("packno=%d\n",ds->pack_no);
//    printf("### pack_no=%d\n",demux->video->pack_no+demux->video->packs);
       avi_video_pts = (demux->video->pack_no+demux->video->packs) *
         (float)((sh_video_t*)demux->video->sh)->video.dwScale /
	 (float)((sh_video_t*)demux->video->sh)->video.dwRate;
#else
     avi_video_pts+=(float)((sh_video_t*)(demux->video->sh))->video.dwScale/(float)((sh_video_t*)(demux->video->sh))->video.dwRate;
//     avi_video_pts+=avi_video_ftime;
#endif
//          printf("\rYYY-V  A: %5.3f  V: %5.3f  \n",avi_audio_pts,avi_video_pts);
     avi_audio_pts=avi_video_pts;
     pts_has_video=1;

  }
  
//  len=stream_read_dword_le(demux->stream);
  skip=(len+1)&(~1); // total bytes in this chunk
  
  if(ds){
    if(verbose>=2) printf("DEMUX_AVI: Read %d data bytes from packet %04X\n",len,id);
    ds_read_packet(ds,demux->stream,len,pts,idxpos,flags);
    skip-=len;
  }
  if(skip){
    if(verbose>=2) printf("DEMUX_AVI: Skipping %d bytes from packet %04X\n",skip,id);
    stream_skip(demux->stream,skip);
  }
  return ds?1:0;
}

//static int num_elementary_packets100=0;
//static int num_elementary_packets101=0;

// return value:
//     0 = EOF or no stream found
//     1 = successfully read a packet
int demux_avi_fill_buffer(demuxer_t *demux){
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
  if(demux->idx_size>0 && demux->idx_pos<demux->idx_size){
    unsigned int pos;
    
    //if(demux->idx_pos<0) printf("Fatal! idx_pos=%d\n",demux->idx_pos);
    
    idx=&((AVIINDEXENTRY *)demux->idx)[demux->idx_pos++];
    
    //printf("[%d]",demux->idx_pos);fflush(stdout);
    
    //stream_seek(demux->stream,idx.dwChunkOffset);
    //printf("IDX  pos=%X  idx.pos=%X  idx.size=%X  idx.flags=%X\n",demux->filepos,
    //  pos-4,idx->dwChunkLength,idx->dwFlags);
    if(idx->dwFlags&AVIIF_LIST){
      // LIST
      continue;
    }
    if(!demux_avi_select_stream(demux,idx->ckid)){
      if(verbose>2) printf("Skip chunk %.4s (0x%X)  \n",(char *)&idx->ckid,(unsigned int)idx->ckid);
      continue; // skip this chunk
    }

    pos=idx->dwChunkOffset+demux->idx_offset;
    if(pos<demux->movi_start || pos>=demux->movi_end){
      printf("ChunkOffset out of range!   idx=0x%X  \n",pos);
      continue;
    }
#if 0
    if(pos!=demux->filepos){
      printf("Warning! pos=0x%X  idx.pos=0x%X  diff=%d   \n",demux->filepos,pos,pos-demux->filepos);
    }
#endif
    stream_seek(demux->stream,pos);
    demux->filepos=stream_tell(demux->stream);
    id=stream_read_dword_le(demux->stream);
    if(stream_eof(demux->stream)) return 0; // EOF!
    
    if(id!=idx->ckid){
      printf("ChunkID mismatch! raw=%.4s idx=%.4s  \n",(char *)&id,(char *)&idx->ckid);
      continue;
    }
    len=stream_read_dword_le(demux->stream);
//    if((len&(~1))!=(idx->dwChunkLength&(~1))){
//    if((len)!=(idx->dwChunkLength)){
    if((len!=idx->dwChunkLength)&&((len+1)!=idx->dwChunkLength)){
      printf("ChunkSize mismatch! raw=%d idx=%ld  \n",len,idx->dwChunkLength);
      continue;
    }
    if(idx->dwFlags&AVIIF_KEYFRAME) flags=1;
  } else {
    demux->filepos=stream_tell(demux->stream);
    if(demux->filepos>=demux->movi_end){
          demux->stream->eof=1;
          return 0;
    }
    id=stream_read_dword_le(demux->stream);
    len=stream_read_dword_le(demux->stream);
    if(stream_eof(demux->stream)) return 0; // EOF!
    
    if(id==mmioFOURCC('L','I','S','T')){
      id=stream_read_dword_le(demux->stream);      // list type
      continue;
    }
  }
  ret=demux_avi_read_packet(demux,id,len,demux->idx_pos-1,flags);
      if(!ret && skip_video_frames<=0)
        if(--max_packs==0){
          demux->stream->eof=1;
          printf("demux: file doesn't contain the selected audio or video stream\n");
          return 0;
        }
} while(ret!=1);
  return 1;
}


// return value:
//     0 = EOF or no stream found
//     1 = successfully read a packet
int demux_avi_fill_buffer_ni(demuxer_t *demux,demux_stream_t* ds){
unsigned int id=0;
unsigned int len;
int max_packs=128;
int ret=0;

do{
  int flags=0;
  AVIINDEXENTRY *idx=NULL;
  int idx_pos=0;
  demux->filepos=stream_tell(demux->stream);
  
  if(ds==demux->video) idx_pos=demux->idx_pos_a++; else
  if(ds==demux->audio) idx_pos=demux->idx_pos_v++; else
                       idx_pos=demux->idx_pos++;
  
  if(demux->idx_size>0 && idx_pos<demux->idx_size){
    unsigned int pos;
    idx=&((AVIINDEXENTRY *)demux->idx)[idx_pos];
//    idx=&demux->idx[idx_pos];
    
    if(idx->dwFlags&AVIIF_LIST){
      // LIST
      continue;
    }
    if(ds && demux_avi_select_stream(demux,idx->ckid)!=ds){
      if(verbose>2) printf("Skip chunk %.4s (0x%X)  \n",(char *)&idx->ckid,(unsigned int)idx->ckid);
      continue; // skip this chunk
    }

    pos=idx->dwChunkOffset+demux->idx_offset;
    if(pos<demux->movi_start || pos>=demux->movi_end){
      printf("ChunkOffset out of range!  current=0x%X  idx=0x%X  \n",demux->filepos,pos);
      continue;
    }
#if 0
    if(pos!=demux->filepos){
      printf("Warning! pos=0x%X  idx.pos=0x%X  diff=%d   \n",demux->filepos,pos,pos-demux->filepos);
    }
#endif
    stream_seek(demux->stream,pos);

    id=stream_read_dword_le(demux->stream);

    if(stream_eof(demux->stream)) return 0;

    if(id!=idx->ckid){
      printf("ChunkID mismatch! raw=%.4s idx=%.4s  \n",(char *)&id,(char *)&idx->ckid);
      continue;
    }
    len=stream_read_dword_le(demux->stream);
//    if((len&(~1))!=(idx->dwChunkLength&(~1))){
//    if((len)!=(idx->dwChunkLength)){
    if((len!=idx->dwChunkLength)&&((len+1)!=idx->dwChunkLength)){
      printf("ChunkSize mismatch! raw=%d idx=%ld  \n",len,idx->dwChunkLength);
      continue;
    }
    if(idx->dwFlags&AVIIF_KEYFRAME) flags=1;
  } else return 0;
  ret=demux_avi_read_packet(demux,id,len,idx_pos,flags);
      if(!ret && skip_video_frames<=0)
        if(--max_packs==0){
          demux->stream->eof=1;
          printf("demux: file doesn't contain the selected audio or video stream\n");
          return 0;
        }
} while(ret!=1);
  return 1;
}


// return value:
//     0 = EOF or no stream found
//     1 = successfully read a packet
int demux_avi_fill_buffer_nini(demuxer_t *demux,demux_stream_t* ds){
unsigned int id=0;
unsigned int len;
int ret=0;
int *fpos=NULL;

  if(ds==demux->video) fpos=&demux->idx_pos_a; else
  if(ds==demux->audio) fpos=&demux->idx_pos_v; else
  return 0;

  stream_seek(demux->stream,fpos[0]);

do{

  demux->filepos=stream_tell(demux->stream);
  if(demux->filepos>=demux->movi_end){
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
  
  if(ds==demux_avi_select_stream(demux,id)){
    // read it!
    ret=demux_avi_read_packet(demux,id,len,demux->idx_pos-1,0);
  } else {
    // skip it!
    int skip=(len+1)&(~1); // total bytes in this chunk
    stream_skip(demux->stream,skip);
  }
  
} while(ret!=1);
  fpos[0]=stream_tell(demux->stream);
  return 1;
}


