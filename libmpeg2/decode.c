/* Copyright (C) Aaron Holtzman <aholtzma@ess.engr.uvic.ca> - Nov 1999 */
/* Some cleanup & hacking by A'rpi/ESP-team - Oct 2000 */

/* mpeg2dec version: */
#define PACKAGE "mpeg2dec"
#define VERSION "0.2.0-release"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "config.h"

#include "video_out.h"
#include <inttypes.h>

#include "mpeg2.h"
#include "mpeg2_internal.h"

#include "../linux/shmem.h"

//#include "motion_comp.h"
//#include "idct.h"
//#include "header.h"
//#include "slice.h"
//#include "stats.h"

#include "attributes.h"
#ifdef __i386__
#include "mmx.h"
#endif

#include "mm_accel.h"


//this is where we keep the state of the decoder
//picture_t picture_data;
//picture_t *picture=&picture_data;
picture_t *picture=NULL;

//global config struct
mpeg2_config_t config;

// the maximum chunk size is determined by vbv_buffer_size which is 224K for
// MP@ML streams. (we make no pretenses ofdecoding anything more than that)
//static uint8_t chunk_buffer[224 * 1024 + 4];
//static uint32_t shift = 0;

static int drop_flag = 0;
static int drop_frame = 0;

#ifdef MPEG12_POSTPROC
int quant_store[MBR+1][MBC+1]; // [Review]
#endif

void mpeg2_init (void)
{

    printf (PACKAGE"-"VERSION" (C) 2000 Aaron Holtzman <aholtzma@ess.engr.uvic.ca>\n");
    config.flags = 0;
#ifdef HAVE_MMX
    config.flags |= MM_ACCEL_X86_MMX;
#endif
#ifdef HAVE_SSE
    config.flags |= MM_ACCEL_X86_MMXEXT;
#endif
#ifdef HAVE_3DNOW
    config.flags |= MM_ACCEL_X86_3DNOW;
#endif
#ifdef HAVE_MLIB
    config.flags |= MM_ACCEL_MLIB;
#endif

    printf("libmpeg2 config flags = 0x%X\n",config.flags);

    picture=shmem_alloc(sizeof(picture_t)); // !!! NEW HACK :) !!!

    header_state_init (picture);
//    picture->repeat_count=0;
    
    picture->pp_options=0;

    idct_init ();
    motion_comp_init ();
}

static vo_frame_t frames[4];

void mpeg2_allocate_image_buffers (picture_t * picture)
{
	int frame_size,buff_size;
        unsigned char *base=NULL;
	int i;

        // height+1 requires for yuv2rgb_mmx code (it reads next line after last)
	frame_size = picture->coded_picture_width * (1+picture->coded_picture_height);
        frame_size = (frame_size+31)&(~31); // align to 32 byte boundary
        buff_size = frame_size + (frame_size/4)*2; // 4Y + 1U + 1V

	// allocate images in YV12 format
#ifdef MPEG12_POSTPROC
	for(i=0;i<4;i++){
#else
	for(i=0;i<3;i++){
#endif
            base = shmem_alloc(buff_size);
	    frames[i].base[0] = base;
	    frames[i].base[1] = base + frame_size * 5 / 4;
	    frames[i].base[2] = base + frame_size;
	    frames[i].copy = NULL;
	    frames[i].vo = NULL;
	}
	
	picture->forward_reference_frame=&frames[0];
	picture->backward_reference_frame=&frames[1];
	picture->current_frame=&frames[2];

}

static void copy_slice (vo_frame_t * frame, uint8_t ** src){
    vo_functions_t * output = frame->vo;
    int stride[3];
    int y=picture->slice<<4;
    uint8_t* src_tmp[3];

    stride[0]=picture->coded_picture_width;
    stride[1]=stride[2]=stride[0]/2;
    
    if(frame!=picture->display_frame){
	uint8_t** base=picture->display_frame->base;
	src_tmp[0]=base[0]+stride[0]*y;
	src_tmp[1]=base[1]+stride[1]*(y>>1);
	src_tmp[2]=base[2]+stride[2]*(y>>1);
	src=src_tmp;
    }
    
    output->draw_slice (src,
		stride, picture->display_picture_width,
		(y+16<=picture->display_picture_height) ? 16 :
		    picture->display_picture_height-y,
		0, y);

    ++picture->slice;
}

static int in_slice_flag=0;

static int parse_chunk (vo_functions_t * output, int code, uint8_t * buffer)
{
    int is_frame_done = 0;

    stats_header (code, buffer);

    is_frame_done = in_slice_flag && ((!code) || (code >= 0xb0));
    if (is_frame_done) {
	in_slice_flag = 0;
        
//        if(picture->picture_structure != FRAME_PICTURE) printf("Field! %d  \n",picture->second_field);
        
	if (((picture->picture_structure == FRAME_PICTURE) ||
		 (picture->second_field))
           ) {
#ifdef MPEG12_POSTPROC
	       if(picture->pp_options){
                    // apply OpenDivX postprocess filter
            	    int stride[3];
            	    stride[0]=picture->coded_picture_width;
            	    stride[1]=stride[2]=stride[0]/2;
                    postprocess((picture->picture_coding_type == B_TYPE) ?
			    picture->current_frame->base :
			    picture->forward_reference_frame->base,
			stride[0], frames[3].base, stride[0],
                        picture->coded_picture_width, picture->coded_picture_height,
                        &quant_store[1][1], (MBC+1), picture->pp_options);
		    output->draw_slice (frames[3].base, stride, 
                        picture->display_picture_width,
                        picture->display_picture_height, 0, 0);
	       }// else
#endif
#if 0
		if (picture->picture_coding_type != B_TYPE) {
            	    int stride[3];
            	    stride[0]=picture->coded_picture_width;
            	    stride[1]=stride[2]=stride[0]/2;
		    output->draw_slice (picture->forward_reference_frame->base,
			stride, 
                        picture->display_picture_width,
                        picture->display_picture_height, 0, 0);
                }
#endif
	}
#ifdef ARCH_X86
	if (config.flags & MM_ACCEL_X86_MMX) emms();
#endif
	output->flip_page();
    }

    switch (code) {
    case 0x00:	/* picture_start_code */
	if (header_process_picture_header (picture, buffer)) {
	    printf ("bad picture header\n");
	    exit (1);
	}

	drop_frame = drop_flag && (picture->picture_coding_type == B_TYPE);
	//decode_reorder_frames ();
	break;

    case 0xb3:	/* sequence_header_code */
	if (header_process_sequence_header (picture, buffer)) {
	    printf ("bad sequence header\n");
	    exit (1);
	}
	break;

    case 0xb5:	/* extension_start_code */
	if (header_process_extension (picture, buffer)) {
	    printf ("bad extension\n");
	    exit (1);
	}
	break;

    default:
//	if (code >= 0xb9)  printf ("stream not demultiplexed ?\n");
	if (code >= 0xb0)  break;

	if (!(in_slice_flag)) {
	    in_slice_flag = 1;

//	    if(!(picture->second_field)) decode_reorder_frames ();

	    // set current_frame pointer:
	    if (picture->second_field){
//		vo_field (picture->current_frame, picture->picture_structure);
	    } else {
		if (picture->picture_coding_type == B_TYPE){
		    picture->display_frame=
		    picture->current_frame = &frames[2];
//		    picture->current_frame->copy=copy_slice;
		} else {
		    picture->current_frame = picture->forward_reference_frame;
		    picture->display_frame=
		    picture->forward_reference_frame = picture->backward_reference_frame;
		    picture->backward_reference_frame = picture->current_frame;
//		    picture->current_frame->copy=NULL;
		}
	    }

#if 1
#ifdef MPEG12_POSTPROC
	            if(picture->pp_options)
			picture->current_frame->copy=NULL; else
#endif
		    picture->current_frame->copy=copy_slice;
#endif
	    
	    picture->current_frame->vo=output;
	    picture->slice=0;

	}

	if (!drop_frame) {

	    slice_process (picture, code, buffer);

#ifdef ARCH_X86
	    if (config.flags & MM_ACCEL_X86_MMX) emms ();
#endif

	}
    }

    return is_frame_done;
}


int mpeg2_decode_data (vo_functions_t *output, uint8_t *current, uint8_t *end)
{
    //static uint8_t code = 0xff;
    //static uint8_t chunk_buffer[65536];
    //static uint8_t *chunk_ptr = chunk_buffer;
    //static uint32_t shift = 0;
  uint8_t code;
  uint8_t *pos=NULL;
  uint8_t *start=current;
  int ret = 0;

//  printf("RCVD %d bytes\n",end-current);

while(current<end){
  // FIND NEXT HEAD:
  unsigned int head=-1;
  uint8_t c;
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
    //if((code&0x100)!=0x100) printf("libmpeg2: FATAL! code=%X\n",code);
    //printf("pos=%d  chunk %3X  size=%d  next-code=%X\n",pos-start,code,current-pos,head|c);
    ret+=parse_chunk(output, code&0xFF, pos);
  }
  //--------------------
  pos=current;code=head|c;
}

  if(code==0x1FF) ret+=parse_chunk(output, 0xFF, NULL); // send 'end of frame'

    return ret;
}

void mpeg2_drop (int flag)
{
    drop_flag = flag;
}

