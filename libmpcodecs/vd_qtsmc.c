#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "mp_msg.h"

#include "vd_internal.h"

static vd_info_t info = {
	"Apple Graphics (SMC) decoder",
	"qtsmc",
	VFM_QTSMC,
	"A'rpi",
	"Mike Melanson",
	"native codec"
};

LIBVD_EXTERN(qtsmc)

// to set/get/query special features/parameters
static int control(sh_video_t *sh,int cmd,void* arg,...){
    return CONTROL_UNKNOWN;
}

int qt_init_decode_smc(void);

// init driver
static int init(sh_video_t *sh){
    if (qt_init_decode_smc() != 0){
	mp_msg(MSGT_DECVIDEO, MSGL_ERR, "SMC decoder could not allocate enough memory");
	return 0;
    }
    
    return mpcodecs_config_vo(sh,sh->disp_w,sh->disp_h,IMGFMT_BGR24);
}

// uninit driver
static void uninit(sh_video_t *sh){
}

//mp_image_t* mpcodecs_get_image(sh_video_t *sh, int mp_imgtype, int mp_imgflag, int w, int h);

void qt_decode_smc(
  unsigned char *encoded,
  int encoded_size,
  unsigned char *decoded,
  int width,
  int height,
  unsigned char *palette_map,
  int bytes_per_pixel);

// decode a frame
static mp_image_t* decode(sh_video_t *sh,void* data,int len,int flags){
    mp_image_t* mpi;
    if(len<=0) return NULL; // skipped frame
    
    mpi=mpcodecs_get_image(sh, MP_IMGTYPE_STATIC, MP_IMGFLAG_PRESERVE, 
	sh->disp_w, sh->disp_h);
    if(!mpi) return NULL;

    qt_decode_smc(
        data,len, mpi->planes[0],
        sh->disp_w, sh->disp_h,
        (unsigned char *)sh->bih+40,
        mpi->bpp/8);
    
    return mpi;
}
