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
#include <assert.h>

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"

#include "vd_internal.h"

#include "libavutil/intreadwrite.h"

static const vd_info_t info = {
   "Theora/VP3",
   "theora",
   "David Kuehling",
   "www.theora.org",
   "Theora project's VP3 codec"
};

LIBVD_EXTERN(theora)

#include <theora/theora.h>

#define THEORA_NUM_HEADER_PACKETS 3

typedef struct theora_struct_st {
    theora_state st;
    theora_comment cc;
    theora_info inf;
} theora_struct_t;

/** Convert Theora pixelformat to the corresponding IMGFMT_ */
static uint32_t theora_pixelformat2imgfmt(theora_pixelformat fmt){
    switch(fmt) {
       case OC_PF_420: return IMGFMT_YV12;
       case OC_PF_422: return IMGFMT_422P;
       case OC_PF_444: return IMGFMT_444P;
    }
    return 0;
}

// to set/get/query special features/parameters
static int control(sh_video_t *sh,int cmd,void* arg,...){
    theora_struct_t *context = sh->context;
    switch(cmd) {
    case VDCTRL_QUERY_FORMAT:
        if (*(int*)arg == theora_pixelformat2imgfmt(context->inf.pixelformat))
	    return CONTROL_TRUE;
	return CONTROL_FALSE;
    }

    return CONTROL_UNKNOWN;
}

/*
 * init driver
 */
static int init(sh_video_t *sh){
    theora_struct_t *context = NULL;
    uint8_t *extradata = (uint8_t *)(sh->bih + 1);
    int extradata_size = sh->bih->biSize - sizeof(*sh->bih);
    int errorCode = 0;
    ogg_packet op;
    int i;

    context = calloc (sizeof (theora_struct_t), 1);
    sh->context = context;
    if (!context)
        goto err_out;

    theora_info_init(&context->inf);
    theora_comment_init(&context->cc);

    /* Read all header packets, pass them to theora_decode_header. */
    for (i = 0; i < THEORA_NUM_HEADER_PACKETS; i++)
    {
        if (extradata_size > 2) {
            op.bytes  = AV_RB16(extradata);
            op.packet = extradata + 2;
            op.b_o_s  = 1;
            if (extradata_size < op.bytes + 2) {
                mp_msg(MSGT_DECVIDEO, MSGL_ERR, "Theora header too small\n");
                goto err_out;
            }
            extradata      += op.bytes + 2;
            extradata_size -= op.bytes + 2;
        } else {
            op.bytes = ds_get_packet (sh->ds, &op.packet);
            op.b_o_s = 1;
        }

        if ( (errorCode = theora_decode_header (&context->inf, &context->cc, &op)) )
        {
            mp_msg(MSGT_DECVIDEO, MSGL_ERR, "Broken Theora header; errorCode=%i!\n", errorCode);
            goto err_out;
        }
    }

    /* now init codec */
    errorCode = theora_decode_init (&context->st, &context->inf);
    if (errorCode)
    {
        mp_msg(MSGT_DECVIDEO,MSGL_ERR,"Theora decode init failed: %i \n", errorCode);
        goto err_out;
    }

    if(sh->aspect==0.0 && context->inf.aspect_denominator!=0)
    {
       sh->aspect = ((double)context->inf.aspect_numerator * context->inf.width)/
          ((double)context->inf.aspect_denominator * context->inf.height);
    }

    mp_msg(MSGT_DECVIDEO,MSGL_V,"INFO: Theora video init ok!\n");
    mp_msg(MSGT_DECVIDEO,MSGL_INFO,"Frame: %dx%d, Picture %dx%d, Offset [%d,%d]\n", context->inf.width, context->inf.height, context->inf.frame_width, context->inf.frame_height, context->inf.offset_x, context->inf.offset_y);

    return mpcodecs_config_vo (sh,context->inf.width,context->inf.height,theora_pixelformat2imgfmt(context->inf.pixelformat));

err_out:
    free(context);
    sh->context = NULL;
    return 0;
}

/*
 * uninit driver
 */
static void uninit(sh_video_t *sh)
{
   theora_struct_t *context = sh->context;

   if (context)
   {
      theora_info_clear(&context->inf);
      theora_comment_clear(&context->cc);
      theora_clear (&context->st);
      free (context);
   }
}

/*
 * decode frame
 */
static mp_image_t* decode(sh_video_t *sh,void* data,int len,int flags)
{
   theora_struct_t *context = sh->context;
   int errorCode = 0;
   ogg_packet op;
   yuv_buffer yuv;
   mp_image_t* mpi;

   // no delayed frames
   if (!data || !len)
       return NULL;

   memset (&op, 0, sizeof (op));
   op.bytes = len;
   op.packet = data;
   op.granulepos = -1;

   errorCode = theora_decode_packetin (&context->st, &op);
   if (errorCode)
   {
      mp_msg(MSGT_DECVIDEO,MSGL_ERR,"Theora decode packetin failed: %i \n",
	     errorCode);
      return NULL;
   }

   errorCode = theora_decode_YUVout (&context->st, &yuv);
   if (errorCode)
   {
      mp_msg(MSGT_DECVIDEO,MSGL_ERR,"Theora decode YUVout failed: %i \n",
	     errorCode);
      return NULL;
   }

    mpi = mpcodecs_get_image(sh, MP_IMGTYPE_EXPORT, 0, yuv.y_width, yuv.y_height);
    if(!mpi) return NULL;

    mpi->planes[0]=yuv.y;
    mpi->stride[0]=yuv.y_stride;
    mpi->planes[1]=yuv.u;
    mpi->stride[1]=yuv.uv_stride;
    mpi->planes[2]=yuv.v;
    mpi->stride[2]=yuv.uv_stride;

    return mpi;
}
