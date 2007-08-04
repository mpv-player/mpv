
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif

#include "libmpcodecs/img_format.h"
#include "libmpcodecs/mp_image.h"

#include "libvo/fastmemcpy.h"

mp_image_t* alloc_mpi(int w, int h, unsigned long int fmt) {
  mp_image_t* mpi = new_mp_image(w,h);

  mp_image_setfmt(mpi,fmt);
  // IF09 - allocate space for 4. plane delta info - unused
  if (mpi->imgfmt == IMGFMT_IF09)
    {
      mpi->planes[0]=memalign(64, mpi->bpp*mpi->width*(mpi->height+2)/8+
			      mpi->chroma_width*mpi->chroma_height);
      /* delta table, just for fun ;) */
      mpi->planes[3]=mpi->planes[0]+2*(mpi->chroma_width*mpi->chroma_height);
    }
  else
    mpi->planes[0]=memalign(64, mpi->bpp*mpi->width*(mpi->height+2)/8);
  if(mpi->flags&MP_IMGFLAG_PLANAR){
    // YV12/I420/YVU9/IF09. feel free to add other planar formats here...
    if(!mpi->stride[0]) mpi->stride[0]=mpi->width;
    if(!mpi->stride[1]) mpi->stride[1]=mpi->stride[2]=mpi->chroma_width;
    if(mpi->flags&MP_IMGFLAG_SWAPPED){
      // I420/IYUV  (Y,U,V)
      mpi->planes[1]=mpi->planes[0]+mpi->width*mpi->height;
      mpi->planes[2]=mpi->planes[1]+mpi->chroma_width*mpi->chroma_height;
    } else {
      // YV12,YVU9,IF09  (Y,V,U)
      mpi->planes[2]=mpi->planes[0]+mpi->width*mpi->height;
      mpi->planes[1]=mpi->planes[2]+mpi->chroma_width*mpi->chroma_height;
    }
  } else {
    if(!mpi->stride[0]) mpi->stride[0]=mpi->width*mpi->bpp/8;
  }
  mpi->flags|=MP_IMGFLAG_ALLOCATED;
  
  return mpi;
}

void copy_mpi(mp_image_t *dmpi, mp_image_t *mpi) {
  if(mpi->flags&MP_IMGFLAG_PLANAR){
    memcpy_pic(dmpi->planes[0],mpi->planes[0], mpi->w, mpi->h,
	       dmpi->stride[0],mpi->stride[0]);
    memcpy_pic(dmpi->planes[1],mpi->planes[1], mpi->chroma_width, mpi->chroma_height,
	       dmpi->stride[1],mpi->stride[1]);
    memcpy_pic(dmpi->planes[2], mpi->planes[2], mpi->chroma_width, mpi->chroma_height,
	       dmpi->stride[2],mpi->stride[2]);
  } else {
    memcpy_pic(dmpi->planes[0],mpi->planes[0], 
	       mpi->w*(dmpi->bpp/8), mpi->h,
	       dmpi->stride[0],mpi->stride[0]);
  }
}
