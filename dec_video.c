
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "mp_msg.h"

extern int verbose; // defined in mplayer.c
extern int divx_quality;

extern double video_time_usage;
extern double vout_time_usage;

extern int frameratecode2framerate[16];

#include "linux/timer.h"
#include "linux/shmem.h"

#include "stream.h"
#include "demuxer.h"
#include "parse_es.h"

#include "wine/mmreg.h"
#include "wine/avifmt.h"
#include "wine/vfw.h"

#include "codec-cfg.h"
#include "stheader.h"

//#include <inttypes.h>
//#include "libvo/img_format.h"

#ifdef USE_LIBVO2
#include "libvo2/libvo2.h"
#else
#include "libvo/video_out.h"
#endif

#include "libmpeg2/mpeg2.h"
#include "libmpeg2/mpeg2_internal.h"

extern picture_t *picture;	// exported from libmpeg2/decode.c

extern int init_video_codec(sh_video_t *sh_video,int ex);

#ifdef USE_DIRECTSHOW
#include "loader/DirectShow/DS_VideoDec.h"
#endif

#ifdef USE_LIBAVCODEC
#include "libavcodec/avcodec.h"
    AVCodec *lavc_codec=NULL;
    AVCodecContext lavc_context;
    AVPicture lavc_picture;
#endif

#ifndef NEW_DECORE
#include "opendivx/decore.h"
#else
#include <decore.h>
#endif

//**************************************************************************//
//             The OpenDivX stuff:
//**************************************************************************//

#ifndef NEW_DECORE

static unsigned char *opendivx_src[3];
static int opendivx_stride[3];

// callback, the opendivx decoder calls this for each frame:
void convert_linux(unsigned char *puc_y, int stride_y,
	unsigned char *puc_u, unsigned char *puc_v, int stride_uv,
	unsigned char *bmp, int width_y, int height_y){

//    printf("convert_yuv called  %dx%d  stride: %d,%d\n",width_y,height_y,stride_y,stride_uv);

    opendivx_src[0]=puc_y;
    opendivx_src[1]=puc_u;
    opendivx_src[2]=puc_v;
    
    opendivx_stride[0]=stride_y;
    opendivx_stride[1]=stride_uv;
    opendivx_stride[2]=stride_uv;
}
#endif

int get_video_quality_max(sh_video_t *sh_video){
 switch(sh_video->codec->driver){
#ifdef USE_DIRECTSHOW
  case VFM_DSHOW:
      return 4;
#endif
#ifdef MPEG12_POSTPROC
  case VFM_MPEG:
#endif
  case VFM_DIVX4:
  case VFM_ODIVX:
      return 6;
 }
 return 0;
}

void set_video_quality(sh_video_t *sh_video,int quality){
 switch(sh_video->codec->driver){
#ifdef USE_DIRECTSHOW
  case VFM_DSHOW: {
   if(quality<0 || quality>4) quality=4;
   DS_SetValue_DivX("Quality",quality);
  }
  break;
#endif
#ifdef MPEG12_POSTPROC
  case VFM_MPEG: {
   if(quality<0 || quality>6) quality=6;
   picture->pp_options=(1<<quality)-1;
  }
  break;
#endif
  case VFM_DIVX4:
  case VFM_ODIVX: {
   DEC_SET dec_set;
   if(quality<0 || quality>6) quality=6;
   dec_set.postproc_level=(1<<quality)-1;
   decore(0x123,DEC_OPT_SETPP,&dec_set,NULL);
  }
  break;
 }
}

int set_video_colors(sh_video_t *sh_video,char *item,int value){
#ifdef USE_DIRECTSHOW
    if(!strcmp(sh_video->codec->name,"divxds")){
	DS_SetValue_DivX(item,value);
	return 1;
    }
#endif
    return 0;
}

int uninit_video(sh_video_t *sh_video){
    if(!sh_video->inited) return;
    printf("uninit video: %d  \n",sh_video->codec->driver);
    switch(sh_video->codec->driver){
#ifdef USE_LIBAVCODEC
    case VFM_FFMPEG:
        if (avcodec_close(&lavc_context) < 0)
    	    mp_msg(MSGT_DECVIDEO,MSGL_ERR, "could not close codec\n");
	break;
#endif
#ifndef USE_DIRECTSHOW
    case VFM_DSHOW: // Win32/DirectShow
	DS_VideoDecoder_Close();
	break;
#endif
    case VFM_MPEG:
	mpeg2_free_image_buffers (picture);
	break;
    }
    if(sh_video->our_out_buffer){
	free(sh_video->our_out_buffer);
	sh_video->our_out_buffer=NULL;
    }
    sh_video->inited=0;
}

int init_video(sh_video_t *sh_video){
unsigned int out_fmt=sh_video->codec->outfmt[sh_video->outfmtidx];

sh_video->our_out_buffer=NULL;

switch(sh_video->codec->driver){
#ifdef USE_WIN32DLL
 case VFM_VFW: {
   if(!init_video_codec(sh_video,0)) {
//     GUI_MSG( mplUnknowError )
      return 0;
   }  
   mp_msg(MSGT_DECVIDEO,MSGL_V,"INFO: Win32 video codec init OK!\n");
   break;
 }
 case VFM_VFWEX: {
   if(!init_video_codec(sh_video,1)) {
//     GUI_MSG( mplUnknowError )
      return 0;
   }  
   mp_msg(MSGT_DECVIDEO,MSGL_V,"INFO: Win32Ex video codec init OK!\n");
   break;
 }
 case VFM_DSHOW: { // Win32/DirectShow
#ifndef USE_DIRECTSHOW
   mp_msg(MSGT_DECVIDEO,MSGL_ERR,"MPlayer was compiled WITHOUT directshow support!\n");
   return 0;
//   GUI_MSG( mplCompileWithoutDSSupport )
#else
   int bpp;
   if(DS_VideoDecoder_Open(sh_video->codec->dll,&sh_video->codec->guid, sh_video->bih, 0, &sh_video->our_out_buffer)){
//   if(DS_VideoDecoder_Open(sh_video->codec->dll,&sh_video->codec->guid, sh_video->bih, 0, NULL)){
        mp_msg(MSGT_DECVIDEO,MSGL_ERR,"ERROR: Couldn't open required DirectShow codec: %s\n",sh_video->codec->dll);
        mp_msg(MSGT_DECVIDEO,MSGL_HINT,"Maybe you forget to upgrade your win32 codecs?? It's time to download the new\n");
        mp_msg(MSGT_DECVIDEO,MSGL_HINT,"package from:  ftp://mplayerhq.hu/MPlayer/releases/w32codec.zip  !\n");
//        mp_msg(MSGT_DECVIDEO,MSGL_HINT,"Or you should disable DShow support: make distclean;make -f Makefile.No-DS\n");
      return 0;
//        #ifdef HAVE_GUI
//         if ( !nogui )
//          {
//           strcpy(  mplShMem->items.videodata.codecdll,sh_video->codec->dll );
//           mplSendMessage( mplDSCodecNotFound );
//           usec_sleep( 10000 );
//          }
//        #endif
   }
   
   switch(out_fmt){
   case IMGFMT_YUY2:
   case IMGFMT_UYVY:
     bpp=16;
     DS_VideoDecoder_SetDestFmt(16,out_fmt);break;        // packed YUV
   case IMGFMT_YV12:
   case IMGFMT_I420:
   case IMGFMT_IYUV:
     bpp=12;
     DS_VideoDecoder_SetDestFmt(12,out_fmt);break;        // planar YUV
   default:
     bpp=((out_fmt&255)+7)&(~7);
     DS_VideoDecoder_SetDestFmt(out_fmt&255,0);           // RGB/BGR
   }

   sh_video->our_out_buffer = memalign(64,sh_video->disp_w*sh_video->disp_h*bpp/8); // FIXME!!!

   DS_VideoDecoder_Start();

   DS_SetAttr_DivX("Quality",divx_quality);
//   printf("DivX setting result = %d\n", DS_SetAttr_DivX("Quality",divx_quality) );
//   printf("DivX setting result = %d\n", DS_SetValue_DivX("Brightness",60) );
   
   mp_msg(MSGT_DECVIDEO,MSGL_V,"INFO: Win32/DShow video codec init OK!\n");
   break;
#endif
 }
#else	/* !USE_WIN32DLL */
 case VFM_VFW:
 case VFM_DSHOW:
 case VFM_VFWEX:
   mp_msg(MSGT_DECVIDEO,MSGL_ERR,"Support for win32 codecs disabled, or unavailable on non-x86 platforms!\n");
   return 0;
#endif	/* !USE_WIN32DLL */
 case VFM_ODIVX: {  // OpenDivX
   mp_msg(MSGT_DECVIDEO,MSGL_V,"OpenDivX video codec\n");
   { DEC_PARAM dec_param;
     DEC_SET dec_set;
        memset(&dec_param,0,sizeof(dec_param));
#ifdef NEW_DECORE
        dec_param.output_format=DEC_USER;
#else
        dec_param.color_depth = 32;
#endif
	dec_param.x_dim = sh_video->bih->biWidth;
	dec_param.y_dim = sh_video->bih->biHeight;
	decore(0x123, DEC_OPT_INIT, &dec_param, NULL);
	dec_set.postproc_level = divx_quality;
	decore(0x123, DEC_OPT_SETPP, &dec_set, NULL);
   }
   mp_msg(MSGT_DECVIDEO,MSGL_V,"INFO: OpenDivX video codec init OK!\n");
   break;
 }
 case VFM_DIVX4: {  // DivX4Linux
#ifndef NEW_DECORE
   mp_msg(MSGT_DECVIDEO,MSGL_ERR,"MPlayer was compiled WITHOUT DivX4Linux (libdivxdecore.so) support!\n");
   return 0;
#else
   mp_msg(MSGT_DECVIDEO,MSGL_V,"DivX4Linux video codec\n");
   { DEC_PARAM dec_param;
     DEC_SET dec_set;
     int bits=16;
        memset(&dec_param,0,sizeof(dec_param));
	switch(out_fmt){
	case IMGFMT_YV12: dec_param.output_format=DEC_YV12;bits=12;break;
	case IMGFMT_YUY2: dec_param.output_format=DEC_YUY2;break;
	case IMGFMT_UYVY: dec_param.output_format=DEC_UYVY;break;
	case IMGFMT_I420: dec_param.output_format=DEC_420;bits=12;break;
	case IMGFMT_BGR15: dec_param.output_format=DEC_RGB555_INV;break;
	case IMGFMT_BGR16: dec_param.output_format=DEC_RGB565_INV;break;
	case IMGFMT_BGR24: dec_param.output_format=DEC_RGB24_INV;bits=24;break;
	case IMGFMT_BGR32: dec_param.output_format=DEC_RGB32_INV;bits=32;break;
	default:
	  mp_msg(MSGT_DECVIDEO,MSGL_ERR,"Unsupported out_fmt: 0x%X\n",out_fmt);
	  return 0;
	}
	dec_param.x_dim = sh_video->bih->biWidth;
	dec_param.y_dim = sh_video->bih->biHeight;
	decore(0x123, DEC_OPT_INIT, &dec_param, NULL);
	dec_set.postproc_level = divx_quality;
	decore(0x123, DEC_OPT_SETPP, &dec_set, NULL);
	sh_video->our_out_buffer = memalign(64,((bits*dec_param.x_dim+7)/8)*dec_param.y_dim);
//	sh_video->our_out_buffer = shmem_alloc(dec_param.x_dim*dec_param.y_dim*5);
   }
   mp_msg(MSGT_DECVIDEO,MSGL_V,"INFO: OpenDivX video codec init OK!\n");
   break;
#endif
 }
 case VFM_FFMPEG: {  // FFmpeg's libavcodec
#ifndef USE_LIBAVCODEC
   mp_msg(MSGT_DECVIDEO,MSGL_ERR,"MPlayer was compiled WITHOUT libavcodec support!\n");
   return 0;
#else
   static int avcodec_inited=0;
   mp_msg(MSGT_DECVIDEO,MSGL_V,"FFmpeg's libavcodec video codec\n");
    if(!avcodec_inited){
      avcodec_init();
      avcodec_register_all();
      avcodec_inited=1;
    }
    lavc_codec = (AVCodec *)avcodec_find_decoder_by_name(sh_video->codec->dll);
    if(!lavc_codec){
	mp_msg(MSGT_DECVIDEO,MSGL_ERR,"Can't find codec '%s' in libavcodec...\n",sh_video->codec->dll);
	return 0;
    }
    memset(&lavc_context, 0, sizeof(lavc_context));
//    sh_video->disp_h/=2; // !!
    lavc_context.width=sh_video->disp_w;
    lavc_context.height=sh_video->disp_h;
    mp_dbg(MSGT_DECVIDEO,MSGL_DBG2,"libavcodec.size: %d x %d\n",lavc_context.width,lavc_context.height);
    /* open it */
    if (avcodec_open(&lavc_context, lavc_codec) < 0) {
        mp_msg(MSGT_DECVIDEO,MSGL_ERR, "could not open codec\n");
        return 0;
    }
   
   mp_msg(MSGT_DECVIDEO,MSGL_V,"INFO: libavcodec init OK!\n");
   break;
#endif
 }

 case VFM_MPEG: {
   // init libmpeg2:
#ifdef MPEG12_POSTPROC
   picture->pp_options=divx_quality;
#else
   if(divx_quality){
       mp_msg(MSGT_DECVIDEO,MSGL_HINT,"WARNING! You requested image postprocessing for an MPEG 1/2 video,\n");
       mp_msg(MSGT_DECVIDEO,MSGL_HINT,"         but compiled MPlayer without MPEG 1/2 postprocessing support!\n");
       mp_msg(MSGT_DECVIDEO,MSGL_HINT,"         #define MPEG12_POSTPROC in config.h, and recompile libmpeg2!\n");
   }
#endif
   mpeg2_allocate_image_buffers (picture);
   break;
 }
 case VFM_RAW: {
   break;
 }
}
  sh_video->inited=1;
  return 1;
}

#ifdef USE_LIBVO2
int decode_video(vo2_handle_t *video_out,sh_video_t *sh_video,unsigned char *start,int in_size,int drop_frame){
#else
int decode_video(vo_functions_t *video_out,sh_video_t *sh_video,unsigned char *start,int in_size,int drop_frame){
#endif
unsigned int out_fmt=sh_video->codec->outfmt[sh_video->outfmtidx];
int planar=(out_fmt==IMGFMT_YV12||out_fmt==IMGFMT_IYUV||out_fmt==IMGFMT_I420);
int blit_frame=0;

uint8_t* planes_[3];
uint8_t** planes=planes_;
int stride_[3];
int* stride=stride_;

unsigned int t=GetTimer();
unsigned int t2;

  //--------------------  Decode a frame: -----------------------
switch(sh_video->codec->driver){
  case VFM_ODIVX: {
    // OpenDivX
    DEC_FRAME dec_frame;
#ifdef NEW_DECORE
    DEC_PICTURE dec_pic;
#endif
    // let's decode
        dec_frame.length = in_size;
	dec_frame.bitstream = start;
	dec_frame.render_flag = drop_frame?0:1;

#ifdef NEW_DECORE
        dec_frame.bmp=&dec_pic;
        dec_pic.y=dec_pic.u=dec_pic.v=NULL;
	decore(0x123, (sh_video->format==mmioFOURCC('D','I','V','3'))?DEC_OPT_FRAME_311:DEC_OPT_FRAME, &dec_frame, NULL);
#else
        opendivx_src[0]=NULL;
	decore(0x123, 0, &dec_frame, NULL);
#endif
  
      if(!drop_frame)

    // let's display
#ifdef NEW_DECORE
      if(dec_pic.y){
        planes[0]=dec_pic.y;
        planes[1]=dec_pic.u;
        planes[2]=dec_pic.v;
        stride[0]=dec_pic.stride_y;
        stride[1]=stride[2]=dec_pic.stride_uv;
        blit_frame=2;
      }
#else
      if(opendivx_src[0]){
        planes=opendivx_src; stride=opendivx_stride;
        blit_frame=2;
      }
#endif

    break;
  }
#ifdef NEW_DECORE
  case VFM_DIVX4: {
    // DivX4Linux
    DEC_FRAME dec_frame;
    // let's decode
        dec_frame.length = in_size;
	dec_frame.bitstream = start;
	dec_frame.render_flag = drop_frame?0:1;
        dec_frame.bmp=sh_video->our_out_buffer;
        dec_frame.stride=sh_video->disp_w;
//	printf("Decoding DivX4 frame\n");
	decore(0x123, (sh_video->format==mmioFOURCC('D','I','V','3'))?DEC_OPT_FRAME_311:DEC_OPT_FRAME, &dec_frame, NULL);
    if(!drop_frame) blit_frame=3;
    break;
  }
#endif
#ifdef USE_DIRECTSHOW
  case VFM_DSHOW: {        // W32/DirectShow
    if(drop_frame<2) DS_VideoDecoder_DecodeFrame(start, in_size, 0, !drop_frame);
    if(!drop_frame && sh_video->our_out_buffer) blit_frame=3;
    break;
  }
#endif
#ifdef USE_LIBAVCODEC
  case VFM_FFMPEG: {        // libavcodec
    int got_picture=0;
    mp_dbg(MSGT_DECVIDEO,MSGL_DBG2,"Calling ffmpeg...\n");
    if(drop_frame<2 && in_size>0){
        int ret = avcodec_decode_video(&lavc_context, &lavc_picture,
	     &got_picture, start, in_size);
if(verbose>1){
     unsigned char *x="???";
     switch(lavc_context.pix_fmt){
     case PIX_FMT_YUV420P: x="YUV420P";break;
     case PIX_FMT_YUV422: x="YUV422";break;
     case PIX_FMT_RGB24: x="RGB24";break;
     case PIX_FMT_BGR24: x="BGR24";break;
#ifdef PIX_FMT_YUV422P
     case PIX_FMT_YUV422P: x="YUV422P";break;
     case PIX_FMT_YUV444P: x="YUV444P";break;
#endif
     }
     mp_dbg(MSGT_DECVIDEO,MSGL_DBG2,"DONE -> got_picture=%d  format=0x%X (%s) \n",got_picture,
	lavc_context.pix_fmt,x);
}
	if(ret<0) mp_msg(MSGT_DECVIDEO,MSGL_WARN, "Error while decoding frame!\n");
	if(!drop_frame && got_picture){
//	if(!drop_frame){
	  if(planar){
	    planes=lavc_picture.data;
	    stride=lavc_picture.linesize;
	    //stride[1]=stride[2]=0;
	    //stride[0]/=2;
            blit_frame=2;
	  } else {
	    int y;
	    // temporary hack - FIXME
	    if(!sh_video->our_out_buffer)
		sh_video->our_out_buffer = memalign(64,sh_video->disp_w*sh_video->disp_h*2);
	    for(y=0;y<sh_video->disp_h;y++){
	      unsigned char *s0=lavc_picture.data[0]+lavc_picture.linesize[0]*y;
	      unsigned char *s1=lavc_picture.data[1]+lavc_picture.linesize[1]*y;
	      unsigned char *s2=lavc_picture.data[2]+lavc_picture.linesize[2]*y;
	      unsigned char *d=sh_video->our_out_buffer+y*2*sh_video->disp_w;
	      int x;
	      for(x=0;x<sh_video->disp_w/2;x++){
	          d[4*x+0]=s0[2*x+0];
	          d[4*x+1]=s1[x];
	          d[4*x+2]=s0[2*x+1];
	          d[4*x+3]=s2[x];
	      }
	    }
            blit_frame=3;
	  }

	}
    }
    break;
  }
#endif
#ifdef USE_WIN32DLL
  case VFM_VFWEX:
  case VFM_VFW:
  {
    HRESULT ret;
    
    if(!in_size) break;
    
      sh_video->bih->biSizeImage = in_size;

//      sh_video->bih->biWidth = 1280;
//      sh_video->o_bih.biWidth = 1280;
	    //      ret = ICDecompress(avi_header.hic, ICDECOMPRESS_NOTKEYFRAME|(ICDECOMPRESS_HURRYUP|ICDECOMPRESS_PREROL), 

if(sh_video->codec->driver==VFM_VFWEX)
      ret = ICDecompressEx(sh_video->hic, 
	  ( (sh_video->ds->flags&1) ? 0 : ICDECOMPRESS_NOTKEYFRAME ) |
	  ( (drop_frame==2 && !(sh_video->ds->flags&1))?(ICDECOMPRESS_HURRYUP|ICDECOMPRESS_PREROL):0 ) , 
                         sh_video->bih,   start,
                        &sh_video->o_bih,
                        drop_frame ? 0 : sh_video->our_out_buffer);
else
      ret = ICDecompress(sh_video->hic, 
	  ( (sh_video->ds->flags&1) ? 0 : ICDECOMPRESS_NOTKEYFRAME ) |
	  ( (drop_frame==2 && !(sh_video->ds->flags&1))?(ICDECOMPRESS_HURRYUP|ICDECOMPRESS_PREROL):0 ) , 
                         sh_video->bih,   start,
                        &sh_video->o_bih,
                        drop_frame ? 0 : sh_video->our_out_buffer);

      if(ret){ mp_msg(MSGT_DECVIDEO,MSGL_WARN,"Error decompressing frame, err=%d\n",(int)ret);break; }

    if(!drop_frame) blit_frame=3;
    break;
  }
#endif
  case VFM_MPEG:
    mpeg2_decode_data(video_out, start, start+in_size,drop_frame);
    if(!drop_frame) blit_frame=1;
    break;
  case VFM_RAW:
    planes[0]=start;
    blit_frame=2;
    break;
} // switch
//------------------------ frame decoded. --------------------

#ifdef HAVE_MMX
	// some codecs is broken, and doesn't restore MMX state :(
	// it happens usually with broken/damaged files.
	__asm __volatile ("emms;":::"memory");
#endif

t2=GetTimer();t=t2-t;video_time_usage+=t*0.000001f;

switch(blit_frame){
case 3:
      if(planar){
        stride[0]=sh_video->disp_w;
        stride[1]=stride[2]=sh_video->disp_w/2;
        planes[0]=sh_video->our_out_buffer;
        planes[2]=planes[0]+sh_video->disp_w*sh_video->disp_h;
        planes[1]=planes[2]+sh_video->disp_w*sh_video->disp_h/4;
      } else
        planes[0]=sh_video->our_out_buffer;
case 2:
#ifdef USE_LIBVO2
    if(planar)
        vo2_draw_slice(video_out,planes,stride,sh_video->disp_w,sh_video->disp_h,0,0);
    else
        vo2_draw_frame(video_out,planes[0],sh_video->disp_w,sh_video->disp_w,sh_video->disp_h);
#else
    if(planar)
        video_out->draw_slice(planes,stride,sh_video->disp_w,sh_video->disp_h,0,0);
    else
        video_out->draw_frame(planes);
#endif
    t2=GetTimer()-t2;vout_time_usage+=t2*0.000001f;
    blit_frame=1;
    break;
}

  return blit_frame;
}


int video_read_properties(sh_video_t *sh_video){
demux_stream_t *d_video=sh_video->ds;

// Determine image properties:
switch(d_video->demuxer->file_format){
 case DEMUXER_TYPE_AVI:
 case DEMUXER_TYPE_ASF: {
  // display info:
    sh_video->format=sh_video->bih->biCompression;
    sh_video->disp_w=sh_video->bih->biWidth;
    sh_video->disp_h=abs(sh_video->bih->biHeight);
  break;
 }
 case DEMUXER_TYPE_MPEG_ES:
 case DEMUXER_TYPE_MPEG_PS: {
   // Find sequence_header first:
   videobuf_len=0; videobuf_code_len=0;
   mp_msg(MSGT_DECVIDEO,MSGL_V,"Searching for sequence header... ");fflush(stdout);
   while(1){
      int i=sync_video_packet(d_video);
      if(i==0x1B3) break; // found it!
      if(!i || !skip_video_packet(d_video)){
        if(verbose)  mp_msg(MSGT_DECVIDEO,MSGL_V,"NONE :(\n");
        mp_msg(MSGT_DECVIDEO,MSGL_ERR,"MPEG: FATAL: EOF while searching for sequence header\n");
	return 0;
//        GUI_MSG( mplMPEGErrorSeqHeaderSearch )
      }
   }
   mp_msg(MSGT_DECVIDEO,MSGL_V,"OK!\n");
//   sh_video=d_video->sh;sh_video->ds=d_video;
   mpeg2_init();
   // ========= Read & process sequence header & extension ============
   if(!videobuffer) videobuffer=memalign(8,VIDEOBUFFER_SIZE);
   if(!videobuffer){ 
     mp_msg(MSGT_DECVIDEO,MSGL_ERR,"Cannot allocate shared memory\n");
     return 0;
//     GUI_MSG( mplErrorShMemAlloc )
   }
   
   if(!read_video_packet(d_video)){ 
     mp_msg(MSGT_DECVIDEO,MSGL_ERR,"FATAL: Cannot read sequence header!\n");
     return 0;
//     GUI_MSG( mplMPEGErrorCannotReadSeqHeader )
   }
   if(header_process_sequence_header (picture, &videobuffer[4])) {
     mp_msg(MSGT_DECVIDEO,MSGL_ERR,"bad sequence header!\n"); 
     return 0;
//     GUI_MSG( mplMPEGErrorBadSeqHeader )
   }
   if(sync_video_packet(d_video)==0x1B5){ // next packet is seq. ext.
//    videobuf_len=0;
    int pos=videobuf_len;
    if(!read_video_packet(d_video)){ 
      mp_msg(MSGT_DECVIDEO,MSGL_ERR,"FATAL: Cannot read sequence header extension!\n");
      return 0;
//      GUI_MSG( mplMPEGErrorCannotReadSeqHeaderExt )
    }
    if(header_process_extension (picture, &videobuffer[pos+4])) {
      mp_msg(MSGT_DECVIDEO,MSGL_ERR,"bad sequence header extension!\n");  
      return 0;
//      GUI_MSG( mplMPEGErrorBadSeqHeaderExt )
    }
   }
   // display info:
   sh_video->format=picture->mpeg1?0x10000001:0x10000002; // mpeg video
   sh_video->fps=frameratecode2framerate[picture->frame_rate_code]*0.0001f;
   if(!sh_video->fps){
//     if(!force_fps){
//       fprintf(stderr,"FPS not specified (or invalid) in the header! Use the -fps option!\n");
//       return 0;
//     }
     sh_video->frametime=0;
   } else {
     sh_video->frametime=10000.0f/(float)frameratecode2framerate[picture->frame_rate_code];
   }
   sh_video->disp_w=picture->display_picture_width;
   sh_video->disp_h=picture->display_picture_height;
   // bitrate:
   if(picture->bitrate!=0x3FFFF) // unspecified/VBR ?
       sh_video->i_bps=1000*picture->bitrate/16;
   // info:
   mp_dbg(MSGT_DECVIDEO,MSGL_DBG2,"mpeg bitrate: %d (%X)\n",picture->bitrate,picture->bitrate);
   mp_msg(MSGT_DECVIDEO,MSGL_INFO,"VIDEO:  %s  %dx%d  (aspect %d)  %4.2f fps  %5.1f kbps (%4.1f kbyte/s)\n",
    picture->mpeg1?"MPEG1":"MPEG2",
    sh_video->disp_w,sh_video->disp_h,
    picture->aspect_ratio_information,
    sh_video->fps,
    picture->bitrate*0.5f,
    picture->bitrate/16.0f );
  break;
 }
} // switch(file_format)

return 1;
}





