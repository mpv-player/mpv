
  if(file_format==DEMUXER_TYPE_AVI && demuxer->idx_size<=0){
    printf("Can't seek in raw .AVI streams! (index required, try with the -idx switch!)  \n");
  } else {
    int skip_audio_bytes=0;
    float skip_audio_secs=0;

    // clear demux buffers:
    if(sh_audio){ ds_free_packs(d_audio);sh_audio->a_buffer_len=0;}
    ds_free_packs(d_video);
    
//    printf("sh_audio->a_buffer_len=%d  \n",sh_audio->a_buffer_len);
    

switch(file_format){

  case DEMUXER_TYPE_AVI: {
  //================= seek in AVI ==========================
    int rel_seek_frames=rel_seek_secs*sh_video->fps;
    int curr_audio_pos=0;
    int audio_chunk_pos=-1;
    int video_chunk_pos=d_video->pos;
    
      skip_video_frames=0;
      avi_audio_pts=0;

      // find nearest video keyframe chunk pos:
      if(rel_seek_frames>0){
        // seek forward
        while(video_chunk_pos<demuxer->idx_size){
          int id=((AVIINDEXENTRY *)demuxer->idx)[video_chunk_pos].ckid;
          if(avi_stream_id(id)==d_video->id){  // video frame
            if((--rel_seek_frames)<0 && ((AVIINDEXENTRY *)demuxer->idx)[video_chunk_pos].dwFlags&AVIIF_KEYFRAME) break;
            ++skip_audio_bytes;
          }
          ++video_chunk_pos;
        }
      } else {
        // seek backward
        while(video_chunk_pos>=0){
          int id=((AVIINDEXENTRY *)demuxer->idx)[video_chunk_pos].ckid;
          if(avi_stream_id(id)==d_video->id){  // video frame
            if((++rel_seek_frames)>0 && ((AVIINDEXENTRY *)demuxer->idx)[video_chunk_pos].dwFlags&AVIIF_KEYFRAME) break;
            --skip_audio_bytes;
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
          
      }

      if(verbose) printf("SEEK: idx=%d  (a:%d v:%d)  v.skip=%d  a.skip=%d/%4.3f  \n",
        demuxer->idx_pos,audio_chunk_pos,video_chunk_pos,
        skip_video_frames,skip_audio_bytes,skip_audio_secs);

  }
  break;

  case DEMUXER_TYPE_ASF: {
  //================= seek in ASF ==========================
    float p_rate=10; // packets / sec
    int rel_seek_packs=rel_seek_secs*p_rate;
    int rel_seek_bytes=rel_seek_packs*asf_packetsize;
    int newpos;
    //printf("ASF: packs: %d  duration: %d  \n",(int)fileh.packets,*((int*)&fileh.duration));
//    printf("ASF_seek: %d secs -> %d packs -> %d bytes  \n",
//       rel_seek_secs,rel_seek_packs,rel_seek_bytes);
    newpos=demuxer->filepos+rel_seek_bytes;
    if(newpos<0 || newpos<demuxer->movi_start) newpos=demuxer->movi_start;
//    printf("\r -- asf: newpos=%d -- \n",newpos);
    stream_seek(demuxer->stream,newpos);

    ds_fill_buffer(d_video);
    if(sh_audio) ds_fill_buffer(d_audio);
    
    while(1){
	if(sh_audio){
	  // sync audio:
          if (d_video->pts > d_audio->pts){
	      if(!ds_fill_buffer(d_audio)) sh_audio=NULL; // skip audio. EOF?
	      continue;
	  }
	}
	if(d_video->flags&1) break; // found a keyframe!
	if(!ds_fill_buffer(d_video)) break; // skip frame.  EOF?
    }

  }
  break;
  
  case DEMUXER_TYPE_MPEG_ES:
  case DEMUXER_TYPE_MPEG_PS: {
  //================= seek in MPEG ==========================
        int newpos;
        if(!sh_video->i_bps) // unspecified?
          newpos=demuxer->filepos+2324*75*rel_seek_secs; // 174.3 kbyte/sec
        else
          newpos=demuxer->filepos+(sh_video->i_bps)*rel_seek_secs;

        if(newpos<seek_to_byte) newpos=seek_to_byte;
        newpos&=~(STREAM_BUFFER_SIZE-1);  /* sector boundary */
        stream_seek(demuxer->stream,newpos);
        // re-sync video:
        videobuf_code_len=0; // reset ES stream buffer
        while(1){
          int i=sync_video_packet(d_video);
          if(i==0x1B3 || i==0x1B8) break; // found it!
          if(!i || !skip_video_packet(d_video)){ eof=1; break;} // EOF
        }
  }
  break;

} // switch(file_format)

      //====================== re-sync audio: =====================
      if(sh_audio){

        if(skip_audio_bytes){
          demux_read_data(d_audio,NULL,skip_audio_bytes);
          //d_audio->pts=0; // PTS is outdated because of the raw data skipping
        }
        
        current_module="resync_audio";
	resync_audio_stream(sh_audio);

        // re-sync PTS (MPEG-PS only!!!)
        if(file_format==DEMUXER_TYPE_MPEG_PS)
        if(d_video->pts && d_audio->pts){
          if (d_video->pts < d_audio->pts){
          
          } else {
            while(d_video->pts > d_audio->pts){
	      skip_audio_frame(sh_audio);
            }
          }
        }

        current_module="audio_reset";
        audio_out->reset(); // stop audio, throwing away buffered data
        current_module=NULL;

        c_total=0; // kell ez?
        printf("A:%6.1f  V:%6.1f  A-V:%7.3f",d_audio->pts,d_video->pts,0.0f);
        printf("  ct:%7.3f   \r",c_total);fflush(stdout);
      } else {
        printf("A: ---   V:%6.1f   \r",d_video->pts);fflush(stdout);
      }

        // Set OSD:
      if(osd_level){
        int len=((demuxer->movi_end-demuxer->movi_start)>>8);
        if(len>0){
          osd_visible=sh_video->fps; // 1 sec
          vo_osd_progbar_type=0;
          vo_osd_progbar_value=(demuxer->filepos-demuxer->movi_start)/len;
        }
      }

      max_pts_correction=0.1;
      frame_corr_num=0; // -5
      frame_correction=0;
      force_redraw=5;
      if(sh_audio) sh_audio->timer=-skip_audio_secs;
      sh_video->timer=0; // !!!!!!
      audio_time_usage=0; video_time_usage=0; vout_time_usage=0;

  }
 rel_seek_secs=0;
