#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif

#include "../config.h"
#include "../mp_msg.h"

#include "img_format.h"
#include "mp_image.h"
#include "vf.h"

extern vf_info_t vf_info_vo;
extern vf_info_t vf_info_crop;
extern vf_info_t vf_info_expand;
extern vf_info_t vf_info_pp;
extern vf_info_t vf_info_scale;
#ifdef USE_LIBFAME
extern vf_info_t vf_info_fame;
#endif
extern vf_info_t vf_info_format;
extern vf_info_t vf_info_yuy2;
extern vf_info_t vf_info_flip;
extern vf_info_t vf_info_rgb2bgr;
extern vf_info_t vf_info_rotate;
extern vf_info_t vf_info_mirror;
extern vf_info_t vf_info_palette;
extern vf_info_t vf_info_lavc;
extern vf_info_t vf_info_dvbscale;
extern vf_info_t vf_info_cropdetect;
extern vf_info_t vf_info_test;
extern vf_info_t vf_info_noise;
extern vf_info_t vf_info_yvu9;

char** vo_plugin_args=(char**) NULL;

// list of available filters:
static vf_info_t* filter_list[]={
    &vf_info_crop,
    &vf_info_expand,
    &vf_info_pp,
    &vf_info_scale,
//    &vf_info_osd,
    &vf_info_vo,
#ifdef USE_LIBFAME
    &vf_info_fame,
#endif
    &vf_info_format,
    &vf_info_yuy2,
    &vf_info_flip,
    &vf_info_rgb2bgr,
    &vf_info_rotate,
    &vf_info_mirror,
    &vf_info_palette,
#ifdef USE_LIBAVCODEC
    &vf_info_lavc,
#endif
    &vf_info_dvbscale,
    &vf_info_cropdetect,
    &vf_info_test,
    &vf_info_noise,
    &vf_info_yvu9,
    NULL
};

//============================================================================
// mpi stuff:

void vf_mpi_clear(mp_image_t* mpi,int x0,int y0,int w,int h){
    int y;
    if(mpi->flags&MP_IMGFLAG_PLANAR){
	int div = (mpi->imgfmt == IMGFMT_YVU9) ? 2 : 1;
	y0&=~1;h+=h&1;
	if(x0==0 && w==mpi->width){
	    // full width clear:
	    memset(mpi->planes[0]+mpi->stride[0]*y0,0,mpi->stride[0]*h);
	    memset(mpi->planes[1]+mpi->stride[1]*(y0>>div),128,mpi->stride[1]*(h>>div));
	    memset(mpi->planes[2]+mpi->stride[2]*(y0>>div),128,mpi->stride[2]*(h>>div));
	} else
	for(y=y0;y<y0+h;y+=2){
	    memset(mpi->planes[0]+x0+mpi->stride[0]*y,0,w);
	    memset(mpi->planes[0]+x0+mpi->stride[0]*(y+1),0,w);
	    memset(mpi->planes[1]+(x0>>div)+mpi->stride[1]*(y>>div),128,(w>>div));
	    memset(mpi->planes[2]+(x0>>div)+mpi->stride[2]*(y>>div),128,(w>>div));
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
	    if(mpi->flags&MP_IMGFLAG_SWAPPED){
	        for(i=0;i<size;i+=4) p[i]=p[i+1]=p[i+2]=p[i+3]=0x00800080;
		for(;i<size;i++) p[i]=0x00800080;
	    } else {
	        for(i=0;i<size;i+=4) p[i]=p[i+1]=p[i+2]=p[i+3]=0x80008000;
		for(;i<size;i++) p[i]=0x80008000;
	    }
	} else
	    memset(dst,0,(mpi->bpp>>3)*w);
    }
}

mp_image_t* vf_get_image(vf_instance_t* vf, unsigned int outfmt, int mp_imgtype, int mp_imgflag, int w, int h){
  mp_image_t* mpi=NULL;
  int w2=w; //(mp_imgflag&MP_IMGFLAG_ACCEPT_STRIDE)?((w+15)&(~15)):w;
  
  if(vf->put_image==vf_next_put_image){
      // passthru mode, if the plugin uses the fallback/default put_image() code
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
    mpi->flags&=~(MP_IMGFLAG_PRESERVE|MP_IMGFLAG_READABLE|MP_IMGFLAG_DIRECT);
    mpi->flags|=mp_imgflag&(MP_IMGFLAG_PRESERVE|MP_IMGFLAG_READABLE|MP_IMGFLAG_ACCEPT_STRIDE|MP_IMGFLAG_ACCEPT_WIDTH|MP_IMGFLAG_ALIGNED_STRIDE|MP_IMGFLAG_DRAW_CALLBACK);
    if(!vf->draw_slice) mpi->flags&=~MP_IMGFLAG_DRAW_CALLBACK;
    if((mpi->width!=w2 || mpi->height!=h) && !(mpi->flags&MP_IMGFLAG_DIRECT)){
	mpi->width=w2;
	mpi->height=h;
	if(mpi->flags&MP_IMGFLAG_ALLOCATED){
	    // need to re-allocate buffer memory:
	    free(mpi->planes[0]);
	    mpi->flags&=~MP_IMGFLAG_ALLOCATED;
	}
    }
    if(!mpi->bpp) mp_image_setfmt(mpi,outfmt);
    if(!(mpi->flags&MP_IMGFLAG_ALLOCATED) && mpi->type>MP_IMGTYPE_EXPORT){

	// check libvo first!
	if(vf->get_image) vf->get_image(vf,mpi);
	
        if(!(mpi->flags&MP_IMGFLAG_DIRECT)){
          // non-direct and not yet allocaed image. allocate it!
	  mpi->planes[0]=memalign(64, mpi->bpp*mpi->width*(mpi->height+2)/8);
	  if(mpi->flags&MP_IMGFLAG_PLANAR){
	      // YV12/I420/YVU9. feel free to add other planar formats here...
	      if(!mpi->stride[0]) mpi->stride[0]=mpi->width;
	      if (!mpi->stride[1])
	      {
	        if (mpi->imgfmt == IMGFMT_YVU9)
		    mpi->stride[1]=mpi->stride[2]=mpi->width/4;
	        else
		    mpi->stride[1]=mpi->stride[2]=mpi->width/2;
	      }
	      if(mpi->flags&MP_IMGFLAG_SWAPPED){
	          // I420/IYUV  (Y,U,V)
	          mpi->planes[1]=mpi->planes[0]+mpi->width*mpi->height;
	          mpi->planes[2]=mpi->planes[1]+(mpi->width>>1)*(mpi->height>>1);
	      } else {
	          // YV12,YVU9  (Y,V,U)
	          mpi->planes[2]=mpi->planes[0]+mpi->width*mpi->height;
		  if (mpi->imgfmt == IMGFMT_YVU9)
		    mpi->planes[1]=mpi->planes[2]+(mpi->width>>2)*(mpi->height>>2);
		  else
	            mpi->planes[1]=mpi->planes[2]+(mpi->width>>1)*(mpi->height>>1);
	      }
	  } else {
	      if(!mpi->stride[0]) mpi->stride[0]=mpi->width*mpi->bpp/8;
	  }
	  vf_mpi_clear(mpi,0,0,mpi->width,mpi->height);
	  mpi->flags|=MP_IMGFLAG_ALLOCATED;
        }
    }
    if(!(mpi->flags&MP_IMGFLAG_TYPE_DISPLAYED)){
	    mp_msg(MSGT_DECVIDEO,MSGL_V,"*** [%s] %s mp_image_t, %dx%dx%dbpp %s %s, %d bytes\n",
		  vf->info->name,
		  (mpi->type==MP_IMGTYPE_EXPORT)?"Exporting":
	          ((mpi->flags&MP_IMGFLAG_DIRECT)?"Direct Rendering":"Allocating"),
	          mpi->width,mpi->height,mpi->bpp,
		  (mpi->flags&MP_IMGFLAG_YUV)?"YUV":"RGB",
		  (mpi->flags&MP_IMGFLAG_PLANAR)?"planar":"packed",
	          mpi->bpp*mpi->width*mpi->height/8);
	    mp_msg(MSGT_DECVIDEO,MSGL_DBG2,"(imgfmt: %x, planes: %x,%x,%x strides: %d,%d,%d)\n",
		mpi->imgfmt, mpi->planes[0], mpi->planes[1], mpi->planes[2],
		mpi->stride[0], mpi->stride[1], mpi->stride[2]);
	    mpi->flags|=MP_IMGFLAG_TYPE_DISPLAYED;
    }

  }
  return mpi;
}

//============================================================================

vf_instance_t* vf_open_plugin(vf_info_t** filter_list, vf_instance_t* next, char *name, char *args){
    vf_instance_t* vf;
    int i;
    for(i=0;;i++){
	if(!filter_list[i]){
	    mp_msg(MSGT_VFILTER,MSGL_ERR,"Couldn't find video filter '%s'\n",name);
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
    vf->query_format=vf_next_query_format;
    vf->put_image=vf_next_put_image;
    vf->default_caps=VFCAP_ACCEPT_STRIDE;
    vf->default_reqs=0;
    if(vf->info->open(vf,args)>0) return vf; // Success!
    free(vf);
    mp_msg(MSGT_VFILTER,MSGL_ERR,"Couldn't open video filter '%s'\n",name);
    return NULL;
}

vf_instance_t* vf_open_filter(vf_instance_t* next, char *name, char *args){
    if(strcmp(name,"vo"))
    mp_msg(MSGT_VFILTER,MSGL_INFO,
	args ? "Opening video filter: [%s=%s]\n"
	     : "Opening video filter: [%s]\n" ,name,args);
    return vf_open_plugin(filter_list,next,name,args);
}

//============================================================================

unsigned int vf_match_csp(vf_instance_t** vfp,unsigned int* list,unsigned int preferred){
    vf_instance_t* vf=*vfp;
    unsigned int* p;
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
	    mp_msg(MSGT_VFILTER,MSGL_ERR,"Cannot find common colorspace, even by inserting 'scale' :(\n");
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
    return vf->next->config(vf->next,width,height,d_width,d_height,voflags,outfmt);
}

int vf_next_control(struct vf_instance_s* vf, int request, void* data){
    return vf->next->control(vf->next,request,data);
}

int vf_next_query_format(struct vf_instance_s* vf, unsigned int fmt){
    int flags=vf->next->query_format(vf->next,fmt);
    if(flags) flags|=vf->default_caps;
    return flags;
}

void vf_next_put_image(struct vf_instance_s* vf,mp_image_t *mpi){
    vf->next->put_image(vf->next,mpi);
}

//============================================================================

vf_instance_t* append_filters(vf_instance_t* last){
    vf_instance_t* vf;
    char** plugin_args = vo_plugin_args;
    if(!vo_plugin_args) return last;
    while(*plugin_args){
	char* name=strdup(*plugin_args);
	char* args=strchr(name,'=');
	if(args){args[0]=0;++args;}
	vf=vf_open_filter(last,name,args);
	if(vf) last=vf;
	free(name);
	++plugin_args;
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

void vf_list_plugins(){
    int i=0;
    while(filter_list[i]){
        mp_msg(MSGT_VFILTER,MSGL_INFO,"\t%-10s: %s\n",filter_list[i]->name,filter_list[i]->info);
        i++;
    }
}
