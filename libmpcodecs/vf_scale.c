#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "../config.h"
#include "../mp_msg.h"

#include "img_format.h"
#include "mp_image.h"
#include "vf.h"

#include "../libvo/fastmemcpy.h"
#include "../postproc/swscale.h"

#include "m_option.h"
#include "m_struct.h"

static struct vf_priv_s {
    int w,h;
    int v_chr_drop;
    int param;
    unsigned int fmt;
    struct SwsContext *ctx;
    unsigned char* palette;
    mp_image_t *dmpi;
} vf_priv_dflt = {
  -1,-1,
  0,
  0,
  0,
  NULL,
  NULL,
  NULL
};

extern int opt_screen_size_x;
extern int opt_screen_size_y;

//===========================================================================//

static unsigned int outfmt_list[]={
// RGB:
    IMGFMT_BGR32,
    IMGFMT_RGB32,
    IMGFMT_BGR24,
    IMGFMT_RGB24,
    IMGFMT_BGR16,
    IMGFMT_RGB16,
    IMGFMT_BGR15,
    IMGFMT_RGB15,
    IMGFMT_BGR8,
    IMGFMT_RGB8,
    IMGFMT_BGR4,
    IMGFMT_RGB4,
    IMGFMT_BG4B,
    IMGFMT_RG4B,
    IMGFMT_BGR1,
    IMGFMT_RGB1,
// YUV:
    IMGFMT_444P,
    IMGFMT_422P,
    IMGFMT_YV12,
    IMGFMT_I420,
    IMGFMT_IYUV,
    IMGFMT_YVU9,
    IMGFMT_IF09,
    IMGFMT_411P,
    IMGFMT_Y800,
    IMGFMT_Y8,
    IMGFMT_YUY2,
    0
};

static unsigned int find_best_out(vf_instance_t *vf){
    unsigned int best=0;
    unsigned int* p=outfmt_list;
    // find the best outfmt:
    while(*p){
	int ret=vf_next_query_format(vf,*p);
	mp_msg(MSGT_VFILTER,MSGL_DBG2,"scale: query(%s) -> %d\n",vo_format_name(*p),ret&3);
	if(ret&VFCAP_CSP_SUPPORTED_BY_HW){ best=*p; break;} // no conversion -> bingo!
	if(ret&VFCAP_CSP_SUPPORTED && !best) best=*p; // best with conversion
	++p;
    }
    return best;
}

static int config(struct vf_instance_s* vf,
        int width, int height, int d_width, int d_height,
	unsigned int flags, unsigned int outfmt){
    unsigned int best=find_best_out(vf);
    int vo_flags;
    int int_sws_flags=0;
    SwsFilter *srcFilter, *dstFilter;
    
    if(!best){
	mp_msg(MSGT_VFILTER,MSGL_WARN,"SwScale: no supported outfmt found :(\n");
	return 0;
    }
    
    vo_flags=vf->next->query_format(vf->next,best);
    
    // scaling to dwidth*d_height, if all these TRUE:
    // - option -zoom
    // - no other sw/hw up/down scaling avail.
    // - we're after postproc
    // - user didn't set w:h
    if(!(vo_flags&VFCAP_POSTPROC) && (flags&4) && 
	    vf->priv->w<0 && vf->priv->h<0){	// -zoom
	int x=(vo_flags&VFCAP_SWSCALE) ? 0 : 1;
	if(d_width<width || d_height<height){
	    // downscale!
	    if(vo_flags&VFCAP_HWSCALE_DOWN) x=0;
	} else {
	    // upscale:
	    if(vo_flags&VFCAP_HWSCALE_UP) x=0;
	}
	if(x){
	    // user wants sw scaling! (-zoom)
	    vf->priv->w=d_width;
	    vf->priv->h=d_height;
	}
    }

    // calculate the missing parameters:
    switch(best) {
    case IMGFMT_YUY2:		/* YUY2 needs w rounded to 2 */
	if(vf->priv->w==-3) vf->priv->w=(vf->priv->h*width/height+1)&~1; else
	if(vf->priv->w==-2) vf->priv->w=(vf->priv->h*d_width/d_height+1)&~1;
	if(vf->priv->w<0) vf->priv->w=width; else
	if(vf->priv->w==0) vf->priv->w=d_width;
	if(vf->priv->h==-3) vf->priv->h=vf->priv->w*height/width; else
	if(vf->priv->h==-2) vf->priv->h=vf->priv->w*d_height/d_width;
	break;
    case IMGFMT_YV12:		/* YV12 needs w & h rounded to 2 */
	if(vf->priv->w==-3) vf->priv->w=(vf->priv->h*width/height+1)&~1; else
	if(vf->priv->w==-2) vf->priv->w=(vf->priv->h*d_width/d_height+1)&~1;
	if(vf->priv->w<0) vf->priv->w=width; else
	if(vf->priv->w==0) vf->priv->w=d_width;
	if(vf->priv->h==-3) vf->priv->h=(vf->priv->w*height/width+1)&~1; else
	if(vf->priv->h==-2) vf->priv->h=(vf->priv->w*d_height/d_width+1)&~1;
	break;
    default:
    if(vf->priv->w==-3) vf->priv->w=vf->priv->h*width/height; else
    if(vf->priv->w==-2) vf->priv->w=vf->priv->h*d_width/d_height;
    if(vf->priv->w<0) vf->priv->w=width; else
    if(vf->priv->w==0) vf->priv->w=d_width;
    if(vf->priv->h==-3) vf->priv->h=vf->priv->w*height/width; else
    if(vf->priv->h==-2) vf->priv->h=vf->priv->w*d_height/d_width;
    break;
    }
    
    if(vf->priv->h<0) vf->priv->h=height; else
    if(vf->priv->h==0) vf->priv->h=d_height;
    
    mp_msg(MSGT_VFILTER,MSGL_DBG2,"SwScale: scaling %dx%d %s to %dx%d %s  \n",
	width,height,vo_format_name(outfmt),
	vf->priv->w,vf->priv->h,vo_format_name(best));

    // free old ctx:
    if(vf->priv->ctx) sws_freeContext(vf->priv->ctx);
    
    // new swscaler:
    sws_getFlagsAndFilterFromCmdLine(&int_sws_flags, &srcFilter, &dstFilter);
    int_sws_flags|= vf->priv->v_chr_drop << SWS_SRC_V_CHR_DROP_SHIFT;
    int_sws_flags|= vf->priv->param      << SWS_PARAM_SHIFT;
    vf->priv->ctx=sws_getContext(width,height,
	    (outfmt==IMGFMT_I420 || outfmt==IMGFMT_IYUV)?IMGFMT_YV12:outfmt,
		  vf->priv->w,vf->priv->h,
	    (best==IMGFMT_I420 || best==IMGFMT_IYUV)?IMGFMT_YV12:best,
	    int_sws_flags, srcFilter, dstFilter);
    if(!vf->priv->ctx){
	// error...
	mp_msg(MSGT_VFILTER,MSGL_WARN,"Couldn't init SwScaler for this setup\n");
	return 0;
    }
    vf->priv->fmt=best;

    if(vf->priv->palette){
	free(vf->priv->palette);
	vf->priv->palette=NULL;
    }
    switch(best){
    case IMGFMT_BGR8: {
      /* set 332 palette for 8 bpp */
	int i;
	vf->priv->palette=malloc(4*256);
	for(i=0; i<256; i++){
	    vf->priv->palette[4*i+0]=4*(i&3)*21;
	    vf->priv->palette[4*i+1]=4*((i>>2)&7)*9;
	    vf->priv->palette[4*i+2]=4*((i>>5)&7)*9;
	}
	break; }
    case IMGFMT_BGR4: 
    case IMGFMT_BG4B: {
	int i;
	vf->priv->palette=malloc(4*16);
	for(i=0; i<16; i++){
	    vf->priv->palette[4*i+0]=4*(i&1)*63;
	    vf->priv->palette[4*i+1]=4*((i>>1)&3)*21;
	    vf->priv->palette[4*i+2]=4*((i>>3)&1)*63;
	}
	break; }
    }

    if(!opt_screen_size_x && !opt_screen_size_y){
	d_width=d_width*vf->priv->w/width;
	d_height=d_height*vf->priv->h/height;
    }
    return vf_next_config(vf,vf->priv->w,vf->priv->h,d_width,d_height,flags,best);
}

static void start_slice(struct vf_instance_s* vf, mp_image_t *mpi){
//    printf("start_slice called! flag=%d\n",mpi->flags&MP_IMGFLAG_DRAW_CALLBACK);
    if(!(mpi->flags&MP_IMGFLAG_DRAW_CALLBACK)) return; // shouldn't happen
    // they want slices!!! allocate the buffer.
    mpi->priv=vf->priv->dmpi=vf_get_image(vf->next,vf->priv->fmt,
//	mpi->type, mpi->flags & (~MP_IMGFLAG_DRAW_CALLBACK),
	MP_IMGTYPE_TEMP, MP_IMGFLAG_ACCEPT_STRIDE | MP_IMGFLAG_PREFER_ALIGNED_STRIDE,
	vf->priv->w, vf->priv->h);
}

static void draw_slice(struct vf_instance_s* vf,
        unsigned char** src, int* stride, int w,int h, int x, int y){
    mp_image_t *dmpi=vf->priv->dmpi;
    if(!dmpi){
	mp_msg(MSGT_VFILTER,MSGL_FATAL,"vf_scale: draw_slice() called with dmpi=NULL (no get_image??)\n");
	return;
    }
//    printf("vf_scale::draw_slice() y=%d h=%d\n",y,h);
    sws_scale(vf->priv->ctx,src,stride,y,h,dmpi->planes,dmpi->stride);
}

static int put_image(struct vf_instance_s* vf, mp_image_t *mpi){
    mp_image_t *dmpi=mpi->priv;

//    printf("vf_scale::put_image(): processing whole frame! dmpi=%p flag=%d\n",
//	dmpi, (mpi->flags&MP_IMGFLAG_DRAW_CALLBACK));
    
  if(!(mpi->flags&MP_IMGFLAG_DRAW_CALLBACK && dmpi)){
  
    // hope we'll get DR buffer:
    dmpi=vf_get_image(vf->next,vf->priv->fmt,
	MP_IMGTYPE_TEMP, MP_IMGFLAG_ACCEPT_STRIDE | MP_IMGFLAG_PREFER_ALIGNED_STRIDE,
	vf->priv->w, vf->priv->h);
    sws_scale(vf->priv->ctx,mpi->planes,mpi->stride,0,mpi->h,dmpi->planes,dmpi->stride);

  }

    if(vf->priv->w==mpi->w && vf->priv->h==mpi->h){
	// just conversion, no scaling -> keep postprocessing data
	// this way we can apply pp filter to non-yv12 source using scaler
	dmpi->qscale=mpi->qscale;
	dmpi->qstride=mpi->qstride;
    }

    if(vf->priv->palette) dmpi->planes[1]=vf->priv->palette; // export palette!
    
    return vf_next_put_image(vf,dmpi);
}

static int control(struct vf_instance_s* vf, int request, void* data){
    int *table;
    int *inv_table;
    int r;
    int brightness, contrast, saturation, srcRange, dstRange;
    vf_equalizer_t *eq;

    switch(request){
    case VFCTRL_GET_EQUALIZER:
	r= sws_getColorspaceDetails(vf->priv->ctx, &inv_table, &srcRange, &table, &dstRange, &brightness, &contrast, &saturation);
	if(r<0) break;

	eq = data;
	if (!strcmp(eq->item,"brightness")) {
		eq->value =  ((brightness*100) + (1<<15))>>16;
	}
	else if (!strcmp(eq->item,"contrast")) {
		eq->value = (((contrast  *100) + (1<<15))>>16) - 100;
	}
	else if (!strcmp(eq->item,"saturation")) {
		eq->value = (((saturation*100) + (1<<15))>>16) - 100;
	}
	else
		break;
	return CONTROL_TRUE;
    case VFCTRL_SET_EQUALIZER:
	r= sws_getColorspaceDetails(vf->priv->ctx, &inv_table, &srcRange, &table, &dstRange, &brightness, &contrast, &saturation);
	if(r<0) break;
//printf("set %f %f %f\n", brightness/(float)(1<<16), contrast/(float)(1<<16), saturation/(float)(1<<16));
	eq = data;

	if (!strcmp(eq->item,"brightness")) {
		brightness = (( eq->value     <<16) + 50)/100;
	}
	else if (!strcmp(eq->item,"contrast")) {
		contrast   = (((eq->value+100)<<16) + 50)/100;
	}
	else if (!strcmp(eq->item,"saturation")) {
		saturation = (((eq->value+100)<<16) + 50)/100;
	}
	else
		break;

	r= sws_setColorspaceDetails(vf->priv->ctx, inv_table, srcRange, table, dstRange, brightness, contrast, saturation);
	if(r<0) break;

	return CONTROL_TRUE;
    default:
    }
    
    return vf_next_control(vf,request,data);
}

//===========================================================================//

//  supported Input formats: YV12, I420, IYUV, YUY2, UYVY, BGR32, BGR24, BGR16, BGR15, RGB32, RGB24, Y8, Y800

static int query_format(struct vf_instance_s* vf, unsigned int fmt){
    switch(fmt){
    case IMGFMT_YV12:
    case IMGFMT_I420:
    case IMGFMT_IYUV:
    case IMGFMT_UYVY:
    case IMGFMT_YUY2:
    case IMGFMT_BGR32:
    case IMGFMT_BGR24:
    case IMGFMT_BGR16:
    case IMGFMT_BGR15:
    case IMGFMT_RGB32:
    case IMGFMT_RGB24:
    case IMGFMT_Y800: 
    case IMGFMT_Y8: 
    case IMGFMT_YVU9: 
    case IMGFMT_IF09: 
    case IMGFMT_444P: 
    case IMGFMT_422P: 
    case IMGFMT_411P: 
    {
	unsigned int best=find_best_out(vf);
	int flags;
	if(!best) return 0;	 // no matching out-fmt
	flags=vf_next_query_format(vf,best);
	if(!(flags&3)) return 0; // huh?
	if(fmt!=best) flags&=~VFCAP_CSP_SUPPORTED_BY_HW;
	// do not allow scaling, if we are before the PP fliter!
	if(!(flags&VFCAP_POSTPROC)) flags|=VFCAP_SWSCALE;
	return flags;
      }
    }
    return 0;	// nomatching in-fmt
}

static int open(vf_instance_t *vf, char* args){
    vf->config=config;
    vf->start_slice=start_slice;
    vf->draw_slice=draw_slice;
    vf->put_image=put_image;
    vf->query_format=query_format;
    vf->control= control;
    if(!vf->priv) {
    vf->priv=malloc(sizeof(struct vf_priv_s));
    // TODO: parse args ->
    vf->priv->ctx=NULL;
    vf->priv->w=
    vf->priv->h=-1;
    vf->priv->v_chr_drop=0;
    vf->priv->param=0;
    vf->priv->palette=NULL;
    } // if(!vf->priv)
    if(args) sscanf(args, "%d:%d:%d:%d",
    &vf->priv->w,
    &vf->priv->h,
    &vf->priv->v_chr_drop,
    &vf->priv->param);
    mp_msg(MSGT_VFILTER,MSGL_V,"SwScale params: %d x %d (-1=no scaling)\n",
    vf->priv->w,
    vf->priv->h);
    return 1;
}

/// An example of presets usage
static struct size_preset {
  char* name;
  int w, h;
} vf_size_presets_defs[] = {
  // TODO add more 'standard' resolutions
  { "pal", 768, 576 },
  { NULL, 0, 0}
};

#define ST_OFF(f) M_ST_OFF(struct size_preset,f)
static m_option_t vf_size_preset_fields[] = {
  {"w", ST_OFF(w), CONF_TYPE_INT, M_OPT_MIN,1 ,0, NULL},
  {"h", ST_OFF(h), CONF_TYPE_INT, M_OPT_MIN,1 ,0, NULL},
  { NULL, NULL, 0, 0, 0, 0,  NULL }
};

static m_struct_t vf_size_preset = {
  "scale_size_preset",
  sizeof(struct size_preset),
  NULL,
  vf_size_preset_fields
};

static m_struct_t vf_opts;
static m_obj_presets_t size_preset = {
  &vf_size_preset, // Input struct desc
  &vf_opts, // Output struct desc
  vf_size_presets_defs, // The list of presets
  ST_OFF(name) // At wich offset is the name field in the preset struct
};

/// Now the options
#undef ST_OFF
#define ST_OFF(f) M_ST_OFF(struct vf_priv_s,f)
static m_option_t vf_opts_fields[] = {
  {"w", ST_OFF(w), CONF_TYPE_INT, M_OPT_MIN,-3 ,0, NULL},
  {"h", ST_OFF(h), CONF_TYPE_INT, M_OPT_MIN,-3 ,0, NULL},
  {"chr-drop", ST_OFF(v_chr_drop), CONF_TYPE_INT, M_OPT_RANGE, 0, 3, NULL},
  {"param", ST_OFF(param), CONF_TYPE_INT, M_OPT_RANGE, 0, 100, NULL},
  // Note that here the 2 field is NULL (ie 0)
  // As we want this option to act on the option struct itself
  {"presize", 0, CONF_TYPE_OBJ_PRESETS, 0, 0, 0, &size_preset},
  { NULL, NULL, 0, 0, 0, 0,  NULL }
};

static m_struct_t vf_opts = {
  "scale",
  sizeof(struct vf_priv_s),
  &vf_priv_dflt,
  vf_opts_fields
};

vf_info_t vf_info_scale = {
    "software scaling",
    "scale",
    "A'rpi",
    "",
    open,
    &vf_opts
};

//===========================================================================//
