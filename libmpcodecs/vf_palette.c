#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "../config.h"
#include "../mp_msg.h"

#include "img_format.h"
#include "mp_image.h"
#include "vf.h"

#include "../postproc/rgb2rgb.h"

//===========================================================================//

static unsigned int bgr_list[]={
    IMGFMT_BGR32,
    IMGFMT_BGR24,
    IMGFMT_BGR16,
    IMGFMT_BGR15,
    0
};
static unsigned int rgb_list[]={
    IMGFMT_RGB32,
    IMGFMT_RGB24,
    IMGFMT_RGB16,
    IMGFMT_RGB15,
    0
};

static unsigned int find_best(struct vf_instance_s* vf, unsigned int fmt){
    unsigned int best=0;
    int ret;
    unsigned int* p;
    if(fmt==IMGFMT_BGR8) p=bgr_list;
    else if(fmt==IMGFMT_RGB8) p=rgb_list;
    else return 0;
    while(*p){
	ret=vf->next->query_format(vf->next,*p);
	mp_msg(MSGT_VFILTER,MSGL_V,"[%s] query(%s) -> %d\n",vf->info->name,vo_format_name(*p),ret&3);
	if(ret&2){ best=*p; break;} // no conversion -> bingo!
	if(ret&1 && !best) best=*p; // best with conversion
	++p;
    }
    return best;
}

//===========================================================================//

struct vf_priv_s {
    unsigned int fmt;
};

static int config(struct vf_instance_s* vf,
        int width, int height, int d_width, int d_height,
	unsigned int flags, unsigned int outfmt){
    vf->priv->fmt=find_best(vf,outfmt);
    if(!vf->priv->fmt){
	// no matching fmt, so force one...
	if(outfmt==IMGFMT_RGB8) vf->priv->fmt=IMGFMT_RGB32;
	else if(outfmt==IMGFMT_BGR8) vf->priv->fmt=IMGFMT_BGR32;
	else return 0;
    }
    return vf_next_config(vf,width,height,d_width,d_height,flags,vf->priv->fmt);
}

static void put_image(struct vf_instance_s* vf, mp_image_t *mpi){
    mp_image_t *dmpi;

    // hope we'll get DR buffer:
    dmpi=vf_get_image(vf->next,vf->priv->fmt,
	MP_IMGTYPE_TEMP, MP_IMGFLAG_ACCEPT_STRIDE,
	mpi->w, mpi->h);

    if(mpi->w==mpi->stride[0] && dmpi->w*(dmpi->bpp>>3)==dmpi->stride[0]){
	// no stride conversion needed
	switch(dmpi->imgfmt&255){
	case 15:
	    palette8torgb15(mpi->planes[0],dmpi->planes[0],mpi->h*mpi->w,mpi->planes[1]);
	    break;
	case 16:
	    palette8torgb16(mpi->planes[0],dmpi->planes[0],mpi->h*mpi->w,mpi->planes[1]);
	    break;
	case 24:
	    palette8torgb24(mpi->planes[0],dmpi->planes[0],mpi->h*mpi->w,mpi->planes[1]);
	    break;
	case 32:
	    palette8torgb32(mpi->planes[0],dmpi->planes[0],mpi->h*mpi->w,mpi->planes[1]);
	    break;
	}
    } else {
	int y;
	for(y=0;y<mpi->h;y++){
	    unsigned char* src=mpi->planes[0]+y*mpi->stride[0];
	    unsigned char* dst=dmpi->planes[0]+y*dmpi->stride[0];
	    switch(dmpi->imgfmt&255){
	    case 15:
		palette8torgb15(src,dst,mpi->w,mpi->planes[1]);break;
	    case 16:
		palette8torgb16(src,dst,mpi->w,mpi->planes[1]);break;
	    case 24:
		palette8torgb24(src,dst,mpi->w,mpi->planes[1]);break;
	    case 32:
		palette8torgb32(src,dst,mpi->w,mpi->planes[1]);break;
	    }
	}
    }
    
    vf_next_put_image(vf,dmpi);
}

//===========================================================================//

static int query_format(struct vf_instance_s* vf, unsigned int fmt){
    int best=find_best(vf,fmt);
    if(!best) return 0; // no match
    return vf->next->query_format(vf->next,best);
}

static int open(vf_instance_t *vf, char* args){
    vf->config=config;
    vf->put_image=put_image;
    vf->query_format=query_format;
    vf->priv=malloc(sizeof(struct vf_priv_s));
    return 1;
}

vf_info_t vf_info_palette = {
    "8bpp indexed (using palette) -> BGR 15/16/24/32 conversion",
    "palette",
    "A'rpi",
    "",
    open
};

//===========================================================================//
