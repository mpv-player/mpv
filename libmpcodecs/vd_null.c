#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "mp_msg.h"

#include "vd_internal.h"

static vd_info_t info = 
{
	"Null video decoder",
	"null",
	"A'rpi",
	"A'rpi",
	"no decoding"
};

LIBVD_EXTERN(null)

// to set/get/query special features/parameters
static int control(sh_video_t *sh,int cmd,void* arg,...){
    return CONTROL_UNKNOWN;
}

// init driver
static int init(sh_video_t *sh){
    if(!mpcodecs_config_vo(sh,sh->disp_w,sh->disp_h,IMGFMT_BGR24)) return 0;
    return 1;
}

// uninit driver
static void uninit(sh_video_t *sh){
}

// decode a frame
static mp_image_t* decode(sh_video_t *sh,void* data,int len,int flags){
    return NULL;
}

