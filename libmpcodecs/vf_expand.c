#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../config.h"
#include "../mp_msg.h"

#include "../mp_image.h"
#include "vf.h"

#include "../libvo/fastmemcpy.h"

struct vf_priv_s {
    int exp_w,exp_h;
    int exp_x,exp_y;
    mp_image_t *dmpi;
};

//===========================================================================//

static int config(struct vf_instance_s* vf,
        int width, int height, int d_width, int d_height,
	unsigned int flags, unsigned int outfmt){
    int ret;
    // calculate the missing parameters:
    if(vf->priv->exp_w<width) vf->priv->exp_w=width;
    if(vf->priv->exp_h<height) vf->priv->exp_h=height;
    if(vf->priv->exp_x<0) vf->priv->exp_x=(vf->priv->exp_w-width)/2;
    if(vf->priv->exp_y<0) vf->priv->exp_y=(vf->priv->exp_h-height)/2;
    // check:
//    if(vf->priv->exp_w+vf->priv->exp_x>width) return 0; // bad width
//    if(vf->priv->exp_h+vf->priv->exp_y>height) return 0; // bad height
    ret=vf_next_config(vf,vf->priv->exp_w,vf->priv->exp_h,d_width,d_height,flags,outfmt);
    return ret;
}

// there are 4 cases:
// codec --DR--> expand --DR--> vo
// codec --DR--> expand -copy-> vo
// codec -copy-> expand --DR--> vo
// codec -copy-> expand -copy-> vo (worst case)

static void get_image(struct vf_instance_s* vf, mp_image_t *mpi){
    if(vf->priv->exp_w==mpi->width ||
       (mpi->flags&(MP_IMGFLAG_ACCEPT_STRIDE|MP_IMGFLAG_ACCEPT_WIDTH)) ){
	// try full DR !
	vf->priv->dmpi=vf_get_image(vf->next,mpi->imgfmt,
	    mpi->type, mpi->flags, vf->priv->exp_w, vf->priv->exp_h);
	// set up mpi as a cropped-down image of dmpi:
	if(mpi->flags&MP_IMGFLAG_PLANAR){
	    mpi->planes[0]=vf->priv->dmpi->planes[0]+
		vf->priv->exp_y*vf->priv->dmpi->stride[0]+vf->priv->exp_x;
	    mpi->planes[1]=vf->priv->dmpi->planes[1]+
		(vf->priv->exp_y>>1)*vf->priv->dmpi->stride[1]+(vf->priv->exp_x>>1);
	    mpi->planes[2]=vf->priv->dmpi->planes[2]+
		(vf->priv->exp_y>>1)*vf->priv->dmpi->stride[2]+(vf->priv->exp_x>>1);
	    mpi->stride[1]=vf->priv->dmpi->stride[1];
	    mpi->stride[2]=vf->priv->dmpi->stride[2];
	} else {
	    mpi->planes[0]=vf->priv->dmpi->planes[0]+
		vf->priv->exp_y*vf->priv->dmpi->stride[0]+
		vf->priv->exp_x*(vf->priv->dmpi->bpp/8);
	}
	mpi->stride[0]=vf->priv->dmpi->stride[0];
	mpi->width=vf->priv->dmpi->width;
	mpi->flags|=MP_IMGFLAG_DIRECT;
    }
}

static void put_image(struct vf_instance_s* vf, mp_image_t *mpi){
    if(mpi->flags&MP_IMGFLAG_DIRECT){
	vf_next_put_image(vf,vf->priv->dmpi);
	return; // we've used DR, so we're ready...
    }

    // hope we'll get DR buffer:
    vf->priv->dmpi=vf_get_image(vf->next,mpi->imgfmt,
	MP_IMGTYPE_TEMP, MP_IMGFLAG_ACCEPT_STRIDE,
	vf->priv->exp_w, vf->priv->exp_h);
    
    // copy mpi->dmpi...
    if(mpi->flags&MP_IMGFLAG_PLANAR){
	memcpy_pic(vf->priv->dmpi->planes[0]+
	        vf->priv->exp_y*vf->priv->dmpi->stride[0]+vf->priv->exp_x,
		mpi->planes[0], mpi->w, mpi->h,
		vf->priv->dmpi->stride[0],mpi->stride[0]);
	memcpy_pic(vf->priv->dmpi->planes[1]+
		(vf->priv->exp_y>>1)*vf->priv->dmpi->stride[1]+(vf->priv->exp_x>>1),
		mpi->planes[1], mpi->w>>1, mpi->h>>1,
		vf->priv->dmpi->stride[1],mpi->stride[1]);
	memcpy_pic(vf->priv->dmpi->planes[2]+
		(vf->priv->exp_y>>1)*vf->priv->dmpi->stride[2]+(vf->priv->exp_x>>1),
		mpi->planes[2], mpi->w>>1, mpi->h>>1,
		vf->priv->dmpi->stride[2],mpi->stride[2]);
    } else {
	memcpy_pic(vf->priv->dmpi->planes[0]+
	        vf->priv->exp_y*vf->priv->dmpi->stride[0]+vf->priv->exp_x*(vf->priv->dmpi->bpp/8),
		mpi->planes[0], mpi->w*(vf->priv->dmpi->bpp/8), mpi->h,
		vf->priv->dmpi->stride[0],mpi->stride[0]);
    }
    vf_next_put_image(vf,vf->priv->dmpi);
}

//===========================================================================//

static int open(vf_instance_t *vf, char* args){
    vf->config=config;
    vf->get_image=get_image;
    vf->put_image=put_image;
    vf->priv=malloc(sizeof(struct vf_priv_s));
    // TODO: parse args ->
    vf->priv->exp_x=
    vf->priv->exp_y=
    vf->priv->exp_w=
    vf->priv->exp_h=-1;
    if(args) sscanf(args, "%d:%d:%d:%d", 
    &vf->priv->exp_w,
    &vf->priv->exp_h,
    &vf->priv->exp_x,
    &vf->priv->exp_y);
    printf("Expand: %d x %d, %d ; %d\n",
    vf->priv->exp_w,
    vf->priv->exp_h,
    vf->priv->exp_x,
    vf->priv->exp_y);
    return 1;
}

vf_info_t vf_info_expand = {
    "expanding",
    "expand",
    "A'rpi",
    "",
    open
};

//===========================================================================//
