//  ASF file parser for DEMUXER v0.3  by A'rpi/ESP-team

// based on asf file-format doc by Eugene [http://divx.euro.ru]

//static float avi_pts_frametime=1.0f/25.0f;
//static float avi_audio_pts=0;
//static float avi_video_pts=0;

//static int skip_video_frames=0;

typedef struct __attribute__((packed)) {
  unsigned char streamno;
  unsigned char seq;
  unsigned long x;
  unsigned char flag;
} ASF_segmhdr_t;


static int demux_asf_read_packet(demuxer_t *demux,unsigned char *data,int len,int id,int seq,unsigned long time,unsigned short dur,int offs){
  demux_stream_t *ds=NULL;
  
  if(verbose>=4) printf("demux_asf.read_packet: id=%d seq=%d len=%d\n",id,seq,len);
  
  if(demux->video->id==-1)
    if(avi_header.v_streams[id])
        demux->video->id=id;

  if(demux->audio->id==-1)
    if(avi_header.a_streams[id])
        demux->audio->id=id;

  if(id==demux->audio->id){
      // audio
      ds=demux->audio;
      if(!ds->sh){
        ds->sh=avi_header.a_streams[id];
        if(verbose) printf("Auto-selected ASF audio ID = %d\n",ds->id);
      }
  } else 
  if(id==demux->video->id){
      // video
      ds=demux->video;
      if(!ds->sh){
        ds->sh=avi_header.v_streams[id];
        if(verbose) printf("Auto-selected ASF video ID = %d\n",ds->id);
      }
  }
  
  if(ds){
    if(ds->asf_packet){
      if(ds->asf_seq!=seq){
        // closed segment, finalize packet:
		if(ds==demux->audio)
		  if(asf_scrambling_h>1 && asf_scrambling_w>1)
		    asf_descrambling(ds->asf_packet->buffer,ds->asf_packet->len);
        ds_add_packet(ds,ds->asf_packet);
        ds->asf_packet=NULL;
      } else {
        // append data to it!
        demux_packet_t* dp=ds->asf_packet;
        if(dp->len!=offs && offs!=-1) printf("warning! fragment.len=%d BUT next fragment offset=%d  \n",dp->len,offs);
        dp->buffer=realloc(dp->buffer,dp->len+len);
        memcpy(dp->buffer+dp->len,data,len);
        if(verbose>=4) printf("data appended! %d+%d\n",dp->len,len);
        dp->len+=len;
        // we are ready now.
        return 1;
      }
    }
    // create new packet:
    { demux_packet_t* dp;
      if(offs>0){
        printf("warning!  broken fragment, %d bytes missing  \n",offs);
        return 0;
      }
      dp=new_demux_packet(len);
      memcpy(dp->buffer,data,len);
      dp->pts=time*0.001f;
//      if(ds==demux->video) printf("ASF time: %8d  dur: %5d  \n",time,dur);
      dp->pos=demux->filepos;
      ds->asf_packet=dp;
      ds->asf_seq=seq;
      // we are ready now.
      return 1;
    }
  }

  return 0;
}

//static int num_elementary_packets100=0;
//static int num_elementary_packets101=0;

// return value:
//     0 = EOF or no stream found
//     1 = successfully read a packet
int demux_asf_fill_buffer(demuxer_t *demux){

  demux->filepos=stream_tell(demux->stream);
  if(demux->filepos>=demux->endpos){
          demux->stream->eof=1;
          return 0;
  }

    stream_read(demux->stream,asf_packet,(int)fileh.packetsize);
    if(demux->stream->eof) return 0; // EOF
    
    if(asf_packet[0]==0x82){
            unsigned char flags=asf_packet[3];
            unsigned char segtype=asf_packet[4];
            unsigned char* p=&asf_packet[5];
            unsigned char* p_end=p+(int)fileh.packetsize;
            unsigned long time;
            unsigned short duration;
            int segs=1;
            unsigned char segsizetype=0x80;
            int seg;
            int padding=0;
            int plen;
            
            if(verbose>1){
                int i;
                for(i=0;i<16;i++) printf(" %02X",asf_packet[i]);
                printf("\n");
            }
            
            //if(segtype!=0x5d) printf("Warning! packet[4] != 0x5d  \n");

            // Calculate packet size (plen):
            if(flags&0x40){
              // Explicit (absoulte) packet size
              plen=p[0]|(p[1]<<8); p+=2;
              if(verbose>1)printf("Explicit packet size specified: %d  \n",plen);
              if(plen>fileh.packetsize) printf("Warning! plen>packetsize! (%d>%d)  \n",plen,(int)fileh.packetsize);
              if(flags&(8|16)){
                padding=p[0];p++;
                if(flags&16){ padding|=p[0]<<8; p++;}
                if(verbose)printf("Warning! explicit=%d  padding=%d  \n",plen,fileh.packetsize-padding);
              }
            } else {
              // Padding (relative) size
              if(flags&8){
                padding=p[0];++p;
              } else
              if(flags&16){
                padding=p[0]|(p[1]<<8);p+=2;
              }
              plen=fileh.packetsize-padding;
            }

            time=*((unsigned long*)p);p+=4;
            duration=*((unsigned short*)p);p+=2;
            if(flags&1){
              segsizetype=p[0] & 0xC0;
              segs=p[0] & 0x3F;
              ++p;
            }
            if(verbose>=4) printf("%08X:  flag=%02X  segs=%d  pad=%d  time=%d  dur=%d\n",
              demux->filepos,flags,segs,padding,time,duration);
            for(seg=0;seg<segs;seg++){
              //ASF_segmhdr_t* sh;
              unsigned char streamno;
              unsigned char seq;
              int len;
              unsigned long x;
              unsigned char type;
              unsigned long time2;

              if(p>=p_end) printf("Warning! invalid packet 1, sig11 coming soon...\n");

              if(verbose>1){
                int i;
                printf("seg %d:",seg);
                for(i=0;i<16;i++) printf(" %02X",p[i]);
                printf("\n");
              }
              
              streamno=p[0]&0x7F;
              seq=p[1];
              p+=2;

              switch(segtype){
              case 0x55:
                 x=*((unsigned char*)p);
                 p++;
                 break;
              case 0x59:
                 x=*((unsigned short*)p);
                 p+=2;
                 break;
              case 0x5D:
                 x=*((unsigned long*)p);
                 p+=4;
                 break;
              default:
                 printf("Warning! unknown segtype == 0x%2X  \n",segtype);
              }

              type=p[0]; p++;        // 0x01: grouping  0x08: single
              
              switch(type){
              case 0x01:
	        //printf("grouping: %02X  \n",p[0]);
                ++p; // skip unknown byte
                break;
              case 0x08:
                //printf("!!! obj_length = %d\n",*((unsigned long*)p));
                p+=4;
                time2=*((unsigned long*)p);p+=4;
                break;
              default:
                printf("unknown segment type: 0x%02X  \n",type);
              }

              if(flags&1){
                // multiple segments
                if(segsizetype==0x40){
                  len=*((unsigned char*)p);p++;        // 1 byte
                } else {
                  len=*((unsigned short*)p);p+=2;      // 2 byte
                }
              } else {
                // single segment
                len=plen-(p-asf_packet);
              }
              if(len<0 || (p+len)>=p_end){
                printf("ASF_parser: warning! segment len=%d\n",len);
              }
              if(verbose>=4) printf("  seg #%d: streamno=%d  seq=%d  type=%02X  len=%d\n",seg,streamno,seq,type,len);

              switch(type){
              case 0x01:
                // GROUPING:
                //printf("ASF_parser: warning! grouping (flag=1) not yet supported!\n",len);
                //printf("  total: %d  \n",len);
		while(len>0){
		  int len2=p[0];
		  p++;
                  //printf("  group part: %d bytes\n",len2);
                  demux_asf_read_packet(demux,p,len2,streamno,seq,x,duration,-1);
                  p+=len2;
		  len-=len2+1;
		}
                if(len!=0){
                  printf("ASF_parser: warning! groups total != len\n");
                }
                break;
              case 0x08:
                // NO GROUPING:
                //printf("fragment offset: %d  \n",sh->x);
                demux_asf_read_packet(demux,p,len,streamno,seq,time2,duration,x);
                p+=len;
                break;
	      }
              
            } // for segs
            return 1; // success
    }
    
    printf("%08X:  UNKNOWN TYPE  %02X %02X %02X %02X %02X...\n",demux->filepos,asf_packet[0],asf_packet[1],asf_packet[2],asf_packet[3],asf_packet[4]);
    return 0;
}
