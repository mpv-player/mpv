#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "mp_msg.h"

#include "vd_internal.h"

static vd_info_t info = 
{
	"MPEG 1/2 Video decoder",
	"libmpeg2",
	VFM_MPEG,
	"A'rpi",
	"Aaron & Walken",
	"native"
};

LIBVD_EXTERN(libmpeg2)

#include "libmpdemux/parse_es.h"

#include "libvo/video_out.h"
#include "libmpeg2/mpeg2.h"
#include "libmpeg2/mpeg2_internal.h"

extern picture_t *picture;	// exported from libmpeg2/decode.c

// to set/get/query special features/parameters
static int control(sh_video_t *sh,int cmd,void* arg,...){
    return CONTROL_UNKNOWN;
}

// init driver
static int init(sh_video_t *sh){
    mpeg2_init();
    picture->pp_options=divx_quality;
    // send seq header to the decoder:  *** HACK ***
    mpeg2_decode_data(NULL,videobuffer,videobuffer+videobuf_len,0);
    mpeg2_allocate_image_buffers (picture);
    return mpcodecs_config_vo(sh,sh->disp_w,sh->disp_h,IMGFMT_YV12);
}

// uninit driver
static void uninit(sh_video_t *sh){
    mpeg2_free_image_buffers (picture);
}

// decode a frame
static mp_image_t* decode(sh_video_t *sh,void* data,int len,int flags){
    mp_image_t* mpi;
    if(sh->codec->outfmt[sh->outfmtidx]==IMGFMT_MPEGPES){
	static vo_mpegpes_t packet;
	mpi=mpcodecs_get_image(sh, MP_IMGTYPE_EXPORT, 0,
	    sh->disp_w, sh->disp_h);
	// hardware decoding:
//	mpeg2_decode_data(video_out, start, start+in_size,3); // parse headers
	packet.data=data;
	packet.size=len-4;
	packet.timestamp=sh->timer*90000.0;
	packet.id=0x1E0; //+sh_video->ds->id;
	mpi->planes[0]=(uint8_t*)(&packet);
    } else {
	mpi=mpcodecs_get_image(sh, MP_IMGTYPE_EXPORT, MP_IMGFLAG_DRAW_CALLBACK,
	    sh->disp_w, sh->disp_h);
	if(
	mpeg2_decode_data(sh->video_out, data, data+len,flags&3)==0 // decode
	)return NULL;//hack for interlaced mpeg2
    }
    return mpi;
}

