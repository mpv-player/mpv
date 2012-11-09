/*
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <libavutil/common.h>

#include "config.h"
#include "core/mp_msg.h"
#include "core/options.h"

#include "video/img_format.h"
#include "video/mp_image.h"
#include "vf.h"

#include "video/memcpy_pic.h"

#include "core/m_option.h"
#include "core/m_struct.h"

static struct vf_priv_s {
    // These four values are a backup of the values parsed from the command line.
    // This is necessary so that we do not get a mess upon filter reinit due to
    // e.g. aspect changes and with only aspect specified on the command line,
    // where we would otherwise use the values calculated for a different aspect
    // instead of recalculating them again.
    int cfg_exp_w, cfg_exp_h;
    int cfg_exp_x, cfg_exp_y;
    int exp_w,exp_h;
    int exp_x,exp_y;
    double aspect;
    int round;
    int first_slice;
} const vf_priv_dflt = {
  -1,-1,
  -1,-1,
  -1,-1,
  -1,-1,
  0.,
  1,
  0
};

//===========================================================================//

static int config(struct vf_instance *vf,
        int width, int height, int d_width, int d_height,
	unsigned int flags, unsigned int outfmt)
{
    struct MPOpts *opts = vf->opts;
    mp_image_t test_mpi;
    mp_image_setfmt(&test_mpi, outfmt);
    if (outfmt == IMGFMT_IF09 || !test_mpi.bpp) return 0;
    vf->priv->exp_x = vf->priv->cfg_exp_x;
    vf->priv->exp_y = vf->priv->cfg_exp_y;
    vf->priv->exp_w = vf->priv->cfg_exp_w;
    vf->priv->exp_h = vf->priv->cfg_exp_h;
    // calculate the missing parameters:
#if 0
    if(vf->priv->exp_w<width) vf->priv->exp_w=width;
    if(vf->priv->exp_h<height) vf->priv->exp_h=height;
#else
    if ( vf->priv->exp_w == -1 ) vf->priv->exp_w=width;
      else if (vf->priv->exp_w < -1 ) vf->priv->exp_w=width - vf->priv->exp_w;
        else if ( vf->priv->exp_w<width ) vf->priv->exp_w=width;
    if ( vf->priv->exp_h == -1 ) vf->priv->exp_h=height;
      else if ( vf->priv->exp_h < -1 ) vf->priv->exp_h=height - vf->priv->exp_h;
        else if( vf->priv->exp_h<height ) vf->priv->exp_h=height;
#endif
    if (vf->priv->aspect) {
        float adjusted_aspect = vf->priv->aspect;
        adjusted_aspect *= ((double)width/height) / ((double)d_width/d_height);
        if (vf->priv->exp_h < vf->priv->exp_w / adjusted_aspect) {
            vf->priv->exp_h = vf->priv->exp_w / adjusted_aspect + 0.5;
        } else {
            vf->priv->exp_w = vf->priv->exp_h * adjusted_aspect + 0.5;
        }
    }
    if (vf->priv->round > 1) { // round up.
        vf->priv->exp_w = (1 + (vf->priv->exp_w - 1) / vf->priv->round) * vf->priv->round;
        vf->priv->exp_h = (1 + (vf->priv->exp_h - 1) / vf->priv->round) * vf->priv->round;
    }

    if(vf->priv->exp_x<0 || vf->priv->exp_x+width>vf->priv->exp_w) vf->priv->exp_x=(vf->priv->exp_w-width)/2;
    if(vf->priv->exp_y<0 || vf->priv->exp_y+height>vf->priv->exp_h) vf->priv->exp_y=(vf->priv->exp_h-height)/2;
    if(test_mpi.flags & MP_IMGFLAG_YUV) {
        int x_align_mask = (1 << test_mpi.chroma_x_shift) - 1;
        int y_align_mask = (1 << test_mpi.chroma_y_shift) - 1;
        // For 1-plane format non-aligned offsets will completely
        // destroy the colours, for planar it will break the chroma
        // sampling position.
        if (vf->priv->exp_x & x_align_mask) {
            vf->priv->exp_x &= ~x_align_mask;
            mp_msg(MSGT_VFILTER, MSGL_ERR, "Specified x offset not supported "
                   "for YUV, reduced to %i.\n", vf->priv->exp_x);
        }
        if (vf->priv->exp_y & y_align_mask) {
            vf->priv->exp_y &= ~y_align_mask;
            mp_msg(MSGT_VFILTER, MSGL_ERR, "Specified y offset not supported "
                   "for YUV, reduced to %i.\n", vf->priv->exp_y);
        }
    }

    if(!opts->screen_size_x && !opts->screen_size_y){
	d_width=d_width*vf->priv->exp_w/width;
	d_height=d_height*vf->priv->exp_h/height;
    }
    return vf_next_config(vf,vf->priv->exp_w,vf->priv->exp_h,d_width,d_height,flags,outfmt);
}

// there are 4 cases:
// codec --DR--> expand --DR--> vo
// codec --DR--> expand -copy-> vo
// codec -copy-> expand --DR--> vo
// codec -copy-> expand -copy-> vo (worst case)

static void get_image(struct vf_instance *vf, mp_image_t *mpi){
//    if(mpi->type==MP_IMGTYPE_IPB) return; // not yet working
    if(vf->priv->exp_w==mpi->width ||
       (mpi->flags&(MP_IMGFLAG_ACCEPT_STRIDE|MP_IMGFLAG_ACCEPT_WIDTH)) ){
	// try full DR !
	mpi->priv=vf->dmpi=vf_get_image(vf->next,mpi->imgfmt,
	    mpi->type, mpi->flags,
            FFMAX(vf->priv->exp_w, mpi->width +vf->priv->exp_x),
            FFMAX(vf->priv->exp_h, mpi->height+vf->priv->exp_y));
	if((vf->dmpi->flags & MP_IMGFLAG_DRAW_CALLBACK) &&
	  !(vf->dmpi->flags & MP_IMGFLAG_DIRECT)){
	    mp_tmsg(MSGT_VFILTER, MSGL_INFO, "Full DR not possible, trying SLICES instead!\n");
	    return;
	}
	// set up mpi as a cropped-down image of dmpi:
	if(mpi->flags&MP_IMGFLAG_PLANAR){
	    mpi->planes[0]=vf->dmpi->planes[0]+
		vf->priv->exp_y*vf->dmpi->stride[0]+vf->priv->exp_x;
	    mpi->planes[1]=vf->dmpi->planes[1]+
		(vf->priv->exp_y>>mpi->chroma_y_shift)*vf->dmpi->stride[1]+(vf->priv->exp_x>>mpi->chroma_x_shift);
	    mpi->planes[2]=vf->dmpi->planes[2]+
		(vf->priv->exp_y>>mpi->chroma_y_shift)*vf->dmpi->stride[2]+(vf->priv->exp_x>>mpi->chroma_x_shift);
	    mpi->stride[1]=vf->dmpi->stride[1];
	    mpi->stride[2]=vf->dmpi->stride[2];
	} else {
	    mpi->planes[0]=vf->dmpi->planes[0]+
		vf->priv->exp_y*vf->dmpi->stride[0]+
		vf->priv->exp_x*(vf->dmpi->bpp/8);
	}
	mpi->stride[0]=vf->dmpi->stride[0];
	mpi->width=vf->dmpi->width;
	mpi->flags|=MP_IMGFLAG_DIRECT;
	mpi->flags&=~MP_IMGFLAG_DRAW_CALLBACK;
//	vf->dmpi->flags&=~MP_IMGFLAG_DRAW_CALLBACK;
    }
}

static void start_slice(struct vf_instance *vf, mp_image_t *mpi){
//    printf("start_slice called! flag=%d\n",mpi->flags&MP_IMGFLAG_DRAW_CALLBACK);
    // they want slices!!! allocate the buffer.
    if(!mpi->priv)
	mpi->priv=vf->dmpi=vf_get_image(vf->next,mpi->imgfmt,
//	MP_IMGTYPE_TEMP, MP_IMGFLAG_ACCEPT_STRIDE | MP_IMGFLAG_PREFER_ALIGNED_STRIDE,
	    MP_IMGTYPE_TEMP, mpi->flags,
            FFMAX(vf->priv->exp_w, mpi->width +vf->priv->exp_x),
            FFMAX(vf->priv->exp_h, mpi->height+vf->priv->exp_y));
    vf->priv->first_slice = 1;
}

static void draw_top_blackbar_slice(struct vf_instance *vf,
				    unsigned char** src, int* stride, int w,int h, int x, int y){
    if(vf->priv->exp_y>0 && y == 0) {
	vf_next_draw_slice(vf, vf->dmpi->planes, vf->dmpi->stride,
			   vf->dmpi->w,vf->priv->exp_y,0,0);
    }

}

static void draw_bottom_blackbar_slice(struct vf_instance *vf,
				    unsigned char** src, int* stride, int w,int h, int x, int y){
    if(vf->priv->exp_y+vf->h<vf->dmpi->h && y+h == vf->h) {
	unsigned char *src2[MP_MAX_PLANES];
	src2[0] = vf->dmpi->planes[0]
		+ (vf->priv->exp_y+vf->h)*vf->dmpi->stride[0];
	if(vf->dmpi->flags&MP_IMGFLAG_PLANAR){
	    src2[1] = vf->dmpi->planes[1]
		+ ((vf->priv->exp_y+vf->h)>>vf->dmpi->chroma_y_shift)*vf->dmpi->stride[1];
	    src2[2] = vf->dmpi->planes[2]
		+ ((vf->priv->exp_y+vf->h)>>vf->dmpi->chroma_y_shift)*vf->dmpi->stride[2];
	} else {
	    src2[1] = vf->dmpi->planes[1]; // passthrough rgb8 palette
	}
	vf_next_draw_slice(vf, src2, vf->dmpi->stride,
			   vf->dmpi->w,vf->dmpi->h-(vf->priv->exp_y+vf->h),
			   0,vf->priv->exp_y+vf->h);
    }
}

static void draw_slice(struct vf_instance *vf,
        unsigned char** src, int* stride, int w,int h, int x, int y){
//    printf("draw_slice() called %d at %d\n",h,y);

    if (y == 0 && y+h == vf->h) {
	// special case - only one slice
	draw_top_blackbar_slice(vf, src, stride, w, h, x, y);
	vf_next_draw_slice(vf,src,stride,w,h,x+vf->priv->exp_x,y+vf->priv->exp_y);
	draw_bottom_blackbar_slice(vf, src, stride, w, h, x, y);
	return;
    }
    if (vf->priv->first_slice) {
	draw_top_blackbar_slice(vf, src, stride, w, h, x, y);
	draw_bottom_blackbar_slice(vf, src, stride, w, h, x, y);
    }
    vf_next_draw_slice(vf,src,stride,w,h,x+vf->priv->exp_x,y+vf->priv->exp_y);
    if (!vf->priv->first_slice) {
	draw_top_blackbar_slice(vf, src, stride, w, h, x, y);
	draw_bottom_blackbar_slice(vf, src, stride, w, h, x, y);
    }
    vf->priv->first_slice = 0;
}

// w, h = width and height of the actual video frame (located at exp_x/exp_y)
static void clear_borders(struct vf_instance *vf, int w, int h)
{
    // upper border (over the full width)
    vf_mpi_clear(vf->dmpi, 0, 0, vf->priv->exp_w, vf->priv->exp_y);
    // lower border
    vf_mpi_clear(vf->dmpi, 0, vf->priv->exp_y + h, vf->priv->exp_w,
                 vf->priv->exp_h - (vf->priv->exp_y + h));
    // left
    vf_mpi_clear(vf->dmpi, 0, vf->priv->exp_y, vf->priv->exp_x, h);
    // right
    vf_mpi_clear(vf->dmpi, vf->priv->exp_x + w, vf->priv->exp_y,
                 vf->priv->exp_w - (vf->priv->exp_x + w), h);
}

static int put_image(struct vf_instance *vf, mp_image_t *mpi, double pts){
    if(mpi->flags&MP_IMGFLAG_DIRECT || mpi->flags&MP_IMGFLAG_DRAW_CALLBACK){
	vf->dmpi=mpi->priv;
	if(!vf->dmpi) { mp_tmsg(MSGT_VFILTER, MSGL_WARN, "Why do we get NULL??\n"); return 0; }
	mpi->priv=NULL;
        clear_borders(vf,mpi->w,mpi->h);
	// we've used DR, so we're ready...
	if(!(mpi->flags&MP_IMGFLAG_PLANAR))
	    vf->dmpi->planes[1] = mpi->planes[1]; // passthrough rgb8 palette
	return vf_next_put_image(vf,vf->dmpi, pts);
    }

    // hope we'll get DR buffer:
    vf->dmpi=vf_get_image(vf->next,mpi->imgfmt,
	MP_IMGTYPE_TEMP, MP_IMGFLAG_ACCEPT_STRIDE,
	vf->priv->exp_w, vf->priv->exp_h);

    // copy mpi->dmpi...
    if(mpi->flags&MP_IMGFLAG_PLANAR){
	memcpy_pic(vf->dmpi->planes[0]+
	        vf->priv->exp_y*vf->dmpi->stride[0]+vf->priv->exp_x,
		mpi->planes[0], mpi->w, mpi->h,
		vf->dmpi->stride[0],mpi->stride[0]);
	memcpy_pic(vf->dmpi->planes[1]+
		(vf->priv->exp_y>>mpi->chroma_y_shift)*vf->dmpi->stride[1]+(vf->priv->exp_x>>mpi->chroma_x_shift),
		mpi->planes[1], (mpi->w>>mpi->chroma_x_shift), (mpi->h>>mpi->chroma_y_shift),
		vf->dmpi->stride[1],mpi->stride[1]);
	memcpy_pic(vf->dmpi->planes[2]+
		(vf->priv->exp_y>>mpi->chroma_y_shift)*vf->dmpi->stride[2]+(vf->priv->exp_x>>mpi->chroma_x_shift),
		mpi->planes[2], (mpi->w>>mpi->chroma_x_shift), (mpi->h>>mpi->chroma_y_shift),
		vf->dmpi->stride[2],mpi->stride[2]);
    } else {
	memcpy_pic(vf->dmpi->planes[0]+
	        vf->priv->exp_y*vf->dmpi->stride[0]+vf->priv->exp_x*(vf->dmpi->bpp/8),
		mpi->planes[0], mpi->w*(vf->dmpi->bpp/8), mpi->h,
		vf->dmpi->stride[0],mpi->stride[0]);
	vf->dmpi->planes[1] = mpi->planes[1]; // passthrough rgb8 palette
    }
    clear_borders(vf,mpi->w,mpi->h);
    return vf_next_put_image(vf,vf->dmpi, pts);
}

//===========================================================================//

static int control(struct vf_instance *vf, int request, void* data){
    return vf_next_control(vf,request,data);
}

static int query_format(struct vf_instance *vf, unsigned int fmt){
  return vf_next_query_format(vf,fmt);
}

static int vf_open(vf_instance_t *vf, char *args){
    vf->config=config;
    vf->control=control;
    vf->query_format=query_format;
    vf->start_slice=start_slice;
    vf->draw_slice=draw_slice;
    vf->get_image=get_image;
    vf->put_image=put_image;
    mp_msg(MSGT_VFILTER, MSGL_INFO, "Expand: %d x %d, %d ; %d, aspect: %f, round: %d\n",
    vf->priv->cfg_exp_w,
    vf->priv->cfg_exp_h,
    vf->priv->cfg_exp_x,
    vf->priv->cfg_exp_y,
    vf->priv->aspect,
    vf->priv->round);
    return 1;
}

#define ST_OFF(f) M_ST_OFF(struct vf_priv_s,f)
static m_option_t vf_opts_fields[] = {
  {"w", ST_OFF(cfg_exp_w), CONF_TYPE_INT, 0, 0 ,0, NULL},
  {"h", ST_OFF(cfg_exp_h), CONF_TYPE_INT, 0, 0 ,0, NULL},
  {"x", ST_OFF(cfg_exp_x), CONF_TYPE_INT, M_OPT_MIN, -1, 0, NULL},
  {"y", ST_OFF(cfg_exp_y), CONF_TYPE_INT, M_OPT_MIN, -1, 0, NULL},
  {"aspect", ST_OFF(aspect), CONF_TYPE_DOUBLE, M_OPT_MIN, 0, 0, NULL},
  {"round", ST_OFF(round), CONF_TYPE_INT, M_OPT_MIN, 1, 0, NULL},
  { NULL, NULL, 0, 0, 0, 0,  NULL }
};

static const m_struct_t vf_opts = {
  "expand",
  sizeof(struct vf_priv_s),
  &vf_priv_dflt,
  vf_opts_fields
};



const vf_info_t vf_info_expand = {
    "expanding",
    "expand",
    "A'rpi",
    "",
    vf_open,
    &vf_opts
};

//===========================================================================//
