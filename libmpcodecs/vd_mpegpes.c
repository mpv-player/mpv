#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "mp_msg.h"

#include "vd_internal.h"

static vd_info_t info = 
{
	"MPEG 1/2 Video passthrough",
	"mpegpes",
	"A'rpi",
	"A'rpi",
	"for hw decoders"
};

LIBVD_EXTERN(mpegpes)

//#include "libmpdemux/parse_es.h"

#include "libvo/video_out.h"

// to set/get/query special features/parameters
static int control(sh_video_t *sh,int cmd,void* arg,...){
    return CONTROL_UNKNOWN;
}

// init driver
static int init(sh_video_t *sh){
    return mpcodecs_config_vo(sh,sh->disp_w,sh->disp_h,IMGFMT_MPEGPES);
}

// uninit driver
static void uninit(sh_video_t *sh){
}

// decode a frame
static mp_image_t* decode(sh_video_t *sh,void* data,int len,int flags){
    mp_image_t* mpi;
    static vo_mpegpes_t packet;
    mpi=mpcodecs_get_image(sh, MP_IMGTYPE_EXPORT, 0, sh->disp_w, sh->disp_h);
    packet.data=data;
    packet.size=len;
    packet.timestamp=sh->timer*90000.0;
    packet.id=0x1E0; //+sh_video->ds->id;
    mpi->planes[0]=(uint8_t*)(&packet);
    return mpi;
}
