#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "../config.h"
#include "../mp_msg.h"

#include "img_format.h"
#include "mp_image.h"
#include "vf.h"

#include "../postproc/postprocess.h"

struct vf_priv_s {
    int pp;
    PPMode ppMode[GET_PP_QUALITY_MAX+1];
    void *context;
    mp_image_t *dmpi;
    unsigned int outfmt;
};

//===========================================================================//

static int config(struct vf_instance_s* vf,
        int width, int height, int d_width, int d_height,
	unsigned int voflags, unsigned int outfmt){
    vf->priv->context= getPPContext(width, height);

    return vf_next_config(vf,width,height,d_width,d_height,voflags,vf->priv->outfmt);
}

static void uninit(struct vf_instance_s* vf){
    if(vf->priv->context) freePPContext(vf->priv->context);
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
	vf->priv->pp= *((unsigned int*)data);
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

static int put_image(struct vf_instance_s* vf, mp_image_t *mpi){
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
	postprocess(mpi->planes           ,mpi->stride,
		    vf->priv->dmpi->planes,vf->priv->dmpi->stride,
		    (mpi->w+7)&(~7),mpi->h,
		    mpi->qscale, mpi->qstride,
		    &vf->priv->ppMode[ vf->priv->pp ], vf->priv->context,
		    mpi->pict_type);
    }
    
    return vf_next_put_image(vf,vf->priv->dmpi);
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
    char *endptr, *name;
    int i;
    int hex_mode=0;
    
    vf->query_format=query_format;
    vf->control=control;
    vf->config=config;
    vf->get_image=get_image;
    vf->put_image=put_image;
    vf->uninit=uninit;
    vf->default_caps=VFCAP_ACCEPT_STRIDE|VFCAP_POSTPROC;
    vf->priv=malloc(sizeof(struct vf_priv_s));
    vf->priv->context=NULL;

    // check csp:
    vf->priv->outfmt=vf_match_csp(&vf->next,fmt_list,IMGFMT_YV12);
    if(!vf->priv->outfmt) return 0; // no csp match :(
    
    if(args){
	if(!strcmp("help", args)){
		printf("%s", postproc_help);
		exit(1);
	}
	
	hex_mode= strtol(args, &endptr, 0);
	if(*endptr){
            name= args;
	}else
            name= NULL;
    }else{
        name="de";
    }
    
    if(name){
        for(i=0; i<=GET_PP_QUALITY_MAX; i++){
            vf->priv->ppMode[i]= getPPModeByNameAndQuality(name, i);
            if(vf->priv->ppMode[i].error) return -1;
        }
    }else{
        /* hex mode for compatibility */
        for(i=0; i<=GET_PP_QUALITY_MAX; i++){
	    PPMode ppMode;
	    
	    ppMode.lumMode= hex_mode;
	    ppMode.chromMode= ((hex_mode&0xFF)>>4) | (hex_mode&0xFFFFFF00);
	    ppMode.maxTmpNoise[0]= 700;
	    ppMode.maxTmpNoise[1]= 1500;
	    ppMode.maxTmpNoise[2]= 3000;
	    ppMode.maxAllowedY= 234;
	    ppMode.minAllowedY= 16;
	    ppMode.baseDcDiff= 256/4;
	    ppMode.flatnessThreshold=40;
    
            vf->priv->ppMode[i]= ppMode;
        }
    }
    
    vf->priv->pp=GET_PP_QUALITY_MAX; //divx_quality;
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
