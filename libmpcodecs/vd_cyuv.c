#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "mp_msg.h"

#include "vd_internal.h"

static vd_info_t info = {
	"Creative YUV decoder",
	"cyuv",
	VFM_CYUV,
	"A'rpi",
	"Dr. Tim Ferguson",
	"native codec"
};

LIBVD_EXTERN(cyuv)

// to set/get/query special features/parameters
static int control(sh_video_t *sh,int cmd,void* arg,...){
    return CONTROL_UNKNOWN;
}

// init driver
static int init(sh_video_t *sh){
    return mpcodecs_config_vo(sh,sh->disp_w,sh->disp_h,IMGFMT_UYVY);
}

// uninit driver
static void uninit(sh_video_t *sh){
}

//mp_image_t* mpcodecs_get_image(sh_video_t *sh, int mp_imgtype, int mp_imgflag, int w, int h);

void decode_cyuv(
  unsigned char *buf,
  int size,
  unsigned char *frame,
  int width,
  int height,
  int bit_per_pixel);

// decode a frame
static mp_image_t* decode(sh_video_t *sh,void* data,int len,int flags){
    mp_image_t* mpi;
    if(len<=0) return NULL; // skipped frame
    
    mpi=mpcodecs_get_image(sh, MP_IMGTYPE_TEMP, 0, 
	sh->disp_w, sh->disp_h);
    if(!mpi) return NULL;

    decode_cyuv(data, len, mpi->planes[0], sh->disp_w, sh->disp_h, 0);

    return mpi;
}
