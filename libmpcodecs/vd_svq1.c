#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "mp_msg.h"

#include "vd_internal.h"

static vd_info_t info = {
	"SVQ1 (Sorenson v1) Video decoder",
	"svq1",
	VFM_SVQ1,
	"A'rpi",
	"XINE team",
	"native codec"
};

LIBVD_EXTERN(svq1)

#include "native/svq1.h"

// to set/get/query special features/parameters
static int control(sh_video_t *sh,int cmd,void* arg,...){
    return CONTROL_UNKNOWN;
}

extern int avcodec_inited;

// init driver
static int init(sh_video_t *sh){

#ifdef USE_LIBAVCODEC
    if(!avcodec_inited){
      avcodec_init();
      avcodec_register_all();
      avcodec_inited=1;
    }
#endif

    if(!mpcodecs_config_vo(sh,sh->disp_w,sh->disp_h,IMGFMT_YVU9)) return 0;

    sh->context=malloc(sizeof(svq1_t));
    memset(sh->context,0,sizeof(svq1_t));
    
    return 1;
}

// uninit driver
static void uninit(sh_video_t *sh){
    svq1_free(sh->context);
    sh->context=NULL;
}

// decode a frame
static mp_image_t* decode(sh_video_t *sh,void* data,int len,int flags){
    mp_image_t* mpi;
    svq1_t* svq1=sh->context;
    int ret;
    
    if(len<=0) return NULL; // skipped frame
    
    ret=svq1_decode_frame(svq1,data);
    
    mpi=mpcodecs_get_image(sh, MP_IMGTYPE_EXPORT, MP_IMGFLAG_PRESERVE, 
	sh->disp_w, sh->disp_h);
    if(!mpi) return NULL;
    
    mp_msg(MSGT_DECVIDEO,MSGL_DBG2,"SVQ1: ret=%d wh=%dx%d p=%p   \n",ret,svq1->width,svq1->height,svq1->current);
    
    mpi->planes[0]=svq1->base[0];
    mpi->planes[1]=svq1->base[1];
    mpi->planes[2]=svq1->base[2];
    mpi->stride[0]=svq1->luma_width;
    mpi->stride[1]=mpi->stride[2]=svq1->chroma_width;
    
    return mpi;
}

