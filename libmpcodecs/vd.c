#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "mp_msg.h"

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
extern vd_functions_t mpcodecs_vd_rle;
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
        &mpcodecs_vd_rle,
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
extern int vaa_use_dr;

int mpcodecs_config_vo(sh_video_t *sh, int w, int h, unsigned int preferred_outfmt){
    mp_msg(MSGT_DECVIDEO,MSGL_INFO,"VDec: vo config request - %d x %d, %s  \n",
	w,h,vo_format_name(preferred_outfmt));
    return 1;
}

static mp_image_t* static_images[2];
static mp_image_t* temp_images[1];
static mp_image_t* export_images[1];
static int static_idx=0;

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
	if(vo && vaa_use_dr) vo->control(VOCTRL_GET_IMAGE,mpi);
	
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

