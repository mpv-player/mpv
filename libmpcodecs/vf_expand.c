#define OSD_SUPPORT

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"

#include "img_format.h"
#include "mp_image.h"
#include "vf.h"

#include "libvo/fastmemcpy.h"

#ifdef OSD_SUPPORT
#include "libvo/sub.h"
#include "libvo/osd.h"
#endif

#include "m_option.h"
#include "m_struct.h"

#define MAX(a,b) ((a) > (b) ? (a) : (b))

static struct vf_priv_s {
    int exp_w,exp_h;
    int exp_x,exp_y;
    int osd;
    double aspect;
    int round;
    unsigned char* fb_ptr;
    int passthrough;
    int first_slice;
} const vf_priv_dflt = {
  -1,-1,
  -1,-1,
  0,
  0.,
  1,
  NULL,
  0,
  0
};

extern int opt_screen_size_x;
extern int opt_screen_size_y;

//===========================================================================//
#ifdef OSD_SUPPORT

static struct vf_instance_s* vf=NULL; // fixme (needs sub.c changes)
static int orig_w,orig_h;

static void remove_func_2(int x0,int y0, int w,int h){
    // TODO: let's cleanup the place
    //printf("OSD clear: %d;%d %dx%d  \n",x0,y0,w,h);
    vf_mpi_clear(vf->dmpi,x0,y0,w,h);
}

static void remove_func(int x0,int y0, int w,int h){
    if(!vo_osd_changed_flag) return;
    // split it to 4 parts:
    if(y0<vf->priv->exp_y){
	// it has parts above the image:
	int y=y0+h;
	if(y>vf->priv->exp_y) y=vf->priv->exp_y;
	remove_func_2(x0,y0,w,y-y0);
	if(y0+h<=vf->priv->exp_y) return;
	h-=y-y0;y0=y;
    }
    if(y0+h>vf->priv->exp_y+orig_h){
	// it has parts under the image:
	int y=y0;
	if(y<vf->priv->exp_y+orig_h) y=vf->priv->exp_y+orig_h;
	remove_func_2(x0,y,w,y0+h-y);
	if(y0>=vf->priv->exp_y+orig_h) return;
	h=y-y0;
    }
    if(x0>=vf->priv->exp_x || x0+w<=vf->priv->exp_x+orig_w) return;
    // TODO  clear left and right side of the image if needed
}

static void draw_func(int x0,int y0, int w,int h,unsigned char* src, unsigned char *srca, int stride){
    unsigned char* dst;
    if(!vo_osd_changed_flag && vf->dmpi->planes[0]==vf->priv->fb_ptr){
	// ok, enough to update the area inside the video, leave the black bands
	// untouched!
	if(x0<vf->priv->exp_x){
	    int tmp=vf->priv->exp_x-x0;
	    w-=tmp; src+=tmp; srca+=tmp; x0+=tmp;
	}
	if(y0<vf->priv->exp_y){
	    int tmp=vf->priv->exp_y-y0;
	    h-=tmp; src+=tmp*stride; srca+=tmp*stride; y0+=tmp;
	}
	if(x0+w>vf->priv->exp_x+orig_w){
	    w=vf->priv->exp_x+orig_w-x0;
	}
	if(y0+h>vf->priv->exp_y+orig_h){
	    h=vf->priv->exp_y+orig_h-y0;
	}
    }
    if(w<=0 || h<=0) return; // nothing to do...
//    printf("OSD redraw: %d;%d %dx%d  \n",x0,y0,w,h);
    dst=vf->dmpi->planes[0]+
			vf->dmpi->stride[0]*y0+
			(vf->dmpi->bpp>>3)*x0;
    switch(vf->dmpi->imgfmt){
    case IMGFMT_BGR15:
    case IMGFMT_RGB15:
	vo_draw_alpha_rgb15(w,h,src,srca,stride,dst,vf->dmpi->stride[0]);
	break;
    case IMGFMT_BGR16:
    case IMGFMT_RGB16:
	vo_draw_alpha_rgb16(w,h,src,srca,stride,dst,vf->dmpi->stride[0]);
	break;
    case IMGFMT_BGR24:
    case IMGFMT_RGB24:
	vo_draw_alpha_rgb24(w,h,src,srca,stride,dst,vf->dmpi->stride[0]);
	break;
    case IMGFMT_BGR32:
    case IMGFMT_RGB32:
	vo_draw_alpha_rgb32(w,h,src,srca,stride,dst,vf->dmpi->stride[0]);
	break;
    case IMGFMT_YV12:
    case IMGFMT_I420:
    case IMGFMT_IYUV:
    case IMGFMT_YVU9:
    case IMGFMT_IF09:
    case IMGFMT_Y800:
    case IMGFMT_Y8:
	vo_draw_alpha_yv12(w,h,src,srca,stride,dst,vf->dmpi->stride[0]);
	break;
    case IMGFMT_YUY2:
	vo_draw_alpha_yuy2(w,h,src,srca,stride,dst,vf->dmpi->stride[0]);
	break;
    case IMGFMT_UYVY:
	vo_draw_alpha_yuy2(w,h,src,srca,stride,dst+1,vf->dmpi->stride[0]);
	break;
    }
}

static void draw_osd(struct vf_instance_s* vf_,int w,int h){
    vf=vf_;orig_w=w;orig_h=h;
//    printf("======================================\n");
    if(vf->priv->exp_w!=w || vf->priv->exp_h!=h ||
	vf->priv->exp_x || vf->priv->exp_y){
	// yep, we're expanding image, not just copy.
	if(vf->dmpi->planes[0]!=vf->priv->fb_ptr){
	    // double buffering, so we need full clear :(
	    if (vf->priv->exp_y > 0)
		remove_func_2(0,0,vf->priv->exp_w,vf->priv->exp_y);
	    if (vf->priv->exp_y+h < vf->priv->exp_h)
		remove_func_2(0,vf->priv->exp_y+h,vf->priv->exp_w,vf->priv->exp_h-h-vf->priv->exp_y);
	} else {
	    // partial clear:
	    vo_remove_text(vf->priv->exp_w,vf->priv->exp_h,remove_func);
	}
    }
    vo_draw_text(vf->priv->exp_w,vf->priv->exp_h,draw_func);
    // save buffer pointer for double buffering detection - yes, i know it's
    // ugly method, but note that codecs with DR support does the same...
    if(vf->dmpi)
      vf->priv->fb_ptr=vf->dmpi->planes[0];
}

#endif
//===========================================================================//

static int config(struct vf_instance_s* vf,
        int width, int height, int d_width, int d_height,
	unsigned int flags, unsigned int outfmt){
    if(outfmt == IMGFMT_MPEGPES) {
      vf->priv->passthrough = 1;
      return vf_next_config(vf,width,height,d_width,d_height,flags,outfmt);
    }
    if (outfmt == IMGFMT_IF09) return 0;
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
    vf->priv->fb_ptr=NULL;

    if(!opt_screen_size_x && !opt_screen_size_y){
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

static void get_image(struct vf_instance_s* vf, mp_image_t *mpi){
//    if(mpi->type==MP_IMGTYPE_IPB) return; // not yet working
#ifdef OSD_SUPPORT
    if(vf->priv->osd && (mpi->flags&MP_IMGFLAG_PRESERVE)){
	// check if we have to render osd!
	vo_update_osd(vf->priv->exp_w, vf->priv->exp_h);
	if(vo_osd_check_range_update(vf->priv->exp_x,vf->priv->exp_y,
	    vf->priv->exp_x+mpi->w,vf->priv->exp_y+mpi->h)) return;
    }
#endif
    if(vf->priv->exp_w==mpi->width ||
       (mpi->flags&(MP_IMGFLAG_ACCEPT_STRIDE|MP_IMGFLAG_ACCEPT_WIDTH)) ){
	// try full DR !
	mpi->priv=vf->dmpi=vf_get_image(vf->next,mpi->imgfmt,
	    mpi->type, mpi->flags, 
            MAX(vf->priv->exp_w, mpi->width +vf->priv->exp_x), 
            MAX(vf->priv->exp_h, mpi->height+vf->priv->exp_y));
#if 1
	if((vf->dmpi->flags & MP_IMGFLAG_DRAW_CALLBACK) &&
	  !(vf->dmpi->flags & MP_IMGFLAG_DIRECT)){
	    mp_msg(MSGT_VFILTER, MSGL_INFO, MSGTR_MPCODECS_FullDRNotPossible);
	    return;
	}
#endif
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

static void start_slice(struct vf_instance_s* vf, mp_image_t *mpi){
//    printf("start_slice called! flag=%d\n",mpi->flags&MP_IMGFLAG_DRAW_CALLBACK);
    if(!vf->next->draw_slice){
	mpi->flags&=~MP_IMGFLAG_DRAW_CALLBACK;
	return;
    }
    // they want slices!!! allocate the buffer.
    if(!mpi->priv)
	mpi->priv=vf->dmpi=vf_get_image(vf->next,mpi->imgfmt,
//	MP_IMGTYPE_TEMP, MP_IMGFLAG_ACCEPT_STRIDE | MP_IMGFLAG_PREFER_ALIGNED_STRIDE,
	    MP_IMGTYPE_TEMP, mpi->flags, 
            MAX(vf->priv->exp_w, mpi->width +vf->priv->exp_x), 
            MAX(vf->priv->exp_h, mpi->height+vf->priv->exp_y));
    if(!(vf->dmpi->flags&MP_IMGFLAG_DRAW_CALLBACK))
	mp_msg(MSGT_VFILTER, MSGL_WARN, MSGTR_MPCODECS_WarnNextFilterDoesntSupportSlices); // shouldn't happen.
    vf->priv->first_slice = 1;
}

static void draw_top_blackbar_slice(struct vf_instance_s* vf,
				    unsigned char** src, int* stride, int w,int h, int x, int y){
    if(vf->priv->exp_y>0 && y == 0) {
	vf_next_draw_slice(vf, vf->dmpi->planes, vf->dmpi->stride,
			   vf->dmpi->w,vf->priv->exp_y,0,0);
    }
    
}

static void draw_bottom_blackbar_slice(struct vf_instance_s* vf,
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

static void draw_slice(struct vf_instance_s* vf,
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

static int put_image(struct vf_instance_s* vf, mp_image_t *mpi, double pts){
    if (vf->priv->passthrough) {
      mp_image_t *dmpi = vf_get_image(vf->next, IMGFMT_MPEGPES,
                                      MP_IMGTYPE_EXPORT, 0, mpi->w, mpi->h);
      dmpi->planes[0]=mpi->planes[0];
      return vf_next_put_image(vf,dmpi, pts);
    }

    if(mpi->flags&MP_IMGFLAG_DIRECT || mpi->flags&MP_IMGFLAG_DRAW_CALLBACK){
	vf->dmpi=mpi->priv;
	if(!vf->dmpi) { mp_msg(MSGT_VFILTER, MSGL_WARN, MSGTR_MPCODECS_FunWhydowegetNULL); return 0; }
	mpi->priv=NULL;
#ifdef OSD_SUPPORT
	if(vf->priv->osd) draw_osd(vf,mpi->w,mpi->h);
#endif
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
#ifdef OSD_SUPPORT
    if(vf->priv->osd) draw_osd(vf,mpi->w,mpi->h);
#endif
    return vf_next_put_image(vf,vf->dmpi, pts);
}

//===========================================================================//

static int control(struct vf_instance_s* vf, int request, void* data){
#ifdef OSD_SUPPORT
    switch(request){
    case VFCTRL_DRAW_OSD:
	if(vf->priv->osd) return CONTROL_TRUE;
    }
#endif
    return vf_next_control(vf,request,data);
}

static int query_format(struct vf_instance_s* vf, unsigned int fmt){
  return (vf_next_query_format(vf,fmt));
}

static int open(vf_instance_t *vf, char* args){
    vf->config=config;
    vf->control=control;
    vf->query_format=query_format;
    vf->start_slice=start_slice;
    vf->draw_slice=draw_slice;
    vf->get_image=get_image;
    vf->put_image=put_image;
    mp_msg(MSGT_VFILTER, MSGL_INFO, "Expand: %d x %d, %d ; %d, osd: %d, aspect: %lf, round: %d\n",
    vf->priv->exp_w,
    vf->priv->exp_h,
    vf->priv->exp_x,
    vf->priv->exp_y,
    vf->priv->osd,
    vf->priv->aspect,
    vf->priv->round);
    return 1;
}

#define ST_OFF(f) M_ST_OFF(struct vf_priv_s,f)
static m_option_t vf_opts_fields[] = {
  {"w", ST_OFF(exp_w), CONF_TYPE_INT, 0, 0 ,0, NULL},
  {"h", ST_OFF(exp_h), CONF_TYPE_INT, 0, 0 ,0, NULL},
  {"x", ST_OFF(exp_x), CONF_TYPE_INT, M_OPT_MIN, -1, 0, NULL},
  {"y", ST_OFF(exp_y), CONF_TYPE_INT, M_OPT_MIN, -1, 0, NULL},
  {"osd", ST_OFF(osd), CONF_TYPE_FLAG, 0 , 0, 1, NULL},
  {"aspect", ST_OFF(aspect), CONF_TYPE_DOUBLE, M_OPT_MIN, 0, 0, NULL},
  {"round", ST_OFF(round), CONF_TYPE_INT, M_OPT_MIN, 1, 0, NULL},
  { NULL, NULL, 0, 0, 0, 0,  NULL }
};

static m_struct_t vf_opts = {
  "expand",
  sizeof(struct vf_priv_s),
  &vf_priv_dflt,
  vf_opts_fields
};



vf_info_t vf_info_expand = {
#ifdef OSD_SUPPORT
    "expanding & osd",
#else
    "expanding",
#endif
    "expand",
    "A'rpi",
    "",
    open,
    &vf_opts
};

//===========================================================================//
