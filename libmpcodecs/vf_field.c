#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "mp_msg.h"

#include "mp_image.h"
#include "vf.h"

#include "libvo/fastmemcpy.h"

struct vf_priv_s {
    int field;
};

//===========================================================================//

static int config(struct vf_instance_s* vf,
        int width, int height, int d_width, int d_height,
	unsigned int flags, unsigned int outfmt){
    return vf_next_config(vf,width,height/2,d_width,d_height,flags,outfmt);
}

static int put_image(struct vf_instance_s* vf, mp_image_t *mpi, double pts){
    vf->dmpi=vf_get_image(vf->next,mpi->imgfmt,
	MP_IMGTYPE_EXPORT, MP_IMGFLAG_ACCEPT_STRIDE,
	mpi->width, mpi->height/2);
    
    // set up mpi as a double-stride image of dmpi:
    vf->dmpi->planes[0]=mpi->planes[0]+mpi->stride[0]*vf->priv->field;
    vf->dmpi->stride[0]=2*mpi->stride[0];
    if(vf->dmpi->flags&MP_IMGFLAG_PLANAR){
        vf->dmpi->planes[1]=mpi->planes[1]+
	    mpi->stride[1]*vf->priv->field;
	vf->dmpi->stride[1]=2*mpi->stride[1];
        vf->dmpi->planes[2]=mpi->planes[2]+
	    mpi->stride[2]*vf->priv->field;
	vf->dmpi->stride[2]=2*mpi->stride[2];
    } else
	vf->dmpi->planes[1]=mpi->planes[1]; // passthru bgr8 palette!!!
    
    return vf_next_put_image(vf,vf->dmpi, pts);
}

//===========================================================================//

static void uninit(struct vf_instance_s* vf)
{
	free(vf->priv);
}

static int open(vf_instance_t *vf, char* args){
    vf->config=config;
    vf->put_image=put_image;
    vf->uninit=uninit;
    vf->default_reqs=VFCAP_ACCEPT_STRIDE;
    vf->priv=calloc(1, sizeof(struct vf_priv_s));
    if (args) sscanf(args, "%d", &vf->priv->field);
    vf->priv->field &= 1;
    return 1;
}

vf_info_t vf_info_field = {
    "extract single field",
    "field",
    "Rich Felker",
    "",
    open,
    NULL
};

//===========================================================================//
