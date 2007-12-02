#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"

#include "img_format.h"
#include "mp_image.h"
#include "vf.h"

#include "m_option.h"
#include "m_struct.h"

static struct vf_priv_s {
    unsigned int fmt;
} const vf_priv_dflt = {
  IMGFMT_YUY2
};

//===========================================================================//

static int query_format(struct vf_instance_s* vf, unsigned int fmt){
    if(fmt==vf->priv->fmt)
	return vf_next_query_format(vf,fmt);
    return 0;
}

static int open(vf_instance_t *vf, char* args){
    vf->query_format=query_format;
    vf->default_caps=0;
    if(!vf->priv) {
      vf->priv=malloc(sizeof(struct vf_priv_s));
      vf->priv->fmt=IMGFMT_YUY2;
    }
    if(args){
	if(!strcasecmp(args,"444p")) vf->priv->fmt=IMGFMT_444P; else
	if(!strcasecmp(args,"422p")) vf->priv->fmt=IMGFMT_422P; else
	if(!strcasecmp(args,"411p")) vf->priv->fmt=IMGFMT_411P; else
	if(!strcasecmp(args,"yuy2")) vf->priv->fmt=IMGFMT_YUY2; else
	if(!strcasecmp(args,"yv12")) vf->priv->fmt=IMGFMT_YV12; else
	if(!strcasecmp(args,"i420")) vf->priv->fmt=IMGFMT_I420; else
	if(!strcasecmp(args,"yvu9")) vf->priv->fmt=IMGFMT_YVU9; else
	if(!strcasecmp(args,"if09")) vf->priv->fmt=IMGFMT_IF09; else
	if(!strcasecmp(args,"iyuv")) vf->priv->fmt=IMGFMT_IYUV; else
	if(!strcasecmp(args,"uyvy")) vf->priv->fmt=IMGFMT_UYVY; else
	if(!strcasecmp(args,"bgr24")) vf->priv->fmt=IMGFMT_BGR24; else
	if(!strcasecmp(args,"bgr32")) vf->priv->fmt=IMGFMT_BGR32; else
	if(!strcasecmp(args,"bgr16")) vf->priv->fmt=IMGFMT_BGR16; else
	if(!strcasecmp(args,"bgr15")) vf->priv->fmt=IMGFMT_BGR15; else
	if(!strcasecmp(args,"bgr8")) vf->priv->fmt=IMGFMT_BGR8; else
	if(!strcasecmp(args,"bgr4")) vf->priv->fmt=IMGFMT_BGR4; else
	if(!strcasecmp(args,"bg4b")) vf->priv->fmt=IMGFMT_BG4B; else
	if(!strcasecmp(args,"bgr1")) vf->priv->fmt=IMGFMT_BGR1; else
	if(!strcasecmp(args,"rgb24")) vf->priv->fmt=IMGFMT_RGB24; else
	if(!strcasecmp(args,"rgb32")) vf->priv->fmt=IMGFMT_RGB32; else
	if(!strcasecmp(args,"rgb16")) vf->priv->fmt=IMGFMT_RGB16; else
	if(!strcasecmp(args,"rgb15")) vf->priv->fmt=IMGFMT_RGB15; else
	if(!strcasecmp(args,"rgb8")) vf->priv->fmt=IMGFMT_RGB8; else
	if(!strcasecmp(args,"rgb4")) vf->priv->fmt=IMGFMT_RGB4; else
	if(!strcasecmp(args,"rg4b")) vf->priv->fmt=IMGFMT_RG4B; else
	if(!strcasecmp(args,"rgb1")) vf->priv->fmt=IMGFMT_RGB1; else
	if(!strcasecmp(args,"rgba")) vf->priv->fmt=IMGFMT_RGBA; else
	if(!strcasecmp(args,"argb")) vf->priv->fmt=IMGFMT_ARGB; else
	if(!strcasecmp(args,"bgra")) vf->priv->fmt=IMGFMT_BGRA; else
	if(!strcasecmp(args,"abgr")) vf->priv->fmt=IMGFMT_ABGR; else
	{ mp_msg(MSGT_VFILTER, MSGL_WARN, MSGTR_MPCODECS_UnknownFormatName, args);return 0;}
    }
        

    return 1;
}

#define ST_OFF(f) M_ST_OFF(struct vf_priv_s,f)
static m_option_t vf_opts_fields[] = {
  {"fmt", ST_OFF(fmt), CONF_TYPE_IMGFMT, 0,0 ,0, NULL},
  { NULL, NULL, 0, 0, 0, 0,  NULL }
};

static m_struct_t vf_opts = {
  "format",
  sizeof(struct vf_priv_s),
  &vf_priv_dflt,
  vf_opts_fields
};

const vf_info_t vf_info_format = {
    "force output format",
    "format",
    "A'rpi",
    "FIXME! get_image()/put_image()",
    open,
    &vf_opts
};

//===========================================================================//
