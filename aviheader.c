
#define MIN(a,b) (((a)<(b))?(a):(b))

void read_avi_header(int no_index){

int stream_id=-1;

//---- AVI header:
avi_header.idx_size=0;
while(1){
  int id=stream_read_dword_le(demuxer->stream);
  int chunksize,size2;
  static int last_fccType=0;
  //
  if(stream_eof(demuxer->stream)) break;
  //
  if(id==mmioFOURCC('L','I','S','T')){
    int len=stream_read_dword_le(demuxer->stream)-4; // list size
    id=stream_read_dword_le(demuxer->stream);        // list type
    if(verbose>=2) printf("LIST %.4s  len=%d\n",&id,len);
    if(id==listtypeAVIMOVIE){
      // found MOVI header
      avi_header.movi_start=stream_tell(demuxer->stream);
      avi_header.movi_end=avi_header.movi_start+len;
      if(verbose>=1) printf("Found movie at 0x%X - 0x%X\n",avi_header.movi_start,avi_header.movi_end);
      len=(len+1)&(~1);
      stream_skip(demuxer->stream,len);
    }
    continue;
  }
  size2=stream_read_dword_le(demuxer->stream);
  if(verbose>=2) printf("CHUNK %.4s  len=%d\n",&id,size2);
  chunksize=(size2+1)&(~1);
  switch(id){
    case ckidAVIMAINHDR:          // read 'avih'
      stream_read(demuxer->stream,(char*) &avi_header.avih,MIN(size2,sizeof(avi_header.avih)));
      chunksize-=MIN(size2,sizeof(avi_header.avih));
      if(verbose) print_avih(&avi_header.avih);
      break;
    case ckidSTREAMHEADER: {      // read 'strh'
      AVIStreamHeader h;
      stream_read(demuxer->stream,(char*) &h,MIN(size2,sizeof(h)));
      chunksize-=MIN(size2,sizeof(h));
      if(h.fccType==streamtypeVIDEO) memcpy(&avi_header.video,&h,sizeof(h));else
      if(h.fccType==streamtypeAUDIO) memcpy(&avi_header.audio,&h,sizeof(h));
      last_fccType=h.fccType;
      if(verbose>=1) print_strh(&h);
	  ++stream_id;
      break; }
    case ckidSTREAMFORMAT: {      // read 'strf'
      if(last_fccType==streamtypeVIDEO){
        stream_read(demuxer->stream,(char*) &avi_header.bih,MIN(size2,sizeof(avi_header.bih)));
        chunksize-=MIN(size2,sizeof(avi_header.bih));
//        init_video_codec();
//        init_video_out();
        if(demuxer->video->id==-1) demuxer->video->id=stream_id;
      } else
      if(last_fccType==streamtypeAUDIO){
        int z=(chunksize<64)?chunksize:64;
        if(verbose>=2) printf("found 'wf', %d bytes of %d\n",chunksize,sizeof(WAVEFORMATEX));
        stream_read(demuxer->stream,(char*) &avi_header.wf_ext,z);
        chunksize-=z;
        if(verbose>=1) print_wave_header((WAVEFORMATEX*)&avi_header.wf_ext);
//        init_audio_codec();
//        init_audio_out();
        if(demuxer->audio->id==-1) demuxer->audio->id=stream_id;
      }
      break;
    }
    case ckidAVINEWINDEX: if(!no_index){
      avi_header.idx_size=size2>>4;
      if(verbose>=1) printf("Reading INDEX block, %d chunks for %d frames\n",
        avi_header.idx_size,avi_header.avih.dwTotalFrames);
      avi_header.idx=malloc(avi_header.idx_size<<4);
      stream_read(demuxer->stream,(char*)avi_header.idx,avi_header.idx_size<<4);
      chunksize-=avi_header.idx_size<<4;
      if(verbose>=2) print_index();
      break;
    }
  }
  if(chunksize>0) stream_skip(demuxer->stream,chunksize); else
  if(chunksize<0) printf("WARNING!!! chunksize=%d  (id=%.4s)\n",chunksize,&id);
  
}

}

#undef MIN

