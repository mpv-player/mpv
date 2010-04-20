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

#include "config.h"

#if HAVE_MALLOC_H
#include <malloc.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "mp_msg.h"
#include "help_mp.h"

#include "vd_internal.h"

#include "loader/dmo/DMO_VideoDecoder.h"

static const vd_info_t info = {
	"DMO video codecs",
	"dmo",
	"A'rpi",
	"based on http://avifile.sf.net",
	"win32 codecs"
};

LIBVD_EXTERN(dmo)

struct context {
    void *decoder;
    uint8_t *buffer;
    int stride;
};

// to set/get/query special features/parameters
static int control(sh_video_t *sh,int cmd,void* arg,...){
    return CONTROL_UNKNOWN;
}

// init driver
static int init(sh_video_t *sh){
    unsigned int out_fmt=sh->codec->outfmt[0];
    struct context *ctx;
    void *decoder;
    if(!(decoder=DMO_VideoDecoder_Open(sh->codec->dll,&sh->codec->guid, sh->bih, 0, 0))){
        mp_msg(MSGT_DECVIDEO,MSGL_ERR,MSGTR_MissingDLLcodec,sh->codec->dll);
        mp_msg(MSGT_DECVIDEO,MSGL_HINT,MSGTR_DownloadCodecPackage);
	return 0;
    }
    if(!mpcodecs_config_vo(sh,sh->disp_w,sh->disp_h,out_fmt)) return 0;
    // mpcodecs_config_vo can change the format
    out_fmt=sh->codec->outfmt[sh->outfmtidx];
    sh->context = ctx = calloc(1, sizeof(*ctx));
    ctx->decoder = decoder;
    switch(out_fmt){
    case IMGFMT_YUY2:
    case IMGFMT_UYVY:
	DMO_VideoDecoder_SetDestFmt(ctx->decoder,16,out_fmt);break; // packed YUV
    case IMGFMT_YV12:
    case IMGFMT_I420:
    case IMGFMT_IYUV:
	DMO_VideoDecoder_SetDestFmt(ctx->decoder,12,out_fmt);break; // planar YUV
    case IMGFMT_YVU9:
        DMO_VideoDecoder_SetDestFmt(ctx->decoder,9,out_fmt);break;
    case IMGFMT_RGB24:
    case IMGFMT_BGR24:
        if (sh->disp_w & 3)
        {
            ctx->stride = ((sh->disp_w * 3) + 3) & ~3;
            ctx->buffer = av_malloc(ctx->stride * sh->disp_h);
        }
    default:
	DMO_VideoDecoder_SetDestFmt(ctx->decoder,out_fmt&255,0);    // RGB/BGR
    }
    DMO_VideoDecoder_StartInternal(ctx->decoder);
    mp_msg(MSGT_DECVIDEO,MSGL_V,MSGTR_DMOInitOK);
    return 1;
}

// uninit driver
static void uninit(sh_video_t *sh){
    struct context *ctx = sh->context;
    DMO_VideoDecoder_Destroy(ctx->decoder);
    av_free(ctx->buffer);
    free(ctx);
    sh->context = NULL;
}

//mp_image_t* mpcodecs_get_image(sh_video_t *sh, int mp_imgtype, int mp_imgflag, int w, int h);

// decode a frame
static mp_image_t* decode(sh_video_t *sh,void* data,int len,int flags){
    struct context *ctx = sh->context;
    uint8_t *buffer = ctx->buffer;
    int type = ctx->buffer ? MP_IMGTYPE_EXPORT : MP_IMGTYPE_TEMP;
    mp_image_t* mpi;
    if(len<=0) return NULL; // skipped frame

    if(flags&3){
	// framedrop:
        DMO_VideoDecoder_DecodeInternal(ctx->decoder, data, len, 0, 0);
	return NULL;
    }

    mpi=mpcodecs_get_image(sh, type, MP_IMGFLAG_COMMON_PLANE,
	sh->disp_w, sh->disp_h);
    if (buffer) {
        mpi->planes[0] = buffer;
        mpi->stride[0] = ctx->stride;
    } else {
        buffer = mpi->planes[0];
    }

    if(!mpi){	// temporary!
	mp_msg(MSGT_DECVIDEO,MSGL_WARN,MSGTR_MPCODECS_CouldntAllocateImageForCinepakCodec);
	return NULL;
    }

    DMO_VideoDecoder_DecodeInternal(ctx->decoder, data, len, 1, buffer);

    return mpi;
}
