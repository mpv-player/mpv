#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#ifdef USE_LIBMPEG2

#include "mp_msg.h"

#include "vd_internal.h"

//#undef MPEG12_POSTPROC

static vd_info_t info = 
{
	"MPEG 1/2 Video decoder libmpeg2-v0.3.1",
	"libmpeg2",
	"A'rpi & Fabian Franz",
	"Aaron & Walken",
	"native"
};

LIBVD_EXTERN(libmpeg2)

//#include "libvo/video_out.h"	// FIXME!!!

#include "libmpeg2/mpeg2.h"
#include "libmpeg2/mpeg2_internal.h"
//#include "libmpeg2/convert.h"

#include "../cpudetect.h"

// to set/get/query special features/parameters
static int control(sh_video_t *sh,int cmd,void* arg,...){
    return CONTROL_UNKNOWN;
}

// init driver
static int init(sh_video_t *sh){
    mpeg2dec_t * mpeg2dec;
    const mpeg2_info_t * info;
    int accel;

    accel = 0;
    if(gCpuCaps.hasMMX)
       accel |= MPEG2_ACCEL_X86_MMX;
    if(gCpuCaps.hasMMX2)
       accel |= MPEG2_ACCEL_X86_MMXEXT;
    if(gCpuCaps.has3DNow)
       accel |= MPEG2_ACCEL_X86_3DNOW;
    #ifdef HAVE_MLIB
       accel |= MPEG2_ACCEL_MLIB;
    #endif
    mpeg2_accel(accel);

    mpeg2dec = mpeg2_init ();

    if(!mpeg2dec) return 0;

    mpeg2_custom_fbuf(mpeg2dec,1); // enable DR1
    
    sh->context=mpeg2dec;

    return 1;
    //return mpcodecs_config_vo(sh,sh->disp_w,sh->disp_h,IMGFMT_YV12);
}

// uninit driver
static void uninit(sh_video_t *sh){
    mpeg2dec_t * mpeg2dec = sh->context;
    mpeg2_close (mpeg2dec);
}

static void draw_slice (void * _sh, uint8_t ** src, unsigned int y){ 
    sh_video_t* sh = (sh_video_t*) _sh;
    mpeg2dec_t* mpeg2dec = sh->context;
    const mpeg2_info_t * info = mpeg2_info (mpeg2dec);
    int stride[3];

    printf("draw_slice() y=%d  \n",y);

    stride[0]=mpeg2dec->decoder.stride;
    stride[1]=stride[2]=mpeg2dec->decoder.uv_stride;

    mpcodecs_draw_slice(sh, (uint8_t **)src,
		stride, info->sequence->display_width,
		(y+16<=info->sequence->display_height) ? 16 :
		    info->sequence->display_height-y,
		0, y);
}

// decode a frame
static mp_image_t* decode(sh_video_t *sh,void* data,int len,int flags){
    mpeg2dec_t * mpeg2dec = sh->context;
    const mpeg2_info_t * info = mpeg2_info (mpeg2dec);
    mp_image_t* mpi=NULL;
    int drop_frame, framedrop=flags&3;

    // append extra 'end of frame' code:
    ((char*)data+len)[0]=0;
    ((char*)data+len)[1]=0;
    ((char*)data+len)[2]=1;
    ((char*)data+len)[3]=0xff;
    len+=4;

    mpeg2_buffer (mpeg2dec, data, data+len);
    
    while(1){
	int state=mpeg2_parse (mpeg2dec);
	switch(state){
	case -1:
	    // parsing of the passed buffer finished, return.
//	    if(!mpi) printf("\nNO PICTURE!\n");
	    return mpi;
	case STATE_SEQUENCE:
	    // video parameters inited/changed, (re)init libvo:
	    if(!mpcodecs_config_vo(sh,
		info->sequence->width,
		info->sequence->height, IMGFMT_YV12)) return 0;
	    break;
	case STATE_PICTURE: {
	    int type=info->current_picture->flags&PIC_MASK_CODING_TYPE;
	    mp_image_t* mpi;
	    
	    drop_frame = framedrop && (mpeg2dec->decoder.coding_type == B_TYPE);
            drop_frame |= framedrop>=2; // hard drop
            if (drop_frame) {
               mpeg2_skip(mpeg2dec, 1);
	       //printf("Dropping Frame ...\n");
	       break;
	    }
            mpeg2_skip(mpeg2dec, 0); //mpeg2skip skips frames until set again to 0

	    // get_buffer "callback":
	     mpi=mpcodecs_get_image(sh,MP_IMGTYPE_IPB,
		(type==PIC_FLAG_CODING_TYPE_B)
		? ((!framedrop && vd_use_slices &&
		    (info->current_picture->flags&PIC_FLAG_PROGRESSIVE_FRAME)) ?
			    MP_IMGFLAG_DRAW_CALLBACK:0)
		: (MP_IMGFLAG_PRESERVE|MP_IMGFLAG_READABLE),
		info->sequence->picture_width,
		info->sequence->picture_height);
	    if(!mpi) return 0; // VO ERROR!!!!!!!!
	    mpeg2_set_buf(mpeg2dec, mpi->planes, mpi);

#ifdef MPEG12_POSTPROC
	    if(!mpi->qscale){
		mpi->qstride=info->sequence->picture_width>>4;
		mpi->qscale=malloc(mpi->qstride*(info->sequence->picture_height>>4));
	    }
	    mpeg2dec->decoder.quant_store=mpi->qscale;
	    mpeg2dec->decoder.quant_stride=mpi->qstride;
	    mpi->pict_type=type; // 1->I, 2->P, 3->B
#endif

	    if(mpi->flags&MP_IMGFLAG_DRAW_CALLBACK &&
		!(mpi->flags&MP_IMGFLAG_DIRECT)){
		   // nice, filter/vo likes draw_callback :)
		    mpeg2dec->decoder.convert=draw_slice;
		    mpeg2dec->decoder.fbuf_id=sh;
		} else
		    mpeg2dec->decoder.convert=NULL;
	    break;
	}
	case STATE_SLICE:
	case STATE_END:
	    // decoding done:
	    if(mpi) printf("AJAJJJJJJJJ2!\n");
	    if(info->display_fbuf) mpi=info->display_fbuf->id;
//	    return mpi;
	}
    }
}
#endif
