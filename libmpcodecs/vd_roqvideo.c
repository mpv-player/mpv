#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "mp_msg.h"

#include "vd_internal.h"

static vd_info_t info = {
	"Id RoQ File Video decoder",
	"roqvideo",
	"A'rpi",
	"Mike Melanson",
	"native codec"
};

LIBVD_EXTERN(roqvideo)

#include "roqav.h"

// to set/get/query special features/parameters
static int control(sh_video_t *sh,int cmd,void* arg,...){
    return CONTROL_UNKNOWN;
}

// init driver
static int init(sh_video_t *sh){
    sh->context = roq_decode_video_init();
    return mpcodecs_config_vo(sh,sh->disp_w,sh->disp_h,IMGFMT_YV12);
}

// uninit driver
static void uninit(sh_video_t *sh){
}

//mp_image_t* mpcodecs_get_image(sh_video_t *sh, int mp_imgtype, int mp_imgflag, int w, int h);

// decode a frame
static mp_image_t* decode(sh_video_t *sh,void* data,int len,int flags){
    mp_image_t* mpi;
    if(len<=0) return NULL; // skipped frame
    
    mpi=mpcodecs_get_image(sh, MP_IMGTYPE_IP, MP_IMGFLAG_PRESERVE | MP_IMGFLAG_READABLE, 
	sh->disp_w, sh->disp_h);
    if(!mpi) return NULL;

    roq_decode_video(sh->context, data, len, mpi);

    return mpi;
}
