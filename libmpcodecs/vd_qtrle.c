#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "mp_msg.h"

#include "vd_internal.h"

static vd_info_t info = {
	"Quicktime Animation (RLE) decoder",
	"qtrle",
	VFM_QTRLE,
	"A'rpi",
	"Mike Melanson",
	"native codec"
};

LIBVD_EXTERN(qtrle)

// to set/get/query special features/parameters
static int control(sh_video_t *sh,int cmd,void* arg,...){
    return CONTROL_UNKNOWN;
}

// init driver
static int init(sh_video_t *sh){
  if (sh->bih->biBitCount != 24){
    mp_msg(MSGT_DECVIDEO,MSGL_ERR,
    "    *** FYI: This Quicktime file is using %d-bit RLE Animation\n" \
    "    encoding, which is not yet supported by MPlayer. But if you upload\n" \
    "    this Quicktime file to the MPlayer FTP, the team could look at it.\n",
    sh->bih->biBitCount);
    return 0;
  }
    
    return mpcodecs_config_vo(sh,sh->disp_w,sh->disp_h,IMGFMT_BGR24);
}

// uninit driver
static void uninit(sh_video_t *sh){
}

//mp_image_t* mpcodecs_get_image(sh_video_t *sh, int mp_imgtype, int mp_imgflag, int w, int h);

void qt_decode_rle(
  unsigned char *encoded,
  int encoded_size,
  unsigned char *decoded,
  int width,
  int height,
  int encoded_bpp,
  int bytes_per_pixel);

// decode a frame
static mp_image_t* decode(sh_video_t *sh,void* data,int len,int flags){
    mp_image_t* mpi;
    if(len<=0) return NULL; // skipped frame
    
    mpi=mpcodecs_get_image(sh, MP_IMGTYPE_STATIC, MP_IMGFLAG_PRESERVE, 
	sh->disp_w, sh->disp_h);
    if(!mpi) return NULL;

    qt_decode_rle(
        data,len, mpi->planes[0],
        sh->disp_w, sh->disp_h,
        sh->bih->biBitCount,
        mpi->bpp/8);
    
    return mpi;
}
