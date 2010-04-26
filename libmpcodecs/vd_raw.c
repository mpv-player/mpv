/*
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "mp_msg.h"

#include "vd_internal.h"

static const vd_info_t info = {
	"RAW Uncompressed Video",
	"raw",
	"A'rpi",
	"A'rpi & Alex",
	"uncompressed"
};

LIBVD_EXTERN(raw)

// to set/get/query special features/parameters
static int control(sh_video_t *sh,int cmd,void* arg,...){
    int format = sh->bih ? sh->bih->biCompression : sh->format;
    switch(cmd){
    case VDCTRL_QUERY_FORMAT:
	if (*(int *)arg == format) return CONTROL_TRUE;
	if (*(int *)arg == IMGFMT_YUY2 && format == MKTAG('y', 'u', 'v', '2')) return CONTROL_TRUE;
	return CONTROL_FALSE;
    }
    return CONTROL_UNKNOWN;
}

// init driver
static int init(sh_video_t *sh){
    // set format fourcc for raw RGB:
    if(sh->bih && sh->bih->biCompression==0){	// set based on bit depth
	switch(sh->bih->biBitCount){
	case 1:  sh->bih->biCompression=IMGFMT_BGR1; break;
	case 4:  sh->bih->biCompression=IMGFMT_BGR4; break;
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
    int frame_size;
    int format = sh->bih ? sh->bih->biCompression : sh->format;

    if(len<=0) return NULL; // skipped frame

    mpi=mpcodecs_get_image(sh, MP_IMGTYPE_EXPORT, 0,
	sh->disp_w, sh->disp_h);
    if(!mpi) return NULL;

    if(mpi->flags&MP_IMGFLAG_PLANAR){
	// TODO !!!
	mpi->planes[0]=data;
	mpi->stride[0]=mpi->width;
	frame_size=mpi->stride[0]*mpi->h;
	if((mpi->imgfmt == IMGFMT_NV12) || (mpi->imgfmt == IMGFMT_NV21))
	{
	    mpi->planes[1]=mpi->planes[0]+mpi->width*mpi->height;
	    mpi->stride[1]=mpi->chroma_width;
	    frame_size+=mpi->chroma_width*mpi->chroma_height;
	} else if(mpi->flags&MP_IMGFLAG_YUV) {
    	    int cb=2, cr=1;
    	    if(mpi->flags&MP_IMGFLAG_SWAPPED) {
        	cb=1; cr=2;
    	    }
            // Support for some common Planar YUV formats
	    /* YV12,I420,IYUV */
            mpi->planes[cb]=mpi->planes[0]+mpi->width*mpi->height;
            mpi->stride[cb]=mpi->chroma_width;
            mpi->planes[cr]=mpi->planes[cb]+mpi->chroma_width*mpi->chroma_height;
            mpi->stride[cr]=mpi->chroma_width;
	    frame_size+=2*mpi->chroma_width*mpi->chroma_height;
       	}
    } else {
	mpi->planes[0]=data;
	mpi->stride[0]=mpi->width*(mpi->bpp/8);
	// .AVI files has uncompressed lines 4-byte aligned:
	if(sh->format==0 || sh->format==3) mpi->stride[0]=(mpi->stride[0]+3)&(~3);
	if(mpi->imgfmt==IMGFMT_RGB8 || mpi->imgfmt==IMGFMT_BGR8){
	    // export palette:
	    mpi->planes[1]=sh->bih ? (unsigned char*)(sh->bih+1) : NULL;
#if 0
	    printf("Exporting palette: %p !!\n",mpi->planes[1]);
	    {	unsigned char* p=mpi->planes[1];
		int i;
		for(i=0;i<64;i++) printf("%3d: %02X %02X %02X (%02X)\n",i,p[4*i],p[4*i+1],p[4*i+2],p[4*i+3]);
	    }
#endif
	}
	frame_size=mpi->stride[0]*mpi->h;
	if (len >= frame_size && format == MKTAG('y', 'u', 'v', '2')) {
	  int i;
	  for (i = 1; i < frame_size; i += 2)
	    mpi->planes[0][i] ^= 128;
	}
	if(mpi->bpp<8) frame_size=frame_size*mpi->bpp/8;
    }

    if(len<frame_size){
        mp_msg(MSGT_DECVIDEO,MSGL_WARN,"Frame too small! (%d<%d) Wrong format?\n",
	    len,frame_size);
	return NULL;
    }

    return mpi;
}
