#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "mp_msg.h"

#include "vd_internal.h"

static vd_info_t info = {
	"Autodesk FLI/FLC Animation decoder",
	"fli",
	"A'rpi",
	"Mike Melanson",
	"native codec"
};

LIBVD_EXTERN(fli)

// to set/get/query special features/parameters
static int control(sh_video_t *sh,int cmd,void* arg,...){
    return CONTROL_UNKNOWN;
}

void *init_fli_decoder(int width, int height);

void decode_fli_frame(
  unsigned char *encoded,
  int encoded_size,
  unsigned char *decoded,
  int width,
  int height,
  int bytes_per_pixel,
  void *context);

// init driver
static int init(sh_video_t *sh){
    if(!mpcodecs_config_vo(sh,sh->disp_w,sh->disp_h,IMGFMT_BGR24)) return 0;
    sh->context = init_fli_decoder(sh->disp_w, sh->disp_h);
    return 1;
}

// uninit driver
static void uninit(sh_video_t *sh){
}

//mp_image_t* mpcodecs_get_image(sh_video_t *sh, int mp_imgtype, int mp_imgflag, int w, int h);

// decode a frame
static mp_image_t* decode(sh_video_t *sh,void* data,int len,int flags){
    mp_image_t* mpi;
    if(len<=0) return NULL; // skipped frame
    
    mpi=mpcodecs_get_image(sh, MP_IMGTYPE_STATIC, MP_IMGFLAG_PRESERVE, 
	sh->disp_w, sh->disp_h);
    if(!mpi) return NULL;

    decode_fli_frame(
        data, len, mpi->planes[0],
        sh->disp_w, sh->disp_h,
        mpi->bpp/8,
        sh->context);
    
    return mpi;
}
