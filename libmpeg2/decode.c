/* Copyright (C) Aaron Holtzman <aholtzma@ess.engr.uvic.ca> - Nov 1999 */
/* Some cleanup & hacking by A'rpi/ESP-team - Oct 2000 */

/* mpeg2dec version: */
#define PACKAGE "mpeg2dec"
//#define VERSION "0.1.7-cvs"
#define VERSION "0.1.8-cvs"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "config.h"

//#include "video_out.h"

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

int quant_store[MBR+1][MBC+1]; // [Review]

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
    picture->repeat_count=0;
    
    picture->pp_options=0;

    idct_init ();
    motion_comp_init ();
}

void mpeg2_allocate_image_buffers (picture_t * picture)
{
	int frame_size,buff_size;
        unsigned char *base=NULL;

        // height+1 requires for yuv2rgb_mmx code (it reads next line after last)
	frame_size = picture->coded_picture_width * (1+picture->coded_picture_height);
        frame_size = (frame_size+31)&(~31); // align to 32 byte boundary
        buff_size = frame_size + (frame_size/4)*2; // 4Y + 1U + 1V

	// allocate images in YV12 format
        base = shmem_alloc(buff_size);
	picture->throwaway_frame[0] = base;
	picture->throwaway_frame[1] = base + frame_size * 5 / 4;
	picture->throwaway_frame[2] = base + frame_size;

        base = shmem_alloc(buff_size);
	picture->backward_reference_frame[0] = base;
	picture->backward_reference_frame[1] = base + frame_size * 5 / 4;
	picture->backward_reference_frame[2] = base + frame_size;

        base = shmem_alloc(buff_size);
	picture->forward_reference_frame[0] = base;
	picture->forward_reference_frame[1] = base + frame_size * 5 / 4;
	picture->forward_reference_frame[2] = base + frame_size;

        base = shmem_alloc(buff_size);
	picture->pp_frame[0] = base;
	picture->pp_frame[1] = base + frame_size * 5 / 4;
	picture->pp_frame[2] = base + frame_size;

}

static void decode_reorder_frames (void)
{
    if (picture->picture_coding_type != B_TYPE) {

	//reuse the soon to be outdated forward reference frame
	picture->current_frame[0] = picture->forward_reference_frame[0];
	picture->current_frame[1] = picture->forward_reference_frame[1];
	picture->current_frame[2] = picture->forward_reference_frame[2];

	//make the backward reference frame the new forward reference frame
	picture->forward_reference_frame[0] =
	    picture->backward_reference_frame[0];
	picture->forward_reference_frame[1] =
	    picture->backward_reference_frame[1];
	picture->forward_reference_frame[2] =
	    picture->backward_reference_frame[2];

	picture->backward_reference_frame[0] = picture->current_frame[0];
	picture->backward_reference_frame[1] = picture->current_frame[1];
	picture->backward_reference_frame[2] = picture->current_frame[2];

    } else {

	picture->current_frame[0] = picture->throwaway_frame[0];
	picture->current_frame[1] = picture->throwaway_frame[1];
	picture->current_frame[2] = picture->throwaway_frame[2];

    }
}

static int in_slice_flag=0;

static int parse_chunk (vo_functions_t * output, int code, uint8_t * buffer)
{
    int is_frame_done = 0;

    stats_header (code, buffer);

    is_frame_done = in_slice_flag && ((!code) || (code >= 0xb0));
    if (is_frame_done) {
	in_slice_flag = 0;
        
        if(picture->picture_structure != FRAME_PICTURE) printf("Field! %d  \n",picture->second_field);
        
	    if ( ((HACK_MODE == 2) || (picture->mpeg1))
                && ((picture->picture_structure == FRAME_PICTURE) ||
		 (picture->second_field))
            ) {
	        uint8_t ** bar;
                int stride[3];

		if (picture->picture_coding_type == B_TYPE)
		    bar = picture->throwaway_frame;
		else
		    bar = picture->forward_reference_frame;
                
                stride[0]=picture->coded_picture_width;
                stride[1]=stride[2]=stride[0]/2;

                if(picture->pp_options){
                    // apply OpenDivX postprocess filter
                    postprocess(bar, stride[0],
                        picture->pp_frame, stride[0],
                        picture->coded_picture_width, picture->coded_picture_height, 
                        &quant_store[1][1], (MBC+1), picture->pp_options);
		    output->draw_slice (picture->pp_frame, stride, 
                        picture->display_picture_width,
                        picture->display_picture_height, 0, 0);
                } else {
		    output->draw_slice (bar, stride, 
                        picture->display_picture_width,
                        picture->display_picture_height, 0, 0);
                }
                
	    }
#ifdef ARCH_X86
	    if (config.flags & MM_ACCEL_X86_MMX) emms ();
#endif
	    output->flip_page ();
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

	    if(!(picture->second_field)) decode_reorder_frames ();
	}

	if (!drop_frame) {
	    uint8_t ** bar;

	    slice_process (picture, code, buffer);

	    if ((HACK_MODE < 2) && (!(picture->mpeg1))) {
		uint8_t * foo[3];
	        uint8_t ** bar;
		//frame_t * bar;
                int stride[3];
		int offset;

		if (picture->picture_coding_type == B_TYPE)
		    bar = picture->throwaway_frame;
		else
		    bar = picture->forward_reference_frame;

		offset = (code-1) * 4 * picture->coded_picture_width;
		if ((! HACK_MODE) && (picture->picture_coding_type == B_TYPE))
		    offset = 0;

		foo[0] = bar[0] + 4 * offset;
		foo[1] = bar[1] + offset;
		foo[2] = bar[2] + offset;
                
                stride[0]=picture->coded_picture_width;
                stride[1]=stride[2]=stride[0]/2;

		output->draw_slice (foo, stride, 
                    picture->display_picture_width, 16, 0, (code-1)*16);
	    }
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

