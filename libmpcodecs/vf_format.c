#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "../config.h"
#include "../mp_msg.h"

#include "../libvo/img_format.h"
#include "../mp_image.h"
#include "vf.h"

struct vf_priv_s {
    unsigned int fmt;
};

//===========================================================================//

static int query_format(struct vf_instance_s* vf, unsigned int fmt){
    if(fmt==vf->priv->fmt)
	return vf_next_query_format(vf,fmt);
    return 0;
}

static int open(vf_instance_t *vf, char* args){
    vf->query_format=query_format;
    vf->priv=malloc(sizeof(struct vf_priv_s));

    if(args){
	if(!strcasecmp(args,"yuy2")) vf->priv->fmt=IMGFMT_YUY2; else
	if(!strcasecmp(args,"yv12")) vf->priv->fmt=IMGFMT_YV12; else
	if(!strcasecmp(args,"i420")) vf->priv->fmt=IMGFMT_I420; else
	if(!strcasecmp(args,"iyuv")) vf->priv->fmt=IMGFMT_IYUV; else
	if(!strcasecmp(args,"uyvy")) vf->priv->fmt=IMGFMT_UYVY; else
	if(!strcasecmp(args,"bgr24")) vf->priv->fmt=IMGFMT_BGR24; else
	if(!strcasecmp(args,"bgr32")) vf->priv->fmt=IMGFMT_BGR32; else
	if(!strcasecmp(args,"bgr16")) vf->priv->fmt=IMGFMT_BGR16; else
	if(!strcasecmp(args,"bgr15")) vf->priv->fmt=IMGFMT_BGR15; else
	{ printf("Unknown format name: '%s'\n",args);return 0;}
    } else
        vf->priv->fmt=IMGFMT_YUY2;

    return 1;
}

vf_info_t vf_info_format = {
    "force output format",
    "format",
    "A'rpi",
    "FIXME! get_image()/put_image()",
    open
};

//===========================================================================//
