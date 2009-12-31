
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if HAVE_MALLOC_H
#include <malloc.h>
#endif

#include "libmpcodecs/img_format.h"
#include "libmpcodecs/mp_image.h"

#include "libvo/fastmemcpy.h"

void mp_image_alloc_planes(mp_image_t *mpi) {
  // IF09 - allocate space for 4. plane delta info - unused
  if (mpi->imgfmt == IMGFMT_IF09) {
    mpi->planes[0]=memalign(64, mpi->bpp*mpi->width*(mpi->height+2)/8+
                            mpi->chroma_width*mpi->chroma_height);
    /* export delta table */
    mpi->planes[3]=mpi->planes[0]+(mpi->width*mpi->height)+2*(mpi->chroma_width*mpi->chroma_height);
  } else
    mpi->planes[0]=memalign(64, mpi->bpp*mpi->width*(mpi->height+2)/8);
  if (mpi->flags&MP_IMGFLAG_PLANAR) {
    int bpp = IMGFMT_IS_YUVP16(mpi->imgfmt)? 2 : 1;
    // YV12/I420/YVU9/IF09. feel free to add other planar formats here...
    mpi->stride[0]=bpp*mpi->width;
    if(mpi->num_planes > 2){
      mpi->stride[1]=mpi->stride[2]=bpp*mpi->chroma_width;
      if(mpi->flags&MP_IMGFLAG_SWAPPED){
        // I420/IYUV  (Y,U,V)
        mpi->planes[1]=mpi->planes[0]+mpi->stride[0]*mpi->height;
        mpi->planes[2]=mpi->planes[1]+mpi->stride[1]*mpi->chroma_height;
      } else {
        // YV12,YVU9,IF09  (Y,V,U)
        mpi->planes[2]=mpi->planes[0]+mpi->stride[0]*mpi->height;
        mpi->planes[1]=mpi->planes[2]+mpi->stride[1]*mpi->chroma_height;
      }
    } else {
      // NV12/NV21
      mpi->stride[1]=mpi->chroma_width;
      mpi->planes[1]=mpi->planes[0]+mpi->stride[0]*mpi->height;
    }
  } else {
    mpi->stride[0]=mpi->width*mpi->bpp/8;
    if (mpi->flags & MP_IMGFLAG_RGB_PALETTE)
      mpi->planes[1] = memalign(64, 1024);
  }
  mpi->flags|=MP_IMGFLAG_ALLOCATED;
}

mp_image_t* alloc_mpi(int w, int h, unsigned long int fmt) {
  mp_image_t* mpi = new_mp_image(w,h);

  mp_image_setfmt(mpi,fmt);
  mp_image_alloc_planes(mpi);

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
