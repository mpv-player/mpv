#include <stdio.h>
#include <stdlib.h>

#include "config.h"

#include "mp_msg.h"

#include "vd_internal.h"

//#undef MPEG12_POSTPROC

static vd_info_t info = 
{
	"MPEG 1/2 Video decoder libmpeg2-v0.4.0b",
	"libmpeg2",
	"A'rpi & Fabian Franz",
	"Aaron & Walken",
	"native"
};

LIBVD_EXTERN(libmpeg2)

//#include "libvo/video_out.h"	// FIXME!!!

#include "libmpeg2/mpeg2.h"
#include "libmpeg2/attributes.h"
#include "libmpeg2/mpeg2_internal.h"

#include "cpudetect.h"

typedef struct {
    mpeg2dec_t *mpeg2dec;
    int quant_store_idx;
    char *quant_store[3];
} vd_libmpeg2_ctx_t;

// to set/get/query special features/parameters
static int control(sh_video_t *sh,int cmd,void* arg,...){
    vd_libmpeg2_ctx_t *context = sh->context;
    mpeg2dec_t * mpeg2dec = context->mpeg2dec;
    const mpeg2_info_t * info = mpeg2_info (mpeg2dec);

    switch(cmd) {
    case VDCTRL_QUERY_FORMAT:
        if (info->sequence->width >> 1 == info->sequence->chroma_width &&
	    info->sequence->height >> 1 == info->sequence->chroma_height &&
	    (*((int*)arg)) == IMGFMT_YV12)
	    return CONTROL_TRUE;
	if (info->sequence->width >> 1 == info->sequence->chroma_width &&
		info->sequence->height == info->sequence->chroma_height &&
	    (*((int*)arg)) == IMGFMT_422P)
	    return CONTROL_TRUE;
	return CONTROL_FALSE;
    }
    
    return CONTROL_UNKNOWN;
}

// init driver
static int init(sh_video_t *sh){
    vd_libmpeg2_ctx_t *context;
    mpeg2dec_t * mpeg2dec;
//    const mpeg2_info_t * info;
    int accel;

    accel = 0;
    if(gCpuCaps.hasMMX)
       accel |= MPEG2_ACCEL_X86_MMX;
    if(gCpuCaps.hasMMX2)
       accel |= MPEG2_ACCEL_X86_MMXEXT;
    if(gCpuCaps.has3DNow)
       accel |= MPEG2_ACCEL_X86_3DNOW;
    if(gCpuCaps.hasAltiVec)
       accel |= MPEG2_ACCEL_PPC_ALTIVEC;
    #ifdef HAVE_VIS
       accel |= MPEG2_ACCEL_SPARC_VIS;
    #endif
    mpeg2_accel(accel);

    mpeg2dec = mpeg2_init ();

    if(!mpeg2dec) return 0;

    mpeg2_custom_fbuf(mpeg2dec,1); // enable DR1

    context = calloc(1, sizeof(vd_libmpeg2_ctx_t));
    context->mpeg2dec = mpeg2dec;
    sh->context = context;

    mpeg2dec->pending_buffer = 0;
    mpeg2dec->pending_length = 0;

    return 1;
}

// uninit driver
static void uninit(sh_video_t *sh){
    int i;
    vd_libmpeg2_ctx_t *context = sh->context;
    mpeg2dec_t * mpeg2dec = context->mpeg2dec;
    if (mpeg2dec->pending_buffer) free(mpeg2dec->pending_buffer);
    mpeg2dec->decoder.convert=NULL;
    mpeg2dec->decoder.convert_id=NULL;
    mpeg2_close (mpeg2dec);
    for (i=0; i < 3; i++)
	free(context->quant_store[i]);
    free(sh->context);
}

static void draw_slice (void * _sh, uint8_t * const * src, unsigned int y){ 
    sh_video_t* sh = (sh_video_t*) _sh;
    vd_libmpeg2_ctx_t *context = sh->context;
    mpeg2dec_t* mpeg2dec = context->mpeg2dec;
    const mpeg2_info_t * info = mpeg2_info (mpeg2dec);
    int stride[3];

//  printf("draw_slice() y=%d  \n",y);

    stride[0]=mpeg2dec->decoder.stride;
    stride[1]=stride[2]=mpeg2dec->decoder.uv_stride;

    mpcodecs_draw_slice(sh, (uint8_t **)src,
		stride, info->sequence->picture_width,
		(y+16<=info->sequence->picture_height) ? 16 :
		    info->sequence->picture_height-y,
		0, y);
}

// decode a frame
static mp_image_t* decode(sh_video_t *sh,void* data,int len,int flags){
    vd_libmpeg2_ctx_t *context = sh->context;
    mpeg2dec_t * mpeg2dec = context->mpeg2dec;
    const mpeg2_info_t * info = mpeg2_info (mpeg2dec);
    int drop_frame, framedrop=flags&3;

    // MPlayer registers its own draw_slice callback, prevent libmpeg2 from freeing the context
    mpeg2dec->decoder.convert=NULL;
    mpeg2dec->decoder.convert_id=NULL;
    
    if(len<=0) return NULL; // skipped null frame
    
    // append extra 'end of frame' code:
    ((char*)data+len)[0]=0;
    ((char*)data+len)[1]=0;
    ((char*)data+len)[2]=1;
    ((char*)data+len)[3]=0xff;
    len+=4;

    if (mpeg2dec->pending_length) {
	mpeg2_buffer (mpeg2dec, mpeg2dec->pending_buffer, mpeg2dec->pending_buffer + mpeg2dec->pending_length);
    } else {
        mpeg2_buffer (mpeg2dec, data, data+len);
    }
    
    while(1){
	int state=mpeg2_parse (mpeg2dec);
	int type, use_callback;
	mp_image_t* mpi_new;
	unsigned long pw, ph;
	
	switch(state){
	case STATE_BUFFER:
	    if (mpeg2dec->pending_length) {
		// just finished the pending data, continue with processing of the passed buffer
		mpeg2dec->pending_length = 0;
    		mpeg2_buffer (mpeg2dec, data, data+len);
    	    } else {
	        // parsing of the passed buffer finished, return.
		return 0;
	    }
	    break;
	case STATE_SEQUENCE:
	    pw = info->sequence->display_width * info->sequence->pixel_width;
	    ph = info->sequence->display_height * info->sequence->pixel_height;
	    if(ph) sh->aspect = (float) pw / (float) ph;
	    // video parameters inited/changed, (re)init libvo:
	    if (info->sequence->width >> 1 == info->sequence->chroma_width &&
		info->sequence->height >> 1 == info->sequence->chroma_height) {
		if(!mpcodecs_config_vo(sh,
				       info->sequence->picture_width,
				       info->sequence->picture_height, IMGFMT_YV12)) return 0;
	    } else if (info->sequence->width >> 1 == info->sequence->chroma_width &&
		info->sequence->height == info->sequence->chroma_height) {
		if(!mpcodecs_config_vo(sh,
				       info->sequence->picture_width,
				       info->sequence->picture_height, IMGFMT_422P)) return 0;
	    } else return 0;
	    break;
	case STATE_PICTURE:
	    type=info->current_picture->flags&PIC_MASK_CODING_TYPE;
	    
	    drop_frame = framedrop && (mpeg2dec->decoder.coding_type == B_TYPE);
            drop_frame |= framedrop>=2; // hard drop
            if (drop_frame) {
               mpeg2_skip(mpeg2dec, 1);
	       //printf("Dropping Frame ...\n");
	       break;
	    }
            mpeg2_skip(mpeg2dec, 0); //mpeg2skip skips frames until set again to 0

	    use_callback = (!framedrop && vd_use_slices &&
	    		    (info->current_picture->flags&PIC_FLAG_PROGRESSIVE_FRAME)) ?
			    MP_IMGFLAG_DRAW_CALLBACK:0;

	    // get_buffer "callback":
	    mpi_new=mpcodecs_get_image(sh,MP_IMGTYPE_IPB,
				       (type==PIC_FLAG_CODING_TYPE_B) ?
					use_callback : (MP_IMGFLAG_PRESERVE|MP_IMGFLAG_READABLE),
				       info->sequence->width,
				       info->sequence->height);

	    if(!mpi_new) return 0; // VO ERROR!!!!!!!!
	    mpeg2_set_buf(mpeg2dec, mpi_new->planes, mpi_new);
	    mpi_new->stride[0] = info->sequence->width;
	    mpi_new->stride[1] = info->sequence->chroma_width;
	    mpi_new->stride[2] = info->sequence->chroma_width;
	    if (info->current_picture->flags&PIC_FLAG_TOP_FIELD_FIRST)
		mpi_new->fields |= MP_IMGFIELD_TOP_FIRST;
	    else mpi_new->fields &= ~MP_IMGFIELD_TOP_FIRST;
	    if (info->current_picture->flags&PIC_FLAG_REPEAT_FIRST_FIELD)
		mpi_new->fields |= MP_IMGFIELD_REPEAT_FIRST;
	    else mpi_new->fields &= ~MP_IMGFIELD_REPEAT_FIRST;
	    mpi_new->fields |= MP_IMGFIELD_ORDERED;
            if (!(info->current_picture->flags&PIC_FLAG_PROGRESSIVE_FRAME))
                mpi_new->fields |= MP_IMGFIELD_INTERLACED;

#ifdef MPEG12_POSTPROC
	    mpi_new->qstride=info->sequence->width>>4;
	    {
	    char **p = &context->quant_store[type==PIC_FLAG_CODING_TYPE_B ?
					2 : (context->quant_store_idx ^= 1)];
	    *p = realloc(*p, mpi_new->qstride*(info->sequence->height>>4));
	    mpi_new->qscale = *p;
	    }
	    mpeg2dec->decoder.quant_store=mpi_new->qscale;
	    mpeg2dec->decoder.quant_stride=mpi_new->qstride;
	    mpi_new->pict_type=type; // 1->I, 2->P, 3->B
	    mpi_new->qscale_type= 1;
#endif

	    if (mpi_new->flags&MP_IMGFLAG_DRAW_CALLBACK
	        && !(mpi_new->flags&MP_IMGFLAG_DIRECT)) {
		// nice, filter/vo likes draw_callback :)
		mpeg2dec->decoder.convert=draw_slice;
		mpeg2dec->decoder.convert_id=sh;
	    } else {
	        mpeg2dec->decoder.convert=NULL;
	        mpeg2dec->decoder.convert_id=NULL;
	    }
	    
	    break;
	case STATE_SLICE:
	case STATE_END:
	case STATE_INVALID_END:
	    // decoding done:
	    if(info->display_fbuf) {
		mp_image_t* mpi = info->display_fbuf->id;
		if (mpeg2dec->pending_length == 0) {
		    mpeg2dec->pending_length = mpeg2dec->buf_end - mpeg2dec->buf_start;
		    mpeg2dec->pending_buffer = realloc(mpeg2dec->pending_buffer, mpeg2dec->pending_length);
		    memcpy(mpeg2dec->pending_buffer, mpeg2dec->buf_start, mpeg2dec->pending_length);
		} else {
		    // still some data in the pending buffer, shouldn't happen
		    mpeg2dec->pending_length = mpeg2dec->buf_end - mpeg2dec->buf_start;
		    memmove(mpeg2dec->pending_buffer, mpeg2dec->buf_start, mpeg2dec->pending_length);
		    mpeg2dec->pending_buffer = realloc(mpeg2dec->pending_buffer, mpeg2dec->pending_length + len);
		    memcpy(mpeg2dec->pending_buffer+mpeg2dec->pending_length, data, len);
		    mpeg2dec->pending_length += len;
		}
//		fprintf(stderr, "pending = %d\n", mpeg2dec->pending_length);
		return mpi;
	    }
	}
    }
}
