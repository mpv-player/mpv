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
	      avi_audio_pts+=pts_correction;
	      pts_corrected=1;
	  } else
          pts_corr_bytes+=len;
      }
            pts=avi_audio_pts; //+pts_correction;
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
     avi_audio_pts=avi_video_pts+pts_correction;
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
      if(verbose) printf("ChunkID mismatch! raw=%.4s idx=%.4s  \n",(char *)&id,(char *)&idx->ckid);
      id=idx->ckid;
//      continue;
    }
    len=stream_read_dword_le(demux->stream);
//    if((len&(~1))!=(idx->dwChunkLength&(~1))){
//    if((len)!=(idx->dwChunkLength)){
    if((len!=idx->dwChunkLength)&&((len+1)!=idx->dwChunkLength)){
      if(verbose) printf("ChunkSize mismatch! raw=%d idx=%ld  \n",len,idx->dwChunkLength);
      len=idx->dwChunkLength;
//      continue;
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
      if(verbose) printf("ChunkID mismatch! raw=%.4s idx=%.4s  \n",(char *)&id,(char *)&idx->ckid);
      id=idx->ckid;
//      continue;
    }
    len=stream_read_dword_le(demux->stream);
//    if((len&(~1))!=(idx->dwChunkLength&(~1))){
//    if((len)!=(idx->dwChunkLength)){
    if((len!=idx->dwChunkLength)&&((len+1)!=idx->dwChunkLength)){
      if(verbose) printf("ChunkSize mismatch! raw=%d idx=%ld  \n",len,idx->dwChunkLength);
      len=idx->dwChunkLength;
//      continue;
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

extern float initial_pts_delay;

void demux_seek_avi(demuxer_t *demuxer,float rel_seek_secs,int flags){
    demux_stream_t *d_audio=demuxer->audio;
    demux_stream_t *d_video=demuxer->video;
    sh_audio_t *sh_audio=d_audio->sh;
    sh_video_t *sh_video=d_video->sh;
    float skip_audio_secs=0;

  //FIXME: OFF_T - Didn't check AVI case yet (avi files can't be >2G anyway?)
  //================= seek in AVI ==========================
    int rel_seek_frames=rel_seek_secs*sh_video->fps;
    int curr_audio_pos=0;
    int audio_chunk_pos=-1;
    int video_chunk_pos=d_video->pos;
    int i;
    
      skip_video_frames=0;
      avi_audio_pts=0;

      // find nearest video keyframe chunk pos:
      if(rel_seek_frames>0){
        // seek forward
        while(video_chunk_pos<demuxer->idx_size){
          int id=((AVIINDEXENTRY *)demuxer->idx)[video_chunk_pos].ckid;
          if(avi_stream_id(id)==d_video->id){  // video frame
            if((--rel_seek_frames)<0 && ((AVIINDEXENTRY *)demuxer->idx)[video_chunk_pos].dwFlags&AVIIF_KEYFRAME) break;
//            ++skip_audio_bytes;
          }
          ++video_chunk_pos;
        }
      } else {
        // seek backward
        while(video_chunk_pos>=0){
          int id=((AVIINDEXENTRY *)demuxer->idx)[video_chunk_pos].ckid;
          if(avi_stream_id(id)==d_video->id){  // video frame
            if((++rel_seek_frames)>0 && ((AVIINDEXENTRY *)demuxer->idx)[video_chunk_pos].dwFlags&AVIIF_KEYFRAME) break;
//            --skip_audio_bytes;
          }
          --video_chunk_pos;
        }
      }
      demuxer->idx_pos_a=demuxer->idx_pos_v=demuxer->idx_pos=video_chunk_pos;
//      printf("%d frames skipped\n",skip_audio_bytes);

      // re-calc video pts:
      d_video->pack_no=0;
      for(i=0;i<video_chunk_pos;i++){
          int id=((AVIINDEXENTRY *)demuxer->idx)[i].ckid;
          if(avi_stream_id(id)==d_video->id) ++d_video->pack_no;
      }
      sh_video->num_frames=d_video->pack_no;
      avi_video_pts=d_video->pack_no*(float)sh_video->video.dwScale/(float)sh_video->video.dwRate;

      if(sh_audio){
        int i;
        int apos=0;
        int last=0;
        int len=0;
	int skip_audio_bytes=0;

        // calc new audio position in audio stream: (using avg.bps value)
        curr_audio_pos=(avi_video_pts) * sh_audio->wf->nAvgBytesPerSec;
        if(curr_audio_pos<0)curr_audio_pos=0;
#if 1
        curr_audio_pos&=~15; // requires for PCM formats!!!
#else
        curr_audio_pos/=sh_audio->wf->nBlockAlign;
        curr_audio_pos*=sh_audio->wf->nBlockAlign;
        demuxer->audio_seekable=1;
#endif

        // find audio chunk pos:
          for(i=0;i<video_chunk_pos;i++){
            int id=((AVIINDEXENTRY *)demuxer->idx)[i].ckid;
            if(avi_stream_id(id)==d_audio->id){
                len=((AVIINDEXENTRY *)demuxer->idx)[i].dwChunkLength;
                last=i;
                if(apos<=curr_audio_pos && curr_audio_pos<(apos+len)){
                  if(verbose)printf("break;\n");
                  break;
                }
                apos+=len;
            }
          }
          if(verbose)printf("XXX i=%d  last=%d  apos=%d  curr_audio_pos=%d  \n",
           i,last,apos,curr_audio_pos);
//          audio_chunk_pos=last; // maybe wrong (if not break; )
          audio_chunk_pos=i; // maybe wrong (if not break; )
          skip_audio_bytes=curr_audio_pos-apos;

          // update stream position:
          d_audio->pos=audio_chunk_pos;
          d_audio->dpos=apos;
	  d_audio->pts=initial_pts_delay+(float)apos/(float)sh_audio->wf->nAvgBytesPerSec;
          demuxer->idx_pos_a=demuxer->idx_pos_v=demuxer->idx_pos=audio_chunk_pos;

          if(!(sh_audio->codec->flags&CODECS_FLAG_SEEKABLE)){
#if 0
//             curr_audio_pos=apos; // selected audio codec can't seek in chunk
             skip_audio_secs=(float)skip_audio_bytes/(float)sh_audio->wf->nAvgBytesPerSec;
             //printf("Seek_AUDIO: %d bytes --> %5.3f secs\n",skip_audio_bytes,skip_audio_secs);
             skip_audio_bytes=0;
#else
             int d=skip_audio_bytes % sh_audio->wf->nBlockAlign;
             skip_audio_bytes-=d;
//             curr_audio_pos-=d;
             skip_audio_secs=(float)d/(float)sh_audio->wf->nAvgBytesPerSec;
             //printf("Seek_AUDIO: %d bytes --> %5.3f secs\n",d,skip_audio_secs);
#endif
          }
          // now: audio_chunk_pos=pos in index
          //      skip_audio_bytes=bytes to skip from that chunk
          //      skip_audio_secs=time to play audio before video (if can't skip)
          
          // calc skip_video_frames & adjust video pts counter:
//          i=last;
	  for(i=demuxer->idx_pos;i<video_chunk_pos;i++){
            int id=((AVIINDEXENTRY *)demuxer->idx)[i].ckid;
            if(avi_stream_id(id)==d_video->id) ++skip_video_frames;
          }
          // requires for correct audio pts calculation (demuxer):
          avi_video_pts-=skip_video_frames*(float)sh_video->video.dwScale/(float)sh_video->video.dwRate;

          if(verbose) printf("SEEK: idx=%d  (a:%d v:%d)  v.skip=%d  a.skip=%d/%4.3f  \n",
            demuxer->idx_pos,audio_chunk_pos,video_chunk_pos,
            skip_video_frames,skip_audio_bytes,skip_audio_secs);

          if(skip_audio_bytes){
            demux_read_data(d_audio,NULL,skip_audio_bytes);
            //d_audio->pts=0; // PTS is outdated because of the raw data skipping
          }
	  resync_audio_stream(sh_audio);

          sh_audio->timer=-skip_audio_secs;

      }


}



