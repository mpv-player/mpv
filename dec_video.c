
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"

#include "linux/timer.h"
#include "linux/shmem.h"

extern int verbose; // defined in mplayer.c

#include "stream.h"
#include "demuxer.h"
#include "parse_es.h"

#include "codec-cfg.h"
#include "stheader.h"

#ifdef USE_LIBVO2
#include "libvo2/libvo2.h"
#else
#include "libvo/video_out.h"
#endif

#include "dec_video.h"

// ===================================================================

extern double video_time_usage;
extern double vout_time_usage;

extern int frameratecode2framerate[16];

#include "dll_init.h"

//#include <inttypes.h>
//#include "libvo/img_format.h"

#include "libmpeg2/mpeg2.h"
#include "libmpeg2/mpeg2_internal.h"

#include "postproc/postprocess.h"

extern picture_t *picture;	// exported from libmpeg2/decode.c

int divx_quality=0;

#ifdef USE_DIRECTSHOW
#include "loader/DirectShow/DS_VideoDec.h"
#endif

#ifdef USE_LIBAVCODEC
#ifdef USE_LIBAVCODEC_SO
#include <libffmpeg/avcodec.h>
#else
#include "libavcodec/avcodec.h"
#endif
    static AVCodec *lavc_codec=NULL;
    static AVCodecContext lavc_context;
    static AVPicture lavc_picture;
    int avcodec_inited=0;
#endif
#ifdef FF_POSTPROCESS
    unsigned int lavc_pp=0;
#endif

#ifndef NEW_DECORE
#include "opendivx/decore.h"
#else
#include <decore.h>
#endif

#ifdef USE_XANIM
#include "xacodec.h"
#endif

#include "mmx_defs.h"

void AVI_Decode_RLE8(char *image,char *delta,int tdsize,
    unsigned int *map,int imagex,int imagey,unsigned char x11_bytes_pixel);

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
#ifdef USE_WIN32DLL
  case VFM_VFW:
  case VFM_VFWEX:
      return 9; // for Divx.dll (divx4)
#endif
#ifdef USE_DIRECTSHOW
  case VFM_DSHOW:
      return 4;
#endif
#ifdef MPEG12_POSTPROC
  case VFM_MPEG:
      return GET_PP_QUALITY_MAX;
#endif
#ifdef FF_POSTPROCESS
  case VFM_FFMPEG:
      return GET_PP_QUALITY_MAX;
#endif
  case VFM_DIVX4:
  case VFM_ODIVX:
#ifdef NEW_DECORE
      return 9; // for divx4linux
#else
      return GET_PP_QUALITY_MAX;  // for opendivx
#endif
 }
 return 0;
}

void set_video_quality(sh_video_t *sh_video,int quality){
 switch(sh_video->codec->driver){
#ifdef USE_WIN32DLL
  case VFM_VFW:
  case VFM_VFWEX:
   vfw_set_postproc(sh_video,10*quality);
  break;
#endif
#ifdef USE_DIRECTSHOW
  case VFM_DSHOW: {
   if(quality<0 || quality>4) quality=4;
   DS_SetValue_DivX("Quality",quality);
  }
  break;
#endif
#ifdef MPEG12_POSTPROC
  case VFM_MPEG: {
   if(quality<0 || quality>GET_PP_QUALITY_MAX) quality=GET_PP_QUALITY_MAX;
   picture->pp_options=getPpModeForQuality(quality);
  }
  break;
#endif
#ifdef FF_POSTPROCESS
  case VFM_FFMPEG:
    if(quality<0 || quality>GET_PP_QUALITY_MAX) quality=GET_PP_QUALITY_MAX;
    lavc_pp=getPpModeForQuality(quality);
    break;
#endif
  case VFM_DIVX4:
  case VFM_ODIVX: {
   DEC_SET dec_set;
#ifdef NEW_DECORE
   if(quality<0 || quality>9) quality=9;
   dec_set.postproc_level=quality*10;
#else
   if(quality<0 || quality>GET_PP_QUALITY_MAX) quality=GET_PP_QUALITY_MAX;
   dec_set.postproc_level=getPpModeForQuality(quality);
#endif
   decore(0x123,DEC_OPT_SETPP,&dec_set,NULL);
  }
  break;
 }
}

int set_video_colors(sh_video_t *sh_video,char *item,int value){
#ifdef USE_DIRECTSHOW
    if(sh_video->codec->driver==VFM_DSHOW){
	DS_SetValue_DivX(item,value);
	return 1;
    }
#endif
#ifdef NEW_DECORE
#ifdef DECORE_VERSION
#if DECORE_VERSION >= 20011010
    if(sh_video->codec->driver==VFM_DIVX4){
         int option;
         if(!strcmp(item,"Brightness")) option=DEC_GAMMA_BRIGHTNESS;
         else if(!strcmp(item, "Contrast")) option=DEC_GAMMA_CONTRAST;
         else if(!strcmp(item,"Saturation")) option=DEC_GAMMA_SATURATION;
         else return 0;
         value = (value * 256) / 100 - 128;
         decore(0x123, DEC_OPT_GAMMA, (void *)option, (void *) value);
         return 1;
    }
#endif
#endif
#endif
    return 0;
}

void uninit_video(sh_video_t *sh_video){
    if(!sh_video->inited) return;
    mp_msg(MSGT_DECVIDEO,MSGL_V,"uninit video: %d  \n",sh_video->codec->driver);
    switch(sh_video->codec->driver){
#ifdef USE_LIBAVCODEC
    case VFM_FFMPEG:
        if (avcodec_close(&lavc_context) < 0)
    	    mp_msg(MSGT_DECVIDEO,MSGL_ERR, MSGTR_CantCloseCodec);
	break;
#endif
#ifdef USE_DIRECTSHOW
    case VFM_DSHOW: // Win32/DirectShow
	DS_VideoDecoder_Close();
	break;
#endif
    case VFM_MPEG:
	mpeg2_free_image_buffers (picture);
	break;
#ifdef USE_XANIM
    case VFM_XANIM:
	xacodec_exit();
	break;
#endif
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
 case VFM_XANIM: {
#ifdef USE_XANIM
   int ret=xacodec_init_video(sh_video,out_fmt);
   if(!ret) return 0;
#else
//   mp_msg(MSGT_DECVIDEO, MSGL_ERR, MSGTR_NoXAnimSupport);
   mp_msg(MSGT_DECVIDEO, MSGL_ERR, "MPlayer was compiled WIHTOUT XAnim support!\n");
   return 0;
#endif
   break;
 }
#ifdef USE_WIN32DLL
 case VFM_VFW: {
   if(!init_vfw_video_codec(sh_video,0)) {
      return 0;
   }  
   mp_msg(MSGT_DECVIDEO,MSGL_V,"INFO: Win32 video codec init OK!\n");
   break;
 }
 case VFM_VFWEX: {
   if(!init_vfw_video_codec(sh_video,1)) {
      return 0;
   }  
   mp_msg(MSGT_DECVIDEO,MSGL_V,"INFO: Win32Ex video codec init OK!\n");
   break;
 }
 case VFM_DSHOW: { // Win32/DirectShow
#ifndef USE_DIRECTSHOW
   mp_msg(MSGT_DECVIDEO,MSGL_ERR,MSGTR_NoDShowSupport);
   return 0;
#else
   int bpp;
   if(DS_VideoDecoder_Open(sh_video->codec->dll,&sh_video->codec->guid, sh_video->bih, 0, &sh_video->our_out_buffer)){
//   if(DS_VideoDecoder_Open(sh_video->codec->dll,&sh_video->codec->guid, sh_video->bih, 0, NULL)){
        mp_msg(MSGT_DECVIDEO,MSGL_ERR,MSGTR_MissingDLLcodec,sh_video->codec->dll);
        mp_msg(MSGT_DECVIDEO,MSGL_HINT,"Maybe you forget to upgrade your win32 codecs?? It's time to download the new\n");
        mp_msg(MSGT_DECVIDEO,MSGL_HINT,"package from:  ftp://mplayerhq.hu/MPlayer/releases/w32codec.zip  !\n");
//        mp_msg(MSGT_DECVIDEO,MSGL_HINT,"Or you should disable DShow support: make distclean;make -f Makefile.No-DS\n");
      return 0;
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

   sh_video->our_out_buffer = (char*)memalign(64,sh_video->disp_w*sh_video->disp_h*bpp/8); // FIXME!!!

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
   mp_msg(MSGT_DECVIDEO,MSGL_ERR,MSGTR_NoWfvSupport);
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
   mp_msg(MSGT_DECVIDEO,MSGL_ERR,MSGTR_NoDivx4Support);
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
	sh_video->our_out_buffer = (char*)memalign(64,((bits*dec_param.x_dim+7)/8)*dec_param.y_dim);
//	sh_video->our_out_buffer = shmem_alloc(dec_param.x_dim*dec_param.y_dim*5);
   }
   mp_msg(MSGT_DECVIDEO,MSGL_V,"INFO: OpenDivX video codec init OK!\n");
   break;
#endif
 }
 case VFM_FFMPEG: {  // FFmpeg's libavcodec
#ifndef USE_LIBAVCODEC
   mp_msg(MSGT_DECVIDEO,MSGL_ERR,MSGTR_NoLAVCsupport);
   return 0;
#else
   mp_msg(MSGT_DECVIDEO,MSGL_V,"FFmpeg's libavcodec video codec\n");
    if(!avcodec_inited){
      avcodec_init();
      avcodec_register_all();
      avcodec_inited=1;
    }
    lavc_codec = (AVCodec *)avcodec_find_decoder_by_name(sh_video->codec->dll);
    if(!lavc_codec){
	mp_msg(MSGT_DECVIDEO,MSGL_ERR,MSGTR_MissingLAVCcodec,sh_video->codec->dll);
	return 0;
    }
    memset(&lavc_context, 0, sizeof(lavc_context));
//    sh_video->disp_h/=2; // !!
    lavc_context.width=sh_video->disp_w;
    lavc_context.height=sh_video->disp_h;
    mp_dbg(MSGT_DECVIDEO,MSGL_DBG2,"libavcodec.size: %d x %d\n",lavc_context.width,lavc_context.height);
    /* open it */
    if (avcodec_open(&lavc_context, lavc_codec) < 0) {
        mp_msg(MSGT_DECVIDEO,MSGL_ERR, MSGTR_CantOpenCodec);
        return 0;
    }
#ifdef FF_POSTPROCESS
   lavc_pp=divx_quality;
#endif
   mp_msg(MSGT_DECVIDEO,MSGL_V,"INFO: libavcodec init OK!\n");
   break;
#endif
 }

 case VFM_MPEG: {
   // init libmpeg2:
   mpeg2_init();
#ifdef MPEG12_POSTPROC
   picture->pp_options=divx_quality;
#else
   if(divx_quality) mp_msg(MSGT_DECVIDEO,MSGL_HINT,MSGTR_MpegPPhint);
#endif
   // send seq header to the decoder:
   mpeg2_decode_data(NULL,videobuffer,videobuffer+videobuf_len,0);
   mpeg2_allocate_image_buffers (picture);
   break;
 }
 case VFM_RAW: {
   break;
 }
 case VFM_RLE: {
   int bpp=((out_fmt&255)+7)/8; // RGB only
   sh_video->our_out_buffer = (char*)memalign(64,sh_video->disp_w*sh_video->disp_h*bpp); // FIXME!!!
   if(bpp==2){  // 15 or 16 bpp ==> palette conversion!
     unsigned int* pal=(unsigned int*)(((char*)sh_video->bih)+40);
     //int cols=(sh_video->bih->biSize-40)/4;
     int cols=1<<(sh_video->bih->biBitCount);
     int i;
     if(cols>256) cols=256;
     mp_msg(MSGT_DECVIDEO,MSGL_V,"RLE: converting palette for %d colors.\n",cols);
     for(i=0;i<cols;i++){
        unsigned int c=pal[i];
	unsigned int b=c&255;
	unsigned int g=(c>>8)&255;
	unsigned int r=(c>>16)&255;
	if((out_fmt&255)==15)
	  pal[i]=((r>>3)<<10)|((g>>3)<<5)|((b>>3));
	else
	  pal[i]=((r>>3)<<11)|((g>>2)<<5)|((b>>3));
     }
   }
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
#ifdef USE_XANIM
  case VFM_XANIM: {
    xacodec_image_t* image=xacodec_decode_frame(start,in_size,drop_frame?1:0);
    if(image){
	blit_frame=2;
	planes=image->planes;
	stride=image->stride;
    }
    break;
  }
#endif
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
#ifdef FF_POSTPROCESS
	    if(lavc_pp){
		// postprocess
		int w=(sh_video->disp_w+15)&(~15);
		int h=(sh_video->disp_h+15)&(~15);
		int xoff=0; //(w-sh_video->disp_w)/2;
		int yoff=0; //(h-sh_video->disp_h)/2;
		if(!sh_video->our_out_buffer){
		    sh_video->our_out_buffer = (char*)memalign(64,w*h*3/2);
		    memset(sh_video->our_out_buffer,0,w*h*3/2);
		}
    		stride[0]=w;
    		stride[1]=stride[2]=w/2;
    		planes[0]=sh_video->our_out_buffer+stride[0]*yoff+xoff;
    		planes[2]=sh_video->our_out_buffer+w*h+stride[2]*(yoff>>1)+(xoff>>1);
    		planes[1]=planes[2]+w*h/4;
		postprocess(lavc_picture.data,lavc_picture.linesize[0],
			    planes,stride[0],
			    sh_video->disp_w,sh_video->disp_h,
			    &quant_store[0][0],MBC+1,lavc_pp);
	    } else
#endif
	    {
		planes=lavc_picture.data;
		stride=lavc_picture.linesize;
		//stride[1]=stride[2]=0;
		//stride[0]/=2;
	    }
    	    blit_frame=2;
	  } else {
	    int y;
	    // temporary hack - FIXME
	    if(!sh_video->our_out_buffer)
		sh_video->our_out_buffer = (char*)memalign(64,sh_video->disp_w*sh_video->disp_h*2);
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
    int ret;
    if(!in_size) break;
    if((ret=vfw_decode_video(sh_video,start,in_size,drop_frame,(sh_video->codec->driver==VFM_VFWEX) ))){
      mp_msg(MSGT_DECVIDEO,MSGL_WARN,"Error decompressing frame, err=%d\n",ret);
      break;
    }
    if(!drop_frame) blit_frame=3;
    break;
  }
#endif
  case VFM_MPEG:
    if(out_fmt==IMGFMT_MPEGPES){
	// hardware decoding:
	static vo_mpegpes_t packet;
	mpeg2_decode_data(video_out, start, start+in_size,3); // parse headers
	packet.data=start;
	packet.size=in_size-4;
	packet.timestamp=sh_video->timer*90000.0;
	packet.id=0x1E0; //+sh_video->ds->id;
	planes[0]=(uint8_t*)(&packet);
	blit_frame=2;
    } else {
	// software decoding:
	if(
	mpeg2_decode_data(video_out, start, start+in_size,drop_frame) > 0 // decode
	&& (!drop_frame)
	   ) blit_frame=1;
    }
    break;
  case VFM_RAW:
    planes[0]=start;
    blit_frame=2;
    break;
  case VFM_RLE:
//void AVI_Decode_RLE8(char *image,char *delta,int tdsize,
//    unsigned int *map,int imagex,int imagey,unsigned char x11_bytes_pixel);
    AVI_Decode_RLE8(sh_video->our_out_buffer,start,in_size, 
       (int*)(((char*)sh_video->bih)+40),
      sh_video->disp_w,sh_video->disp_h,((out_fmt&255)+7)/8);
    blit_frame=3;
    break;
} // switch
//------------------------ frame decoded. --------------------

#ifdef HAVE_MMX
	// some codecs is broken, and doesn't restore MMX state :(
	// it happens usually with broken/damaged files.
	__asm __volatile (EMMS:::"memory");
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
      } else {
        planes[0]=sh_video->our_out_buffer;
	if(sh_video->bih && sh_video->bih->biSize==1064)
	    planes[1]=&sh_video->bih[1]; // pointer to palette
	else
	    planes[1]=NULL;
      }
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

