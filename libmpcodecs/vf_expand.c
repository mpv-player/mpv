#define OSD_SUPPORT

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../config.h"
#include "../mp_msg.h"

#include "img_format.h"
#include "mp_image.h"
#include "vf.h"

#include "../libvo/fastmemcpy.h"

#ifdef OSD_SUPPORT
#include "../libvo/sub.h"
#include "../libvo/osd.h"
#endif

struct vf_priv_s {
    int exp_w,exp_h;
    int exp_x,exp_y;
    mp_image_t *dmpi;
    int osd;
    unsigned char* fb_ptr;
};

//===========================================================================//
#ifdef OSD_SUPPORT

static struct vf_instance_s* vf=NULL; // fixme (needs sub.c changes)
static int orig_w,orig_h;

static void remove_func_2(int x0,int y0, int w,int h){
    // TODO: let's cleanup the place
    //printf("OSD clear: %d;%d %dx%d  \n",x0,y0,w,h);
    vf_mpi_clear(vf->priv->dmpi,x0,y0,w,h);
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
    if(!vo_osd_changed_flag && vf->priv->dmpi->planes[0]==vf->priv->fb_ptr){
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
    dst=vf->priv->dmpi->planes[0]+
			vf->priv->dmpi->stride[0]*y0+
			(vf->priv->dmpi->bpp>>3)*x0;
    switch(vf->priv->dmpi->imgfmt){
    case IMGFMT_BGR15:
    case IMGFMT_RGB15:
	vo_draw_alpha_rgb15(w,h,src,srca,stride,dst,vf->priv->dmpi->stride[0]);
	break;
    case IMGFMT_BGR16:
    case IMGFMT_RGB16:
	vo_draw_alpha_rgb16(w,h,src,srca,stride,dst,vf->priv->dmpi->stride[0]);
	break;
    case IMGFMT_BGR24:
    case IMGFMT_RGB24:
	vo_draw_alpha_rgb24(w,h,src,srca,stride,dst,vf->priv->dmpi->stride[0]);
	break;
    case IMGFMT_BGR32:
    case IMGFMT_RGB32:
	vo_draw_alpha_rgb32(w,h,src,srca,stride,dst,vf->priv->dmpi->stride[0]);
	break;
    case IMGFMT_YV12:
    case IMGFMT_I420:
    case IMGFMT_IYUV:
	vo_draw_alpha_yv12(w,h,src,srca,stride,dst,vf->priv->dmpi->stride[0]);
	break;
    case IMGFMT_YUY2:
	vo_draw_alpha_yuy2(w,h,src,srca,stride,dst,vf->priv->dmpi->stride[0]);
	break;
    case IMGFMT_UYVY:
	vo_draw_alpha_yuy2(w,h,src,srca,stride,dst+1,vf->priv->dmpi->stride[0]);
	break;
    }
}

static void draw_osd(struct vf_instance_s* vf_,int w,int h){
    vf=vf_;orig_w=w;orig_h=h;
//    printf("======================================\n");
    if(vf->priv->exp_w!=w || vf->priv->exp_h!=h ||
	vf->priv->exp_x || vf->priv->exp_y){
	// yep, we're expanding image, not just copy.
	if(vf->priv->dmpi->planes[0]!=vf->priv->fb_ptr){
	    // double buffering, so we need full clear :(
	    remove_func(0,0,vf->priv->exp_w,vf->priv->exp_h);
	} else {
	    // partial clear:
	    vo_remove_text(vf->priv->exp_w,vf->priv->exp_h,remove_func);
	}
    }
    vo_draw_text(vf->priv->exp_w,vf->priv->exp_h,draw_func);
    // save buffer pointer for double buffering detection - yes, i know it's
    // ugly method, but note that codecs with DR support does the same...
    vf->priv->fb_ptr=vf->priv->dmpi->planes[0];
}

#endif
//===========================================================================//

static int config(struct vf_instance_s* vf,
        int width, int height, int d_width, int d_height,
	unsigned int flags, unsigned int outfmt){
    int ret;
    // calculate the missing parameters:
    if(vf->priv->exp_w<width) vf->priv->exp_w=width;
    if(vf->priv->exp_h<height) vf->priv->exp_h=height;
    if(vf->priv->exp_x<0 || vf->priv->exp_x+width>vf->priv->exp_w) vf->priv->exp_x=(vf->priv->exp_w-width)/2;
    if(vf->priv->exp_y<0 || vf->priv->exp_y+height>vf->priv->exp_h) vf->priv->exp_y=(vf->priv->exp_h-height)/2;
    vf->priv->fb_ptr=NULL;
    ret=vf_next_config(vf,vf->priv->exp_w,vf->priv->exp_h,d_width,d_height,flags,outfmt);
    return ret;
}

// there are 4 cases:
// codec --DR--> expand --DR--> vo
// codec --DR--> expand -copy-> vo
// codec -copy-> expand --DR--> vo
// codec -copy-> expand -copy-> vo (worst case)

static void get_image(struct vf_instance_s* vf, mp_image_t *mpi){
    if(mpi->type==MP_IMGTYPE_IPB) return; // not yet working
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
	vf->priv->dmpi=vf_get_image(vf->next,mpi->imgfmt,
	    mpi->type, mpi->flags, vf->priv->exp_w, vf->priv->exp_h);
	// set up mpi as a cropped-down image of dmpi:
	if(mpi->flags&MP_IMGFLAG_PLANAR){
	    mpi->planes[0]=vf->priv->dmpi->planes[0]+
		vf->priv->exp_y*vf->priv->dmpi->stride[0]+vf->priv->exp_x;
	    mpi->planes[1]=vf->priv->dmpi->planes[1]+
		(vf->priv->exp_y>>1)*vf->priv->dmpi->stride[1]+(vf->priv->exp_x>>1);
	    mpi->planes[2]=vf->priv->dmpi->planes[2]+
		(vf->priv->exp_y>>1)*vf->priv->dmpi->stride[2]+(vf->priv->exp_x>>1);
	    mpi->stride[1]=vf->priv->dmpi->stride[1];
	    mpi->stride[2]=vf->priv->dmpi->stride[2];
	} else {
	    mpi->planes[0]=vf->priv->dmpi->planes[0]+
		vf->priv->exp_y*vf->priv->dmpi->stride[0]+
		vf->priv->exp_x*(vf->priv->dmpi->bpp/8);
	}
	mpi->stride[0]=vf->priv->dmpi->stride[0];
	mpi->width=vf->priv->dmpi->width;
	mpi->flags|=MP_IMGFLAG_DIRECT;
    }
}

static void put_image(struct vf_instance_s* vf, mp_image_t *mpi){
    if(mpi->flags&MP_IMGFLAG_DIRECT){
	vf_next_put_image(vf,vf->priv->dmpi);
#ifdef OSD_SUPPORT
	if(vf->priv->osd) draw_osd(vf,mpi->w,mpi->h);
#endif
	return; // we've used DR, so we're ready...
    }

    // hope we'll get DR buffer:
    vf->priv->dmpi=vf_get_image(vf->next,mpi->imgfmt,
	MP_IMGTYPE_TEMP, MP_IMGFLAG_ACCEPT_STRIDE,
	vf->priv->exp_w, vf->priv->exp_h);
    
    // copy mpi->dmpi...
    if(mpi->flags&MP_IMGFLAG_PLANAR){
	memcpy_pic(vf->priv->dmpi->planes[0]+
	        vf->priv->exp_y*vf->priv->dmpi->stride[0]+vf->priv->exp_x,
		mpi->planes[0], mpi->w, mpi->h,
		vf->priv->dmpi->stride[0],mpi->stride[0]);
	memcpy_pic(vf->priv->dmpi->planes[1]+
		(vf->priv->exp_y>>1)*vf->priv->dmpi->stride[1]+(vf->priv->exp_x>>1),
		mpi->planes[1], mpi->w>>1, mpi->h>>1,
		vf->priv->dmpi->stride[1],mpi->stride[1]);
	memcpy_pic(vf->priv->dmpi->planes[2]+
		(vf->priv->exp_y>>1)*vf->priv->dmpi->stride[2]+(vf->priv->exp_x>>1),
		mpi->planes[2], mpi->w>>1, mpi->h>>1,
		vf->priv->dmpi->stride[2],mpi->stride[2]);
    } else {
	memcpy_pic(vf->priv->dmpi->planes[0]+
	        vf->priv->exp_y*vf->priv->dmpi->stride[0]+vf->priv->exp_x*(vf->priv->dmpi->bpp/8),
		mpi->planes[0], mpi->w*(vf->priv->dmpi->bpp/8), mpi->h,
		vf->priv->dmpi->stride[0],mpi->stride[0]);
    }
#ifdef OSD_SUPPORT
    if(vf->priv->osd) draw_osd(vf,mpi->w,mpi->h);
#endif
    vf_next_put_image(vf,vf->priv->dmpi);
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

static int open(vf_instance_t *vf, char* args){
    vf->config=config;
    vf->control=control;
    vf->get_image=get_image;
    vf->put_image=put_image;
    vf->priv=malloc(sizeof(struct vf_priv_s));
    // TODO: parse args ->
    vf->priv->exp_x=
    vf->priv->exp_y=
    vf->priv->exp_w=
    vf->priv->exp_h=-1;
    if(args) sscanf(args, "%d:%d:%d:%d:%d", 
    &vf->priv->exp_w,
    &vf->priv->exp_h,
    &vf->priv->exp_x,
    &vf->priv->exp_y,
    &vf->priv->osd);
    printf("Expand: %d x %d, %d ; %d  (-1=autodetect) osd: %d\n",
    vf->priv->exp_w,
    vf->priv->exp_h,
    vf->priv->exp_x,
    vf->priv->exp_y,
    vf->priv->osd);
    return 1;
}

vf_info_t vf_info_expand = {
#ifdef OSD_SUPPORT
    "expanding & osd",
#else
    "expanding",
#endif
    "expand",
    "A'rpi",
    "",
    open
};

//===========================================================================//
