#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "config.h"
#include "mp_msg.h"

#include "img_format.h"
#include "mp_image.h"
#include "vf.h"

//===========================================================================//

static unsigned int bgr_list[]={
    IMGFMT_Y800,
    IMGFMT_Y8,
    IMGFMT_BGR8,
    IMGFMT_RGB8,

    IMGFMT_YVU9,
    IMGFMT_411P,
    IMGFMT_YV12,
    IMGFMT_I420,
    IMGFMT_IYUV,
    IMGFMT_422P,
    IMGFMT_444P,
    
    IMGFMT_YUY2,
    IMGFMT_BGR15,
    IMGFMT_RGB15,
    IMGFMT_BGR16,
    IMGFMT_RGB16,

    IMGFMT_BGR32,
    IMGFMT_RGB32,

//    IMGFMT_BGR24,
//    IMGFMT_RGB24,
    0
};

static unsigned int find_best(struct vf_instance_s* vf){
    unsigned int best=0;
    int ret;
    unsigned int* p=bgr_list;
    while(*p){
	ret=vf->next->query_format(vf->next,*p);
	mp_msg(MSGT_VFILTER,MSGL_V,"[%s] query(%s) -> %d\n",vf->info->name,vo_format_name(*p),ret&3);
	if(ret&VFCAP_CSP_SUPPORTED_BY_HW){ best=*p; break;} // no conversion -> bingo!
	if(ret&VFCAP_CSP_SUPPORTED && !best) best=*p; // best with conversion
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
    if (!vf->priv->fmt)
	vf->priv->fmt=find_best(vf);
    if(!vf->priv->fmt){
	// no matching fmt, so force one...
	if(outfmt==IMGFMT_RGB8) vf->priv->fmt=IMGFMT_RGB32;
	else if(outfmt==IMGFMT_BGR8) vf->priv->fmt=IMGFMT_BGR32;
	else return 0;
    }
    return vf_next_config(vf,width,height,d_width,d_height,flags,vf->priv->fmt);
}

static int bittab[8]={128,64,32,16,8,4,2,1};

static void convert(mp_image_t *mpi, mp_image_t *dmpi, int value0, int value1,int bpp){
    int y;
    for(y=0;y<mpi->h;y++){
	unsigned char* src=mpi->planes[0]+mpi->stride[0]*y;
	switch(bpp){
	case 1: {
	    unsigned char* dst=dmpi->planes[0]+dmpi->stride[0]*y;
	    int x;
	    for(x=0;x<mpi->w;x++)
		dst[x]=(src[x>>3]&bittab[x&7]) ? value1 : value0;
	    break; }
	case 2: {
	    uint16_t* dst=(uint16_t*)(dmpi->planes[0]+dmpi->stride[0]*y);
	    int x;
	    for(x=0;x<mpi->w;x++)
		dst[x]=(src[x>>3]&bittab[x&7]) ? value1 : value0;
	    break; }
	case 4: {
	    uint32_t* dst=(uint32_t*)(dmpi->planes[0]+dmpi->stride[0]*y);
	    int x;
	    for(x=0;x<mpi->w;x++)
		dst[x]=(src[x>>3]&bittab[x&7]) ? value1 : value0;
	    break; }
	}
    }
}

static int put_image(struct vf_instance_s* vf, mp_image_t *mpi, double pts){
    mp_image_t *dmpi;
    
    // hope we'll get DR buffer:
    dmpi=vf_get_image(vf->next,vf->priv->fmt,
	MP_IMGTYPE_TEMP, MP_IMGFLAG_ACCEPT_STRIDE,
	mpi->w, mpi->h);

    switch(dmpi->imgfmt){
    case IMGFMT_Y800:
    case IMGFMT_Y8:
    case IMGFMT_BGR8:
    case IMGFMT_RGB8:
	convert(mpi,dmpi,0,255,1);
	break;
    case IMGFMT_YVU9:
    case IMGFMT_411P:
    case IMGFMT_YV12:
    case IMGFMT_I420:
    case IMGFMT_IYUV:
    case IMGFMT_422P:
    case IMGFMT_444P:
	convert(mpi,dmpi,0,255,1);
	memset(dmpi->planes[1],128,dmpi->stride[1]*dmpi->chroma_height);
	memset(dmpi->planes[2],128,dmpi->stride[2]*dmpi->chroma_height);
	break;
    case IMGFMT_YUY2:
	convert(mpi,dmpi,0x8000,0x80ff,2);
	break;
    case IMGFMT_BGR15:
    case IMGFMT_RGB15:
	convert(mpi,dmpi,0,0x7fff,2);
	break;
    case IMGFMT_BGR16:
    case IMGFMT_RGB16:
	convert(mpi,dmpi,0,0xffff,2);
	break;
    case IMGFMT_BGR32:
    case IMGFMT_RGB32:
	convert(mpi,dmpi,0,0x00ffffff,4);
	break;
    default:
	mp_msg(MSGT_VFILTER,MSGL_ERR,"Unhandled format: 0x%X\n",dmpi->imgfmt);
	return 0;
    }

    return vf_next_put_image(vf,dmpi, pts);
}

//===========================================================================//

static int query_format(struct vf_instance_s* vf, unsigned int fmt){
    int best;
    if(fmt!=IMGFMT_RGB1 && fmt!=IMGFMT_BGR1) return 0;
    best=find_best(vf);
    if(!best) return 0; // no match
    return vf->next->query_format(vf->next,best);
}

static int open(vf_instance_t *vf, char* args){
    vf->config=config;
    vf->put_image=put_image;
    vf->query_format=query_format;
    vf->priv=malloc(sizeof(struct vf_priv_s));
    memset(vf->priv, 0, sizeof(struct vf_priv_s));
    return 1;
}

vf_info_t vf_info_1bpp = {
    "1bpp bitmap -> YUV/BGR 8/15/16/32 conversion",
    "1bpp",
    "A'rpi",
    "",
    open,
    NULL
};

//===========================================================================//
