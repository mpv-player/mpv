#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "../config.h"
#include "../mp_msg.h"

#include "img_format.h"
#include "mp_image.h"
#include "vf.h"

#include "../libvo/fastmemcpy.h"
#include "../postproc/rgb2rgb.h"


static void mirror(unsigned char* dst,unsigned char* src,int dststride,int srcstride,int w,int h,int bpp){
    int x;

    // 
    for(x=0;x<h;x++){
	int y;
	switch(bpp){
	case 1:
	    for(y=0;y<w;y++) dst[y]=src[w-y-1+x*srcstride];
	    break;
	case 2:
	    for(y=0;y<w;y++) *((short*)(dst+y*2))=*((short*)(src+w-y*2-1+x*srcstride));
	    break;
	case 3:
	    for(y=0;y<w;y++){
		dst[y*3+0]=src[0+w-y*3-1+x*srcstride];
		dst[y*3+1]=src[1+w-y*3-1+x*srcstride];
		dst[y*3+2]=src[2+w-y*3-1+x*srcstride];
	    }
	    break;
	case 4:
	    for(y=0;y<w;y++) *((int*)(dst+y*4))=*((int*)(src+w-y*4-1+x*srcstride));
	}
	dst+=dststride;
    }

    //

}

//===========================================================================//

static void put_image(struct vf_instance_s* vf, mp_image_t *mpi){
    mp_image_t *dmpi;

    // hope we'll get DR buffer:
    dmpi=vf_get_image(vf->next,mpi->imgfmt,
	MP_IMGTYPE_TEMP, MP_IMGFLAG_ACCEPT_STRIDE,
	mpi->w, mpi->h);
	//mpi->h, mpi->w);

    if(mpi->flags&MP_IMGFLAG_PLANAR){
	       mirror(dmpi->planes[0],mpi->planes[0],
	       dmpi->stride[0],mpi->stride[0],
	       dmpi->w,dmpi->h,1);
	       mirror(dmpi->planes[1],mpi->planes[1],
	       dmpi->stride[1],mpi->stride[1],
	       dmpi->w>>1,dmpi->h>>1,1);
	       mirror(dmpi->planes[2],mpi->planes[2],
	       dmpi->stride[2],mpi->stride[2],
	       dmpi->w>>1,dmpi->h>>1,1);
    } else {
	mirror(dmpi->planes[0],mpi->planes[0],
	       dmpi->stride[0],mpi->stride[0],
	       dmpi->w,dmpi->h,dmpi->bpp>>3);	
    }
    
    vf_next_put_image(vf,dmpi);
}

//===========================================================================//

static int open(vf_instance_t *vf, char* args){
    //vf->config=config;
    vf->put_image=put_image;
    return 1;
}

vf_info_t vf_info_mirror = {
    "mirror",
    "mirror",
    "Eyck",
    "",
    open
};

//===========================================================================//
