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

//===========================================================================//

static int config(struct vf_instance_s* vf,
        int width, int height, int d_width, int d_height,
	unsigned int flags, unsigned int outfmt){
    
    if(vf_next_query_format(vf,IMGFMT_YUY2)<=0){
	printf("yuy2 not supported by next filter/vo :(\n");
	return 0;
    }
    
    return vf_next_config(vf,width,height,d_width,d_height,flags,IMGFMT_YUY2);
}

static int put_image(struct vf_instance_s* vf, mp_image_t *mpi){
    mp_image_t *dmpi;

    // hope we'll get DR buffer:
    dmpi=vf_get_image(vf->next,IMGFMT_YUY2,
	MP_IMGTYPE_TEMP, MP_IMGFLAG_ACCEPT_STRIDE,
	mpi->w, mpi->h);

    if(mpi->imgfmt==IMGFMT_422P)
    yuv422ptoyuy2(mpi->planes[0],mpi->planes[1],mpi->planes[2], dmpi->planes[0],
	    mpi->w,mpi->h, mpi->stride[0],mpi->stride[1],dmpi->stride[0]);
    else
    yv12toyuy2(mpi->planes[0],mpi->planes[1],mpi->planes[2], dmpi->planes[0],
	    mpi->w,mpi->h, mpi->stride[0],mpi->stride[1],dmpi->stride[0]);
    
    dmpi->qscale=mpi->qscale;
    dmpi->qstride=mpi->qstride;
    
    return vf_next_put_image(vf,dmpi);
}

//===========================================================================//

static int query_format(struct vf_instance_s* vf, unsigned int fmt){
    switch(fmt){
    case IMGFMT_YV12:
    case IMGFMT_I420:
    case IMGFMT_IYUV:
    case IMGFMT_422P:
	return vf_next_query_format(vf,IMGFMT_YUY2) & (~VFCAP_CSP_SUPPORTED_BY_HW);
    }
    return 0;
}

static int open(vf_instance_t *vf, char* args){
    vf->config=config;
    vf->put_image=put_image;
    vf->query_format=query_format;
    return 1;
}

vf_info_t vf_info_yuy2 = {
    "fast YV12/Y422p -> YUY2 conversion",
    "yuy2",
    "A'rpi",
    "",
    open
};

//===========================================================================//
