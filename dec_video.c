
// This file is OBSOLETED. DO not modify, stuff moved to libmpcodecs/ from here
#error OBSOLETED

#define USE_MP_IMAGE

#include "config.h"

#include <stdio.h>
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#include <stdlib.h>
#include <unistd.h>

#include "mp_msg.h"
#include "help_mp.h"

#include "linux/timer.h"
#include "linux/shmem.h"

extern int verbose; // defined in mplayer.c

#include "stream.h"
#include "demuxer.h"
#include "parse_es.h"

#include "codec-cfg.h"

#ifdef USE_LIBVO2
#include "libvo2/libvo2.h"
#else
#include "libvo/video_out.h"
#endif

#include "stheader.h"

#include "dec_video.h"

#include "roqav.h"

// ===================================================================

extern int benchmark;
extern double video_time_usage;
extern double vout_time_usage;
extern double max_video_time_usage;
extern double max_vout_time_usage;
extern double cur_video_time_usage;
extern double cur_vout_time_usage;
extern vo_vaa_t vo_vaa;

extern int frameratecode2framerate[16];

#include "dll_init.h"

//#include <inttypes.h>
//#include "libvo/img_format.h"

#include "libmpeg2/mpeg2.h"
#include "libmpeg2/mpeg2_internal.h"

#include "postproc/postprocess.h"

#include "cpudetect.h"

extern picture_t *picture;	// exported from libmpeg2/decode.c

int divx_quality=0;

#ifdef USE_DIRECTSHOW
#include "loader/dshow/DS_VideoDecoder.h"
static DS_VideoDecoder* ds_vdec=NULL;
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

#ifdef USE_DIVX
#ifndef NEW_DECORE
#include "opendivx/decore.h"
#else
#include <decore.h>
#endif
#endif

#ifdef USE_XANIM
#include "xacodec.h"
#endif

#ifdef USE_TV
#include "libmpdemux/tv.h"
extern int tv_param_on;
extern tvi_handle_t *tv_handler;
#endif

void AVI_Decode_RLE8(char *image,char *delta,int tdsize,
    unsigned int *map,int imagex,int imagey,unsigned char x11_bytes_pixel);

void AVI_Decode_Video1_16(
  char *encoded,
  int encoded_size,
  char *decoded,
  int width,
  int height,
  int bytes_per_pixel);

void AVI_Decode_Video1_8(
  char *encoded,
  int encoded_size,
  char *decoded,
  int width,
  int height,
  unsigned char *palette_map,
  int bytes_per_pixel);

void *init_fli_decoder(int width, int height);

void decode_fli_frame(
  unsigned char *encoded,
  int encoded_size,
  unsigned char *decoded,
  int width,
  int height,
  int bytes_per_pixel,
  void *context);

void qt_decode_rle(
  unsigned char *encoded,
  int encoded_size,
  unsigned char *decoded,
  int width,
  int height,
  int encoded_bpp,
  int bytes_per_pixel);

void decode_nuv(
  unsigned char *encoded,
  int encoded_size,
  unsigned char *decoded,
  int width,
  int height);

void *decode_cinepak_init(void);

void decode_cinepak(
  void *context,
  unsigned char *buf,
  int size,
  mp_image_t *mpi);

void decode_cyuv(
  unsigned char *buf,
  int size,
  unsigned char *frame,
  int width,
  int height,
  int bit_per_pixel);

int qt_init_decode_smc(void);

void qt_decode_smc(
  unsigned char *encoded,
  int encoded_size,
  unsigned char *decoded,
  int width,
  int height,
  unsigned char *palette_map,
  int bytes_per_pixel);

void decode_duck_tm1(
  unsigned char *encoded,
  int encoded_size,
  unsigned char *decoded,
  int width,
  int height,
  int bytes_per_pixel);

#ifdef HAVE_PNG
void decode_mpng(
  unsigned char *encoded,
  int encoded_size,
  unsigned char *decoded,
  int width,
  int height,
  int bytes_per_pixel);
#endif

void qt_decode_rpza(
  unsigned char *encoded,
  int encoded_size,
  unsigned char *decoded,
  int width,
  int height,
  int bytes_per_pixel);

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
#ifdef USE_DIVX
  case VFM_DIVX4:
  case VFM_ODIVX:
#ifdef NEW_DECORE
      return 9; // for divx4linux
#else
      return GET_PP_QUALITY_MAX;  // for opendivx
#endif
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
   DS_VideoDecoder_SetValue(ds_vdec,"Quality",quality);
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
#ifdef USE_DIVX
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
#endif
  break;
 }
}

int set_video_colors(sh_video_t *sh_video,char *item,int value)
{
    if(vo_vaa.get_video_eq)
    {
	vidix_video_eq_t veq;
	if(vo_vaa.get_video_eq(&veq) == 0)
	{
	    int v_hw_equ_cap = veq.cap;
	    if(v_hw_equ_cap != 0)
	    {
		if(vo_vaa.set_video_eq)
		{
		    vidix_video_eq_t veq;
		    veq.flags = VEQ_FLG_ITU_R_BT_601; /* Fixme please !!! */
		    if(strcmp(item,"Brightness") == 0)
		    {
			if(!(v_hw_equ_cap & VEQ_CAP_BRIGHTNESS)) goto try_sw_control;
			veq.brightness = value*10;
			veq.cap = VEQ_CAP_BRIGHTNESS;
		    }
		    else
		    if(strcmp(item,"Contrast") == 0)
		    {
			if(!(v_hw_equ_cap & VEQ_CAP_CONTRAST)) goto try_sw_control;
			veq.contrast = value*10;
			veq.cap = VEQ_CAP_CONTRAST;
		    }
		    else
		    if(strcmp(item,"Saturation") == 0)
		    {
			if(!(v_hw_equ_cap & VEQ_CAP_SATURATION)) goto try_sw_control;
			veq.saturation = value*10;
			veq.cap = VEQ_CAP_SATURATION;
		    }
		    else
		    if(strcmp(item,"Hue") == 0)
		    {
			if(!(v_hw_equ_cap & VEQ_CAP_HUE)) goto try_sw_control;
			veq.hue = value*10;
			veq.cap = VEQ_CAP_HUE;
		    }
		    else goto try_sw_control;;
		    vo_vaa.set_video_eq(&veq);
		}
		return 1;
	    }
	}
    }
    try_sw_control:
#ifdef USE_DIRECTSHOW
    if(sh_video->codec->driver==VFM_DSHOW){
	DS_VideoDecoder_SetValue(ds_vdec,item,value);
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

#ifdef USE_TV
    
    if (tv_param_on == 1)
    {
	if (!strcmp(item, "Brightness"))
	{
	    tv_set_color_options(tv_handler, TV_COLOR_BRIGHTNESS, value);
	    return(1);
	}
	if (!strcmp(item, "Hue"))
	{
	    tv_set_color_options(tv_handler, TV_COLOR_HUE, value);
	    return(1);
	}
	if (!strcmp(item, "Saturation"))
	{
	    tv_set_color_options(tv_handler, TV_COLOR_SATURATION, value);
	    return(1);
	}
	if (!strcmp(item, "Contrast"))
	{
	    tv_set_color_options(tv_handler, TV_COLOR_CONTRAST, value);
	    return(1);
	}
    }
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
	if(ds_vdec){ DS_VideoDecoder_Destroy(ds_vdec); ds_vdec=NULL; }
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
#ifdef USE_DIVX
    case VFM_DIVX4:
    case VFM_ODIVX:
      decore(0x123,DEC_OPT_RELEASE,NULL,NULL);
      break;
#endif
    }
    if(sh_video->our_out_buffer){
	free(sh_video->our_out_buffer);
	sh_video->our_out_buffer=NULL;
    }
    sh_video->inited=0;
}

int init_video(sh_video_t *sh_video,int *pitches)
{
unsigned int out_fmt=sh_video->codec->outfmt[sh_video->outfmtidx];
pitches[0] = pitches[1] =pitches[2] = 0; /* fake unknown */

sh_video->our_out_buffer=NULL;
sh_video->our_out_buffer_size=0U;

sh_video->image=new_mp_image(sh_video->disp_w,sh_video->disp_h);
mp_image_setfmt(sh_video->image,out_fmt);

switch(sh_video->codec->driver){
 case VFM_ROQVIDEO:
#ifdef USE_MP_IMAGE
   sh_video->image->type=MP_IMGTYPE_IP;
#else
   sh_video->our_out_buffer_size = sh_video->disp_w * sh_video->disp_h * 1.5;
   sh_video->our_out_buffer = (char*)memalign(64, sh_video->our_out_buffer_size);
#endif
   sh_video->context = roq_decode_video_init();
   break;
 case VFM_CINEPAK: {
#ifdef USE_MP_IMAGE
   sh_video->image->type=MP_IMGTYPE_STATIC;
   sh_video->image->width=(sh_video->image->width+3)&(~3);
   sh_video->image->height=(sh_video->image->height+3)&(~3);
#else
   int bpp=((out_fmt&255)+7)/8;
   sh_video->our_out_buffer_size = sh_video->disp_w*sh_video->disp_h*bpp;
   sh_video->our_out_buffer = (char*)memalign(64, sh_video->our_out_buffer_size);
#endif
   sh_video->context = decode_cinepak_init();
   break;
 }
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
   /* Warning: these pitches tested only with YUY2 fourcc */
   pitches[0] = 16; pitches[1] = pitches[2] = 8;
   break;
 }
 case VFM_VFWEX: {
   if(!init_vfw_video_codec(sh_video,1)) {
      return 0;
   }  
   mp_msg(MSGT_DECVIDEO,MSGL_V,"INFO: Win32Ex video codec init OK!\n");
   /* Warning: these pitches tested only with YUY2 fourcc */
   pitches[0] = 16; pitches[1] = pitches[2] = 8;
   break;
 }
 case VFM_DSHOW: { // Win32/DirectShow
#ifndef USE_DIRECTSHOW
   mp_msg(MSGT_DECVIDEO,MSGL_ERR,MSGTR_NoDShowSupport);
   return 0;
#else
   int bpp;
   if(!(ds_vdec=DS_VideoDecoder_Open(sh_video->codec->dll,&sh_video->codec->guid, sh_video->bih, 0, 0))){
//   if(DS_VideoDecoder_Open(sh_video->codec->dll,&sh_video->codec->guid, sh_video->bih, 0, NULL)){
        mp_msg(MSGT_DECVIDEO,MSGL_ERR,MSGTR_MissingDLLcodec,sh_video->codec->dll);
        mp_msg(MSGT_DECVIDEO,MSGL_HINT,"Maybe you forget to upgrade your win32 codecs?? It's time to download the new\n");
        mp_msg(MSGT_DECVIDEO,MSGL_HINT,"package from:  ftp://mplayerhq.hu/MPlayer/releases/w32codec.zip  !\n");
//        mp_msg(MSGT_DECVIDEO,MSGL_HINT,"Or you should disable DShow support: make distclean;make -f Makefile.No-DS\n");
      return 0;
   }

#ifdef USE_MP_IMAGE
   sh_video->image->type=MP_IMGTYPE_STATIC;
   bpp=sh_video->image->bpp;
   if(sh_video->image->flags&MP_IMGFLAG_YUV){
     DS_VideoDecoder_SetDestFmt(ds_vdec,bpp,out_fmt);     // YUV
   } else {
     DS_VideoDecoder_SetDestFmt(ds_vdec,out_fmt&255,0);           // RGB/BGR
   }
#else
   switch(out_fmt){
   case IMGFMT_YUY2:
   case IMGFMT_UYVY:
     bpp=16;
     DS_VideoDecoder_SetDestFmt(ds_vdec,16,out_fmt);break;        // packed YUV
   case IMGFMT_YV12:
   case IMGFMT_I420:
   case IMGFMT_IYUV:
     bpp=12;
     DS_VideoDecoder_SetDestFmt(ds_vdec,12,out_fmt);break;        // planar YUV
   default:
     bpp=((out_fmt&255)+7)&(~7);
     DS_VideoDecoder_SetDestFmt(ds_vdec,out_fmt&255,0);           // RGB/BGR
   }
   sh_video->our_out_buffer_size = sh_video->disp_w*sh_video->disp_h*bpp/8; // FIXME!!!
   sh_video->our_out_buffer = (char*)memalign(64,sh_video->our_out_buffer_size); 
#endif
   /* Warning: these pitches tested only with YUY2 fourcc */
   pitches[0] = 16; pitches[1] = pitches[2] = 8;
   DS_SetAttr_DivX("Quality",divx_quality);

   DS_VideoDecoder_StartInternal(ds_vdec);
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
#ifndef USE_DIVX
   mp_msg(MSGT_DECVIDEO,MSGL_ERR,"MPlayer was compiled WITHOUT OpenDivx support!\n");
   return 0;
#else
   mp_msg(MSGT_DECVIDEO,MSGL_V,"OpenDivX video codec\n");
   { DEC_PARAM dec_param;
     DEC_SET dec_set;
        memset(&dec_param,0,sizeof(dec_param));
#ifdef NEW_DECORE
        dec_param.output_format=DEC_USER;
#else
        dec_param.color_depth = 32;
#endif
#ifdef DECORE_DIVX5
	/* codec_version should be 311, 400 or 500 according
	 * to DivX version used in video, let's hope 500 is 
	 * compatible with all DivX4 content, otherwise we
	 * should find some logic to also choose between
	 * 400 and 500 - Atmos
	 */
	dec_param.codec_version = (sh_video->format==mmioFOURCC('D','I','V','3'))?311:500;
	dec_param.build_number = 0;
#endif
	dec_param.x_dim = sh_video->bih->biWidth;
	dec_param.y_dim = sh_video->bih->biHeight;
	decore(0x123, DEC_OPT_INIT, &dec_param, NULL);
	dec_set.postproc_level = divx_quality;
	decore(0x123, DEC_OPT_SETPP, &dec_set, NULL);
   }
   mp_msg(MSGT_DECVIDEO,MSGL_V,"INFO: OpenDivX video codec init OK!\n");
   break;
#endif
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
#ifdef DECORE_DIVX5
	dec_param.codec_version = (sh_video->format==mmioFOURCC('D','I','V','3'))?311:500;
	dec_param.build_number = 0;
#endif
	dec_param.x_dim = sh_video->bih->biWidth;
	dec_param.y_dim = sh_video->bih->biHeight;
	decore(0x123, DEC_OPT_INIT, &dec_param, NULL);
	dec_set.postproc_level = divx_quality;
	decore(0x123, DEC_OPT_SETPP, &dec_set, NULL);
#ifdef USE_MP_IMAGE
	sh_video->image->type=MP_IMGTYPE_STATIC;
#else
	sh_video->our_out_buffer_size = ((bits*dec_param.x_dim+7)/8)*dec_param.y_dim;
	sh_video->our_out_buffer = (char*)memalign(64,sh_video->our_out_buffer_size);
//	sh_video->our_out_buffer = shmem_alloc(dec_param.x_dim*dec_param.y_dim*5);
#endif
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
   /* Just because we know that */
   pitches[0] = 16;
   pitches[1] = pitches[2] = 8;
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
    if (sh_video->format == mmioFOURCC('R', 'V', '1', '3'))
	lavc_context.sub_id = 3;
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
   /* Just because we know that */
   pitches[0] = 16;
   pitches[1] = pitches[2] = 8;
   // send seq header to the decoder:
   mpeg2_decode_data(NULL,videobuffer,videobuffer+videobuf_len,0);
   mpeg2_allocate_image_buffers (picture);
   break;
 }
 case VFM_RAW: {
   if (sh_video->format != 0x0)
    /* set out_fmt */
	sh_video->codec->outfmt[sh_video->outfmtidx] = sh_video->format;
   break;
 }
 case VFM_RLE: {
   int bpp=((out_fmt&255)+7)/8; // RGB only
#ifdef USE_MP_IMAGE
    sh_video->image->type=MP_IMGTYPE_STATIC;
#else
    sh_video->our_out_buffer_size = sh_video->disp_w*sh_video->disp_h*bpp; // FIXME!!!
    sh_video->our_out_buffer = (char*)memalign(64,sh_video->our_out_buffer_size); 
#endif
   if(bpp==2){  // 15 or 16 bpp ==> palette conversion!
     unsigned int* pal=(unsigned int*)(((char*)sh_video->bih)+40);
     int cols=(sh_video->bih->biSize-40)/4;
     //int cols=1<<(sh_video->bih->biBitCount);
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
 case VFM_FLI:
   sh_video->context = init_fli_decoder(sh_video->disp_w, sh_video->disp_h);
 case VFM_MSVIDC:
 case VFM_QTRLE:
 case VFM_DUCKTM1:
#ifdef HAVE_PNG
 case VFM_MPNG:
#endif
 case VFM_QTRPZA:
   {
#ifdef USE_MP_IMAGE
    sh_video->image->type=MP_IMGTYPE_STATIC;
#else
   int bpp=((out_fmt&255)+7)/8; // RGB only
   sh_video->our_out_buffer_size =  sh_video->disp_w*sh_video->disp_h*bpp; // FIXME!!!
   sh_video->our_out_buffer = (char*)memalign(64,sh_video->our_out_buffer_size); 
#endif
if ((sh_video->codec->driver == VFM_QTRLE) && (sh_video->bih->biBitCount != 24))
  printf (
    "    *** FYI: This Quicktime file is using %d-bit RLE Animation\n" \
    "    encoding, which is not yet supported by MPlayer. But if you upload\n" \
    "    this Quicktime file to the MPlayer FTP, the team could look at it.\n",
    sh_video->bih->biBitCount);

   break;
   }
 case VFM_QTSMC:
   {
   if (qt_init_decode_smc() != 0)
     mp_msg(MSGT_DECVIDEO, MSGL_ERR, "SMC decoder could not allocate enough memory");
#ifdef USE_MP_IMAGE
    sh_video->image->type=MP_IMGTYPE_STATIC;
#else
   int bpp=((out_fmt&255)+7)/8; // RGB only
   sh_video->our_out_buffer_size = sh_video->disp_w*sh_video->disp_h*bpp; // FIXME!!!
   sh_video->our_out_buffer = (char*)memalign(64, sh_video->our_out_buffer_size); 
#endif
   break;
   }
 case VFM_NUV:
#ifdef USE_MP_IMAGE
    sh_video->image->type=MP_IMGTYPE_STATIC;
#else
    sh_video->our_out_buffer_size =  sh_video->disp_w*sh_video->disp_h*3/2;
    sh_video->our_out_buffer = (char *)memalign(64,sh_video->our_out_buffer_size);
#endif
   break;
 case VFM_CYUV: {
//   int bpp=((out_fmt&255)+7)/8;
#ifdef USE_MP_IMAGE
    sh_video->image->type=MP_IMGTYPE_STATIC;
#else
   sh_video->our_out_buffer_size = sh_video->disp_w*sh_video->disp_h*3;
   sh_video->our_out_buffer = (char*)memalign(64, sh_video->our_out_buffer_size);
#endif
   break;
   }
 }
}
  sh_video->inited=1;
  return 1;
}

extern int vaa_use_dr;

static int use_dr=0,use_dr_422=0;
static bes_da_t bda;
static int multi_buff_num = 0;
void init_video_vaa( unsigned width )
{
  unsigned adp;
  memset(&bda,0,sizeof(bes_da_t));
  if(vo_vaa.query_bes_da)
    use_dr = vo_vaa.query_bes_da(&bda) ? 0 : 1;
  if(!vaa_use_dr) use_dr = 0;
  if(use_dr)
  {
    uint32_t sstride,dstride;
    sstride=width*2;
    adp = bda.dest.pitch.y-1;
    dstride=(width*2+adp)&~adp;
    if(sstride == dstride) use_dr_422 = 1;
  }
}

#ifdef USE_LIBVO2
int decode_video(vo2_handle_t *video_out,sh_video_t *sh_video,unsigned char *start,int in_size,int drop_frame){
#else
int decode_video(vo_functions_t *video_out,sh_video_t *sh_video,unsigned char *start,int in_size,int drop_frame){
#endif

mp_image_t *mpi=sh_video->image;
unsigned int out_fmt=mpi->imgfmt; //sh_video->codec->outfmt[sh_video->outfmtidx];
int planar=(mpi->flags&MP_IMGFLAG_PLANAR)!=0; //(out_fmt==IMGFMT_YV12||out_fmt==IMGFMT_IYUV||out_fmt==IMGFMT_I420);
int blit_frame=0;
void *vmem;
int painted;

//uint8_t* planes_[3];
//uint8_t** planes=planes_;
//int stride_[3];
//int* stride=stride_;

unsigned int t=GetTimer();
unsigned int t2;
double tt;

  painted = 0;
#ifdef USE_MP_IMAGE
if(mpi->type!=MP_IMGTYPE_EXPORT)
if( !(mpi->flags&MP_IMGFLAG_ALLOCATED) && !(mpi->flags&MP_IMGFLAG_DIRECT) ){
    // allocate image buffer:
    sh_video->our_out_buffer = (char *)memalign(64, mpi->width*mpi->height*mpi->bpp/8);
    if((mpi->flags|MP_IMGFLAG_PLANAR) && (mpi->flags|MP_IMGFLAG_YUV)){
	// planar YUV
	mpi->stride[0]=mpi->width;
	mpi->stride[1]=mpi->stride[2]=mpi->width/2;
	mpi->planes[0]=sh_video->our_out_buffer;
	mpi->planes[1]=mpi->planes[0]+mpi->stride[0]*mpi->height;
	mpi->planes[2]=mpi->planes[1]+mpi->stride[0]*mpi->height/4;
    } else {
	// packed YUV / RGB
	mpi->stride[0]=mpi->width*mpi->bpp;
	mpi->planes[0]=sh_video->our_out_buffer;
    }
    mpi->flags|=MP_IMGFLAG_ALLOCATED;
    mp_msg(MSGT_DECVIDEO,MSGL_INFO,"mp_image: allocated %d bytes for %dx%dx%d [0x%X] image\n", 
	    mpi->width*mpi->height*mpi->bpp/8, mpi->width, mpi->height, mpi->bpp, mpi->imgfmt);
}
#endif

//printf("decode_frame(start: %p, size: %d, w: %d, h: %d)\n",
//    start, in_size, sh_video->disp_w, sh_video->disp_h);

  //--------------------  Decode a frame: -----------------------
switch(sh_video->codec->driver){
 case VFM_CINEPAK:
   if (in_size == 0)
     blit_frame = 0;
   else
   {
     decode_cinepak(sh_video->context, start, in_size, mpi);
     blit_frame = 2;
   }
   break;
#ifdef USE_XANIM
  case VFM_XANIM: {
    xacodec_image_t* image=xacodec_decode_frame(start,in_size,drop_frame?1:0);
    if(image){
	blit_frame=2;
	//planes=image->planes;
	//stride=image->stride;
	mpi->planes[0]=image->planes[0];
	mpi->planes[1]=image->planes[1];
	mpi->planes[2]=image->planes[2];
	mpi->stride[0]=image->stride[0];
	mpi->stride[1]=image->stride[1];
	mpi->stride[2]=image->stride[2];
    }
    break;
  }
#endif
#ifdef USE_DIVX
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
#ifdef DECORE_DIVX5
	decore(0x123, DEC_OPT_FRAME, &dec_frame, NULL);
#else
	decore(0x123, (sh_video->format==mmioFOURCC('D','I','V','3'))?DEC_OPT_FRAME_311:DEC_OPT_FRAME, &dec_frame, NULL);
#endif
#else
        opendivx_src[0]=NULL;
	decore(0x123, 0, &dec_frame, NULL);
#endif
  
      if(!drop_frame)

    // let's display
#ifdef NEW_DECORE
      if(dec_pic.y){
        mpi->planes[0]=dec_pic.y;
        mpi->planes[1]=dec_pic.u;
        mpi->planes[2]=dec_pic.v;
        mpi->stride[0]=dec_pic.stride_y;
        mpi->stride[1]=mpi->stride[2]=dec_pic.stride_uv;
        blit_frame=2;
      }
#else
      if(opendivx_src[0]){
//        planes=opendivx_src;
//	stride=opendivx_stride;
	mpi->planes[0]=opendivx_src[0];
	mpi->planes[1]=opendivx_src[1];
	mpi->planes[2]=opendivx_src[2];
	mpi->stride[0]=opendivx_stride[0];
	mpi->stride[1]=opendivx_stride[1];
	mpi->stride[2]=opendivx_stride[2];
        blit_frame=2;
      }
#endif

    break;
  }
#endif
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
#ifdef DECORE_DIVX5
	decore(0x123, DEC_OPT_FRAME, &dec_frame, NULL);
#else
	decore(0x123, (sh_video->format==mmioFOURCC('D','I','V','3'))?DEC_OPT_FRAME_311:DEC_OPT_FRAME, &dec_frame, NULL);
#endif
    if(!drop_frame) blit_frame=3;
    break;
  }
#endif
#ifdef USE_DIRECTSHOW
  case VFM_DSHOW: {        // W32/DirectShow
    if(drop_frame<2)
    {
	/* FIXME: WILL WORK ONLY FOR PACKED FOURCC. BUT WHAT ABOUT PLANAR? */
        vmem = 0;
	if(use_dr_422)
	{
	    vmem = bda.dga_addr + bda.offsets[0] + bda.offset.y;
	    if(vo_doublebuffering && bda.num_frames>1)
	    {
		vmem = bda.dga_addr + bda.offsets[multi_buff_num] + bda.offset.y;
		multi_buff_num=(multi_buff_num+1)%bda.num_frames;
	    }
	}
	DS_VideoDecoder_DecodeInternal(ds_vdec, start, in_size, 0, drop_frame ? 0 : vmem ? vmem : sh_video->our_out_buffer);
	if(vmem) painted = 1; 
    }
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
#ifdef MBC
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
    		mpi->stride[0]=w;
    		mpi->stride[1]=mpi->stride[2]=w/2;
    		mpi->planes[0]=sh_video->our_out_buffer+mpi->stride[0]*yoff+xoff;
    		mpi->planes[2]=sh_video->our_out_buffer+w*h+mpi->stride[2]*(yoff>>1)+(xoff>>1);
    		mpi->planes[1]=mpi->planes[2]+w*h/4;
		postprocess(lavc_picture.data,lavc_picture.linesize[0],
			    mpi->planes,mpi->stride[0],
			    sh_video->disp_w,sh_video->disp_h,
			    &quant_store[0][0],MBC+1,lavc_pp);
	    } else
#endif
#endif
	    {
		//planes=lavc_picture.data;
		//stride=lavc_picture.linesize;
		mpi->planes[0]=lavc_picture.data[0];
		mpi->planes[1]=lavc_picture.data[1];
		mpi->planes[2]=lavc_picture.data[2];
		mpi->stride[0]=lavc_picture.linesize[0];
		mpi->stride[1]=lavc_picture.linesize[1];
		mpi->stride[2]=lavc_picture.linesize[2];
		if(lavc_context.pix_fmt==PIX_FMT_YUV422P){
		    mpi->stride[1]*=2;
		    mpi->stride[2]*=2;
		}
		
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
	/* FIXME: WILL WORK ONLY FOR PACKED FOURCC. BUT WHAT ABOUT PLANAR? */
        vmem = 0;
	if(use_dr_422)
	{
	    vmem = bda.dga_addr + bda.offsets[0] + bda.offset.y;
	    if(vo_doublebuffering && bda.num_frames>1)
	    {
		vmem = bda.dga_addr + bda.offsets[multi_buff_num] + bda.offset.y;
		multi_buff_num=(multi_buff_num+1)%bda.num_frames;
	    }
	    sh_video->our_out_buffer = vmem;
	}
    if((ret=vfw_decode_video(sh_video,start,in_size,drop_frame,(sh_video->codec->driver==VFM_VFWEX) ))){
      mp_msg(MSGT_DECVIDEO,MSGL_WARN,"Error decompressing frame, err=%d\n",ret);
      break;
    }
    if(vmem) painted=1;
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
	mpi->planes[0]=(uint8_t*)(&packet);
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
//    planes[0]=start;
//    blit_frame=2;
    sh_video->our_out_buffer = start;
    blit_frame=3;
    break;
  case VFM_RLE:
//void AVI_Decode_RLE8(char *image,char *delta,int tdsize,
//    unsigned int *map,int imagex,int imagey,unsigned char x11_bytes_pixel);
    AVI_Decode_RLE8(sh_video->our_out_buffer,start,in_size, 
       (int*)(((char*)sh_video->bih)+40),
      sh_video->disp_w,sh_video->disp_h,((out_fmt&255)+7)/8);
    blit_frame=3;
    break;
  case VFM_MSVIDC:
    if (sh_video->bih->biBitCount == 16)
      AVI_Decode_Video1_16(
        start, in_size, sh_video->our_out_buffer,
        sh_video->disp_w, sh_video->disp_h,
        ((out_fmt&255)+7)/8);
    else
      AVI_Decode_Video1_8(
        start, in_size, sh_video->our_out_buffer,
        sh_video->disp_w, sh_video->disp_h,
        (char *)sh_video->bih+40, ((out_fmt&255)+7)/8);
    blit_frame = 3;
    break;
  case VFM_FLI:
    decode_fli_frame(
        start, in_size, sh_video->our_out_buffer,
        sh_video->disp_w, sh_video->disp_h,
        ((out_fmt&255)+7)/8,
        sh_video->context);
    blit_frame = 3;
    break;
  case VFM_NUV:
    decode_nuv(
        start, in_size, sh_video->our_out_buffer,
        sh_video->disp_w, sh_video->disp_h);
    blit_frame = 3;
    break;
  case VFM_QTRLE:
    qt_decode_rle(
        start, in_size, sh_video->our_out_buffer,
        sh_video->disp_w, sh_video->disp_h,
        sh_video->bih->biBitCount,
        ((out_fmt&255)+7)/8);
    blit_frame = 3;
    break;
  case VFM_QTSMC:
    qt_decode_smc(
        start, in_size, sh_video->our_out_buffer,
        sh_video->disp_w, sh_video->disp_h,
        (unsigned char *)sh_video->bih+40,
        ((out_fmt&255)+7)/8);
    blit_frame = 3;
    break;
  case VFM_DUCKTM1:
    decode_duck_tm1(
        start, in_size, sh_video->our_out_buffer,
        sh_video->disp_w, sh_video->disp_h,
        ((out_fmt&255)+7)/8);
    blit_frame = 3;
    break;
#ifdef HAVE_PNG
 case VFM_MPNG:
    decode_mpng(
        start, in_size, sh_video->our_out_buffer,
	sh_video->disp_w,sh_video->disp_h,
	((out_fmt&255)+7)/8
    );
    blit_frame = 3;
    break;
#endif
 case VFM_CYUV:
   decode_cyuv(start, in_size, sh_video->our_out_buffer,
      sh_video->disp_w, sh_video->disp_h, (out_fmt==IMGFMT_YUY2)?16:(out_fmt&255));
   blit_frame = 3;
   break;
 case VFM_ROQVIDEO:
   roq_decode_video(sh_video->context, start, in_size, mpi);
   blit_frame = 2;
   break;
  case VFM_QTRPZA:
    qt_decode_rpza(
        start, in_size, sh_video->our_out_buffer,
        sh_video->disp_w, sh_video->disp_h,
        ((out_fmt&255)+7)/8);
    blit_frame = 3;
    break;
} // switch
//------------------------ frame decoded. --------------------

#ifdef ARCH_X86
	// some codecs is broken, and doesn't restore MMX state :(
	// it happens usually with broken/damaged files.
if(gCpuCaps.has3DNow){
	__asm __volatile ("femms\n\t":::"memory");
}
else if(gCpuCaps.hasMMX){
	__asm __volatile ("emms\n\t":::"memory");
}
#endif

t2=GetTimer();t=t2-t;
tt = t*0.000001f;
video_time_usage+=tt;
if(benchmark)
{
    if(tt > max_video_time_usage) max_video_time_usage=tt;
    cur_video_time_usage=tt;
}
if(painted) return 1;
switch(blit_frame){
case 3:
      if(planar){
        mpi->stride[0]=sh_video->disp_w;
        mpi->stride[1]=mpi->stride[2]=sh_video->disp_w/2;
        mpi->planes[0]=sh_video->our_out_buffer;
        mpi->planes[2]=mpi->planes[0]+sh_video->disp_w*sh_video->disp_h;
        mpi->planes[1]=mpi->planes[2]+sh_video->disp_w*sh_video->disp_h/4;
      } else {
        mpi->planes[0]=sh_video->our_out_buffer;
	mpi->stride[0]=sh_video->disp_w*mpi->bpp;
	if(sh_video->bih && sh_video->bih->biSize==1064)
	    mpi->planes[1]=&sh_video->bih[1]; // pointer to palette
	else
	    mpi->planes[1]=NULL;
      }
//#define VFM_RAW_POSTPROC
#ifdef VFM_RAW_POSTPROC
    if (sh_video->codec->driver == VFM_RAW)
    {
	mp_dbg(MSGT_DECVIDEO, MSGL_V, "Postprocessing raw %s!\n",
	    vo_format_name(out_fmt));
	switch(out_fmt)
	{
	    case IMGFMT_YV12:
		postprocess(planes, stride[0], planes, stride[0],
		    sh_video->disp_w, sh_video->disp_h, planes[0],
		    0, /*0x20000*/divx_quality);
		break;
//	    case IMGFMT_UYVY:
//		uyvytoyv12(start, planes[0], planes[1], planes[2],
//		    sh_video->disp_w, sh_video->disp_h, stride[0], stride[1],
//		    sh_video->disp_w*2);
//		postprocess(planes, stride[0], planes, stride[0],
//		    sh_video->disp_w, sh_video->disp_h, planes[0],
//		    0, /*0x20000*/divx_quality);
//		break;
	    default:
		mp_dbg(MSGT_DECVIDEO, MSGL_DBG2, "Unsuitable outformat (%s) for raw pp!\n",
		    vo_format_name(out_fmt));
	}
    }
#endif
case 2:
#ifdef USE_LIBVO2
    if(planar)
        vo2_draw_slice(video_out,planes,stride,sh_video->disp_w,sh_video->disp_h,0,0);
    else
        vo2_draw_frame(video_out,planes[0],sh_video->disp_w,sh_video->disp_w,sh_video->disp_h);
#else
    if(planar)
        video_out->draw_slice(mpi->planes,mpi->stride,sh_video->disp_w,sh_video->disp_h,0,0);
    else
        video_out->draw_frame(mpi->planes);
#endif
    t2=GetTimer()-t2;
    tt=t2*0.000001f;
    vout_time_usage+=tt;
    if(benchmark)
    {
	if(tt > max_vout_time_usage) max_vout_time_usage = tt;
	cur_vout_time_usage=tt;
    }
    blit_frame=1;
    break;
}

  return blit_frame;
}


