#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../config.h"
#include "../mp_msg.h"

#include "../libvo/img_format.h"
#include "../mp_image.h"
#include "vf.h"

extern vf_info_t vf_info_vo;
extern vf_info_t vf_info_crop;
extern vf_info_t vf_info_expand;
extern vf_info_t vf_info_pp;
extern vf_info_t vf_info_scale;

char** vo_plugin_args=(char**) NULL;

// list of available filters:
static vf_info_t* filter_list[]={
    &vf_info_crop,
    &vf_info_expand,
    &vf_info_pp,
    &vf_info_scale,
//    &vf_info_osd,
    &vf_info_vo,
    NULL
};

//============================================================================

// mpi stuff:

mp_image_t* vf_get_image(vf_instance_t* vf, unsigned int outfmt, int mp_imgtype, int mp_imgflag, int w, int h){
  mp_image_t* mpi=NULL;
  int w2=w; //(mp_imgflag&MP_IMGFLAG_ACCEPT_STRIDE)?((w+15)&(~15)):w;
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
	      // YV12/I420. feel free to add other planar formats here...
	      if(!mpi->stride[0]) mpi->stride[0]=mpi->width;
	      if(!mpi->stride[1]) mpi->stride[1]=mpi->stride[2]=mpi->width/2;
	      if(mpi->flags&MP_IMGFLAG_SWAPPED){
	          // I420/IYUV  (Y,U,V)
	          mpi->planes[1]=mpi->planes[0]+mpi->width*mpi->height;
	          mpi->planes[2]=mpi->planes[1]+(mpi->width>>1)*(mpi->height>>1);
	      } else {
	          // YV12,YVU9  (Y,V,U)
	          mpi->planes[2]=mpi->planes[0]+mpi->width*mpi->height;
	          mpi->planes[1]=mpi->planes[2]+(mpi->width>>1)*(mpi->height>>1);
	      }
	  } else {
	      if(!mpi->stride[0]) mpi->stride[0]=mpi->width*mpi->bpp/8;
	  }
	  mpi->flags|=MP_IMGFLAG_ALLOCATED;
        }
    }
    if(!(mpi->flags&MP_IMGFLAG_TYPE_DISPLAYED)){
	    mp_msg(MSGT_DECVIDEO,MSGL_INFO,"*** [%s] %s mp_image_t, %dx%dx%dbpp %s %s, %d bytes\n",
		  vf->info->name,
		  (mpi->type==MP_IMGTYPE_EXPORT)?"Exporting":
	          ((mpi->flags&MP_IMGFLAG_DIRECT)?"Direct Rendering":"Allocating"),
	          mpi->width,mpi->height,mpi->bpp,
		  (mpi->flags&MP_IMGFLAG_YUV)?"YUV":"RGB",
		  (mpi->flags&MP_IMGFLAG_PLANAR)?"planar":"packed",
	          mpi->bpp*mpi->width*mpi->height/8);
	    mpi->flags|=MP_IMGFLAG_TYPE_DISPLAYED;
    }

  }
  return mpi;
}

//============================================================================

vf_instance_t* vf_open_filter(vf_instance_t* next, char *name, char *args){
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
    if(vf->info->open(vf,args)>0) return vf; // Success!
    free(vf);
    mp_msg(MSGT_VFILTER,MSGL_ERR,"Couldn't open video filter '%s'\n",name);
    return NULL;
}

//============================================================================

int vf_next_config(struct vf_instance_s* vf,
        int width, int height, int d_width, int d_height,
	unsigned int flags, unsigned int outfmt){
    return vf->next->config(vf->next,width,height,d_width,d_height,flags,outfmt);
}

int vf_next_control(struct vf_instance_s* vf, int request, void* data){
    return vf->next->control(vf->next,request,data);
}

int vf_next_query_format(struct vf_instance_s* vf, unsigned int fmt){
    return vf->next->query_format(vf->next,fmt);
}

void vf_next_put_image(struct vf_instance_s* vf,mp_image_t *mpi){
    return vf->next->put_image(vf->next,mpi);
}

//============================================================================

vf_instance_t* append_filters(vf_instance_t* last){
    vf_instance_t* vf;
    if(!vo_plugin_args) return last;
    while(*vo_plugin_args){
	char* name=strdup(*vo_plugin_args);
	char* args=strchr(name,'=');
	if(args){args[0]=0;++args;}
	mp_msg(MSGT_VFILTER,MSGL_INFO,"Opening video filter '%s' with args '%s'...\n",name,args);
	vf=vf_open_filter(last,name,args);
	if(vf) last=vf;
	free(name);
	++vo_plugin_args;
    }
    return last;
}

//============================================================================

