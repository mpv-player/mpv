#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "mp_msg.h"

#include "vd_internal.h"

static vd_info_t info = {
	"NuppelVideo decoder",
	"nuv",
	"A'rpi",
	"Alex & Panagiotis Issaris <takis@lumumba.luc.ac.be>",
	"native codecs"
};

LIBVD_EXTERN(nuv)

// to set/get/query special features/parameters
static int control(sh_video_t *sh,int cmd,void* arg,...){
    return CONTROL_UNKNOWN;
}

// init driver
static int init(sh_video_t *sh){
    return mpcodecs_config_vo(sh,sh->disp_w,sh->disp_h,IMGFMT_I420);
}

// uninit driver
static void uninit(sh_video_t *sh){
}

//mp_image_t* mpcodecs_get_image(sh_video_t *sh, int mp_imgtype, int mp_imgflag, int w, int h);

void decode_nuv(
  unsigned char *encoded,
  int encoded_size,
  unsigned char *decoded,
  int width,
  int height);

// decode a frame
static mp_image_t* decode(sh_video_t *sh,void* data,int len,int flags){
    mp_image_t* mpi;
    if(len<=0) return NULL; // skipped frame
    
    mpi=mpcodecs_get_image(sh, MP_IMGTYPE_TEMP, 0, 
	sh->disp_w, sh->disp_h);
    if(!mpi) return NULL;

    decode_nuv(data, len, mpi->planes[0], sh->disp_w, sh->disp_h);

    return mpi;
}
