#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#ifdef USE_XANIM

#include "mp_msg.h"

#include "vd_internal.h"

static vd_info_t info = {
	"XAnim codecs",
	"xanim",
	VFM_XANIM,
	"A'rpi & Alex",
	"Xanim (http://xanim.va.pubnix.com/)",
	"binary codec plugins"
};

LIBVD_EXTERN(xanim)

#include "xacodec.h"

// to set/get/query special features/parameters
static int control(sh_video_t *sh,int cmd,void* arg,...){
    return CONTROL_UNKNOWN;
}

// init driver
static int init(sh_video_t *sh){
    if(!mpcodecs_config_vo(sh,sh->disp_w,sh->disp_h,sh->format)) return 0;
    return xacodec_init_video(sh,sh->codec->outfmt[sh->outfmtidx]);
}

// uninit driver
static void uninit(sh_video_t *sh){
    xacodec_exit();
}

//mp_image_t* mpcodecs_get_image(sh_video_t *sh, int mp_imgtype, int mp_imgflag, int w, int h);

// decode a frame
static mp_image_t* decode(sh_video_t *sh,void* data,int len,int flags){
    mp_image_t* mpi;
    xacodec_image_t* image;
    
    if(len<=0) return NULL; // skipped frame

    image=xacodec_decode_frame(data,len,(flags&3)?1:0);
    if(!image) return NULL;

    mpi=mpcodecs_get_image(sh, MP_IMGTYPE_EXPORT, MP_IMGFLAG_PRESERVE, 
	sh->disp_w, sh->disp_h);
    if(!mpi) return NULL;

    mpi->planes[0]=image->planes[0];
    mpi->planes[1]=image->planes[1];
    mpi->planes[2]=image->planes[2];
    mpi->stride[0]=image->stride[0];
    mpi->stride[1]=image->stride[1];
    mpi->stride[2]=image->stride[2];
    
    return mpi;
}

#endif
