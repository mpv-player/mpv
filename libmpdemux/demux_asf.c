//  ASF file parser for DEMUXER v0.3  by A'rpi/ESP-team

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"

#include "stream/stream.h"
#include "asf.h"
#include "demuxer.h"

#include "libvo/fastmemcpy.h"

#define ASFMIN(a,b) ((a) > (b) ? (b) : (a))
#define SLICE_MIN_START_CODE    0x00000101
#define SLICE_MAX_START_CODE    0x000001af
#define END_NOT_FOUND -100

/*
 * Load 16/32-bit values in little endian byte order
 * from an unaligned address
 */
#ifdef ARCH_X86
#define	LOAD_LE32(p)	(*(unsigned int*)(p))
#define	LOAD_LE16(p)	(*(unsigned short*)(p))
#define	LOAD_BE32(p)	(((unsigned char*)(p))[3]     | \
 			 ((unsigned char*)(p))[2]<< 8 | \
 			 ((unsigned char*)(p))[1]<<16 | \
 			 ((unsigned char*)(p))[0]<<24 )
#else
#define	LOAD_LE32(p)	(((unsigned char*)(p))[0]     | \
 			 ((unsigned char*)(p))[1]<< 8 | \
 			 ((unsigned char*)(p))[2]<<16 | \
 			 ((unsigned char*)(p))[3]<<24 )
#define	LOAD_LE16(p)	(((unsigned char*)(p))[0]     | \
			 ((unsigned char*)(p))[1]<<8)
#define	LOAD_BE32(p)	(*(unsigned int*)(p))
#endif

// defined at asfheader.c:

extern int asf_check_header(demuxer_t *demuxer);
extern int read_asf_header(demuxer_t *demuxer,struct asf_priv* asf);

// based on asf file-format doc by Eugene [http://divx.euro.ru]

static void asf_descrambling(unsigned char **src,unsigned len, struct asf_priv* asf){
  unsigned char *dst=malloc(len);
  unsigned char *s2=*src;
  unsigned i=0,x,y;
  while(len>=asf->scrambling_h*asf->scrambling_w*asf->scrambling_b+i){
//    mp_msg(MSGT_DEMUX,MSGL_DBG4,"descrambling! (w=%d  b=%d)\n",w,asf_scrambling_b);
	//i+=asf_scrambling_h*asf_scrambling_w;
	for(x=0;x<asf->scrambling_w;x++)
	  for(y=0;y<asf->scrambling_h;y++){
	    memcpy(dst+i,s2+(y*asf->scrambling_w+x)*asf->scrambling_b,asf->scrambling_b);
		i+=asf->scrambling_b;
	  }
	s2+=asf->scrambling_h*asf->scrambling_w*asf->scrambling_b;
  }
  //if(i<len) memcpy(dst+i,src+i,len-i);
  free(*src);
  *src = dst;
}

#ifdef USE_LIBAVCODEC_SO
#include <ffmpeg/avcodec.h>
#elif defined(USE_LIBAVCODEC)
#include "libavcodec/avcodec.h"
#else
#define FF_INPUT_BUFFER_PADDING_SIZE 8
#endif

static const uint8_t *find_start_code(const uint8_t * restrict p, const uint8_t *end, uint32_t * restrict state){
  int i;
  if(p>=end)
    return end;

  for(i=0; i<3; i++){
    uint32_t tmp= *state << 8;
    *state= tmp + *(p++);
    if(tmp == 0x100 || p==end)
      return p;
  }

  while(p<end){
    if     (p[-1] > 1      ) p+= 3;
    else if(p[-2]          ) p+= 2;
    else if(p[-3]|(p[-1]-1)) p++;
    else{
      p++;
      break;
    }
  }

  p= ASFMIN(p, end)-4;
  *state=  LOAD_BE32(p);

  return p+4;
}

static int mpeg1_find_frame_end(demuxer_t *demux, const uint8_t *buf, int buf_size)
{
  int i;
  struct asf_priv* asf = demux->priv;

  i=0;
   if(!asf->asf_frame_start_found){
    for(i=0; i<buf_size; i++){
      i= find_start_code(buf+i, buf+buf_size, &asf->asf_frame_state) - buf - 1;
      if(asf->asf_frame_state >= SLICE_MIN_START_CODE && asf->asf_frame_state <= SLICE_MAX_START_CODE){
        i++;
        asf->asf_frame_start_found=1;
        break;
      }
    }
  }

  if(asf->asf_frame_start_found){
    /* EOF considered as end of frame */
      if (buf_size == 0)
          return 0;
            
    for(; i<buf_size; i++){
      i= find_start_code(buf+i, buf+buf_size, &asf->asf_frame_state) - buf - 1;
      if((asf->asf_frame_state&0xFFFFFF00) == 0x100){
        //if NOT in range 257 - 431
        if(asf->asf_frame_state < SLICE_MIN_START_CODE || asf->asf_frame_state > SLICE_MAX_START_CODE){
          asf->asf_frame_start_found=0;
          asf->asf_frame_state=-1;
          return i-3;
        }
      }
    }
  }
  return END_NOT_FOUND;
}

static void demux_asf_append_to_packet(demux_packet_t* dp,unsigned char *data,int len,int offs)
{
  if(dp->len!=offs && offs!=-1) mp_msg(MSGT_DEMUX,MSGL_V,"warning! fragment.len=%d BUT next fragment offset=%d  \n",dp->len,offs);
  dp->buffer=realloc(dp->buffer,dp->len+len+FF_INPUT_BUFFER_PADDING_SIZE);
  memcpy(dp->buffer+dp->len,data,len);
  memset(dp->buffer+dp->len+len, 0, FF_INPUT_BUFFER_PADDING_SIZE);
  mp_dbg(MSGT_DEMUX,MSGL_DBG4,"data appended! %d+%d\n",dp->len,len);
  dp->len+=len;
}

static int demux_asf_read_packet(demuxer_t *demux,unsigned char *data,int len,int id,int seq,unsigned long time,unsigned short dur,int offs,int keyframe){
  struct asf_priv* asf = demux->priv;
  demux_stream_t *ds=NULL;
  int close_seg=0;
  int frame_end_pos=END_NOT_FOUND;
  
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
      demux_packet_t* dp=ds->asf_packet;

      if (ds==demux->video && asf->asf_is_dvr_ms) {
        frame_end_pos=mpeg1_find_frame_end(demux, data, len);

        if (frame_end_pos != END_NOT_FOUND) {
          dp->pos=demux->filepos;
          if (frame_end_pos > 0) {
            demux_asf_append_to_packet(dp,data,frame_end_pos,offs);
            data += frame_end_pos;
            len -= frame_end_pos;
          }
          close_seg = 1;
          if (asf->avg_vid_frame_time > 0.0 ) {
            // correct the pts for the packet
            // because dvr-ms files do not contain accurate
            // pts values but we can deduce them using
            // the average frame time
            if (asf->dvr_last_vid_pts > 0.0)
              dp->pts=asf->dvr_last_vid_pts+asf->avg_vid_frame_time;
            asf->dvr_last_vid_pts = dp->pts;
          }
        } else seq = ds->asf_seq;
      } else close_seg = ds->asf_seq!=seq;

      if(close_seg){
        // closed segment, finalize packet:
		if(ds==demux->audio)
		  if(asf->scrambling_h>1 && asf->scrambling_w>1 && asf->scrambling_b>0)
		    asf_descrambling(&ds->asf_packet->buffer,ds->asf_packet->len,asf);
        ds_add_packet(ds,ds->asf_packet);
        ds->asf_packet=NULL;
      } else {
        // append data to it!
        demux_asf_append_to_packet(dp,data,len,offs);
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
static int demux_asf_fill_buffer(demuxer_t *demux, demux_stream_t *ds){
  struct asf_priv* asf = demux->priv;

  demux->filepos=stream_tell(demux->stream);
  // Brodcast stream have movi_start==movi_end
  // Better test ?
  if((demux->movi_start < demux->movi_end) && (demux->filepos>=demux->movi_end)){
          demux->stream->eof=1;
          return 0;
  }

    stream_read(demux->stream,asf->packet,asf->packetsize);
    if(demux->stream->eof) return 0; // EOF
    
    {
	    unsigned char* p=asf->packet;
            unsigned char* p_end=asf->packet+asf->packetsize;
            unsigned char flags=p[0];
            unsigned char segtype=p[1];
            unsigned padding;
            unsigned plen;
	    unsigned sequence;
            unsigned long time=0;
            unsigned short duration=0;

            int segs=1;
            unsigned char segsizetype=0x80;
            int seg=-1;
            
            if( mp_msg_test(MSGT_DEMUX,MSGL_DBG2) ){
                int i;
                for(i=0;i<16;i++) printf(" %02X",asf->packet[i]);
                printf("\n");
            }
            
	    // skip ECC data if present by testing bit 7 of flags
	    // 1xxxbbbb -> ecc data present, skip bbbb byte(s)
	    // 0xxxxxxx -> payload parsing info starts
	    if (flags & 0x80)
	    {
		p += (flags & 0x0f)+1;
		flags = p[0];
		segtype = p[1];
	    }
	    
            //if(segtype!=0x5d) printf("Warning! packet[4] != 0x5d  \n");

	    p+=2; // skip flags & segtype

            // Read packet size (plen):
	    switch((flags>>5)&3){
	    case 3: plen=LOAD_LE32(p);p+=4;break;	// dword
	    case 2: plen=LOAD_LE16(p);p+=2;break;	// word
	    case 1: plen=p[0];p++;break;		// byte
	    default: plen=0;
		//plen==0 is handled later
		//mp_msg(MSGT_DEMUX,MSGL_V,"Invalid plen type! assuming plen=0\n");
	    }

            // Read sequence:
	    switch((flags>>1)&3){
	    case 3: sequence=LOAD_LE32(p);p+=4;break;	// dword
	    case 2: sequence=LOAD_LE16(p);p+=2;break;	// word
	    case 1: sequence=p[0];p++;break;		// byte
	    default: sequence=0;
	    }

            // Read padding size (padding):
	    switch((flags>>3)&3){
	    case 3: padding=LOAD_LE32(p);p+=4;break;	// dword
	    case 2: padding=LOAD_LE16(p);p+=2;break;	// word
	    case 1: padding=p[0];p++;break;		// byte
	    default: padding=0;
	    }
	    
	    if(((flags>>5)&3)!=0){
              // Explicit (absoulte) packet size
              mp_dbg(MSGT_DEMUX,MSGL_DBG2,"Explicit packet size specified: %d  \n",plen);
              if(plen>asf->packetsize) mp_msg(MSGT_DEMUX,MSGL_V,"Warning! plen>packetsize! (%d>%d)  \n",plen,asf->packetsize);
	    } else {
              // Padding (relative) size
              plen=asf->packetsize-padding;
	    }

	    // Read time & duration:
	    time = LOAD_LE32(p); p+=4;
	    duration = LOAD_LE16(p); p+=2;

	    // Read payload flags:
            if(flags&1){
	      // multiple sub-packets
              segsizetype=p[0]>>6;
              segs=p[0] & 0x3F;
              ++p;
            }
            mp_dbg(MSGT_DEMUX,MSGL_DBG4,"%08"PRIu64":  flag=%02X  segs=%d  seq=%u  plen=%u  pad=%u  time=%ld  dur=%d\n",
              (uint64_t)demux->filepos,flags,segs,sequence,plen,padding,time,duration);

            for(seg=0;seg<segs;seg++){
              //ASF_segmhdr_t* sh;
              unsigned char streamno;
              unsigned int seq;
              unsigned int x;	// offset or timestamp
	      unsigned int rlen;
	      //
              int len;
              unsigned int time2=0;
	      int keyframe=0;

              if(p>=p_end) {
                mp_msg(MSGT_DEMUX,MSGL_V,"Warning! invalid packet 1, aborting parsing...\n");
                break;
              }

              if( mp_msg_test(MSGT_DEMUX,MSGL_DBG2) ){
                int i;
                printf("seg %d:",seg);
                for(i=0;i<16;i++) printf(" %02X",p[i]);
                printf("\n");
              }

              streamno=p[0]&0x7F;
	      if(p[0]&0x80) keyframe=1;
	      p++;

              // Read media object number (seq):
	      switch((segtype>>4)&3){
	      case 3: seq=LOAD_LE32(p);p+=4;break;	// dword
	      case 2: seq=LOAD_LE16(p);p+=2;break;	// word
	      case 1: seq=p[0];p++;break;		// byte
	      default: seq=0;
	      }
	      
              // Read offset or timestamp:
	      switch((segtype>>2)&3){
	      case 3: x=LOAD_LE32(p);p+=4;break;	// dword
	      case 2: x=LOAD_LE16(p);p+=2;break;	// word
	      case 1: x=p[0];p++;break;		// byte
	      default: x=0;
	      }

              // Read replic.data len:
	      switch((segtype)&3){
	      case 3: rlen=LOAD_LE32(p);p+=4;break;	// dword
	      case 2: rlen=LOAD_LE16(p);p+=2;break;	// word
	      case 1: rlen=p[0];p++;break;		// byte
	      default: rlen=0;
	      }
	      
//	      printf("### rlen=%d   \n",rlen);
      
              switch(rlen){
              case 0x01: // 1 = special, means grouping
	        //printf("grouping: %02X  \n",p[0]);
                ++p; // skip PTS delta
                break;
              default:
	        if(rlen>=8){
            	    p+=4;	// skip object size
            	    time2=LOAD_LE32(p); // read PTS
		    p+=rlen-4;
		} else {
            	    mp_msg(MSGT_DEMUX,MSGL_V,"unknown segment type (rlen): 0x%02X  \n",rlen);
		    time2=0; // unknown
		    p+=rlen;
		}
              }

              if(flags&1){
                // multiple segments
		switch(segsizetype){
	          case 3: len=LOAD_LE32(p);p+=4;break;	// dword
	          case 2: len=LOAD_LE16(p);p+=2;break;	// word
	          case 1: len=p[0];p++;break;		// byte
	          default: len=plen-(p-asf->packet); // ???
		}
              } else {
                // single segment
                len=plen-(p-asf->packet);
              }
              if(len<0 || (p+len)>p_end){
                mp_msg(MSGT_DEMUX,MSGL_V,"ASF_parser: warning! segment len=%d\n",len);
              }
              mp_dbg(MSGT_DEMUX,MSGL_DBG4,"  seg #%d: streamno=%d  seq=%d  type=%02X  len=%d\n",seg,streamno,seq,rlen,len);

              switch(rlen){
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
		  ++seq;
		}
                if(len!=0){
                  mp_msg(MSGT_DEMUX,MSGL_V,"ASF_parser: warning! groups total != len\n");
                }
                break;
              default:
                // NO GROUPING:
                //printf("fragment offset: %d  \n",sh->x);
                demux_asf_read_packet(demux,p,len,streamno,seq,time2,duration,x,keyframe);
                p+=len;
                break;
	      }
              
            } // for segs
            return 1; // success
    }
    
    mp_msg(MSGT_DEMUX,MSGL_V,"%08"PRIX64":  UNKNOWN TYPE  %02X %02X %02X %02X %02X...\n",(int64_t)demux->filepos,asf->packet[0],asf->packet[1],asf->packet[2],asf->packet[3],asf->packet[4]);
    return 0;
}

#include "stheader.h"

extern void skip_audio_frame(sh_audio_t *sh_audio);

static void demux_seek_asf(demuxer_t *demuxer,float rel_seek_secs,float audio_delay,int flags){
    struct asf_priv* asf = demuxer->priv;
    demux_stream_t *d_audio=demuxer->audio;
    demux_stream_t *d_video=demuxer->video;
    sh_audio_t *sh_audio=d_audio->sh;
//    sh_video_t *sh_video=d_video->sh;

  //FIXME: OFF_T - didn't test ASF case yet (don't have a large asf...)
  //FIXME: reports good or bad to steve@daviesfam.org please

  //================= seek in ASF ==========================
    float p_rate=asf->packetrate; // packets / sec
    off_t rel_seek_packs=(flags&2)?	 // FIXME: int may be enough?
	(rel_seek_secs*(demuxer->movi_end-demuxer->movi_start)/asf->packetsize):
	(rel_seek_secs*p_rate);
    off_t rel_seek_bytes=rel_seek_packs*asf->packetsize;
    off_t newpos;
    //printf("ASF: packs: %d  duration: %d  \n",(int)fileh.packets,*((int*)&fileh.duration));
//    printf("ASF_seek: %d secs -> %d packs -> %d bytes  \n",
//       rel_seek_secs,rel_seek_packs,rel_seek_bytes);
    newpos=((flags&1)?demuxer->movi_start:demuxer->filepos)+rel_seek_bytes;
    if(newpos<0 || newpos<demuxer->movi_start) newpos=demuxer->movi_start;
//    printf("\r -- asf: newpos=%d -- \n",newpos);
    stream_seek(demuxer->stream,newpos);

    if (asf->asf_is_dvr_ms) asf->dvr_last_vid_pts = 0.0f;

    if (d_video->id >= 0)
    ds_fill_buffer(d_video);
    if(sh_audio){
      ds_fill_buffer(d_audio);
    }
    
    if (d_video->id >= 0)
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

static int demux_asf_control(demuxer_t *demuxer,int cmd, void *arg){
    struct asf_priv* asf = demuxer->priv;
/*  demux_stream_t *d_audio=demuxer->audio;
    demux_stream_t *d_video=demuxer->video;
    sh_audio_t *sh_audio=d_audio->sh;
    sh_video_t *sh_video=d_video->sh;
*/
    switch(cmd) {
	case DEMUXER_CTRL_GET_TIME_LENGTH:
	    *((double *)arg)=(double)(asf->movielength);
	    return DEMUXER_CTRL_OK;

	case DEMUXER_CTRL_GET_PERCENT_POS:
		return DEMUXER_CTRL_DONTKNOW;

	default:
	    return DEMUXER_CTRL_NOTIMPL;
    }
}


static demuxer_t* demux_open_asf(demuxer_t* demuxer)
{
    struct asf_priv* asf = demuxer->priv;
    sh_audio_t *sh_audio=NULL;
    sh_video_t *sh_video=NULL;

    //---- ASF header:
    if(!asf) return NULL;
    if (!read_asf_header(demuxer,asf)) {
        free(asf);
        return NULL;
    }
    stream_reset(demuxer->stream);
    stream_seek(demuxer->stream,demuxer->movi_start);
//    demuxer->idx_pos=0;
//    demuxer->endpos=avi_header.movi_end;
    if(demuxer->video->id != -2) {
        if(!ds_fill_buffer(demuxer->video)){
            mp_msg(MSGT_DEMUXER,MSGL_WARN,"ASF: " MSGTR_MissingVideoStream);
            demuxer->video->sh=NULL;
            //printf("ASF: missing video stream!? contact the author, it may be a bug :(\n");
        } else {
            sh_video=demuxer->video->sh;sh_video->ds=demuxer->video;
            //sh_video->fps=1000.0f; sh_video->frametime=0.001f; // 1ms  - now set when reading asf header
            //sh_video->i_bps=10*asf->packetsize; // FIXME!

            if (asf->asf_is_dvr_ms) {
                sh_video->bih->biWidth = 0;
                sh_video->bih->biHeight = 0;
            }
        }
    }

    if(demuxer->audio->id!=-2){
        mp_msg(MSGT_DEMUXER,MSGL_V,MSGTR_ASFSearchingForAudioStream,demuxer->audio->id);
        if(!ds_fill_buffer(demuxer->audio)){
            mp_msg(MSGT_DEMUXER,MSGL_INFO,"ASF: " MSGTR_MissingAudioStream);
            demuxer->audio->sh=NULL;
        } else {
            sh_audio=demuxer->audio->sh;sh_audio->ds=demuxer->audio;
            sh_audio->format=sh_audio->wf->wFormatTag;
        }
    }
    if(!demuxer->stream->seek)
        demuxer->seekable=0;

    return demuxer;
}


demuxer_desc_t demuxer_desc_asf = {
  "ASF demuxer",
  "asf",
  "ASF",
  "A'rpi",
  "ASF, WMV, WMA",
  DEMUXER_TYPE_ASF,
  1, // safe autodetect
  asf_check_header,
  demux_asf_fill_buffer,
  demux_open_asf,
  NULL, //demux_close_asf,
  demux_seek_asf,
  demux_asf_control
};
