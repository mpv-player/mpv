#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"

#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif

#include "codec-cfg.h"
//#include "mp_image.h"

#include "../libvo/img_format.h"

#include "stream.h"
#include "demuxer.h"
#include "stheader.h"

#include "vd.h"
//#include "vd_internal.h"

extern vd_functions_t mpcodecs_vd_null;
extern vd_functions_t mpcodecs_vd_cinepak;
extern vd_functions_t mpcodecs_vd_qtrpza;
extern vd_functions_t mpcodecs_vd_ffmpeg;
extern vd_functions_t mpcodecs_vd_dshow;
extern vd_functions_t mpcodecs_vd_vfw;
extern vd_functions_t mpcodecs_vd_vfwex;
extern vd_functions_t mpcodecs_vd_odivx;
extern vd_functions_t mpcodecs_vd_divx4;
extern vd_functions_t mpcodecs_vd_raw;
extern vd_functions_t mpcodecs_vd_xanim;
extern vd_functions_t mpcodecs_vd_msrle;
extern vd_functions_t mpcodecs_vd_msvidc;
extern vd_functions_t mpcodecs_vd_fli;
extern vd_functions_t mpcodecs_vd_qtrle;
extern vd_functions_t mpcodecs_vd_qtsmc;
extern vd_functions_t mpcodecs_vd_roqvideo;
extern vd_functions_t mpcodecs_vd_cyuv;
extern vd_functions_t mpcodecs_vd_nuv;
extern vd_functions_t mpcodecs_vd_mpng;
extern vd_functions_t mpcodecs_vd_ijpg;
extern vd_functions_t mpcodecs_vd_libmpeg2;

vd_functions_t* mpcodecs_vd_drivers[] = {
        &mpcodecs_vd_null,
        &mpcodecs_vd_cinepak,
        &mpcodecs_vd_qtrpza,
#ifdef USE_LIBAVCODEC
        &mpcodecs_vd_ffmpeg,
#endif
#ifdef USE_WIN32DLL
#ifdef USE_DIRECTSHOW
        &mpcodecs_vd_dshow,
#endif
        &mpcodecs_vd_vfw,
        &mpcodecs_vd_vfwex,
#endif
#ifdef USE_DIVX
        &mpcodecs_vd_odivx,
#ifdef NEW_DECORE
        &mpcodecs_vd_divx4,
#endif
#endif
        &mpcodecs_vd_raw,
        &mpcodecs_vd_msrle,
        &mpcodecs_vd_msvidc,
        &mpcodecs_vd_fli,
        &mpcodecs_vd_qtrle,
        &mpcodecs_vd_qtsmc,
        &mpcodecs_vd_roqvideo,
        &mpcodecs_vd_cyuv,
        &mpcodecs_vd_nuv,
#ifdef USE_XANIM
        &mpcodecs_vd_xanim,
#endif
#ifdef HAVE_PNG
        &mpcodecs_vd_mpng,
#endif
#ifdef HAVE_JPEG
	&mpcodecs_vd_ijpg,
#endif
        &mpcodecs_vd_libmpeg2,
	NULL
};

#include "libvo/video_out.h"
extern int vo_directrendering;

// libvo opts:
int fullscreen=0;
int vidmode=0;
int softzoom=0;
int flip=-1;
int opt_screen_size_x=0;
int opt_screen_size_y=0;
int screen_size_xy=0;
float movie_aspect=-1.0;
int vo_flags=0;

static vo_tune_info_t vtune;

static mp_image_t* static_images[2];
static mp_image_t* temp_images[1];
static mp_image_t* export_images[1];
static int static_idx=0;

extern vd_functions_t* mpvdec; // FIXME!

int mpcodecs_config_vo(sh_video_t *sh, int w, int h, unsigned int preferred_outfmt){
    int i,j;
    unsigned int out_fmt=0;
    int screen_size_x=0;//SCREEN_SIZE_X;
    int screen_size_y=0;//SCREEN_SIZE_Y;
    vo_functions_t* video_out=sh->video_out;

    mp_msg(MSGT_DECVIDEO,MSGL_INFO,"VDec: vo config request - %d x %d, %s  \n",
	w,h,vo_format_name(preferred_outfmt));

    if(!video_out) return 1; // temp hack

    // check if libvo and codec has common outfmt (no conversion):
    j=-1;
    for(i=0;i<CODECS_MAX_OUTFMT;i++){
	out_fmt=sh->codec->outfmt[i];
	if(out_fmt==(signed int)0xFFFFFFFF) continue;
	vo_flags=video_out->control(VOCTRL_QUERY_FORMAT, &out_fmt);
	mp_msg(MSGT_CPLAYER,MSGL_DBG2,"vo_debug: query(%s) returned 0x%X (i=%d) \n",vo_format_name(out_fmt),vo_flags,i);
	if((vo_flags&2) || (vo_flags && j<0)){
	    // check (query) if codec really support this outfmt...
	    if(mpvdec->control(sh,VDCTRL_QUERY_FORMAT,&out_fmt)==CONTROL_FALSE)
		continue;
	    j=i; if(vo_flags&2) break;
	}
    }
    if(j<0){
	// TODO: no match - we should use conversion...
	mp_msg(MSGT_CPLAYER,MSGL_FATAL,MSGTR_VOincompCodec);
	return 0;	// failed
    }
    out_fmt=sh->codec->outfmt[j];
    sh->outfmtidx=j;

    // autodetect flipping
    if(flip==-1){
	flip=0;
	if(sh->codec->outflags[j]&CODECS_FLAG_FLIP)
	    if(!(sh->codec->outflags[j]&CODECS_FLAG_NOFLIP))
		flip=1;
    }

    // time to do aspect ratio corrections...

  if(movie_aspect>-1.0) sh->aspect = movie_aspect; // cmdline overrides autodetect
//  if(!sh->aspect) sh->aspect=1.0;
  screen_size_x = opt_screen_size_x;
  screen_size_y = opt_screen_size_y;
  if(screen_size_xy||screen_size_x||screen_size_y){
   if(screen_size_xy>0){
     if(screen_size_xy<=8){
       screen_size_x=screen_size_xy*sh->disp_w;
       screen_size_y=screen_size_xy*sh->disp_h;
     } else {
       screen_size_x=screen_size_xy;
       screen_size_y=screen_size_xy*sh->disp_h/sh->disp_w;
     }
   } else if(!vidmode){
     if(!screen_size_x) screen_size_x=SCREEN_SIZE_X;
     if(!screen_size_y) screen_size_y=SCREEN_SIZE_Y;
     if(screen_size_x<=8) screen_size_x*=sh->disp_w;
     if(screen_size_y<=8) screen_size_y*=sh->disp_h;
   }
  } else {
    // check source format aspect, calculate prescale ::atmos
    screen_size_x=sh->disp_w;
    screen_size_y=sh->disp_h;
    if(sh->aspect>0.01){
      mp_msg(MSGT_CPLAYER,MSGL_INFO,"Movie-Aspect is %.2f:1 - prescaling to correct movie aspect.\n",
             sh->aspect);
      screen_size_x=(int)((float)sh->disp_h*sh->aspect);
      screen_size_x+=screen_size_x%2; // round
      if(screen_size_x<sh->disp_w){
        screen_size_x=sh->disp_w;
        screen_size_y=(int)((float)sh->disp_w*(1.0/sh->aspect));
        screen_size_y+=screen_size_y%2; // round
      }
    } else {
      mp_msg(MSGT_CPLAYER,MSGL_INFO,"Movie-Aspect is undefined - no prescaling applied.\n");
    }
  }

  if(video_out->get_info)
  { const vo_info_t *info = video_out->get_info();
    mp_msg(MSGT_CPLAYER,MSGL_INFO,"VO: [%s] %dx%d => %dx%d %s %s%s%s%s\n",info->short_name,
         sh->disp_w,sh->disp_h,
         screen_size_x,screen_size_y,
	 vo_format_name(out_fmt),
         fullscreen?"fs ":"",
         vidmode?"vm ":"",
         softzoom?"zoom ":"",
         (flip==1)?"flip ":"");
    mp_msg(MSGT_CPLAYER,MSGL_V,"VO: Description: %s\n",info->name);
    mp_msg(MSGT_CPLAYER,MSGL_V,"VO: Author: %s\n", info->author);
    if(info->comment && strlen(info->comment) > 0)
        mp_msg(MSGT_CPLAYER,MSGL_V,"VO: Comment: %s\n", info->comment);
  }

    // Time to config libvo!
    mp_msg(MSGT_CPLAYER,MSGL_V,"video_out->init(%dx%d->%dx%d,flags=%d,'%s',0x%X)\n",
                      sh->disp_w,sh->disp_h,
                      screen_size_x,screen_size_y,
                      fullscreen|(vidmode<<1)|(softzoom<<2)|(flip<<3),
                      "MPlayer",out_fmt);

    memset(&vtune,0,sizeof(vo_tune_info_t));
    if(video_out->config(sh->disp_w,sh->disp_h,
                      screen_size_x,screen_size_y,
                      fullscreen|(vidmode<<1)|(softzoom<<2)|(flip<<3),
                      "MPlayer",out_fmt,&vtune)){
	mp_msg(MSGT_CPLAYER,MSGL_FATAL,MSGTR_CannotInitVO);
	return 0; // exit_player(MSGTR_Exit_error);
    }

#define FREE_MPI(mpi) if(mpi){if(mpi->flags&MP_IMGFLAG_ALLOCATED) free(mpi->planes[0]); free(mpi); mpi=NULL;}
    FREE_MPI(static_images[0])
    FREE_MPI(static_images[1])
    FREE_MPI(temp_images[0])
    FREE_MPI(export_images[0])
#undef FREE_MPI
    return 1;
}

// mp_imgtype: buffering type, see mp_image.h
// mp_imgflag: buffer requirements (read/write, preserve, stride limits), see mp_image.h
// returns NULL or allocated mp_image_t*
// Note: buffer allocation may be moved to mpcodecs_config_vo() later...
mp_image_t* mpcodecs_get_image(sh_video_t *sh, int mp_imgtype, int mp_imgflag, int w, int h){
  mp_image_t* mpi=NULL;
  int w2=(mp_imgflag&MP_IMGFLAG_ACCEPT_STRIDE)?((w+15)&(~15)):w;
  // Note: we should call libvo first to check if it supports direct rendering
  // and if not, then fallback to software buffers:
  switch(mp_imgtype){
  case MP_IMGTYPE_EXPORT:
//    mpi=new_mp_image(w,h);
    if(!export_images[0]) export_images[0]=new_mp_image(w2,h);
    mpi=export_images[0];
    break;
  case MP_IMGTYPE_STATIC:
    if(!static_images[0]) static_images[0]=new_mp_image(w2,h);
    mpi=static_images[0];
    break;
  case MP_IMGTYPE_TEMP:
    if(!temp_images[0]) temp_images[0]=new_mp_image(w2,h);
    mpi=temp_images[0];
    break;
  case MP_IMGTYPE_IPB:
    if(!(mp_imgflag&MP_IMGFLAG_READABLE)){ // B frame:
      if(!temp_images[0]) temp_images[0]=new_mp_image(w2,h);
      mpi=temp_images[0];
      break;
    }
  case MP_IMGTYPE_IP:
    if(!static_images[static_idx]) static_images[static_idx]=new_mp_image(w2,h);
    mpi=static_images[static_idx];
    static_idx^=1;
    break;
  }
  if(mpi){
    mpi->type=mp_imgtype;
    mpi->flags&=~(MP_IMGFLAG_PRESERVE|MP_IMGFLAG_READABLE|MP_IMGFLAG_DIRECT);
    mpi->flags|=mp_imgflag&(MP_IMGFLAG_PRESERVE|MP_IMGFLAG_READABLE|MP_IMGFLAG_ACCEPT_STRIDE|MP_IMGFLAG_ACCEPT_WIDTH|MP_IMGFLAG_ALIGNED_STRIDE|MP_IMGFLAG_DRAW_CALLBACK);
    if((mpi->width!=w2 || mpi->height!=h) && !(mpi->flags&MP_IMGFLAG_DIRECT)){
	mpi->width=w2;
	mpi->height=h;
	if(mpi->flags&MP_IMGFLAG_ALLOCATED){
	    // need to re-allocate buffer memory:
	    free(mpi->planes[0]);
	    mpi->flags&=~MP_IMGFLAG_ALLOCATED;
	}
    }
    if(!mpi->bpp) mp_image_setfmt(mpi,sh->codec->outfmt[sh->outfmtidx]);
    if(!(mpi->flags&MP_IMGFLAG_ALLOCATED) && mpi->type>MP_IMGTYPE_EXPORT){

	// check libvo first!
	vo_functions_t* vo=sh->video_out;
	if(vo && vo_directrendering) vo->control(VOCTRL_GET_IMAGE,mpi);
	
        if(!(mpi->flags&MP_IMGFLAG_DIRECT)){
          // non-direct and not yet allocaed image. allocate it!
	  mpi->planes[0]=memalign(64, mpi->bpp*mpi->width*mpi->height/8);
	  if(mpi->flags&MP_IMGFLAG_PLANAR){
	      // YV12/I420. feel free to add other planar formats here...
	      if(!mpi->stride[0]) mpi->stride[0]=mpi->width;
	      if(!mpi->stride[1]) mpi->stride[1]=mpi->stride[2]=mpi->width/2;
	      mpi->planes[1]=mpi->planes[0]+mpi->width*mpi->height;
	      mpi->planes[2]=mpi->planes[1]+mpi->width*mpi->height/4;
	  } else {
	      if(!mpi->stride[0]) mpi->stride[0]=mpi->width*mpi->bpp/8;
	  }
	  mpi->flags|=MP_IMGFLAG_ALLOCATED;
        }
	if(!(mpi->flags&MP_IMGFLAG_TYPE_DISPLAYED)){
	    mp_msg(MSGT_DECVIDEO,MSGL_INFO,"*** %s mp_image_t, %dx%dx%dbpp %s %s, %d bytes\n",
	          (mpi->flags&MP_IMGFLAG_DIRECT)?"Direct Rendering":"Allocating",
	          mpi->width,mpi->height,mpi->bpp,
		  (mpi->flags&MP_IMGFLAG_YUV)?"YUV":"RGB",
		  (mpi->flags&MP_IMGFLAG_PLANAR)?"planar":"packed",
	          mpi->bpp*mpi->width*mpi->height/8);
	    mpi->flags|=MP_IMGFLAG_TYPE_DISPLAYED;
	}
	
    }
  }
  return mpi;
}

