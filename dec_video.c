
#include <stdio.h>
#include <stdlib.h>

#include "config.h"

extern int verbose; // defined in mplayer.c
extern int divx_quality;

extern double video_time_usage;
extern double vout_time_usage;

#include "stream.h"
#include "demuxer.h"

#include "wine/mmreg.h"
#include "wine/avifmt.h"
#include "wine/vfw.h"

#include "codec-cfg.h"
#include "stheader.h"

//#include <inttypes.h>
//#include "libvo/img_format.h"

#include "libvo/video_out.h"

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

#include "opendivx/decore.h"

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


int init_video(sh_video_t *sh_video){
unsigned int out_fmt=sh_video->codec->outfmt[sh_video->outfmtidx];

switch(sh_video->codec->driver){
#ifdef ARCH_X86
 case 2: {
   if(!init_video_codec(sh_video,0)) {
//     GUI_MSG( mplUnknowError )
//     exit(1);
      return 0;
   }  
   if(verbose) printf("INFO: Win32 video codec init OK!\n");
   break;
 }
 case 6: {
   if(!init_video_codec(sh_video,1)) {
//     GUI_MSG( mplUnknowError )
//     exit(1);
      return 0;
   }  
   if(verbose) printf("INFO: Win32Ex video codec init OK!\n");
   break;
 }
 case 4: { // Win32/DirectShow
#ifndef USE_DIRECTSHOW
   fprintf(stderr,"MPlayer was compiled WITHOUT directshow support!\n");
   return 0;
//   GUI_MSG( mplCompileWithoutDSSupport )
//   exit(1);
#else
   sh_video->our_out_buffer=NULL;
   if(DS_VideoDecoder_Open(sh_video->codec->dll,&sh_video->codec->guid, sh_video->bih, 0, &sh_video->our_out_buffer)){
//   if(DS_VideoDecoder_Open(sh_video->codec->dll,&sh_video->codec->guid, sh_video->bih, 0, NULL)){
        printf("ERROR: Couldn't open required DirectShow codec: %s\n",sh_video->codec->dll);
        printf("Maybe you forget to upgrade your win32 codecs?? It's time to download the new\n");
        printf("package from:  ftp://thot.banki.hu/esp-team/linux/MPlayer/w32codec.zip  !\n");
        printf("Or you should disable DShow support: make distclean;make -f Makefile.No-DS\n");
      return 0;
//        #ifdef HAVE_GUI
//         if ( !nogui )
//          {
//           strcpy(  mplShMem->items.videodata.codecdll,sh_video->codec->dll );
//           mplSendMessage( mplDSCodecNotFound );
//           usec_sleep( 10000 );
//          }
//        #endif
//        exit(1);
   }
   
   switch(out_fmt){
   case IMGFMT_YUY2:
   case IMGFMT_UYVY:
     DS_VideoDecoder_SetDestFmt(16,out_fmt);break;        // packed YUV
   case IMGFMT_YV12:
   case IMGFMT_I420:
   case IMGFMT_IYUV:
     DS_VideoDecoder_SetDestFmt(12,out_fmt);break;        // planar YUV
   default:
     DS_VideoDecoder_SetDestFmt(out_fmt&255,0);           // RGB/BGR
   }

   DS_VideoDecoder_Start();

   DS_SetAttr_DivX("Quality",divx_quality);
//   printf("DivX setting result = %d\n", DS_SetAttr_DivX("Quality",divx_quality) );
//   printf("DivX setting result = %d\n", DS_SetValue_DivX("Brightness",60) );
   
   if(verbose) printf("INFO: Win32/DShow video codec init OK!\n");
   break;
#endif
 }
#else	/* !ARCH_X86 */
 case 2:
 case 4:
 case 6:
   fprintf(stderr,"MPlayer does not support win32 codecs on non-x86 platforms!\n");
   return 0;
#endif	/* !ARCH_X86 */
 case 3: {  // OpenDivX
   if(verbose) printf("OpenDivX video codec\n");
   { DEC_PARAM dec_param;
     DEC_SET dec_set;
#ifdef NEW_DECORE
     DEC_MEM_REQS dec_mem;
        dec_param.output_format=DEC_USER;
#else
        dec_param.color_depth = 32;
#endif
	dec_param.x_dim = sh_video->bih->biWidth;
	dec_param.y_dim = sh_video->bih->biHeight;
#ifdef NEW_DECORE
        // 0.50-CVS new malloc scheme
        decore(0x123, DEC_OPT_MEMORY_REQS, &dec_param, &dec_mem);
        dec_param.buffers.mp4_edged_ref_buffers=malloc(dec_mem.mp4_edged_ref_buffers_size);
        dec_param.buffers.mp4_edged_for_buffers=malloc(dec_mem.mp4_edged_for_buffers_size);
        dec_param.buffers.mp4_display_buffers=malloc(dec_mem.mp4_display_buffers_size);
        dec_param.buffers.mp4_state=malloc(dec_mem.mp4_state_size);
        dec_param.buffers.mp4_tables=malloc(dec_mem.mp4_tables_size);
        dec_param.buffers.mp4_stream=malloc(dec_mem.mp4_stream_size);
#endif
	decore(0x123, DEC_OPT_INIT, &dec_param, NULL);

	dec_set.postproc_level = divx_quality;
	decore(0x123, DEC_OPT_SETPP, &dec_set, NULL);

   }
   if(verbose) printf("INFO: OpenDivX video codec init OK!\n");
   break;
 }
 case 5: {  // FFmpeg's libavcodec
#ifndef USE_LIBAVCODEC
   fprintf(stderr,"MPlayer was compiled WITHOUT libavcodec support!\n");
   return 0; //exit(1);
#else
   if(verbose) printf("FFmpeg's libavcodec video codec\n");
    avcodec_init();
    avcodec_register_all();
    lavc_codec = (AVCodec *)avcodec_find_decoder_by_name(sh_video->codec->dll);
    if(!lavc_codec){
	fprintf(stderr,"Can't find codec '%s' in libavcodec...\n",sh_video->codec->dll);
	return 0; //exit(1);
    }
    memset(&lavc_context, 0, sizeof(lavc_context));
    lavc_context.width=sh_video->disp_w;
    lavc_context.height=sh_video->disp_h;
    printf("libavcodec.size: %d x %d\n",lavc_context.width,lavc_context.height);
    /* open it */
    if (avcodec_open(&lavc_context, lavc_codec) < 0) {
        fprintf(stderr, "could not open codec\n");
        return 0; //exit(1);
    }
   
   if(verbose) printf("INFO: libavcodec init OK!\n");
   break;
#endif
 }

 case 1: {
   // init libmpeg2:
#ifdef MPEG12_POSTPROC
   picture->pp_options=divx_quality;
#else
   if(divx_quality){
       printf("WARNING! You requested image postprocessing for an MPEG 1/2 video,\n");
       printf("         but compiled MPlayer without MPEG 1/2 postprocessing support!\n");
       printf("         #define MPEG12_POSTPROC in config.h, and recompile libmpeg2!\n");
   }
#endif
   mpeg2_allocate_image_buffers (picture);
   break;
 }
}

  return 1;
}

int decode_video(vo_functions_t *video_out,sh_video_t *sh_video,unsigned char *start,int in_size,int drop_frame){
unsigned int out_fmt=sh_video->codec->outfmt[sh_video->outfmtidx];
int blit_frame=0;

  //--------------------  Decode a frame: -----------------------
switch(sh_video->codec->driver){
  case 3: {
    // OpenDivX
    unsigned int t=GetTimer();
    unsigned int t2;
    DEC_FRAME dec_frame;
#ifdef NEW_DECORE
    DEC_PICTURE dec_pic;
#endif
    // let's decode
        dec_frame.length = in_size;
	dec_frame.bitstream = start;
	dec_frame.render_flag = 1;
#ifdef NEW_DECORE
        dec_frame.bmp=&dec_pic;
        dec_pic.y=dec_pic.u=dec_pic.v=NULL;
#endif
	decore(0x123, 0, &dec_frame, NULL);
      t2=GetTimer();t=t2-t;video_time_usage+=t*0.000001f;

#ifdef NEW_DECORE
      if(dec_pic.y){
        void* src[3];
        int stride[3];
        src[0]=dec_pic.y;
        src[1]=dec_pic.u;
        src[2]=dec_pic.v;
        stride[0]=dec_pic.stride_y;
        stride[1]=stride[2]=dec_pic.stride_uv;
        video_out->draw_slice(src,stride,
                            sh_video->disp_w,sh_video->disp_h,0,0);
        blit_frame=1;
      }
#else
      if(opendivx_src[0]){
        video_out->draw_slice(opendivx_src,opendivx_stride,
                            sh_video->disp_w,sh_video->disp_h,0,0);
        opendivx_src[0]=NULL;
        blit_frame=1;
      }
#endif
      t2=GetTimer()-t2;vout_time_usage+=t2*0.000001f;

    break;
  }
#ifdef USE_DIRECTSHOW
  case 4: {        // W32/DirectShow
    unsigned int t=GetTimer();
    unsigned int t2;

    if(drop_frame<2) DS_VideoDecoder_DecodeFrame(start, in_size, 0, !drop_frame);

    if(!drop_frame && sh_video->our_out_buffer){
      t2=GetTimer();t=t2-t;video_time_usage+=t*0.000001f;
      if(out_fmt==IMGFMT_YV12||out_fmt==IMGFMT_IYUV||out_fmt==IMGFMT_I420){
        uint8_t* dst[3];
        int stride[3];
        stride[0]=sh_video->disp_w;
        stride[1]=stride[2]=sh_video->disp_w/2;
        dst[0]=sh_video->our_out_buffer;
        dst[2]=dst[0]+sh_video->disp_w*sh_video->disp_h;
        dst[1]=dst[2]+sh_video->disp_w*sh_video->disp_h/4;
        video_out->draw_slice(dst,stride,sh_video->disp_w,sh_video->disp_h,0,0);
      } else
        video_out->draw_frame((uint8_t **)&sh_video->our_out_buffer);
      t2=GetTimer()-t2;vout_time_usage+=t2*0.000001f;
      blit_frame=1;
    }
    break;
  }
#endif
#ifdef USE_LIBAVCODEC
  case 5: {        // libavcodec
    unsigned int t=GetTimer();
    unsigned int t2;
    int got_picture=0;

    if(drop_frame<2 && in_size>0){
        int ret = avcodec_decode_video(&lavc_context, &lavc_picture,
	     &got_picture, start, in_size);
	if(ret<0) fprintf(stderr, "Error while decoding frame!\n");
    }

    if(!drop_frame && got_picture){
      t2=GetTimer();t=t2-t;video_time_usage+=t*0.000001f;
      video_out->draw_slice(lavc_picture.data,lavc_picture.linesize,sh_video->disp_w,sh_video->disp_h,0,0);
      t2=GetTimer()-t2;vout_time_usage+=t2*0.000001f;
      blit_frame=1;
    }
    
    break;
  }
#endif
  case 6:
  case 2:
#ifdef ARCH_X86
  {
    HRESULT ret;
    unsigned int t=GetTimer();
    unsigned int t2;
    
    if(in_size){
      sh_video->bih->biSizeImage = in_size;

//      sh_video->bih->biWidth = 1280;
//      sh_video->o_bih.biWidth = 1280;
	    //      ret = ICDecompress(avi_header.hic, ICDECOMPRESS_NOTKEYFRAME|(ICDECOMPRESS_HURRYUP|ICDECOMPRESS_PREROL), 

if(sh_video->codec->driver==6)
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

      if(ret){ printf("Error decompressing frame, err=%d\n",(int)ret);break; }
    }
//    current_module="draw_frame";
    if(!drop_frame){
      t2=GetTimer();t=t2-t;video_time_usage+=t*0.000001f;
//      if(out_fmt==IMGFMT_YV12){
      if(out_fmt==IMGFMT_YV12||out_fmt==IMGFMT_IYUV||out_fmt==IMGFMT_I420){
        uint8_t* dst[3];
        int stride[3];
        stride[0]=sh_video->disp_w;
        stride[1]=stride[2]=sh_video->disp_w/2;
        dst[0]=sh_video->our_out_buffer;
        dst[2]=dst[0]+sh_video->disp_w*sh_video->disp_h;
        dst[1]=dst[2]+sh_video->disp_w*sh_video->disp_h/4;
        video_out->draw_slice(dst,stride,sh_video->disp_w,sh_video->disp_h,0,0);
      } else
        video_out->draw_frame((uint8_t **)&sh_video->our_out_buffer);
      t2=GetTimer()-t2;vout_time_usage+=t2*0.000001f;
      blit_frame=1;
    }
    break;
  }
#else
    printf("Win32 video codec unavailable on non-x86 CPU -> force nosound :(\n");
    break;
#endif
  case 1: {
        int in_frame=0;
        int t=0;
        float newfps;

        t-=GetTimer();
          mpeg2_decode_data(video_out, start, start+in_size,drop_frame);
        t+=GetTimer(); video_time_usage+=t*0.000001;
        blit_frame=1;

    break;
  }
} // switch
//------------------------ frame decoded. --------------------

  return blit_frame;
}
