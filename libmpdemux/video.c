// read video frame

#include "config.h"

#include <stdio.h>
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mp_msg.h"
#include "help_mp.h"

#include "stream.h"
#include "demuxer.h"
#include "stheader.h"
#include "parse_es.h"
#include "mpeg_hdr.h"

/* sub_cc (closed captions)*/
#include "../sub_cc.h"

/* biCompression constant */
#define BI_RGB        0L

static mp_mpeg_header_t picture;

int video_read_properties(sh_video_t *sh_video){
demux_stream_t *d_video=sh_video->ds;

// Determine image properties:
switch(d_video->demuxer->file_format){
 case DEMUXER_TYPE_AVI:
 case DEMUXER_TYPE_ASF: {
  // display info: 
  
#if 0
    if(sh_video->bih->biCompression == BI_RGB &&
       (sh_video->video.fccHandler == mmioFOURCC('D', 'I', 'B', ' ') ||
        sh_video->video.fccHandler == mmioFOURCC('R', 'G', 'B', ' ') ||
        sh_video->video.fccHandler == mmioFOURCC('R', 'A', 'W', ' ') ||
        sh_video->video.fccHandler == 0)) {
                sh_video->format = mmioFOURCC(0, 'R', 'G', 'B') | sh_video->bih->biBitCount;
    }
    else 					    
#endif
        sh_video->format=sh_video->bih->biCompression;

    sh_video->disp_w=sh_video->bih->biWidth;
    sh_video->disp_h=abs(sh_video->bih->biHeight);

#if 1
    /* hack to support decoding of mpeg1 chunks in AVI's with libmpeg2 -- 2002 alex */
    if ((sh_video->format == 0x10000001) ||
	(sh_video->format == 0x10000002) ||
	(sh_video->format == mmioFOURCC('m','p','g','1')) ||
	(sh_video->format == mmioFOURCC('M','P','G','1')) ||
	(sh_video->format == mmioFOURCC('m','p','g','2')) ||
	(sh_video->format == mmioFOURCC('M','P','G','2')) ||
	(sh_video->format == mmioFOURCC('m','p','e','g')) ||
	(sh_video->format == mmioFOURCC('M','P','E','G')))
    {
	int saved_pos, saved_type;

	/* demuxer pos saving is required for libavcodec mpeg decoder as it's
	   reading the mpeg header self! */
	
	saved_pos = d_video->buffer_pos;
	saved_type = d_video->demuxer->file_format;

	d_video->demuxer->file_format = DEMUXER_TYPE_MPEG_ES;
	video_read_properties(sh_video);
	d_video->demuxer->file_format = saved_type;
	d_video->buffer_pos = saved_pos;
//	goto mpeg_header_parser;
    }
#endif
  break;
 }
#ifdef STREAMING_LIVE_DOT_COM
 case DEMUXER_TYPE_RTP:
   // If the RTP stream is a MPEG stream, then we use this code to check
   // for MPEG headers:
   if (!demux_is_mpeg_rtp_stream(d_video->demuxer)) break;
   // otherwise fall through to...
#endif
 case DEMUXER_TYPE_PVA:
 case DEMUXER_TYPE_MPEG_ES:
 case DEMUXER_TYPE_MPEG_PS: {
//mpeg_header_parser:
   // Find sequence_header first:
   videobuf_len=0; videobuf_code_len=0;
   mp_msg(MSGT_DECVIDEO,MSGL_V,"Searching for sequence header... ");fflush(stdout);
   while(1){
      int i=sync_video_packet(d_video);
      if(i==0x1B3) break; // found it!
      if(!i || !skip_video_packet(d_video)){
        if(verbose)  mp_msg(MSGT_DECVIDEO,MSGL_V,"NONE :(\n");
        mp_msg(MSGT_DECVIDEO,MSGL_ERR,MSGTR_MpegNoSequHdr);
	return 0;
      }
   }
   mp_msg(MSGT_DECVIDEO,MSGL_V,"OK!\n");
//   sh_video=d_video->sh;sh_video->ds=d_video;
//   mpeg2_init();
   // ========= Read & process sequence header & extension ============
   if(!videobuffer) videobuffer=(char*)memalign(8,VIDEOBUFFER_SIZE);
   if(!videobuffer){ 
     mp_msg(MSGT_DECVIDEO,MSGL_ERR,MSGTR_ShMemAllocFail);
     return 0;
   }
   
   if(!read_video_packet(d_video)){ 
     mp_msg(MSGT_DECVIDEO,MSGL_ERR,MSGTR_CannotReadMpegSequHdr);
     return 0;
   }
   if(mp_header_process_sequence_header (&picture, &videobuffer[4])) {
     mp_msg(MSGT_DECVIDEO,MSGL_ERR,MSGTR_BadMpegSequHdr); 
     return 0;
   }
   if(sync_video_packet(d_video)==0x1B5){ // next packet is seq. ext.
//    videobuf_len=0;
    int pos=videobuf_len;
    if(!read_video_packet(d_video)){ 
      mp_msg(MSGT_DECVIDEO,MSGL_ERR,MSGTR_CannotReadMpegSequHdrEx);
      return 0;
    }
    if(mp_header_process_extension (&picture, &videobuffer[pos+4])) {
      mp_msg(MSGT_DECVIDEO,MSGL_ERR,MSGTR_BadMpegSequHdrEx);
      return 0;
    }
   }
   
//   printf("picture.fps=%d\n",picture.fps);
   
   // fill aspect info:
   switch(picture.aspect_ratio_information){
     case 2:  // PAL/NTSC SVCD/DVD 4:3
     case 4:  // SECAM 4:3? - XXX check with more files! 
     case 8:  // PAL VCD 4:3
     case 12: // NTSC VCD 4:3
       sh_video->aspect=4.0/3.0;
     break;
     case 3:  // PAL/NTSC Widescreen SVCD/DVD 16:9
     case 6:  // (PAL?)/NTSC Widescreen SVCD 16:9
       sh_video->aspect=16.0/9.0;
     break;
     case 9: // Movie Type ??? / 640x480
       sh_video->aspect=0.0;
     break;
     default:
       fprintf(stderr,"Detected unknown aspect_ratio_information in mpeg sequence header.\n"
               "Please report the aspect value (%i) along with the movie type (VGA,PAL,NTSC,"
               "SECAM) and the movie resolution (720x576,352x240,480x480,...) to the MPlayer"
               " developers, so that we can add support for it!\nAssuming 1:1 aspect for now.\n",
               picture.aspect_ratio_information);
     case 1:  // VGA 1:1 - do not prescale
       sh_video->aspect=0.0;
     break;
   }
   // display info:
   sh_video->format=picture.mpeg1?0x10000001:0x10000002; // mpeg video
   sh_video->fps=picture.fps*0.0001f;
   if(!sh_video->fps){
//     if(!force_fps){
//       fprintf(stderr,"FPS not specified (or invalid) in the header! Use the -fps option!\n");
//       return 0;
//     }
     sh_video->frametime=0;
   } else {
     sh_video->frametime=10000.0f/(float)picture.fps;
   }
   sh_video->disp_w=picture.display_picture_width;
   sh_video->disp_h=picture.display_picture_height;
   // bitrate:
   if(picture.bitrate!=0x3FFFF) // unspecified/VBR ?
       sh_video->i_bps=1000*picture.bitrate/16;
   // info:
   mp_dbg(MSGT_DECVIDEO,MSGL_DBG2,"mpeg bitrate: %d (%X)\n",picture.bitrate,picture.bitrate);
   mp_msg(MSGT_DECVIDEO,MSGL_INFO,"VIDEO:  %s  %dx%d  (aspect %d)  %4.2f fps  %5.1f kbps (%4.1f kbyte/s)\n",
    picture.mpeg1?"MPEG1":"MPEG2",
    sh_video->disp_w,sh_video->disp_h,
    picture.aspect_ratio_information,
    sh_video->fps,
    picture.bitrate*0.5f,
    picture.bitrate/16.0f );
  break;
 }
} // switch(file_format)

return 1;
}

static void process_userdata(unsigned char* buf,int len){
    int i;
    /* if the user data starts with "CC", assume it is a CC info packet */
    if(len>2 && buf[0]=='C' && buf[1]=='C'){
//    	mp_msg(MSGT_DECVIDEO,MSGL_DBG2,"video.c: process_userdata() detected Closed Captions!\n");
	if(subcc_enabled) subcc_process_data(buf+2,len-2);
    }
    if(verbose<2) return;
    printf( "user_data: len=%3d  %02X %02X %02X %02X '",
	    len, buf[0], buf[1], buf[2], buf[3]);
    for(i=0;i<len;i++)
//	if(buf[i]>=32 && buf[i]<127) putchar(buf[i]);
	if(buf[i]&0x60) putchar(buf[i]&0x7F);
    printf("'\n");
}

int video_read_frame(sh_video_t* sh_video,float* frame_time_ptr,unsigned char** start,int force_fps){
    demux_stream_t *d_video=sh_video->ds;
    demuxer_t *demuxer=d_video->demuxer;
    float frame_time=1;
    float pts1=d_video->pts;
//    unsigned char* start=NULL;
    int in_size=0;
    
    *start=NULL;

  if(demuxer->file_format==DEMUXER_TYPE_MPEG_ES || demuxer->file_format==DEMUXER_TYPE_MPEG_PS
		  || demuxer->file_format==DEMUXER_TYPE_PVA
#ifdef STREAMING_LIVE_DOT_COM
    || (demuxer->file_format==DEMUXER_TYPE_RTP && demux_is_mpeg_rtp_stream(demuxer))
#endif
  ){
        int in_frame=0;
        //float newfps;
        //videobuf_len=0;
        while(videobuf_len<VIDEOBUFFER_SIZE-MAX_VIDEO_PACKET_SIZE){
          int i=sync_video_packet(d_video);
	  //void* buffer=&videobuffer[videobuf_len+4];
	  int start=videobuf_len+4;
          if(in_frame){
            if(i<0x101 || i>=0x1B0){  // not slice code -> end of frame
#if 0
              // send END OF FRAME code:
              videobuffer[videobuf_len+0]=0;
              videobuffer[videobuf_len+1]=0;
              videobuffer[videobuf_len+2]=1;
              videobuffer[videobuf_len+3]=0xFF;
              videobuf_len+=4;
#endif
              if(!i) return -1; // EOF
              break;
            }
          } else {
            //if(i==0x100) in_frame=1; // picture startcode
            if(i>=0x101 && i<0x1B0) in_frame=1; // picture startcode
            else if(!i) return -1; // EOF
          }
	  //if(grab_frames==2 && (i==0x1B3 || i==0x1B8)) grab_frames=1;
          if(!read_video_packet(d_video)) return -1; // EOF
          //printf("read packet 0x%X, len=%d\n",i,videobuf_len);
	  // process headers:
	  switch(i){
	      case 0x1B3: mp_header_process_sequence_header (&picture, &videobuffer[start]);break;
	      case 0x1B5: mp_header_process_extension (&picture, &videobuffer[start]);break;
	      case 0x1B2: process_userdata (&videobuffer[start], videobuf_len-start);break;
	  }
        }
        
        // if(videobuf_len>max_framesize) max_framesize=videobuf_len; // debug
        //printf("--- SEND %d bytes\n",videobuf_len);
//	if(grab_frames==1){
//	      FILE *f=fopen("grab.mpg","ab");
//	      fwrite(videobuffer,videobuf_len-4,1,f);
//	      fclose(f);
//	}

	*start=videobuffer; in_size=videobuf_len;
	//blit_frame=decode_video(video_out,sh_video,videobuffer,videobuf_len,drop_frame);

#if 1
    // get mpeg fps:
    //newfps=frameratecode2framerate[picture->frame_rate_code]*0.0001f;
    if((int)(sh_video->fps*10000+0.5)!=picture.fps) if(!force_fps){
            mp_msg(MSGT_CPLAYER,MSGL_WARN,"Warning! FPS changed %5.3f -> %5.3f  (%f) [%d]  \n",sh_video->fps,picture.fps*0.0001,sh_video->fps-picture.fps*0.0001,picture.frame_rate_code);
            sh_video->fps=picture.fps*0.0001;
            sh_video->frametime=10000.0f/(float)picture.fps;
    }
#endif

    // fix mpeg2 frametime:
    frame_time=(picture.display_time)*0.01f;
    picture.display_time=100;
    videobuf_len=0;

  } else {
      // frame-based file formats: (AVI,ASF,MOV)
    in_size=ds_get_packet(d_video,start);
    if(in_size<0) return -1; // EOF
//    if(in_size>max_framesize) max_framesize=in_size;
//    blit_frame=decode_video(video_out,sh_video,start,in_size,drop_frame);
  }

//  vdecode_time=video_time_usage-vdecode_time;

//------------------------ frame decoded. --------------------

    // Increase video timers:
    sh_video->num_frames+=frame_time;
    ++sh_video->num_frames_decoded;

    frame_time*=sh_video->frametime;

    // override frame_time for variable/unknown FPS formats:
    if(!force_fps) switch(demuxer->file_format){
      case DEMUXER_TYPE_REAL:
	if(d_video->pts>0 && pts1>0 && d_video->pts>pts1)
	  frame_time=d_video->pts-pts1;
        break;
#ifdef USE_TV
      case DEMUXER_TYPE_TV:
#endif
      case DEMUXER_TYPE_MOV:
      case DEMUXER_TYPE_FILM:
      case DEMUXER_TYPE_VIVO:
      case DEMUXER_TYPE_ASF: {
        float next_pts = ds_get_next_pts(d_video);
        float d= next_pts > 0 ? next_pts - d_video->pts : d_video->pts-pts1;
        if(d>=0){
          if(d>0){
            if((int)sh_video->fps==1000)
              mp_msg(MSGT_CPLAYER,MSGL_V,"\navg. framerate: %d fps             \n",(int)(1.0f/d));
	    sh_video->frametime=d; // 1ms
            sh_video->fps=1.0f/d;
	  }
          frame_time = d;
        } else {
          mp_msg(MSGT_CPLAYER,MSGL_WARN,"\nInvalid frame duration value (%5.3f/%5.3f => %5.3f). Defaulting to %5.3f sec.\n",d_video->pts,next_pts,d,frame_time);
          // frame_time = 1/25.0;
        }
      }
    }
    
    if(demuxer->file_format==DEMUXER_TYPE_MPEG_PS ||
       demuxer->file_format==DEMUXER_TYPE_MPEG_ES) d_video->pts+=frame_time;
    
    if(frame_time_ptr) *frame_time_ptr=frame_time;
    return in_size;

}

