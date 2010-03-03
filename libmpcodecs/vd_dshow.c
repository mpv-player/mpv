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
#include <stdarg.h>

#include "config.h"

#include "mp_msg.h"
#include "help_mp.h"

#include "vd_internal.h"

#include "loader/dshow/DS_VideoDecoder.h"

static const vd_info_t info = {
	"DirectShow video codecs",
	"dshow",
	"A'rpi",
	"based on http://avifile.sf.net",
	"win32 codecs"
};

LIBVD_EXTERN(dshow)

// to set/get/query special features/parameters
static int control(sh_video_t *sh,int cmd,void* arg,...){
    switch(cmd){
    case VDCTRL_QUERY_MAX_PP_LEVEL:
	return 4;
    case VDCTRL_SET_PP_LEVEL:
	if(!sh->context) return CONTROL_ERROR;
	DS_VideoDecoder_SetValue(sh->context,"Quality",*((int*)arg));
	return CONTROL_OK;

    case VDCTRL_SET_EQUALIZER: {
	va_list ap;
	int value;
	va_start(ap, arg);
	value=va_arg(ap, int);
	va_end(ap);
	if(DS_VideoDecoder_SetValue(sh->context,arg,50+value/2)==0)
	    return CONTROL_OK;
	return CONTROL_FALSE;
    }

    }
    return CONTROL_UNKNOWN;
}

// init driver
static int init(sh_video_t *sh){
    unsigned int out_fmt=sh->codec->outfmt[0];

    /* Hack for VSSH codec: new dll can't decode old files
     * In my samples old files have no extradata, so use that info
     * to decide what dll should be used (here and in vd_vfw).
     */
    if (!strcmp(sh->codec->dll, "vsshdsd.dll") && (sh->bih->biSize == 40))
      return 0;

    if(!(sh->context=DS_VideoDecoder_Open(sh->codec->dll,&sh->codec->guid, sh->bih, 0, 0))){
        mp_msg(MSGT_DECVIDEO,MSGL_ERR,MSGTR_MissingDLLcodec,sh->codec->dll);
        mp_msg(MSGT_DECVIDEO,MSGL_HINT,MSGTR_DownloadCodecPackage);
	return 0;
    }
    if(!mpcodecs_config_vo(sh,sh->disp_w,sh->disp_h,out_fmt)) return 0;
    // mpcodecs_config_vo can change the format
    out_fmt=sh->codec->outfmt[sh->outfmtidx];
    switch(out_fmt){
    case IMGFMT_YUY2:
    case IMGFMT_UYVY:
	DS_VideoDecoder_SetDestFmt(sh->context,16,out_fmt);break; // packed YUV
    case IMGFMT_YV12:
    case IMGFMT_I420:
    case IMGFMT_IYUV:
	DS_VideoDecoder_SetDestFmt(sh->context,12,out_fmt);break; // planar YUV
    case IMGFMT_YVU9:
        DS_VideoDecoder_SetDestFmt(sh->context,9,out_fmt);break;
    default:
	DS_VideoDecoder_SetDestFmt(sh->context,out_fmt&255,0);    // RGB/BGR
    }
    DS_SetAttr_DivX("Quality",divx_quality);
    DS_VideoDecoder_StartInternal(sh->context);
    mp_msg(MSGT_DECVIDEO,MSGL_V,MSGTR_DShowInitOK);
    return 1;
}

// uninit driver
static void uninit(sh_video_t *sh){
    DS_VideoDecoder_Destroy(sh->context);
}

//mp_image_t* mpcodecs_get_image(sh_video_t *sh, int mp_imgtype, int mp_imgflag, int w, int h);

// decode a frame
static mp_image_t* decode(sh_video_t *sh,void* data,int len,int flags){
    mp_image_t* mpi;
    if(len<=0) return NULL; // skipped frame

    if(flags&3){
	// framedrop:
        DS_VideoDecoder_DecodeInternal(sh->context, data, len, 0, 0);
	return NULL;
    }

    mpi=mpcodecs_get_image(sh, MP_IMGTYPE_TEMP, MP_IMGFLAG_COMMON_PLANE,
	sh->disp_w, sh->disp_h);

    if(!mpi){	// temporary!
	mp_msg(MSGT_DECVIDEO,MSGL_WARN,MSGTR_MPCODECS_CouldntAllocateImageForCinepakCodec);
	return NULL;
    }

    DS_VideoDecoder_DecodeInternal(sh->context, data, len, 0, mpi->planes[0]);

    return mpi;
}
