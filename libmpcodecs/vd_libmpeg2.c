#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "mp_msg.h"

#include "vd_internal.h"

static vd_info_t info = 
{
	"MPEG 1/2 Video decoder v2.0",
	"libmpeg2",
	VFM_MPEG,
	"A'rpi",
	"Aaron & Walken",
	"native"
};

LIBVD_EXTERN(libmpeg2)

#define USE_SIGJMP_TRICK

#ifdef USE_SIGJMP_TRICK
#include <signal.h>
#include <setjmp.h>
#endif

//#include "libmpdemux/parse_es.h"

#include "libvo/video_out.h"	// FIXME!!!

#include "libmpeg2/mpeg2.h"
#include "libmpeg2/mpeg2_internal.h"
#include "libmpeg2/mm_accel.h"

#include "../cpudetect.h"

mpeg2_config_t config;	// FIXME!!!
static picture_t *picture=NULL;	// exported from libmpeg2/decode.c

static int table_init_state=0;

// to set/get/query special features/parameters
static int control(sh_video_t *sh,int cmd,void* arg,...){
    return CONTROL_UNKNOWN;
}

static vo_frame_t frames[3];

// init driver
static int init(sh_video_t *sh){

    config.flags = 0;
if(gCpuCaps.hasMMX)
    config.flags |= MM_ACCEL_X86_MMX;
if(gCpuCaps.hasMMX2)
    config.flags |= MM_ACCEL_X86_MMXEXT;
if(gCpuCaps.has3DNow)
    config.flags |= MM_ACCEL_X86_3DNOW;
#ifdef HAVE_MLIB
    config.flags |= MM_ACCEL_MLIB;
#endif

    picture=malloc(sizeof(picture_t)); // !!! NEW HACK :) !!!
    memset(picture,0,sizeof(picture_t));
    header_state_init (picture);

    if(!table_init_state){
	idct_init ();
	motion_comp_init ();
	table_init_state=1;
    }

    picture->pp_options=divx_quality;
    
    picture->forward_reference_frame=&frames[0];
    picture->backward_reference_frame=&frames[1];
    picture->temp_frame=&frames[2];
    picture->current_frame=NULL;
    
    // send seq header to the decoder:  *** HACK ***
//    mpeg2_decode_data(NULL,videobuffer,videobuffer+videobuf_len,0);
//    mpeg2_allocate_image_buffers (picture);
    return mpcodecs_config_vo(sh,sh->disp_w,sh->disp_h,IMGFMT_YV12);
}

// uninit driver
static void uninit(sh_video_t *sh){
//    mpeg2_free_image_buffers (picture);
}

static void draw_slice (vo_frame_t * frame, uint8_t ** src){
    vo_functions_t * output = frame->vo;
    int stride[3];
    int y=picture->slice<<4;

    stride[0]=picture->coded_picture_width;
    stride[1]=stride[2]=stride[0]/2;

    output->draw_slice (src,
		stride, picture->display_picture_width,
		(y+16<=picture->display_picture_height) ? 16 :
		    picture->display_picture_height-y,
		0, y);
    
    ++picture->slice;
}

static int in_slice_flag=0; // FIXME! move to picture struct
static int drop_frame=0;    // FIXME! move to picture struct

static mp_image_t* parse_chunk (sh_video_t* sh, int code, uint8_t * buffer, int framedrop){
    mp_image_t* mpi=NULL;

//    stats_header (code, buffer);

    if (in_slice_flag && ((!code) || (code >= 0xb0))) {
	// ok, we've completed decoding a frame/field!
	in_slice_flag = 0;
	mpi=picture->display_frame->mpi;
	if(picture->picture_structure!=FRAME_PICTURE && !picture->second_field)
	    mpi=NULL; // we don't draw first fields!
    }

    switch (code) {
    case 0x00:	/* picture_start_code */
	if (header_process_picture_header (picture, buffer)) {
	    printf ("bad picture header\n");
	}
	drop_frame = framedrop && (picture->picture_coding_type == B_TYPE);
	drop_frame |= framedrop>=2; // hard drop
	break;

    case 0xb3:	/* sequence_header_code */
	if (header_process_sequence_header (picture, buffer)) {
	    printf ("bad sequence header\n");
	}
	break;

    case 0xb5:	/* extension_start_code */
	if (header_process_extension (picture, buffer)) {
	    printf ("bad extension\n");
	}
	break;

    default:
	if (code >= 0xb0)  break;

	if (!in_slice_flag) {
	    in_slice_flag = 1;

	    // set current_frame pointer:
	    if (!picture->second_field){
		mp_image_t* mpi;
		int flags;
		if (picture->picture_coding_type == B_TYPE){
		    flags=(!framedrop && vd_use_slices)?MP_IMGFLAG_DRAW_CALLBACK:0;
		    picture->display_frame=
		    picture->current_frame = picture->temp_frame;
		} else {
		    flags=MP_IMGFLAG_PRESERVE|MP_IMGFLAG_READABLE;
		    picture->current_frame = picture->forward_reference_frame;
		    picture->display_frame=
		    picture->forward_reference_frame = picture->backward_reference_frame;
		    picture->backward_reference_frame = picture->current_frame;
		}
		mpi=mpcodecs_get_image(sh,MP_IMGTYPE_IPB, flags,
			picture->coded_picture_width,
			picture->coded_picture_height);
		// ok, lets see what did we get:
		if(mpi->flags&MP_IMGFLAG_DRAW_CALLBACK &&
		 !(mpi->flags&MP_IMGFLAG_DIRECT)){
		    // nice, filter/vo likes draw_callback :)
		    picture->current_frame->copy=draw_slice;
		} else
		    picture->current_frame->copy=NULL;
		// let's, setup pointers!
		picture->current_frame->base[0]=mpi->planes[0];
		picture->current_frame->base[1]=mpi->planes[1];
		picture->current_frame->base[2]=mpi->planes[2];
		picture->current_frame->mpi=mpi;	// tricky!
#ifdef MPEG12_POSTPROC
		mpi->qscale=&picture->current_frame->quant_store[1][1];
		mpi->qstride=(MPEG2_MBC+1);
#endif
		mp_msg(MSGT_DECVIDEO,MSGL_DBG2,"mpeg2: [%c] %p  %s  \n",
		    (picture->picture_coding_type == B_TYPE) ? 'B':'P',
		    mpi, (mpi->flags&MP_IMGFLAG_DIRECT)?"DR!":"");
	    }

	    picture->current_frame->vo=sh->video_out;
	    picture->slice=0;

	}

	if (!drop_frame) {
	    slice_process (picture, code, buffer);
#ifdef ARCH_X86
	    if (config.flags & MM_ACCEL_X86_MMX) __asm__ __volatile__ ("emms");
#endif
	}

    }
    return mpi;
}

#ifdef USE_SIGJMP_TRICK

static jmp_buf mpeg2_jmp_buf;

static void mpeg2_sighandler(int sig){
    longjmp(mpeg2_jmp_buf,1);
}
#endif

// decode a frame
static mp_image_t* decode(sh_video_t *sh,void* data,int len,int flags){
    static uint32_t code;
    static uint8_t* pos;
    static uint8_t* current;
    uint8_t* end=data+len;
    static mp_image_t* mpi;
    mp_image_t* ret=NULL;
    int framedrop=flags&3;
    void* old_sigh;

    // Note: static is REQUIRED because of longjmp() may destroy stack!
    pos=NULL;
    current=data;
    mpi=NULL;

#ifdef USE_SIGJMP_TRICK
    old_sigh=signal(SIGSEGV,mpeg2_sighandler);
#endif

while(current<end){
  // FIND NEXT HEAD:
  static unsigned int head;
  static uint8_t c;
  head=-1;
  //--------------------
  while(current<end){
      c=current[0];
      ++current;
      head<<=8;
      if(head==0x100) break; // synced
      head|=c;
  }
  //--------------------
  if(pos){
#ifdef USE_SIGJMP_TRICK
    if(setjmp(mpeg2_jmp_buf)){
#ifdef ARCH_X86
	if (config.flags & MM_ACCEL_X86_MMX) __asm__ __volatile__ ("emms");
#endif
	printf("@@@ libmpeg2 returned from sig11... (bad file?) @@@\n");
    } else
#endif
    {
	ret=parse_chunk(sh, code&0xFF, pos, framedrop);
	if(ret) mpi=ret;
    }
  }
  //--------------------
  pos=current;code=head|c;
}

#ifdef USE_SIGJMP_TRICK
    signal(SIGSEGV,old_sigh); // restore sighandler
#endif

    if(code==0x1FF){
	ret=parse_chunk(sh, 0xFF, NULL, framedrop); // send 'end of frame'
	if(ret) mpi=ret;
    }

    return mpi;
}
