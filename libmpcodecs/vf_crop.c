#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../config.h"
#include "../mp_msg.h"

#include "../mp_image.h"
#include "vf.h"

struct vf_priv_s {
    int crop_w,crop_h;
    int crop_x,crop_y;
};

//===========================================================================//

static int config(struct vf_instance_s* vf,
        int width, int height, int d_width, int d_height,
	unsigned int flags, unsigned int outfmt){
    int ret;
    printf("crop->config() called\n");
    // calculate the missing parameters:
    if(vf->priv->crop_w<=0 || vf->priv->crop_w>width) vf->priv->crop_w=width;
    if(vf->priv->crop_h<=0 || vf->priv->crop_h>height) vf->priv->crop_h=height;
    if(vf->priv->crop_x<0) vf->priv->crop_x=(width-vf->priv->crop_w)/2;
    if(vf->priv->crop_y<0) vf->priv->crop_y=(height-vf->priv->crop_h)/2;
    // check:
    if(vf->priv->crop_w+vf->priv->crop_x>width) return 0; // bad width
    if(vf->priv->crop_h+vf->priv->crop_y>height) return 0; // bad height
    ret=vf_next_config(vf,vf->priv->crop_w,vf->priv->crop_h,d_width,d_height,flags,outfmt);
    printf("crop->config() return %d\n",ret);
    return ret;
}

static void put_image(struct vf_instance_s* vf, mp_image_t *mpi){
    mp_image_t *dmpi=vf_get_image(vf->next,mpi->imgfmt,
	MP_IMGTYPE_EXPORT, 0,
	vf->priv->crop_w, vf->priv->crop_h);
    if(mpi->flags&MP_IMGFLAG_PLANAR){
	dmpi->planes[0]=mpi->planes[0]+
	    vf->priv->crop_y*mpi->stride[0]+vf->priv->crop_x;
	dmpi->planes[1]=mpi->planes[1]+
	    (vf->priv->crop_y>>1)*mpi->stride[1]+(vf->priv->crop_x>>1);
	dmpi->planes[2]=mpi->planes[2]+
	    (vf->priv->crop_y>>1)*mpi->stride[2]+(vf->priv->crop_x>>1);
	dmpi->stride[0]=mpi->stride[0];
	dmpi->stride[1]=mpi->stride[1];
	dmpi->stride[2]=mpi->stride[2];
    } else {
	dmpi->planes[0]=mpi->planes[0]+
	    vf->priv->crop_y*mpi->stride[0]+
	    vf->priv->crop_x*(mpi->bpp/8);
	dmpi->stride[0]=mpi->stride[0];
    }
    vf_next_put_image(vf,dmpi);
}

//===========================================================================//

static int open(vf_instance_t *vf, char* args){
    vf->config=config;
    vf->put_image=put_image;
    vf->default_reqs=VFCAP_ACCEPT_STRIDE;
    vf->priv=malloc(sizeof(struct vf_priv_s));
    // TODO: parse args ->
    vf->priv->crop_x=
    vf->priv->crop_y=
    vf->priv->crop_w=
    vf->priv->crop_h=-1;
    if(args) sscanf(args, "%d:%d:%d:%d", 
    &vf->priv->crop_w,
    &vf->priv->crop_h,
    &vf->priv->crop_x,
    &vf->priv->crop_y);
    printf("Crop: %d x %d, %d ; %d\n",
    vf->priv->crop_w,
    vf->priv->crop_h,
    vf->priv->crop_x,
    vf->priv->crop_y);
    return 1;
}

vf_info_t vf_info_crop = {
    "cropping",
    "crop",
    "A'rpi",
    "",
    open
};

//===========================================================================//
