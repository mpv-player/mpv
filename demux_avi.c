//  AVI file parser for DEMUXER v2.6  by A'rpi/ESP-team

//static float avi_pts_frametime=1.0f/25.0f;
static float avi_audio_pts=0;
static float avi_video_pts=0;

static int skip_video_frames=0;

static inline int avi_stream_id(unsigned int id){
  unsigned char *p=(unsigned char *)&id;
  unsigned char a,b;
  a=p[0]-'0'; b=p[1]-'0';
  if(a>9 || b>9) return 100; // invalid ID
  return a*10+b;
}

// Select ds from ID
static inline demux_stream_t* demux_avi_select_stream(demuxer_t *demux,unsigned int id){
  int stream_id=avi_stream_id(id);
  if(stream_id==demux->audio->id) return demux->audio;
  if(stream_id==demux->video->id) return demux->video;
  if(id!=mmioFOURCC('J','U','N','K')){
     // unknown
     if(verbose>=2) printf("Unknown chunk: %.4s (%X)\n",&id,id);
  }
  return NULL;
}

static int demux_avi_read_packet(demuxer_t *demux,unsigned int id,unsigned int len,int idxpos){
  int skip;
  float pts=0;
  demux_stream_t *ds=demux_avi_select_stream(demux,id);
  
  if(verbose>=3) printf("demux_avi.read_packet: %X\n",id);

  if(ds==demux->audio){
            pts=avi_audio_pts;
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
     avi_video_pts+=(float)((sh_video_t*)(demux->video->sh))->video.dwScale/(float)((sh_video_t*)(demux->video->sh))->video.dwRate;
     avi_audio_pts=avi_video_pts;
  }
  
//  len=stream_read_dword_le(demux->stream);
  skip=(len+1)&(~1); // total bytes in this chunk
  
  if(ds){
    if(verbose>=2) printf("DEMUX_AVI: Read %d data bytes from packet %04X\n",len,id);
    ds_read_packet(ds,demux->stream,len,pts,idxpos);
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
  AVIINDEXENTRY *idx=NULL;
  demux->filepos=stream_tell(demux->stream);
  if(demux->filepos>=demux->endpos){
          demux->stream->eof=1;
          return 0;
  }
  if(stream_eof(demux->stream)) return 0;
  if(avi_header.idx_size>0 && avi_header.idx_pos<avi_header.idx_size){
    unsigned int pos;
    
    //if(avi_header.idx_pos<0) printf("Fatal! idx_pos=%d\n",avi_header.idx_pos);
    
    idx=&avi_header.idx[avi_header.idx_pos++];
    
    //printf("[%d]",avi_header.idx_pos);fflush(stdout);
    
    //stream_seek(demux->stream,idx.dwChunkOffset);
    //printf("IDX  pos=%X  idx.pos=%X  idx.size=%X  idx.flags=%X\n",demux->filepos,
    //  pos-4,idx->dwChunkLength,idx->dwFlags);
    if(idx->dwFlags&AVIIF_LIST){
      // LIST
      continue;
    }
    if(!demux_avi_select_stream(demux,idx->ckid)){
      if(verbose>2) printf("Skip chunk %.4s (0x%X)  \n",&idx->ckid,idx->ckid);
      continue; // skip this chunk
    }

    pos=idx->dwChunkOffset+avi_header.idx_offset;
    if(pos<avi_header.movi_start || pos>=avi_header.movi_end){
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
    if(id!=idx->ckid){
      printf("ChunkID mismatch! raw=%.4s idx=%.4s  \n",&id,&idx->ckid);
      continue;
    }
    len=stream_read_dword_le(demux->stream);
//    if((len&(~1))!=(idx->dwChunkLength&(~1))){
//    if((len)!=(idx->dwChunkLength)){
    if((len!=idx->dwChunkLength)&&((len+1)!=idx->dwChunkLength)){
      printf("ChunkSize mismatch! raw=%d idx=%d  \n",len,idx->dwChunkLength);
      continue;
    }
  } else {
    id=stream_read_dword_le(demux->stream);
    len=stream_read_dword_le(demux->stream);
    if(id==mmioFOURCC('L','I','S','T')){
      id=stream_read_dword_le(demux->stream);      // list type
      continue;
    }
  }
  ret=demux_avi_read_packet(demux,id,len,avi_header.idx_pos-1);
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
  AVIINDEXENTRY *idx=NULL;
  int idx_pos=0;
  demux->filepos=stream_tell(demux->stream);
  
  if(ds==demux->video) idx_pos=avi_header.idx_pos_a++; else
  if(ds==demux->audio) idx_pos=avi_header.idx_pos_v++; else
                       idx_pos=avi_header.idx_pos++;
  
  if(avi_header.idx_size>0 && idx_pos<avi_header.idx_size){
    unsigned int pos;
    idx=&avi_header.idx[idx_pos];
    
    if(idx->dwFlags&AVIIF_LIST){
      // LIST
      continue;
    }
    if(ds && demux_avi_select_stream(demux,idx->ckid)!=ds){
      if(verbose>2) printf("Skip chunk %.4s (0x%X)  \n",&idx->ckid,idx->ckid);
      continue; // skip this chunk
    }

    pos=idx->dwChunkOffset+avi_header.idx_offset;
    if(pos<avi_header.movi_start || pos>=avi_header.movi_end){
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
      printf("ChunkID mismatch! raw=%.4s idx=%.4s  \n",&id,&idx->ckid);
      continue;
    }
    len=stream_read_dword_le(demux->stream);
//    if((len&(~1))!=(idx->dwChunkLength&(~1))){
//    if((len)!=(idx->dwChunkLength)){
    if((len!=idx->dwChunkLength)&&((len+1)!=idx->dwChunkLength)){
      printf("ChunkSize mismatch! raw=%d idx=%d  \n",len,idx->dwChunkLength);
      continue;
    }
  } else return 0;
  ret=demux_avi_read_packet(demux,id,len,idx_pos);
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

  if(ds==demux->video) fpos=&avi_header.idx_pos_a; else
  if(ds==demux->audio) fpos=&avi_header.idx_pos_v; else
  return 0;

  stream_seek(demux->stream,fpos[0]);

do{

  demux->filepos=stream_tell(demux->stream);
  if(demux->filepos>=demux->endpos){
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
    ret=demux_avi_read_packet(demux,id,len,avi_header.idx_pos-1);
  } else {
    // skip it!
    int skip=(len+1)&(~1); // total bytes in this chunk
    stream_skip(demux->stream,skip);
  }
  
} while(ret!=1);
  fpos[0]=stream_tell(demux->stream);
  return 1;
}


