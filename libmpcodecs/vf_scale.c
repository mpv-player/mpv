#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "../config.h"
#include "../mp_msg.h"

#include "../libvo/img_format.h"
#include "../mp_image.h"
#include "vf.h"

#include "../libvo/fastmemcpy.h"
#include "../postproc/swscale.h"

struct vf_priv_s {
    int w,h;
    unsigned int fmt;
    SwsContext *ctx;
};

//===========================================================================//

static unsigned int outfmt_list[]={
    IMGFMT_BGR32,
    IMGFMT_BGR24,
    IMGFMT_BGR16,
    IMGFMT_BGR15,
    IMGFMT_YV12,
    IMGFMT_I420,
    IMGFMT_IYUV,
    NULL
};

static int config(struct vf_instance_s* vf,
        int width, int height, int d_width, int d_height,
	unsigned int flags, unsigned int outfmt){
    unsigned int* p=outfmt_list;
    unsigned int best=0;
    // find the best outfmt:
    while(*p){
	int ret=vf_next_query_format(vf,*p);
	mp_msg(MSGT_VFILTER,MSGL_V,"scale: query(%s) -> %d\n",vo_format_name(*p),ret&3);
	if(ret&2){ best=*p; break;} // no conversion -> bingo!
	if(ret&1 && !best) best=*p; // best with conversion
	++p;
    }
    if(!best){
	printf("SwScale: no supported outfmt found :(\n");
	return 0;
    }

    // calculate the missing parameters:
    if(vf->priv->w<=0) vf->priv->w=width;
    if(vf->priv->h<=0) vf->priv->h=height;
    
    printf("SwScale scaling %dx%d %s to %dx%d %s  \n",
	width,height,vo_format_name(outfmt),
	vf->priv->w,vf->priv->h,vo_format_name(best));
    
    // new swscaler:
    vf->priv->ctx=getSwsContextFromCmdLine(width,height,outfmt,
		  vf->priv->w,vf->priv->h,best);
    if(!vf->priv->ctx){
	// error...
	printf("Couldn't init SwScaler for this setup\n");
	return 0;
    }
    vf->priv->fmt=best;
    return vf_next_config(vf,vf->priv->w,vf->priv->h,d_width,d_height,flags,outfmt);
}

static void put_image(struct vf_instance_s* vf, mp_image_t *mpi){
    mp_image_t *dmpi;

    // hope we'll get DR buffer:
    dmpi=vf_get_image(vf->next,vf->priv->fmt,
	MP_IMGTYPE_TEMP, MP_IMGFLAG_ACCEPT_STRIDE,
	vf->priv->w, vf->priv->h);
    
    vf->priv->ctx->swScale(vf->priv->ctx,mpi->planes,mpi->stride,0,mpi->h,dmpi->planes,dmpi->stride);
    
    vf_next_put_image(vf,dmpi);
}

//===========================================================================//

//  supported Input formats: YV12, I420, IYUV, YUY2, BGR32, BGR24, BGR16, BGR15, RGB32, RGB24, Y8, Y800

static int query_format(struct vf_instance_s* vf, unsigned int fmt){
    switch(fmt){
    case IMGFMT_YV12:
    case IMGFMT_I420:
    case IMGFMT_IYUV:
    case IMGFMT_YUY2:
    case IMGFMT_BGR32:
    case IMGFMT_BGR24:
    case IMGFMT_BGR16:
    case IMGFMT_BGR15:
    case IMGFMT_RGB32:
    case IMGFMT_RGB24:
    case IMGFMT_Y800:
	return 3; //vf_next_query_format(vf,fmt);
    }
    return 0;
}

static int open(vf_instance_t *vf, char* args){
    vf->config=config;
    vf->put_image=put_image;
    vf->priv=malloc(sizeof(struct vf_priv_s));
    // TODO: parse args ->
    vf->priv->ctx=NULL;
    vf->priv->w=
    vf->priv->h=-1;
    if(args) sscanf(args, "%d:%d",
    &vf->priv->w,
    &vf->priv->h);
    printf("SwScale: %d x %d\n",
    vf->priv->w,
    vf->priv->h);
    return 1;
}

vf_info_t vf_info_scale = {
    "software scaling",
    "scale",
    "A'rpi",
    "",
    open
};

//===========================================================================//
