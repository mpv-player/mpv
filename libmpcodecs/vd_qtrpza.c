#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "mp_msg.h"

#include "codec-cfg.h"
#include "../libvo/img_format.h"

#include "stream.h"
#include "demuxer.h"
#include "stheader.h"

#include "vd.h"
#include "vd_internal.h"

static vd_info_t info = {
	"Quicktime Apple Video",
	"qtrpza",
	VFM_QTRPZA,
	"Roberto Togni",
	"Roberto Togni",
	"native codec"
};

LIBVD_EXTERN(qtrpza)

// to set/get/query special features/parameters
static int control(sh_video_t *sh,int cmd,void* arg,...){
    return CONTROL_UNKNOWN;
}

//int mpcodecs_config_vo(sh_video_t *sh, int w, int h, unsigned int preferred_outfmt);

// init driver
static int init(sh_video_t *sh){
    mpcodecs_config_vo(sh,sh->disp_w,sh->disp_h,IMGFMT_BGR16);
    return 1;
}

// uninit driver
static void uninit(sh_video_t *sh){
}

//mp_image_t* mpcodecs_get_image(sh_video_t *sh, int mp_imgtype, int mp_imgflag, int w, int h);

void qt_decode_rpza(char *encoded, int encodec_size, char *decodec, int width, int height, int bytes_per_pixel);

// decode a frame
static mp_image_t* decode(sh_video_t *sh,void* data,int len,int flags){
    mp_image_t* mpi;
    if(len<=0) return NULL; // skipped frame
    
    mpi=mpcodecs_get_image(sh, MP_IMGTYPE_STATIC, MP_IMGFLAG_PRESERVE, sh->disp_w, sh->disp_h);
    
    if(!mpi){	// temporary!
	printf("couldn't allocate image for qtrpza codec\n");
	return NULL;
    }
    
    qt_decode_rpza(data, len, mpi->planes[0], sh->disp_w, sh->disp_h,
	mpi->bpp/8);
    
    return mpi;
}
