#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../config.h"
#include "../mp_msg.h"

#include "mp_image.h"
#include "vf.h"

#include "../libvo/fastmemcpy.h"

struct vf_priv_s {
    mp_image_t *dmpi;
};

//===========================================================================//

static void get_image(struct vf_instance_s* vf, mp_image_t *mpi){
    if(mpi->flags&MP_IMGFLAG_ACCEPT_STRIDE){
	// try full DR !
	vf->priv->dmpi=vf_get_image(vf->next,mpi->imgfmt,
	    mpi->type, mpi->flags, mpi->width, mpi->height);
	// set up mpi as a upside-down image of dmpi:
	mpi->planes[0]=vf->priv->dmpi->planes[0]+
		    vf->priv->dmpi->stride[0]*(vf->priv->dmpi->height-1);
	mpi->stride[0]=-vf->priv->dmpi->stride[0];
	if(mpi->flags&MP_IMGFLAG_PLANAR){
	    mpi->planes[1]=vf->priv->dmpi->planes[1]+
		    vf->priv->dmpi->stride[1]*((vf->priv->dmpi->height>>1)-1);
	    mpi->stride[1]=-vf->priv->dmpi->stride[1];
	    mpi->planes[2]=vf->priv->dmpi->planes[2]+
		    vf->priv->dmpi->stride[2]*((vf->priv->dmpi->height>>1)-1);
	    mpi->stride[2]=-vf->priv->dmpi->stride[2];
	}
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
	mpi->width, mpi->height);
    
    // set up mpi as a upside-down image of dmpi:
    vf->priv->dmpi->planes[0]=mpi->planes[0]+
		    mpi->stride[0]*(mpi->height-1);
    vf->priv->dmpi->stride[0]=-mpi->stride[0];
    if(vf->priv->dmpi->flags&MP_IMGFLAG_PLANAR){
        vf->priv->dmpi->planes[1]=mpi->planes[1]+
	    mpi->stride[1]*((mpi->height>>1)-1);
	vf->priv->dmpi->stride[1]=-mpi->stride[1];
	vf->priv->dmpi->planes[2]=mpi->planes[2]+
	    mpi->stride[2]*((mpi->height>>1)-1);
	vf->priv->dmpi->stride[2]=-mpi->stride[2];
    }
    
    vf_next_put_image(vf,vf->priv->dmpi);
}

//===========================================================================//

static int open(vf_instance_t *vf, char* args){
    vf->get_image=get_image;
    vf->put_image=put_image;
    vf->default_reqs=VFCAP_ACCEPT_STRIDE;
    vf->priv=malloc(sizeof(struct vf_priv_s));
    return 1;
}

vf_info_t vf_info_flip = {
    "flip image upside-down",
    "flip",
    "A'rpi",
    "",
    open
};

//===========================================================================//
