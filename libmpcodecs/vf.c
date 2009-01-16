#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif

#ifdef MP_DEBUG
#include <assert.h>
#endif

#include "mp_msg.h"
#include "help_mp.h"
#include "m_option.h"
#include "m_struct.h"


#include "img_format.h"
#include "mp_image.h"
#include "vf.h"

#include "libvo/fastmemcpy.h"

extern const vf_info_t vf_info_vo;
extern const vf_info_t vf_info_rectangle;
extern const vf_info_t vf_info_bmovl;
extern const vf_info_t vf_info_crop;
extern const vf_info_t vf_info_expand;
extern const vf_info_t vf_info_pp;
extern const vf_info_t vf_info_scale;
extern const vf_info_t vf_info_format;
extern const vf_info_t vf_info_noformat;
extern const vf_info_t vf_info_yuy2;
extern const vf_info_t vf_info_flip;
extern const vf_info_t vf_info_rgb2bgr;
extern const vf_info_t vf_info_rotate;
extern const vf_info_t vf_info_mirror;
extern const vf_info_t vf_info_palette;
extern const vf_info_t vf_info_lavc;
extern const vf_info_t vf_info_zrmjpeg;
extern const vf_info_t vf_info_dvbscale;
extern const vf_info_t vf_info_cropdetect;
extern const vf_info_t vf_info_test;
extern const vf_info_t vf_info_noise;
extern const vf_info_t vf_info_yvu9;
extern const vf_info_t vf_info_lavcdeint;
extern const vf_info_t vf_info_eq;
extern const vf_info_t vf_info_eq2;
extern const vf_info_t vf_info_halfpack;
extern const vf_info_t vf_info_dint;
extern const vf_info_t vf_info_1bpp;
extern const vf_info_t vf_info_2xsai;
extern const vf_info_t vf_info_unsharp;
extern const vf_info_t vf_info_swapuv;
extern const vf_info_t vf_info_il;
extern const vf_info_t vf_info_fil;
extern const vf_info_t vf_info_boxblur;
extern const vf_info_t vf_info_sab;
extern const vf_info_t vf_info_smartblur;
extern const vf_info_t vf_info_perspective;
extern const vf_info_t vf_info_down3dright;
extern const vf_info_t vf_info_field;
extern const vf_info_t vf_info_denoise3d;
extern const vf_info_t vf_info_hqdn3d;
extern const vf_info_t vf_info_detc;
extern const vf_info_t vf_info_telecine;
extern const vf_info_t vf_info_tinterlace;
extern const vf_info_t vf_info_tfields;
extern const vf_info_t vf_info_ivtc;
extern const vf_info_t vf_info_ilpack;
extern const vf_info_t vf_info_dsize;
extern const vf_info_t vf_info_decimate;
extern const vf_info_t vf_info_softpulldown;
extern const vf_info_t vf_info_pullup;
extern const vf_info_t vf_info_filmdint;
extern const vf_info_t vf_info_framestep;
extern const vf_info_t vf_info_tile;
extern const vf_info_t vf_info_delogo;
extern const vf_info_t vf_info_remove_logo;
extern const vf_info_t vf_info_hue;
extern const vf_info_t vf_info_spp;
extern const vf_info_t vf_info_uspp;
extern const vf_info_t vf_info_fspp;
extern const vf_info_t vf_info_pp7;
extern const vf_info_t vf_info_yuvcsp;
extern const vf_info_t vf_info_kerndeint;
extern const vf_info_t vf_info_rgbtest;
extern const vf_info_t vf_info_qp;
extern const vf_info_t vf_info_phase;
extern const vf_info_t vf_info_divtc;
extern const vf_info_t vf_info_harddup;
extern const vf_info_t vf_info_softskip;
extern const vf_info_t vf_info_screenshot;
extern const vf_info_t vf_info_ass;
extern const vf_info_t vf_info_mcdeint;
extern const vf_info_t vf_info_yadif;
extern const vf_info_t vf_info_blackframe;
extern const vf_info_t vf_info_geq;
extern const vf_info_t vf_info_ow;

// list of available filters:
static const vf_info_t* const filter_list[]={
    &vf_info_rectangle,
#ifdef HAVE_POSIX_SELECT
    &vf_info_bmovl,
#endif
    &vf_info_crop,
    &vf_info_expand,
#ifdef CONFIG_LIBPOSTPROC
    &vf_info_pp,
#endif
    &vf_info_scale,
//    &vf_info_osd,
    &vf_info_vo,
    &vf_info_format,
    &vf_info_noformat,
    &vf_info_yuy2,
    &vf_info_flip,
    &vf_info_rgb2bgr,
    &vf_info_rotate,
    &vf_info_mirror,
    &vf_info_palette,
    &vf_info_pp7,
#ifdef CONFIG_LIBAVCODEC
    &vf_info_lavc,
    &vf_info_lavcdeint,
    &vf_info_screenshot,
#endif
#ifdef CONFIG_ZR
    &vf_info_zrmjpeg,
#endif
    &vf_info_dvbscale,
    &vf_info_cropdetect,
    &vf_info_test,
    &vf_info_noise,
    &vf_info_yvu9,
    &vf_info_eq,
    &vf_info_eq2,
    &vf_info_halfpack,
    &vf_info_dint,
    &vf_info_1bpp,
    &vf_info_2xsai,
    &vf_info_unsharp,
    &vf_info_swapuv,
    &vf_info_il,
    &vf_info_fil,
    &vf_info_boxblur,
    &vf_info_sab,
    &vf_info_smartblur,
    &vf_info_perspective,
    &vf_info_down3dright,
    &vf_info_field,
    &vf_info_denoise3d,
    &vf_info_hqdn3d,
    &vf_info_detc,
    &vf_info_telecine,
    &vf_info_tinterlace,
    &vf_info_tfields,
    &vf_info_ivtc,
    &vf_info_ilpack,
    &vf_info_dsize,
    &vf_info_decimate,
    &vf_info_softpulldown,
    &vf_info_pullup,
    &vf_info_filmdint,
    &vf_info_framestep,
    &vf_info_tile,
    &vf_info_delogo,
    &vf_info_remove_logo,
    &vf_info_hue,
#ifdef CONFIG_LIBAVCODEC_A
    &vf_info_spp,
    &vf_info_uspp,
    &vf_info_fspp,
    &vf_info_qp,
    &vf_info_mcdeint,
    &vf_info_geq,
#endif
    &vf_info_yuvcsp,
    &vf_info_kerndeint,
    &vf_info_rgbtest,
    &vf_info_phase,
    &vf_info_divtc,
    &vf_info_harddup,
    &vf_info_softskip,
#ifdef CONFIG_ASS
    &vf_info_ass,
#endif
    &vf_info_yadif,
    &vf_info_blackframe,
    &vf_info_ow,
    NULL
};

// For the vf option
m_obj_settings_t* vf_settings = NULL;
const m_obj_list_t vf_obj_list = {
  (void**)filter_list,
  M_ST_OFF(vf_info_t,name),
  M_ST_OFF(vf_info_t,info),
  M_ST_OFF(vf_info_t,opts)
};

//============================================================================
// mpi stuff:

void vf_mpi_clear(mp_image_t* mpi,int x0,int y0,int w,int h){
    int y;
    if(mpi->flags&MP_IMGFLAG_PLANAR){
	y0&=~1;h+=h&1;
	if(x0==0 && w==mpi->width){
	    // full width clear:
	    memset(mpi->planes[0]+mpi->stride[0]*y0,0,mpi->stride[0]*h);
	    memset(mpi->planes[1]+mpi->stride[1]*(y0>>mpi->chroma_y_shift),128,mpi->stride[1]*(h>>mpi->chroma_y_shift));
	    memset(mpi->planes[2]+mpi->stride[2]*(y0>>mpi->chroma_y_shift),128,mpi->stride[2]*(h>>mpi->chroma_y_shift));
	} else
	for(y=y0;y<y0+h;y+=2){
	    memset(mpi->planes[0]+x0+mpi->stride[0]*y,0,w);
	    memset(mpi->planes[0]+x0+mpi->stride[0]*(y+1),0,w);
	    memset(mpi->planes[1]+(x0>>mpi->chroma_x_shift)+mpi->stride[1]*(y>>mpi->chroma_y_shift),128,(w>>mpi->chroma_x_shift));
	    memset(mpi->planes[2]+(x0>>mpi->chroma_x_shift)+mpi->stride[2]*(y>>mpi->chroma_y_shift),128,(w>>mpi->chroma_x_shift));
	}
	return;
    }
    // packed:
    for(y=y0;y<y0+h;y++){
	unsigned char* dst=mpi->planes[0]+mpi->stride[0]*y+(mpi->bpp>>3)*x0;
	if(mpi->flags&MP_IMGFLAG_YUV){
	    unsigned int* p=(unsigned int*) dst;
	    int size=(mpi->bpp>>3)*w/4;
	    int i;
#ifdef WORDS_BIGENDIAN
#define CLEAR_PACKEDYUV_PATTERN 0x00800080
#define CLEAR_PACKEDYUV_PATTERN_SWAPPED 0x80008000
#else
#define CLEAR_PACKEDYUV_PATTERN 0x80008000
#define CLEAR_PACKEDYUV_PATTERN_SWAPPED 0x00800080
#endif
	    if(mpi->flags&MP_IMGFLAG_SWAPPED){
	        for(i=0;i<size-3;i+=4) p[i]=p[i+1]=p[i+2]=p[i+3]=CLEAR_PACKEDYUV_PATTERN_SWAPPED;
		for(;i<size;i++) p[i]=CLEAR_PACKEDYUV_PATTERN_SWAPPED;
	    } else {
	        for(i=0;i<size-3;i+=4) p[i]=p[i+1]=p[i+2]=p[i+3]=CLEAR_PACKEDYUV_PATTERN;
		for(;i<size;i++) p[i]=CLEAR_PACKEDYUV_PATTERN;
	    }
	} else
	    memset(dst,0,(mpi->bpp>>3)*w);
    }
}

mp_image_t* vf_get_image(vf_instance_t* vf, unsigned int outfmt, int mp_imgtype, int mp_imgflag, int w, int h){
  mp_image_t* mpi=NULL;
  int w2;

#ifdef MP_DEBUG
  assert(w == -1 || w >= vf->w);
  assert(h == -1 || h >= vf->h);
  assert(vf->w > 0);
  assert(vf->h > 0);
#endif

//  fprintf(stderr, "get_image: %d:%d, vf: %d:%d\n", w,h,vf->w,vf->h);

  if (w == -1) w = vf->w;
  if (h == -1) h = vf->h;

  w2=(mp_imgflag&MP_IMGFLAG_ACCEPT_ALIGNED_STRIDE)?((w+15)&(~15)):w;
  
  if(vf->put_image==vf_next_put_image){
      // passthru mode, if the filter uses the fallback/default put_image() code
      return vf_get_image(vf->next,outfmt,mp_imgtype,mp_imgflag,w,h);
  }
  
  // Note: we should call libvo first to check if it supports direct rendering
  // and if not, then fallback to software buffers:
  switch(mp_imgtype){
  case MP_IMGTYPE_EXPORT:
    if(!vf->imgctx.export_images[0]) vf->imgctx.export_images[0]=new_mp_image(w2,h);
    mpi=vf->imgctx.export_images[0];
    break;
  case MP_IMGTYPE_STATIC:
    if(!vf->imgctx.static_images[0]) vf->imgctx.static_images[0]=new_mp_image(w2,h);
    mpi=vf->imgctx.static_images[0];
    break;
  case MP_IMGTYPE_TEMP:
    if(!vf->imgctx.temp_images[0]) vf->imgctx.temp_images[0]=new_mp_image(w2,h);
    mpi=vf->imgctx.temp_images[0];
    break;
  case MP_IMGTYPE_IPB:
    if(!(mp_imgflag&MP_IMGFLAG_READABLE)){ // B frame:
      if(!vf->imgctx.temp_images[0]) vf->imgctx.temp_images[0]=new_mp_image(w2,h);
      mpi=vf->imgctx.temp_images[0];
      break;
    }
  case MP_IMGTYPE_IP:
    if(!vf->imgctx.static_images[vf->imgctx.static_idx]) vf->imgctx.static_images[vf->imgctx.static_idx]=new_mp_image(w2,h);
    mpi=vf->imgctx.static_images[vf->imgctx.static_idx];
    vf->imgctx.static_idx^=1;
    break;
  }
  if(mpi){
    mpi->type=mp_imgtype;
    mpi->w=vf->w; mpi->h=vf->h;
    // keep buffer allocation status & color flags only:
//    mpi->flags&=~(MP_IMGFLAG_PRESERVE|MP_IMGFLAG_READABLE|MP_IMGFLAG_DIRECT);
    mpi->flags&=MP_IMGFLAG_ALLOCATED|MP_IMGFLAG_TYPE_DISPLAYED|MP_IMGFLAGMASK_COLORS;
    // accept restrictions & draw_slice flags only:
    mpi->flags|=mp_imgflag&(MP_IMGFLAGMASK_RESTRICTIONS|MP_IMGFLAG_DRAW_CALLBACK);
    if(!vf->draw_slice) mpi->flags&=~MP_IMGFLAG_DRAW_CALLBACK;
    if(mpi->width!=w2 || mpi->height!=h){
//	printf("vf.c: MPI parameters changed!  %dx%d -> %dx%d   \n", mpi->width,mpi->height,w2,h);
	if(mpi->flags&MP_IMGFLAG_ALLOCATED){
	    if(mpi->width<w2 || mpi->height<h){
		// need to re-allocate buffer memory:
		free(mpi->planes[0]);
		mpi->flags&=~MP_IMGFLAG_ALLOCATED;
		mp_msg(MSGT_VFILTER,MSGL_V,"vf.c: have to REALLOCATE buffer memory :(\n");
	    }
//	} else {
	} {
	    mpi->width=w2; mpi->chroma_width=(w2 + (1<<mpi->chroma_x_shift) - 1)>>mpi->chroma_x_shift;
	    mpi->height=h; mpi->chroma_height=(h + (1<<mpi->chroma_y_shift) - 1)>>mpi->chroma_y_shift;
	}
    }
    if(!mpi->bpp) mp_image_setfmt(mpi,outfmt);
    if(!(mpi->flags&MP_IMGFLAG_ALLOCATED) && mpi->type>MP_IMGTYPE_EXPORT){

	// check libvo first!
	if(vf->get_image) vf->get_image(vf,mpi);
	
        if(!(mpi->flags&MP_IMGFLAG_DIRECT)){
          // non-direct and not yet allocated image. allocate it!
	  
	  // check if codec prefer aligned stride:  
	  if(mp_imgflag&MP_IMGFLAG_PREFER_ALIGNED_STRIDE){
	      int align=(mpi->flags&MP_IMGFLAG_PLANAR &&
	                 mpi->flags&MP_IMGFLAG_YUV) ?
			 (8<<mpi->chroma_x_shift)-1 : 15; // -- maybe FIXME
	      w2=((w+align)&(~align));
	      if(mpi->width!=w2){
	          // we have to change width... check if we CAN co it:
		  int flags=vf->query_format(vf,outfmt); // should not fail
		  if(!(flags&3)) mp_msg(MSGT_DECVIDEO,MSGL_WARN,"??? vf_get_image{vf->query_format(outfmt)} failed!\n");
//		  printf("query -> 0x%X    \n",flags);
		  if(flags&VFCAP_ACCEPT_STRIDE){
	              mpi->width=w2;
		      mpi->chroma_width=(w2 + (1<<mpi->chroma_x_shift) - 1)>>mpi->chroma_x_shift;
		  }
	      }
	  }
	  
	  // IF09 - allocate space for 4. plane delta info - unused
	  if (mpi->imgfmt == IMGFMT_IF09)
	  {
	     mpi->planes[0]=memalign(64, mpi->bpp*mpi->width*(mpi->height+2)/8+
	    				mpi->chroma_width*mpi->chroma_height);
	     /* export delta table */
	     mpi->planes[3]=mpi->planes[0]+(mpi->width*mpi->height)+2*(mpi->chroma_width*mpi->chroma_height);
	  }
	  else
	     mpi->planes[0]=memalign(64, mpi->bpp*mpi->width*(mpi->height+2)/8);
	  if(mpi->flags&MP_IMGFLAG_PLANAR){
	      // YV12/I420/YVU9/IF09. feel free to add other planar formats here...
	      //if(!mpi->stride[0]) 
	      mpi->stride[0]=mpi->width;
	      //if(!mpi->stride[1]) 
	      if(mpi->num_planes > 2){
	      mpi->stride[1]=mpi->stride[2]=mpi->chroma_width;
	      if(mpi->flags&MP_IMGFLAG_SWAPPED){
	          // I420/IYUV  (Y,U,V)
	          mpi->planes[1]=mpi->planes[0]+mpi->width*mpi->height;
	          mpi->planes[2]=mpi->planes[1]+mpi->chroma_width*mpi->chroma_height;
	      } else {
	          // YV12,YVU9,IF09  (Y,V,U)
	          mpi->planes[2]=mpi->planes[0]+mpi->width*mpi->height;
	          mpi->planes[1]=mpi->planes[2]+mpi->chroma_width*mpi->chroma_height;
	      }
	      } else {
	          // NV12/NV21
	          mpi->stride[1]=mpi->chroma_width;
	          mpi->planes[1]=mpi->planes[0]+mpi->width*mpi->height;
	      }
	  } else {
	      //if(!mpi->stride[0]) 
	      mpi->stride[0]=mpi->width*mpi->bpp/8;
	  }
//	  printf("clearing img!\n");
	  vf_mpi_clear(mpi,0,0,mpi->width,mpi->height);
	  mpi->flags|=MP_IMGFLAG_ALLOCATED;
        }
    }
    if(mpi->flags&MP_IMGFLAG_DRAW_CALLBACK)
	if(vf->start_slice) vf->start_slice(vf,mpi);
    if(!(mpi->flags&MP_IMGFLAG_TYPE_DISPLAYED)){
	    mp_msg(MSGT_DECVIDEO,MSGL_V,"*** [%s] %s%s mp_image_t, %dx%dx%dbpp %s %s, %d bytes\n",
		  vf->info->name,
		  (mpi->type==MP_IMGTYPE_EXPORT)?"Exporting":
	          ((mpi->flags&MP_IMGFLAG_DIRECT)?"Direct Rendering":"Allocating"),
	          (mpi->flags&MP_IMGFLAG_DRAW_CALLBACK)?" (slices)":"",
	          mpi->width,mpi->height,mpi->bpp,
		  (mpi->flags&MP_IMGFLAG_YUV)?"YUV":((mpi->flags&MP_IMGFLAG_SWAPPED)?"BGR":"RGB"),
		  (mpi->flags&MP_IMGFLAG_PLANAR)?"planar":"packed",
	          mpi->bpp*mpi->width*mpi->height/8);
	    mp_msg(MSGT_DECVIDEO,MSGL_DBG2,"(imgfmt: %x, planes: %p,%p,%p strides: %d,%d,%d, chroma: %dx%d, shift: h:%d,v:%d)\n",
		mpi->imgfmt, mpi->planes[0], mpi->planes[1], mpi->planes[2],
		mpi->stride[0], mpi->stride[1], mpi->stride[2],
		mpi->chroma_width, mpi->chroma_height, mpi->chroma_x_shift, mpi->chroma_y_shift);
	    mpi->flags|=MP_IMGFLAG_TYPE_DISPLAYED;
    }

  mpi->qscale = NULL;
  }
//    printf("\rVF_MPI: %p %p %p %d %d %d    \n",
//	mpi->planes[0],mpi->planes[1],mpi->planes[2],
//	mpi->stride[0],mpi->stride[1],mpi->stride[2]);
  return mpi;
}

//============================================================================

// By default vf doesn't accept MPEGPES
static int vf_default_query_format(struct vf_instance_s* vf, unsigned int fmt){
  if(fmt == IMGFMT_MPEGPES) return 0;
  return vf_next_query_format(vf,fmt);
}

vf_instance_t* vf_open_plugin(const vf_info_t* const* filter_list, vf_instance_t* next, const char *name, char **args){
    vf_instance_t* vf;
    int i;
    for(i=0;;i++){
	if(!filter_list[i]){
	    mp_msg(MSGT_VFILTER,MSGL_ERR,MSGTR_CouldNotFindVideoFilter,name);
	    return NULL; // no such filter!
	}
	if(!strcmp(filter_list[i]->name,name)) break;
    }
    vf=malloc(sizeof(vf_instance_t));
    memset(vf,0,sizeof(vf_instance_t));
    vf->info=filter_list[i];
    vf->next=next;
    vf->config=vf_next_config;
    vf->control=vf_next_control;
    vf->query_format=vf_default_query_format;
    vf->put_image=vf_next_put_image;
    vf->default_caps=VFCAP_ACCEPT_STRIDE;
    vf->default_reqs=0;
    if(vf->info->opts) { // vf_vo get some special argument
      const m_struct_t* st = vf->info->opts;
      void* vf_priv = m_struct_alloc(st);
      int n;
      for(n = 0 ; args && args[2*n] ; n++)
	m_struct_set(st,vf_priv,args[2*n],args[2*n+1]);
      vf->priv = vf_priv;
      args = NULL;
    } else // Otherwise we should have the '_oldargs_'
      if(args && !strcmp(args[0],"_oldargs_"))
	args = (char**)args[1];
      else
	args = NULL;
    if(vf->info->open(vf,(char*)args)>0) return vf; // Success!
    free(vf);
    mp_msg(MSGT_VFILTER,MSGL_ERR,MSGTR_CouldNotOpenVideoFilter,name);
    return NULL;
}

vf_instance_t* vf_open_filter(vf_instance_t* next, const char *name, char **args){
  if(args && strcmp(args[0],"_oldargs_")) {
    int i,l = 0;
    for(i = 0 ; args && args[2*i] ; i++)
      l += 1 + strlen(args[2*i]) + 1 + strlen(args[2*i+1]);
    l += strlen(name);
    {
      char str[l+1];
      char* p = str;
      p += sprintf(str,"%s",name);
      for(i = 0 ; args && args[2*i] ; i++)
	p += sprintf(p," %s=%s",args[2*i],args[2*i+1]);
      mp_msg(MSGT_VFILTER,MSGL_INFO,MSGTR_OpeningVideoFilter "[%s]\n",str);
    }
  } else if(strcmp(name,"vo")) {
    if(args && strcmp(args[0],"_oldargs_") == 0)
      mp_msg(MSGT_VFILTER,MSGL_INFO,MSGTR_OpeningVideoFilter
	     "[%s=%s]\n", name,args[1]);
    else
      mp_msg(MSGT_VFILTER,MSGL_INFO,MSGTR_OpeningVideoFilter
	     "[%s]\n", name);
  }
  return vf_open_plugin(filter_list,next,name,args);
}

/**
 * \brief adds a filter before the last one (which should be the vo filter).
 * \param vf start of the filter chain.
 * \param name name of the filter to add.
 * \param args argument list for the filter.
 * \return pointer to the filter instance that was created.
 */
vf_instance_t* vf_add_before_vo(vf_instance_t **vf, char *name, char **args) {
  vf_instance_t *vo, *prev = NULL, *new;
  // Find the last filter (should be vf_vo)
  for (vo = *vf; vo->next; vo = vo->next)
    prev = vo;
  new = vf_open_filter(vo, name, args);
  if (prev)
    prev->next = new;
  else
    *vf = new;
  return new;
}

//============================================================================

unsigned int vf_match_csp(vf_instance_t** vfp,const unsigned int* list,unsigned int preferred){
    vf_instance_t* vf=*vfp;
    const unsigned int* p;
    unsigned int best=0;
    int ret;
    if((p=list)) while(*p){
	ret=vf->query_format(vf,*p);
	mp_msg(MSGT_VFILTER,MSGL_V,"[%s] query(%s) -> %d\n",vf->info->name,vo_format_name(*p),ret&3);
	if(ret&2){ best=*p; break;} // no conversion -> bingo!
	if(ret&1 && !best) best=*p; // best with conversion
	++p;
    }
    if(best) return best; // bingo, they have common csp!
    // ok, then try with scale:
    if(vf->info == &vf_info_scale) return 0; // avoid infinite recursion!
    vf=vf_open_filter(vf,"scale",NULL);
    if(!vf) return 0; // failed to init "scale"
    // try the preferred csp first:
    if(preferred && vf->query_format(vf,preferred)) best=preferred; else
    // try the list again, now with "scaler" :
    if((p=list)) while(*p){
	ret=vf->query_format(vf,*p);
	mp_msg(MSGT_VFILTER,MSGL_V,"[%s] query(%s) -> %d\n",vf->info->name,vo_format_name(*p),ret&3);
	if(ret&2){ best=*p; break;} // no conversion -> bingo!
	if(ret&1 && !best) best=*p; // best with conversion
	++p;
    }
    if(best) *vfp=vf; // else uninit vf  !FIXME!
    return best;
}

void vf_clone_mpi_attributes(mp_image_t* dst, mp_image_t* src){
    dst->pict_type= src->pict_type;
    dst->fields = src->fields;
    dst->qscale_type= src->qscale_type;
    if(dst->width == src->width && dst->height == src->height){
	dst->qstride= src->qstride;
	dst->qscale= src->qscale;
    }
}

void vf_queue_frame(vf_instance_t *vf, int (*func)(vf_instance_t *))
{
    vf->continue_buffered_image = func;
}

// Output the next buffered image (if any) from the filter chain.
// The queue could be kept as a simple stack/list instead avoiding the
// looping here, but there's currently no good context variable where
// that could be stored so this was easier to implement.

int vf_output_queued_frame(vf_instance_t *vf)
{
    while (1) {
	int ret;
	vf_instance_t *current;
	vf_instance_t *last=NULL;
	int (*tmp)(vf_instance_t *);
	for (current = vf; current; current = current->next)
	    if (current->continue_buffered_image)
		last = current;
	if (!last)
	    return 0;
	tmp = last->continue_buffered_image;
	last->continue_buffered_image = NULL;
	ret = tmp(last);
	if (ret > 0) {
	    vf->control(vf, VFCTRL_DRAW_OSD, NULL);
#ifdef CONFIG_ASS
	    vf->control(vf, VFCTRL_DRAW_EOSD, NULL);
#endif
	}
	if (ret)
	    return ret;
    }
}


/**
 * \brief Video config() function wrapper
 *
 * Blocks config() calls with different size or format for filters
 * with VFCAP_CONSTANT
 *
 * First call is redirected to vf->config.
 *
 * In following calls, it verifies that the configuration parameters
 * are unchanged, and returns either success or error.
 *
*/
int vf_config_wrapper(struct vf_instance_s* vf,
		    int width, int height, int d_width, int d_height,
		    unsigned int flags, unsigned int outfmt)
{
    int r;
    if ((vf->default_caps&VFCAP_CONSTANT) && vf->fmt.have_configured) {
        if ((vf->fmt.orig_width != width)
	    || (vf->fmt.orig_height != height)
	    || (vf->fmt.orig_fmt != outfmt)) {
            mp_msg(MSGT_VFILTER,MSGL_ERR,MSGTR_ResolutionDoesntMatch);
            return 0;
        }
        return 1;
    }
    vf->fmt.have_configured = 1;
    vf->fmt.orig_height = height;
    vf->fmt.orig_width = width;
    vf->fmt.orig_fmt = outfmt;
    r = vf->config(vf, width, height, d_width, d_height, flags, outfmt);
    if (!r) vf->fmt.have_configured = 0;
    return r;
}

int vf_next_config(struct vf_instance_s* vf,
        int width, int height, int d_width, int d_height,
	unsigned int voflags, unsigned int outfmt){
    int miss;
    int flags=vf->next->query_format(vf->next,outfmt);
    if(!flags){
	// hmm. colorspace mismatch!!!
	// let's insert the 'scale' filter, it does the job for us:
	vf_instance_t* vf2;
	if(vf->next->info==&vf_info_scale) return 0; // scale->scale
	vf2=vf_open_filter(vf->next,"scale",NULL);
	if(!vf2) return 0; // shouldn't happen!
	vf->next=vf2;
	flags=vf->next->query_format(vf->next,outfmt);
	if(!flags){
	    mp_msg(MSGT_VFILTER,MSGL_ERR,MSGTR_CannotFindColorspace);
	    return 0; // FAIL
	}
    }
    mp_msg(MSGT_VFILTER,MSGL_V,"REQ: flags=0x%X  req=0x%X  \n",flags,vf->default_reqs);
    miss=vf->default_reqs - (flags&vf->default_reqs);
    if(miss&VFCAP_ACCEPT_STRIDE){
	// vf requires stride support but vf->next doesn't support it!
	// let's insert the 'expand' filter, it does the job for us:
	vf_instance_t* vf2=vf_open_filter(vf->next,"expand",NULL);
	if(!vf2) return 0; // shouldn't happen!
	vf->next=vf2;
    }
    vf->next->w = width; vf->next->h = height;
    return vf_config_wrapper(vf->next,width,height,d_width,d_height,voflags,outfmt);
}

int vf_next_control(struct vf_instance_s* vf, int request, void* data){
    return vf->next->control(vf->next,request,data);
}

void vf_extra_flip(struct vf_instance_s* vf) {
    vf_next_control(vf, VFCTRL_DRAW_OSD, NULL);
#ifdef CONFIG_ASS
    vf_next_control(vf, VFCTRL_DRAW_EOSD, NULL);
#endif
    vf_next_control(vf, VFCTRL_FLIP_PAGE, NULL);
}

int vf_next_query_format(struct vf_instance_s* vf, unsigned int fmt){
    int flags=vf->next->query_format(vf->next,fmt);
    if(flags) flags|=vf->default_caps;
    return flags;
}

int vf_next_put_image(struct vf_instance_s* vf,mp_image_t *mpi, double pts){
    return vf->next->put_image(vf->next,mpi, pts);
}

void vf_next_draw_slice(struct vf_instance_s* vf,unsigned char** src, int * stride,int w, int h, int x, int y){
    if (vf->next->draw_slice) {
	vf->next->draw_slice(vf->next,src,stride,w,h,x,y);
	return;
    }
    if (!vf->dmpi) {
	mp_msg(MSGT_VFILTER,MSGL_ERR,"draw_slice: dmpi not stored by vf_%s\n", vf->info->name);
	return;
    }
    if (!(vf->dmpi->flags & MP_IMGFLAG_PLANAR)) {
	memcpy_pic(vf->dmpi->planes[0]+y*vf->dmpi->stride[0]+vf->dmpi->bpp/8*x,
	    src[0], vf->dmpi->bpp/8*w, h, vf->dmpi->stride[0], stride[0]);
	return;
    }
    memcpy_pic(vf->dmpi->planes[0]+y*vf->dmpi->stride[0]+x, src[0],
	w, h, vf->dmpi->stride[0], stride[0]);
    memcpy_pic(vf->dmpi->planes[1]+(y>>vf->dmpi->chroma_y_shift)*vf->dmpi->stride[1]+(x>>vf->dmpi->chroma_x_shift),
	src[1], w>>vf->dmpi->chroma_x_shift, h>>vf->dmpi->chroma_y_shift, vf->dmpi->stride[1], stride[1]);
    memcpy_pic(vf->dmpi->planes[2]+(y>>vf->dmpi->chroma_y_shift)*vf->dmpi->stride[2]+(x>>vf->dmpi->chroma_x_shift),
	src[2], w>>vf->dmpi->chroma_x_shift, h>>vf->dmpi->chroma_y_shift, vf->dmpi->stride[2], stride[2]);
}

//============================================================================

vf_instance_t* append_filters(vf_instance_t* last){
  vf_instance_t* vf;
  int i; 

  if(vf_settings) {
    // We want to add them in the 'right order'
    for(i = 0 ; vf_settings[i].name ; i++)
      /* NOP */;
    for(i-- ; i >= 0 ; i--) {
      //printf("Open filter %s\n",vf_settings[i].name);
      vf = vf_open_filter(last,vf_settings[i].name,vf_settings[i].attribs);
      if(vf) last=vf;
    }
  }
  return last;
}

//============================================================================

void vf_uninit_filter(vf_instance_t* vf){
    if(vf->uninit) vf->uninit(vf);
    free_mp_image(vf->imgctx.static_images[0]);
    free_mp_image(vf->imgctx.static_images[1]);
    free_mp_image(vf->imgctx.temp_images[0]);
    free_mp_image(vf->imgctx.export_images[0]);
    free(vf);
}

void vf_uninit_filter_chain(vf_instance_t* vf){
    while(vf){
	vf_instance_t* next=vf->next;
	vf_uninit_filter(vf);
	vf=next;
    }
}
