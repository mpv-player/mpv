#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "../config.h"
#include "../mp_msg.h"

#include "../mp_image.h"
#include "vf.h"

#include "../libvo/fastmemcpy.h"
#include "../postproc/swscale.h"

struct vf_priv_s {
    int w,h;
    SwsContext *ctx;
};

//===========================================================================//

static int config(struct vf_instance_s* vf,
        int width, int height, int d_width, int d_height,
	unsigned int flags, unsigned int outfmt){
    // calculate the missing parameters:
    if(vf->priv->w<=0) vf->priv->w=width;
    if(vf->priv->h<=0) vf->priv->h=height;
    // new swscaler:
    vf->priv->ctx=getSwsContextFromCmdLine(width,height,outfmt,
		  vf->priv->w,vf->priv->h,outfmt);
    if(!vf->priv->ctx){
	// error...
	printf("Couldn't init SwScaler for this setup\n");
	return 0;
    }
    return vf_next_config(vf,vf->priv->w,vf->priv->h,d_width,d_height,flags,outfmt);
}

static void put_image(struct vf_instance_s* vf, mp_image_t *mpi){
    mp_image_t *dmpi;

    // hope we'll get DR buffer:
    dmpi=vf_get_image(vf->next,mpi->imgfmt,
	MP_IMGTYPE_TEMP, MP_IMGFLAG_ACCEPT_STRIDE,
	vf->priv->w, vf->priv->h);
    
    vf->priv->ctx->swScale(vf->priv->ctx,mpi->planes,mpi->stride,0,mpi->h,dmpi->planes,dmpi->stride);
    
    vf_next_put_image(vf,dmpi);
}

//===========================================================================//

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
