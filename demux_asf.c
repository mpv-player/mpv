//  ASF file parser for DEMUXER v0.3  by A'rpi/ESP-team

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "mp_msg.h"

#include "stream.h"
#include "asf.h"
#include "demuxer.h"


/*
 * Load 16/32-bit values in little endian byte order
 * from an unaligned address
 */
#ifdef ARCH_X86
#define	LOAD_LE32(p)	(*(unsigned int*)(p))
#define	LOAD_LE16(p)	(*(unsigned short*)(p))
#else
#define	LOAD_LE32(p)	(((unsigned char*)(p))[0]     | \
 			 ((unsigned char*)(p))[1]<< 8 | \
 			 ((unsigned char*)(p))[2]<<16 | \
 			 ((unsigned char*)(p))[3]<<24 )
#define	LOAD_LE16(p)	(((unsigned char*)(p))[0]     | \
			 ((unsigned char*)(p))[1]<<8)
#endif

// defined at asfheader.c:
extern unsigned char* asf_packet;
extern int asf_scrambling_h;
extern int asf_scrambling_w;
extern int asf_scrambling_b;
extern int asf_packetsize;


// based on asf file-format doc by Eugene [http://divx.euro.ru]

static void asf_descrambling(unsigned char *src,int len){
  unsigned char *dst=malloc(len);
  unsigned char *s2=src;
  int i=0,x,y;
  while(len-i>=asf_scrambling_h*asf_scrambling_w*asf_scrambling_b){
//    mp_msg(MSGT_DEMUX,MSGL_DBG4,"descrambling! (w=%d  b=%d)\n",w,asf_scrambling_b);
	//i+=asf_scrambling_h*asf_scrambling_w;
	for(x=0;x<asf_scrambling_w;x++)
	  for(y=0;y<asf_scrambling_h;y++){
	    memcpy(dst+i,s2+(y*asf_scrambling_w+x)*asf_scrambling_b,asf_scrambling_b);
		i+=asf_scrambling_b;
	  }
	s2+=asf_scrambling_h*asf_scrambling_w*asf_scrambling_b;
  }
  //if(i<len) memcpy(dst+i,src+i,len-i);
  memcpy(src,dst,i);
  free(dst);
}


static int demux_asf_read_packet(demuxer_t *demux,unsigned char *data,int len,int id,int seq,unsigned long time,unsigned short dur,int offs,int keyframe){
  demux_stream_t *ds=NULL;
  
  mp_dbg(MSGT_DEMUX,MSGL_DBG4,"demux_asf.read_packet: id=%d seq=%d len=%d\n",id,seq,len);
  
  if(demux->video->id==-1)
    if(demux->v_streams[id])
        demux->video->id=id;

  if(demux->audio->id==-1)
    if(demux->a_streams[id])
        demux->audio->id=id;

  if(id==demux->audio->id){
      // audio
      ds=demux->audio;
      if(!ds->sh){
        ds->sh=demux->a_streams[id];
        mp_msg(MSGT_DEMUX,MSGL_V,"Auto-selected ASF audio ID = %d\n",ds->id);
      }
  } else 
  if(id==demux->video->id){
      // video
      ds=demux->video;
      if(!ds->sh){
        ds->sh=demux->v_streams[id];
        mp_msg(MSGT_DEMUX,MSGL_V,"Auto-selected ASF video ID = %d\n",ds->id);
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
        if(dp->len!=offs && offs!=-1) mp_msg(MSGT_DEMUX,MSGL_V,"warning! fragment.len=%d BUT next fragment offset=%d  \n",dp->len,offs);
        dp->buffer=realloc(dp->buffer,dp->len+len);
        memcpy(dp->buffer+dp->len,data,len);
        mp_dbg(MSGT_DEMUX,MSGL_DBG4,"data appended! %d+%d\n",dp->len,len);
        dp->len+=len;
        // we are ready now.
        return 1;
      }
    }
    // create new packet:
    { demux_packet_t* dp;
      if(offs>0){
        mp_msg(MSGT_DEMUX,MSGL_V,"warning!  broken fragment, %d bytes missing  \n",offs);
        return 0;
      }
      dp=new_demux_packet(len);
      memcpy(dp->buffer,data,len);
      dp->pts=time*0.001f;
      dp->flags=keyframe;
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
  if(demux->filepos>=demux->movi_end){
          demux->stream->eof=1;
          return 0;
  }

    stream_read(demux->stream,asf_packet,asf_packetsize);
    if(demux->stream->eof) return 0; // EOF
    
    if(asf_packet[0]==0x82){
            unsigned char flags=asf_packet[3];
            unsigned char segtype=asf_packet[4];
            unsigned char* p=&asf_packet[5];
            unsigned char* p_end=p+asf_packetsize;
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
              plen=LOAD_LE16(p); p+=2;
              mp_dbg(MSGT_DEMUX,MSGL_DBG2,"Explicit packet size specified: %d  \n",plen);
              if(plen>asf_packetsize) mp_msg(MSGT_DEMUX,MSGL_V,"Warning! plen>packetsize! (%d>%d)  \n",plen,asf_packetsize);
              if(flags&(8|16)){
                padding=p[0];p++;
                if(flags&16){ padding|=p[0]<<8; p++;}
                mp_msg(MSGT_DEMUX,MSGL_V,"Warning! explicit=%d  padding=%d  \n",plen,asf_packetsize-padding);
              }
            } else {
              // Padding (relative) size
              if(flags&8){
                padding=p[0];++p;
              } else
              if(flags&16){
                padding=LOAD_LE16(p);p+=2;
              }
              plen=asf_packetsize-padding;
            }

	    time = LOAD_LE32(p); p+=4;
	    duration = LOAD_LE16(p); p+=2;
            if(flags&1){
              segsizetype=p[0] & 0xC0;
              segs=p[0] & 0x3F;
              ++p;
            }
            mp_dbg(MSGT_DEMUX,MSGL_DBG4,"%08X:  flag=%02X  segs=%d  pad=%d  time=%ld  dur=%d\n",
              demux->filepos,flags,segs,padding,time,duration);
            for(seg=0;seg<segs;seg++){
              //ASF_segmhdr_t* sh;
              unsigned char streamno;
              unsigned char seq;
              int len;
              unsigned long x;
              unsigned char type;
              unsigned long time2;
	      int keyframe=0;

              if(p>=p_end) mp_msg(MSGT_DEMUX,MSGL_V,"Warning! invalid packet 1, sig11 coming soon...\n");

              if(verbose>1){
                int i;
                printf("seg %d:",seg);
                for(i=0;i<16;i++) printf(" %02X",p[i]);
                printf("\n");
              }
              
              streamno=p[0]&0x7F;
	      if(p[0]&0x80) keyframe=1;
              seq=p[1];
              p+=2;

              switch(segtype){
              case 0x55:
                 x=*((unsigned char*)p);
                 p++;
                 break;
              case 0x59:
                 x=LOAD_LE16(p);
                 p+=2;
                 break;
              case 0x5D:
		 x=LOAD_LE32(p);
                 p+=4;
                 break;
              default:
                 mp_msg(MSGT_DEMUX,MSGL_V,"Warning! unknown segtype == 0x%2X  \n",segtype);
		 x=0;
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
                time2=LOAD_LE32(p);p+=4;
                break;
              default:
                mp_msg(MSGT_DEMUX,MSGL_V,"unknown segment type: 0x%02X  \n",type);
              }

              if(flags&1){
                // multiple segments
                if(segsizetype==0x40){
                  len=*((unsigned char*)p);p++;		// 1 byte
                } else {
                  len=LOAD_LE16(p);p+=2;		// 2 byte
                }
              } else {
                // single segment
                len=plen-(p-asf_packet);
              }
              if(len<0 || (p+len)>=p_end){
                mp_msg(MSGT_DEMUX,MSGL_V,"ASF_parser: warning! segment len=%d\n",len);
              }
              mp_dbg(MSGT_DEMUX,MSGL_DBG4,"  seg #%d: streamno=%d  seq=%d  type=%02X  len=%d\n",seg,streamno,seq,type,len);

              switch(type){
              case 0x01:
                // GROUPING:
                //printf("ASF_parser: warning! grouping (flag=1) not yet supported!\n",len);
                //printf("  total: %d  \n",len);
		while(len>0){
		  int len2=p[0];
		  p++;
                  //printf("  group part: %d bytes\n",len2);
                  demux_asf_read_packet(demux,p,len2,streamno,seq,x,duration,-1,keyframe);
                  p+=len2;
		  len-=len2+1;
		}
                if(len!=0){
                  mp_msg(MSGT_DEMUX,MSGL_V,"ASF_parser: warning! groups total != len\n");
                }
                break;
              case 0x08:
                // NO GROUPING:
                //printf("fragment offset: %d  \n",sh->x);
                demux_asf_read_packet(demux,p,len,streamno,seq,time2,duration,x,keyframe);
                p+=len;
                break;
	      }
              
            } // for segs
            return 1; // success
    }
    
    mp_msg(MSGT_DEMUX,MSGL_V,"%08X:  UNKNOWN TYPE  %02X %02X %02X %02X %02X...\n",demux->filepos,asf_packet[0],asf_packet[1],asf_packet[2],asf_packet[3],asf_packet[4]);
    return 0;
}


#include "wine/mmreg.h"
#include "wine/avifmt.h"
#include "wine/vfw.h"

#include "codec-cfg.h"
#include "stheader.h"


void demux_seek_asf(demuxer_t *demuxer,float rel_seek_secs,int flags){
    demux_stream_t *d_audio=demuxer->audio;
    demux_stream_t *d_video=demuxer->video;
    sh_audio_t *sh_audio=d_audio->sh;
//    sh_video_t *sh_video=d_video->sh;

  //FIXME: OFF_T - didn't test ASF case yet (don't have a large asf...)
  //FIXME: reports good or bad to steve@daviesfam.org please

  //================= seek in ASF ==========================
    float p_rate=10; // packets / sec
    off_t rel_seek_packs=rel_seek_secs*p_rate; // FIXME: int may be enough?
    off_t rel_seek_bytes=rel_seek_packs*asf_packetsize;
    off_t newpos;
    //printf("ASF: packs: %d  duration: %d  \n",(int)fileh.packets,*((int*)&fileh.duration));
//    printf("ASF_seek: %d secs -> %d packs -> %d bytes  \n",
//       rel_seek_secs,rel_seek_packs,rel_seek_bytes);
    newpos=demuxer->filepos+rel_seek_bytes;
    if(newpos<0 || newpos<demuxer->movi_start) newpos=demuxer->movi_start;
//    printf("\r -- asf: newpos=%d -- \n",newpos);
    stream_seek(demuxer->stream,newpos);

    ds_fill_buffer(d_video);
    if(sh_audio){
      ds_fill_buffer(d_audio);
      resync_audio_stream(sh_audio);
    }
    
    while(1){
	if(sh_audio && !d_audio->eof){
	  float a_pts=d_audio->pts;
          a_pts+=(ds_tell_pts(d_audio)-sh_audio->a_in_buffer_len)/(float)sh_audio->i_bps;
	  // sync audio:
          if (d_video->pts > a_pts){
	      skip_audio_frame(sh_audio);
//	      if(!ds_fill_buffer(d_audio)) sh_audio=NULL; // skip audio. EOF?
	      continue;
	  }
	}
	if(d_video->flags&1) break; // found a keyframe!
	if(!ds_fill_buffer(d_video)) break; // skip frame.  EOF?
    }


}

