//  MPG/VOB file parser for DEMUXER v2.5  by A'rpi/ESP-team

//#define MAX_PS_PACKETSIZE 2048
#define MAX_PS_PACKETSIZE (224*1024)

static unsigned int read_mpeg_timestamp(stream_t *s,int c){
  int d,e;
  unsigned int pts;
  d=stream_read_word(s);
  e=stream_read_word(s);
  if( ((c&1)!=1) || ((d&1)!=1) || ((e&1)!=1) ) return 0; // invalid pts
  pts=(((c>>1)&7)<<30)|((d>>1)<<15)|(e>>1);
  if(verbose>=3) printf("{%d}",pts);
  return pts;
}

//static char dvdaudio_table[256];
//static unsigned int packet_start_pos=0;

static int demux_mpg_read_packet(demuxer_t *demux,int id){
  int d;
  int len;
#ifdef HAVE_LIBCSS
  int css=0;
#endif
  unsigned char c=0;
  unsigned int pts=0;
  unsigned int dts=0;
  demux_stream_t *ds=NULL;
  
  if(verbose>=3) printf("demux_read_packet: %X\n",id);

//  if(id==0x1F0){
//    demux->synced=0; // force resync after 0x1F0
//    return -1;
//}

//  if(id==0x1BA) packet_start_pos=stream_tell(demux->stream);
  if(id<0x1BC || id>=0x1F0) return -1;
  if(id==0x1BE) return -1; // padding stream
  if(id==0x1BF) return -1; // private2

  len=stream_read_word(demux->stream);
  if(verbose>=3)  printf("PACKET len=%d",len);
//  if(len==62480){ demux->synced=0;return -1;} /* :) */
//  if(len==0 || len>MAX_PS_PACKETSIZE) return -2;  // invalid packet !!!!!!
  if(len==0 || len>MAX_PS_PACKETSIZE){
    if(verbose>=2) printf("Invalid PS packet len: %d\n",len);
    return -2;  // invalid packet !!!!!!
  }

  while(len>0){   // Skip stuFFing bytes
    c=stream_read_char(demux->stream);--len;
    if(c!=0xFF)break;
  }
  if((c>>6)==1){  // Read (skip) STD scale & size value
//    printf("  STD_scale=%d",(c>>5)&1);
    d=((c&0x1F)<<8)|stream_read_char(demux->stream);
    len-=2;
//    printf("  STD_size=%d",d);
    c=stream_read_char(demux->stream);
  }
  // Read System-1 stream timestamps:
  if((c>>4)==2){
    pts=read_mpeg_timestamp(demux->stream,c);
    len-=4;
  } else
  if((c>>4)==3){
    pts=read_mpeg_timestamp(demux->stream,c);
    c=stream_read_char(demux->stream);
    if((c>>4)!=1) pts=0; //printf("{ERROR4}");
    dts=read_mpeg_timestamp(demux->stream,c);
    len-=4+1+4;
  } else
  if((c>>6)==2){
    int pts_flags;
    int hdrlen;
    // System-2 (.VOB) stream:
    if((c>>4)&3) {
#ifdef HAVE_LIBCSS
        css=1;
#else
        printf("Encrypted VOB file (not compiled with libcss support)! Read file DOCS/DVD\n");
#endif
    }
    c=stream_read_char(demux->stream); pts_flags=c>>6;
    c=stream_read_char(demux->stream); hdrlen=c;
    len-=2;
    if(verbose>=3) printf("  hdrlen=%d  (len=%d)",hdrlen,len);
    if(hdrlen>len){ printf("demux_mpg: invalid header length  \n"); return -1;}
    if(pts_flags==2){
      c=stream_read_char(demux->stream);
      pts=read_mpeg_timestamp(demux->stream,c);
      len-=5;hdrlen-=5;
    } else
    if(pts_flags==3){
      c=stream_read_char(demux->stream);
      pts=read_mpeg_timestamp(demux->stream,c);
      c=stream_read_char(demux->stream);
      dts=read_mpeg_timestamp(demux->stream,c);
      len-=10;hdrlen-=10;
    }
    len-=hdrlen;
    if(hdrlen>0) stream_skip(demux->stream,hdrlen); // skip header bytes
    
    //============== DVD Audio sub-stream ======================
    if(id==0x1BD){
      int aid=128+(stream_read_char(demux->stream)&0x7F);--len;
      if(len<3) return -1; // invalid audio packet

      if(!avi_header.a_streams[aid]) new_sh_audio(aid);
      if(demux->audio->id==-1) demux->audio->id=aid;

      if(demux->audio->id==aid){
//        int type;
        ds=demux->audio;
        if(!ds->sh) ds->sh=avi_header.a_streams[aid];
        // READ Packet: Skip additional audio header data:
        c=stream_read_char(demux->stream);//type=c;
        c=stream_read_char(demux->stream);//type|=c<<8;
        c=stream_read_char(demux->stream);//type|=c<<16;
//        printf("[%06X]",type);
        len-=3;
        if(ds->type==-1){
          // autodetect type
          ds->type=((aid&0x70)==0x20)?2:3;
        }
        if(ds->type==2 && len>=2){
          // read PCM header
          int head;
          head=stream_read_char(demux->stream); head=c<<8;
          c=stream_read_char(demux->stream);    head|=c;  len-=2;
          while(len>0 && head!=0x180){
            head=c<<8;
            c=stream_read_char(demux->stream);
            head|=c;--len;
          }
          if(!len) printf("End of packet while searching for PCM header\n");
        }
      }
    }

  } else {
    if(c!=0x0f){
      printf("  {ERROR5,c=%d}  \n",c);
      return -1;  // invalid packet !!!!!!
    }
  }
  if(verbose>=3) printf(" => len=%d\n",len);

//  if(len<=0 || len>MAX_PS_PACKETSIZE) return -1;  // Invalid packet size
  if(len<=0 || len>MAX_PS_PACKETSIZE){
    if(verbose>=2) printf("Invalid PS data len: %d\n",len);
    return -1;  // invalid packet !!!!!!
  }
  
  if(id>=0x1C0 && id<=0x1DF){
    // mpeg audio
    int aid=id-0x1C0;
    if(!avi_header.a_streams[aid]) new_sh_audio(aid);
    if(demux->audio->id==-1) demux->audio->id=aid;
    if(demux->audio->id==aid){
      ds=demux->audio;
      if(!ds->sh) ds->sh=avi_header.a_streams[aid];
      if(ds->type==-1) ds->type=1;
    }
  } else
  if(id>=0x1E0 && id<=0x1EF){
    // mpeg video
    int aid=id-0x1E0;
    if(!avi_header.v_streams[aid]) new_sh_video(aid);
    if(demux->video->id==-1) demux->video->id=aid;
    if(demux->video->id==aid){
      ds=demux->video;
      if(!ds->sh) ds->sh=avi_header.v_streams[aid];
    }
  }

  if(ds){
    if(verbose>=2) printf("DEMUX_MPG: Read %d data bytes from packet %04X\n",len,id);
//    printf("packet start = 0x%X  \n",stream_tell(demux->stream)-packet_start_pos);
#ifdef HAVE_LIBCSS
    if (css) CSSDescramble(demux->stream->buffer,key_title);
#endif
    ds_read_packet(ds,demux->stream,len,pts/90000.0f,0);
    return 1;
  }
  if(verbose>=2) printf("DEMUX_MPG: Skipping %d data bytes from packet %04X\n",len,id);
  if(len<=2356) stream_skip(demux->stream,len);
  return 0;
}

static int num_elementary_packets100=0;
static int num_elementary_packets101=0;

int demux_mpg_es_fill_buffer(demuxer_t *demux){
//if(demux->type==DEMUXER_TYPE_MPEG_ES)
  // Elementary video stream
  if(demux->stream->eof) return 0;
  demux->filepos=stream_tell(demux->stream);
  ds_read_packet(demux->video,demux->stream,STREAM_BUFFER_SIZE,0,0);
  return 1;
}

int demux_mpg_fill_buffer(demuxer_t *demux){
unsigned int head=0;
int skipped=0;
int max_packs=128;
int ret=0;

// System stream
do{
  demux->filepos=stream_tell(demux->stream);
  head=stream_read_dword(demux->stream);
  demux->filepos-=skipped;
  while(1){
    int c=stream_read_char(demux->stream);
    if(c<0) break; //EOF
    head<<=8;
    if(head!=0x100){
      head|=c;
      ++skipped; //++demux->filepos;
      continue;
    }
    head|=c;
    break;
  }
  demux->filepos+=skipped;
  if(stream_eof(demux->stream)) break;
  // sure: head=0x000001XX
  if(verbose>=4) printf("*** head=0x%X\n",head);
  if(demux->synced==0){
    if(head==0x1BA) demux->synced=1;
  } else
  if(demux->synced==1){
    if(head==0x1BB || (head>=0x1C0 && head<=0x1EF)){
      demux->synced=2;
      if(verbose) printf("system stream synced at 0x%X (%d)!\n",demux->filepos,demux->filepos);
      num_elementary_packets100=0; // requires for re-sync!
      num_elementary_packets101=0; // requires for re-sync!
    } else demux->synced=0;
  } // else
  if(demux->synced==2){
      ret=demux_mpg_read_packet(demux,head);
      if(!ret)
        if(--max_packs==0){
          demux->stream->eof=1;
          printf("demux: file doesn't contain the selected audio or video stream\n");
          return 0;
        }
  } else {
    if(head>=0x100 && head<0x1B0){
      if(head==0x100)
        ++num_elementary_packets100;
      else
        if(head==0x101) ++num_elementary_packets101;
      if(verbose>=3) printf("Opps... elementary video packet found: %03X\n",head);
    }
#if 1
    if(num_elementary_packets100>50 && num_elementary_packets101>50
       && skipped>4000000){
        if(verbose) printf("sync_mpeg_ps: seems to be ES stream...\n");
        demux->stream->eof=1;
        break;
    }
#endif
  }
} while(ret!=1);
  if(verbose>=2) printf("demux: %d bad bytes skipped\n",skipped);
  if(demux->stream->eof){
    if(verbose>=2) printf("MPEG Stream reached EOF\n");
    return 0;
  }
  return 1;
}

