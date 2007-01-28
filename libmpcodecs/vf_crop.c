#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"

#include "img_format.h"
#include "mp_image.h"
#include "vf.h"

#include "m_option.h"
#include "m_struct.h"

static struct vf_priv_s {
    int crop_w,crop_h;
    int crop_x,crop_y;
} const vf_priv_dflt = {
  -1,-1,
  -1,-1
};

extern int opt_screen_size_x;
extern int opt_screen_size_y;

//===========================================================================//

static int config(struct vf_instance_s* vf,
        int width, int height, int d_width, int d_height,
	unsigned int flags, unsigned int outfmt){
    // calculate the missing parameters:
    if(vf->priv->crop_w<=0 || vf->priv->crop_w>width) vf->priv->crop_w=width;
    if(vf->priv->crop_h<=0 || vf->priv->crop_h>height) vf->priv->crop_h=height;
    if(vf->priv->crop_x<0) vf->priv->crop_x=(width-vf->priv->crop_w)/2;
    if(vf->priv->crop_y<0) vf->priv->crop_y=(height-vf->priv->crop_h)/2;
    // rounding:
    if(!IMGFMT_IS_RGB(outfmt) && !IMGFMT_IS_BGR(outfmt)){
	switch(outfmt){
	case IMGFMT_444P:
	case IMGFMT_Y800:
	case IMGFMT_Y8:
	    break;
	case IMGFMT_YVU9:
	case IMGFMT_IF09:
	    vf->priv->crop_y&=~3;
	case IMGFMT_411P:
	    vf->priv->crop_x&=~3;
	    break;
	case IMGFMT_YV12:
	case IMGFMT_I420:
	case IMGFMT_IYUV:
	    vf->priv->crop_y&=~1;
	default:
	    vf->priv->crop_x&=~1;
	}
    }
    // check:
    if(vf->priv->crop_w+vf->priv->crop_x>width ||
       vf->priv->crop_h+vf->priv->crop_y>height){
	mp_msg(MSGT_VFILTER, MSGL_WARN, MSGTR_MPCODECS_CropBadPositionWidthHeight);
	return 0;
    }
    if(!opt_screen_size_x && !opt_screen_size_y){
	d_width=d_width*vf->priv->crop_w/width;
	d_height=d_height*vf->priv->crop_h/height;
    }
    return vf_next_config(vf,vf->priv->crop_w,vf->priv->crop_h,d_width,d_height,flags,outfmt);
}

static int put_image(struct vf_instance_s* vf, mp_image_t *mpi, double pts){
    mp_image_t *dmpi;
    if (mpi->flags&MP_IMGFLAG_DRAW_CALLBACK)
	return vf_next_put_image(vf,vf->dmpi, pts);
    dmpi=vf_get_image(vf->next,mpi->imgfmt,
	MP_IMGTYPE_EXPORT, 0,
	vf->priv->crop_w, vf->priv->crop_h);
    if(mpi->flags&MP_IMGFLAG_PLANAR){
	dmpi->planes[0]=mpi->planes[0]+
	    vf->priv->crop_y*mpi->stride[0]+vf->priv->crop_x;
	dmpi->planes[1]=mpi->planes[1]+
	    (vf->priv->crop_y>>mpi->chroma_y_shift)*mpi->stride[1]+(vf->priv->crop_x>>mpi->chroma_x_shift);
	dmpi->planes[2]=mpi->planes[2]+
	    (vf->priv->crop_y>>mpi->chroma_y_shift)*mpi->stride[2]+(vf->priv->crop_x>>mpi->chroma_x_shift);
	dmpi->stride[1]=mpi->stride[1];
	dmpi->stride[2]=mpi->stride[2];
    } else {
	dmpi->planes[0]=mpi->planes[0]+
	    vf->priv->crop_y*mpi->stride[0]+
	    vf->priv->crop_x*(mpi->bpp/8);
	dmpi->planes[1]=mpi->planes[1]; // passthrough rgb8 palette
    }
    dmpi->stride[0]=mpi->stride[0];
    dmpi->width=mpi->width;
    return vf_next_put_image(vf,dmpi, pts);
}

static void start_slice(struct vf_instance_s* vf, mp_image_t *mpi){
    vf->dmpi = vf_get_image(vf->next, mpi->imgfmt, mpi->type, mpi->flags,
	vf->priv->crop_w, vf->priv->crop_h);
}

static void draw_slice(struct vf_instance_s* vf,
        unsigned char** src, int* stride, int w,int h, int x, int y){
    unsigned char *src2[3];
    src2[0] = src[0];
    if (vf->dmpi->flags & MP_IMGFLAG_PLANAR) {
	    src2[1] = src[1];
	    src2[2] = src[2];
    }
    //mp_msg(MSGT_VFILTER, MSGL_V, "crop slice %d %d %d %d ->", w,h,x,y);
    if ((x -= vf->priv->crop_x) < 0) {
	x = -x;
	src2[0] += x;
	if (vf->dmpi->flags & MP_IMGFLAG_PLANAR) {
		src2[1] += x>>vf->dmpi->chroma_x_shift;
		src2[2] += x>>vf->dmpi->chroma_x_shift;
	}
	w -= x;
	x = 0;
    }
    if ((y -= vf->priv->crop_y) < 0) {
	y = -y;
	src2[0] += y*stride[0];
	if (vf->dmpi->flags & MP_IMGFLAG_PLANAR) {
		src2[1] += (y>>vf->dmpi->chroma_y_shift)*stride[1];
		src2[2] += (y>>vf->dmpi->chroma_y_shift)*stride[2];
	}
	h -= y;
	y = 0;
    }
    if (x+w > vf->priv->crop_w) w = vf->priv->crop_w-x;
    if (y+h > vf->priv->crop_h) h = vf->priv->crop_h-y;
    //mp_msg(MSGT_VFILTER, MSGL_V, "%d %d %d %d\n", w,h,x,y);
    if ((w < 0) || (h < 0)) return;
    vf_next_draw_slice(vf,src2,stride,w,h,x,y);
}

//===========================================================================//

static int open(vf_instance_t *vf, char* args){
    vf->config=config;
    vf->put_image=put_image;
    vf->start_slice=start_slice;
    vf->draw_slice=draw_slice;
    vf->default_reqs=VFCAP_ACCEPT_STRIDE;
    if(!vf->priv) {
    vf->priv=malloc(sizeof(struct vf_priv_s));
    // TODO: parse args ->
    vf->priv->crop_x=
    vf->priv->crop_y=
    vf->priv->crop_w=
    vf->priv->crop_h=-1;
    } //if(!vf->priv)
    if(args) sscanf(args, "%d:%d:%d:%d", 
    &vf->priv->crop_w,
    &vf->priv->crop_h,
    &vf->priv->crop_x,
    &vf->priv->crop_y);
    mp_msg(MSGT_VFILTER, MSGL_INFO, "Crop: %d x %d, %d ; %d\n",
    vf->priv->crop_w,
    vf->priv->crop_h,
    vf->priv->crop_x,
    vf->priv->crop_y);
    return 1;
}

#define ST_OFF(f) M_ST_OFF(struct vf_priv_s,f)
static m_option_t vf_opts_fields[] = {
  {"w", ST_OFF(crop_w), CONF_TYPE_INT, M_OPT_MIN,0 ,0, NULL},
  {"h", ST_OFF(crop_h), CONF_TYPE_INT, M_OPT_MIN,0 ,0, NULL},
  {"x", ST_OFF(crop_x), CONF_TYPE_INT, M_OPT_MIN,-1 ,0, NULL},
  {"y", ST_OFF(crop_y), CONF_TYPE_INT, M_OPT_MIN,-1 ,0, NULL},
  { NULL, NULL, 0, 0, 0, 0,  NULL }
};

static m_struct_t vf_opts = {
  "crop",
  sizeof(struct vf_priv_s),
  &vf_priv_dflt,
  vf_opts_fields
};

vf_info_t vf_info_crop = {
    "cropping",
    "crop",
    "A'rpi",
    "",
    open,
    &vf_opts
};

//===========================================================================//
