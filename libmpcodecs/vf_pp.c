#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../config.h"
#include "../mp_msg.h"

#include "../mp_image.h"
#include "vf.h"

#include "../postproc/postprocess.h"

struct vf_priv_s {
    unsigned int pp;
    mp_image_t *dmpi;
};

//===========================================================================//

static void get_image(struct vf_instance_s* vf, mp_image_t *mpi){
    if(vf->priv->pp&0xFFFF) return; // non-local filters enabled
    if((mpi->type==MP_IMGTYPE_IPB || vf->priv->pp) && 
	mpi->flags&MP_IMGFLAG_PRESERVE) return; // don't change
    // ok, we can do pp in-place (or pp disabled):
    vf->priv->dmpi=vf_get_image(vf->next,mpi->imgfmt,
        mpi->type, mpi->flags, mpi->w, mpi->h);
    mpi->planes[0]=vf->priv->dmpi->planes[0];
    mpi->stride[0]=vf->priv->dmpi->stride[0];
    mpi->width=vf->priv->dmpi->width;
    if(mpi->flags&MP_IMGFLAG_PLANAR){
        mpi->planes[1]=vf->priv->dmpi->planes[1];
        mpi->planes[2]=vf->priv->dmpi->planes[2];
	mpi->stride[1]=vf->priv->dmpi->stride[1];
	mpi->stride[2]=vf->priv->dmpi->stride[2];
    }
    mpi->flags|=MP_IMGFLAG_DIRECT;
}

static void put_image(struct vf_instance_s* vf, mp_image_t *mpi){
    if(!(mpi->flags&MP_IMGFLAG_DIRECT)){
	// no DR, so get a new image! hope we'll get DR buffer:
	vf->priv->dmpi=vf_get_image(vf->next,mpi->imgfmt,
	    MP_IMGTYPE_TEMP, MP_IMGFLAG_ACCEPT_STRIDE|MP_IMGFLAG_ALIGNED_STRIDE,
	    mpi->w,mpi->h);
    }
    
    if(vf->priv->pp || !(mpi->flags&MP_IMGFLAG_DIRECT)){
	// do the postprocessing! (or copy if no DR)
	postprocess(mpi->planes,mpi->stride[0],
		    vf->priv->dmpi->planes,vf->priv->dmpi->stride[0],
		    mpi->w,mpi->h,
		    mpi->qscale, mpi->qstride,
		    vf->priv->pp);
    }
    
    vf_next_put_image(vf,vf->priv->dmpi);
}

//===========================================================================//

extern int divx_quality;

static int open(vf_instance_t *vf, char* args){
    vf->get_image=get_image;
    vf->put_image=put_image;
    vf->priv=malloc(sizeof(struct vf_priv_s));
    vf->priv->pp=divx_quality;
    return 1;
}

vf_info_t vf_info_pp = {
    "postprocessing",
    "pp",
    "A'rpi",
    "",
    open
};

//===========================================================================//
