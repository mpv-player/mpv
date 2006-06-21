#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "config.h"

#include "mp_msg.h"
#include "help_mp.h"

#include "vd_internal.h"

#include "loader/dmo/DMO_VideoDecoder.h"

static vd_info_t info = {
	"DMO video codecs",
	"dmo",
	"A'rpi",
	"based on http://avifile.sf.net",
	"win32 codecs"
};

LIBVD_EXTERN(dmo)

// to set/get/query special features/parameters
static int control(sh_video_t *sh,int cmd,void* arg,...){
    return CONTROL_UNKNOWN;
}

// init driver
static int init(sh_video_t *sh){
    unsigned int out_fmt;
    if(!(sh->context=DMO_VideoDecoder_Open(sh->codec->dll,&sh->codec->guid, sh->bih, 0, 0))){
        mp_msg(MSGT_DECVIDEO,MSGL_ERR,MSGTR_MissingDLLcodec,sh->codec->dll);
        mp_msg(MSGT_DECVIDEO,MSGL_HINT,MSGTR_DownloadCodecPackage);
	return 0;
    }
    if(!mpcodecs_config_vo(sh,sh->disp_w,sh->disp_h,IMGFMT_YUY2)) return 0;
    out_fmt=sh->codec->outfmt[sh->outfmtidx];
    switch(out_fmt){
    case IMGFMT_YUY2:
    case IMGFMT_UYVY:
	DMO_VideoDecoder_SetDestFmt(sh->context,16,out_fmt);break; // packed YUV
    case IMGFMT_YV12:
    case IMGFMT_I420:
    case IMGFMT_IYUV:
	DMO_VideoDecoder_SetDestFmt(sh->context,12,out_fmt);break; // planar YUV
    case IMGFMT_YVU9:
        DMO_VideoDecoder_SetDestFmt(sh->context,9,out_fmt);break;
    default:
	DMO_VideoDecoder_SetDestFmt(sh->context,out_fmt&255,0);    // RGB/BGR
    }
    DMO_VideoDecoder_StartInternal(sh->context);
    mp_msg(MSGT_DECVIDEO,MSGL_V,MSGTR_DMOInitOK);
    return 1;
}

// uninit driver
static void uninit(sh_video_t *sh){
    DMO_VideoDecoder_Destroy(sh->context);
}

//mp_image_t* mpcodecs_get_image(sh_video_t *sh, int mp_imgtype, int mp_imgflag, int w, int h);

// decode a frame
static mp_image_t* decode(sh_video_t *sh,void* data,int len,int flags){
    mp_image_t* mpi;
    if(len<=0) return NULL; // skipped frame
    
    if(flags&3){
	// framedrop:
        DMO_VideoDecoder_DecodeInternal(sh->context, data, len, 0, 0);
	return NULL;
    }
    
    mpi=mpcodecs_get_image(sh, MP_IMGTYPE_TEMP, 0 /*MP_IMGFLAG_ACCEPT_STRIDE*/, 
	sh->disp_w, sh->disp_h);
    
    if(!mpi){	// temporary!
	mp_msg(MSGT_DECVIDEO,MSGL_WARN,MSGTR_MPCODECS_CouldntAllocateImageForCinepakCodec);
	return NULL;
    }

    DMO_VideoDecoder_DecodeInternal(sh->context, data, len, 1, mpi->planes[0]);

    return mpi;
}
