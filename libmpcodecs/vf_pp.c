#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../config.h"
#include "../mp_msg.h"

#include "img_format.h"
#include "mp_image.h"
#include "vf.h"

#include "../postproc/postprocess.h"

struct vf_priv_s {
    unsigned int pp;
    mp_image_t *dmpi;
    unsigned int outfmt;
};

//===========================================================================//

static int config(struct vf_instance_s* vf,
        int width, int height, int d_width, int d_height,
	unsigned int voflags, unsigned int outfmt){
    return vf_next_config(vf,width,height,d_width,d_height,voflags,vf->priv->outfmt);
}

static int query_format(struct vf_instance_s* vf, unsigned int fmt){
    switch(fmt){
    case IMGFMT_YV12:
    case IMGFMT_I420:
    case IMGFMT_IYUV:
	return vf_next_query_format(vf,vf->priv->outfmt);
    }
    return 0;
}

static int control(struct vf_instance_s* vf, int request, void* data){
    switch(request){
    case VFCTRL_QUERY_MAX_PP_LEVEL:
	return GET_PP_QUALITY_MAX;
    case VFCTRL_SET_PP_LEVEL:
	vf->priv->pp=getPpModeForQuality(*((unsigned int*)data));
	return CONTROL_TRUE;
    }
    return vf_next_control(vf,request,data);
}

static void get_image(struct vf_instance_s* vf, mp_image_t *mpi){
    if(vf->priv->pp&0xFFFF) return; // non-local filters enabled
    if((mpi->type==MP_IMGTYPE_IPB || vf->priv->pp) && 
	mpi->flags&MP_IMGFLAG_PRESERVE) return; // don't change
    if(!(mpi->flags&MP_IMGFLAG_ACCEPT_STRIDE) && mpi->imgfmt!=vf->priv->outfmt)
	return; // colorspace differ
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
	vf->priv->dmpi=vf_get_image(vf->next,vf->priv->outfmt,
	    MP_IMGTYPE_TEMP, MP_IMGFLAG_ACCEPT_STRIDE|MP_IMGFLAG_PREFER_ALIGNED_STRIDE,
//	    MP_IMGTYPE_TEMP, MP_IMGFLAG_ACCEPT_STRIDE,
//	    mpi->w,mpi->h);
	    (mpi->w+7)&(~7),(mpi->h+7)&(~7));
	vf->priv->dmpi->w=mpi->w; vf->priv->dmpi->h=mpi->h; // display w;h
    }
    
    if(vf->priv->pp || !(mpi->flags&MP_IMGFLAG_DIRECT)){
	// do the postprocessing! (or copy if no DR)
	postprocess(mpi->planes,mpi->stride[0],
		    vf->priv->dmpi->planes,vf->priv->dmpi->stride[0],
		    (mpi->w+7)&(~7),mpi->h,
		    mpi->qscale, mpi->qstride,
		    vf->priv->pp);
    }
    
    vf_next_put_image(vf,vf->priv->dmpi);
}

//===========================================================================//

extern int divx_quality;

static unsigned int fmt_list[]={
    IMGFMT_YV12,
    IMGFMT_I420,
    IMGFMT_IYUV,
    0
};

static int open(vf_instance_t *vf, char* args){
    char *endptr;
    vf->query_format=query_format;
    vf->control=control;
    vf->config=config;
    vf->get_image=get_image;
    vf->put_image=put_image;
    vf->default_caps=VFCAP_ACCEPT_STRIDE|VFCAP_POSTPROC;
    vf->priv=malloc(sizeof(struct vf_priv_s));

    // check csp:
    vf->priv->outfmt=vf_match_csp(&vf->next,fmt_list,IMGFMT_YV12);
    if(!vf->priv->outfmt) return 0; // no csp match :(
    
    if(args){
	vf->priv->pp=strtol(args, &endptr, 0);
	if(!(*endptr)) return 1;
    }
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
