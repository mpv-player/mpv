#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "mp_msg.h"

#include "vd_internal.h"

static vd_info_t info = {
	"RAW Uncompressed Video",
	"raw",
	"A'rpi",
	"A'rpi & Alex",
	"uncompressed"
};

LIBVD_EXTERN(raw)

// to set/get/query special features/parameters
static int control(sh_video_t *sh,int cmd,void* arg,...){
    switch(cmd){
    case VDCTRL_QUERY_FORMAT:
	if( (*((int*)arg)) == (sh->bih ? sh->bih->biCompression : sh->format) ) return CONTROL_TRUE;
	return CONTROL_FALSE;
    }
    return CONTROL_UNKNOWN;
}

// init driver
static int init(sh_video_t *sh){
    // set format fourcc for raw RGB:
    if(sh->bih && sh->bih->biCompression==0){	// set based on bit depth
	switch(sh->bih->biBitCount){
	case 8:  sh->bih->biCompression=IMGFMT_BGR8; break;
	case 15: sh->bih->biCompression=IMGFMT_BGR15; break;
	// workaround bitcount==16 => bgr15 case for avi files:
	case 16: sh->bih->biCompression=(sh->format)?IMGFMT_BGR16:IMGFMT_BGR15; break;
	case 24: sh->bih->biCompression=IMGFMT_BGR24; break;
	case 32: sh->bih->biCompression=IMGFMT_BGR32; break;
	default:
	    mp_msg(MSGT_DECVIDEO,MSGL_WARN,"RAW: depth %d not supported\n",sh->bih->biBitCount);
	}
    }
    return mpcodecs_config_vo(sh,sh->disp_w,sh->disp_h,sh->bih ? sh->bih->biCompression : sh->format);
}

// uninit driver
static void uninit(sh_video_t *sh){
}

//mp_image_t* mpcodecs_get_image(sh_video_t *sh, int mp_imgtype, int mp_imgflag, int w, int h);

// decode a frame
static mp_image_t* decode(sh_video_t *sh,void* data,int len,int flags){
    mp_image_t* mpi;
    if(len<=0) return NULL; // skipped frame
    
    mpi=mpcodecs_get_image(sh, MP_IMGTYPE_EXPORT, 0, 
	sh->disp_w, sh->disp_h);
    if(!mpi) return NULL;

    if(mpi->flags&MP_IMGFLAG_PLANAR){
	// TODO !!!
	mpi->planes[0]=data;
	mpi->stride[0]=mpi->width;
        if(mpi->flags&MP_IMGFLAG_YUV) {
            // Support for some common Planar YUV formats
	    /* YV12,I420,IYUV */
            int cb=2, cr=1;
            if(mpi->flags&MP_IMGFLAG_SWAPPED) {
                cb=1; cr=2;
            }
            mpi->planes[cb]=mpi->planes[0]+mpi->width*mpi->height;
            mpi->stride[cb]=mpi->chroma_width;
            mpi->planes[cr]=mpi->planes[cb]+mpi->chroma_width*mpi->chroma_height;
            mpi->stride[cr]=mpi->chroma_width;
       	}
    } else {
	mpi->planes[0]=data;
	mpi->stride[0]=mpi->width*(mpi->bpp/8);
	// .AVI files has uncompressed lines 4-byte aligned:
	if(sh->format==0 || sh->format==3) mpi->stride[0]=(mpi->stride[0]+3)&(~3);
	if(mpi->imgfmt==IMGFMT_RGB8 || mpi->imgfmt==IMGFMT_BGR8){
	    // export palette:
	    mpi->planes[1]=sh->bih ? (((unsigned char*)&sh->bih)+40) : NULL;
	}
    }
    
    return mpi;
}
