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

vd_functions_t* mpcodecs_vd_drivers[] = {
        &mpcodecs_vd_null,
        &mpcodecs_vd_cinepak,
	NULL
};

int mpcodecs_config_vo(sh_video_t *sh, int w, int h, unsigned int preferred_outfmt){

    return 1;
}

static mp_image_t* static_images[2];
static mp_image_t* temp_images[1];
static int static_idx=0;

// mp_imgtype: buffering type, see mp_image.h
// mp_imgflag: buffer requirements (read/write, preserve, stride limits), see mp_image.h
// returns NULL or allocated mp_image_t*
// Note: buffer allocation may be moved to mpcodecs_config_vo() later...
mp_image_t* mpcodecs_get_image(sh_video_t *sh, int mp_imgtype, int mp_imgflag, int w, int h){
  mp_image_t* mpi=NULL;
  // Note: we should call libvo first to check if it supports direct rendering
  // and if not, then fallback to software buffers:
  switch(mp_imgtype){
  case MP_IMGTYPE_EXPORT:
    mpi=new_mp_image(w,h);
    break;
  case MP_IMGTYPE_STATIC:
    if(!static_images[0]) static_images[0]=new_mp_image(w,h);
    mpi=static_images[0];
    break;
  case MP_IMGTYPE_TEMP:
    if(!temp_images[0]) temp_images[0]=new_mp_image(w,h);
    mpi=temp_images[0];
    break;
  case MP_IMGTYPE_IPB:
    if(!(mp_imgflag&MP_IMGFLAG_READABLE)){ // B frame:
      if(!temp_images[0]) temp_images[0]=new_mp_image(w,h);
      mpi=temp_images[0];
      break;
    }
  case MP_IMGTYPE_IP:
    if(!static_images[static_idx]) static_images[static_idx]=new_mp_image(w,h);
    mpi=static_images[static_idx];
    static_idx^=1;
    break;
  }
  if(mpi){
    mpi->type=mp_imgtype;
    mpi->flags&=~(MP_IMGFLAG_PRESERVE|MP_IMGFLAG_READABLE);
    mpi->flags|=mp_imgflag&(MP_IMGFLAG_PRESERVE|MP_IMGFLAG_READABLE);
    if(!mpi->bpp){
      mp_image_setfmt(mpi,sh->codec->outfmt[sh->outfmtidx]);
      if(!(mpi->flags&(MP_IMGFLAG_ALLOCATED|MP_IMGFLAG_DIRECT)) 
         && mpi->type>MP_IMGTYPE_EXPORT){
          // non-direct and not yet allocaed image. allocate it!
	  printf("*** Allocating mp_image_t, %d bytes\n",mpi->bpp*mpi->width*mpi->height/8);
	  mpi->planes[0]=memalign(64, mpi->bpp*mpi->width*mpi->height/8);
	  if(!mpi->stride[0]) mpi->stride[0]=mpi->width;
	  if(mpi->flags&MP_IMGFLAG_PLANAR){
	      // YV12/I420. feel free to add other planar formats here...
	      if(!mpi->stride[1]) mpi->stride[1]=mpi->stride[2]=mpi->width/2;
	      mpi->planes[1]=mpi->planes[0]+mpi->width*mpi->height;
	      mpi->planes[2]=mpi->planes[1]+mpi->width*mpi->height/4;
	  }
	  mpi->flags|=MP_IMGFLAG_ALLOCATED;
      }
    }
  }
  return mpi;
}

