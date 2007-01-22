
// How many MegaBytes of RAM is on your G200/G400 card?
#define RAM_SIZE 16

/* 
 *    video_out_syncfb.c
 *
 *	Copyright (C) Aaron Holtzman - Aug 1999
 *
 *  This file is part of mpeg2dec, a free MPEG-2 video stream decoder.
 *	
 *  mpeg2dec is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *   
 *  mpeg2dec is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *   
 *  You should have received a copy of the GNU General Public License
 *  along with mpeg2dec; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"
#include "video_out.h"
#include "video_out_internal.h"

#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <linux/videodev.h>

#include "drivers/syncfb/syncfb.h"

#include "fastmemcpy.h"

static vo_info_t info =
{
	"Matrox G200/G400 Synchronous framebuffer (/dev/syncfb)",
	"syncfb",
	"Matthias Oelmann <mao@well.com>",
	""
};

LIBVO_EXTERN(syncfb)

/* deinterlacing on? looks only good in 50 Hz(PAL) or 60 Hz(NTSC) modes */
static int vo_conf_deinterlace = 0;

/* 72/75 Hz Monitor frequency for progressive output */
static int vo_conf_cinemode = 0;


static syncfb_config_t _config;
static syncfb_capability_t sfb_caps;

static syncfb_buffer_info_t bufinfo;

static uint_8 *vid_data;
static uint_8 *frame_mem;

static int debug_skip_first = 250;
static int dbg_singleframe = 0;

static int conf_palette;

static int f;



/*
	it seems that mpeg2dec never calls
	draw_frame, so i could not test it....
*/

static void
write_frame_YUV422(uint_8 *y,uint_8 *cr, uint_8 *cb)
{
	uint_8 *crp, *cbp;
	uint_32 *dest32;
	uint_32 bespitch,h,w;


	bespitch = _config.src_pitch;
	dest32 = (uint_32 *)vid_data;

	for(h=0; h < _config.src_height/2; h++)
	{
		cbp = cb;
		crp = cr;
		for(w=0; w < _config.src_width/2; w++)
		{
			*dest32++ = (*y) + ((*cr)<<8) + ((*(y+1))<<16) + ((*cb)<<24);
			y++; y++; cb++; cr++;
		}
		dest32 += (bespitch - _config.src_width) / 2;

		for(w=0; w < _config.src_width/2; w++)
		{
			*dest32++ = (*y) + ((*crp)<<8) + ((*(y+1))<<16) + ((*cbp)<<24);
			y++; y++; cbp++; crp++;
		}
		dest32 += (bespitch - _config.src_width) / 2;
	}
}


static void
write_frame_YUV420P2(uint_8 *y,uint_8 *cr, uint_8 *cb)
{
	uint_8 *dest, *tmp;
	uint_32 bespitch,h,w;

	bespitch = _config.src_pitch;
	dest = frame_mem + bufinfo.offset;

	for(h=0; h < _config.src_height; h++)
	{
		memcpy(dest, y, _config.src_width);
		y += _config.src_width;
		dest += bespitch;
	}

	dest = frame_mem + bufinfo.offset_p2;
	for(h=0; h < _config.src_height/2; h++)
	{
		tmp = dest;
		for(w=0; w < _config.src_width/2; w++)
		{
			*tmp++ =  *cr++;
			*tmp++ =  *cb++;
		}
		dest += bespitch;
	}
}

static void
write_frame_YUV420P3(uint_8 *y,uint_8 *cr, uint_8 *cb)
{
}

static void
write_slice_YUV420P2(uint_8 *y,uint_8 *cr, uint_8 *cb,uint_32 slice_num)
{
	uint_8 *dest, *tmp;
	uint_32 bespitch,h,w;

	bespitch = _config.src_pitch;
	dest = frame_mem + bufinfo.offset + (bespitch * 16 * slice_num);

	for(h=0; h < 16; h++)
	{
		memcpy(dest, y, _config.src_width);
		y += _config.src_width;
		dest += bespitch;
	}

	dest = frame_mem + bufinfo.offset_p2 + (bespitch * 16 * slice_num) /2;
	for(h=0; h < 8; h++)
	{
		tmp = dest;
		for(w=0; w < _config.src_width/2; w++)
		{
			*tmp++ =  *cr++;
			*tmp++ =  *cb++;
		}
		dest += bespitch;
	}
}

static void
write_slice_YUV420P3(uint_8 *y,uint_8 *cr, uint_8 *cb,int stride[],uint_32 ypos,uint_32 xsize,uint_32 ysize)
{
	uint_8 *dest;
	uint_32 bespitch,h;

	bespitch = _config.src_pitch;

	dest = frame_mem + bufinfo.offset + (bespitch * ypos);
	for(h=0; h < ysize; h++)
	{
		memcpy(dest, y, xsize);
		y += stride[0];
		dest += bespitch;
	}
        
        xsize/=2;
        ysize/=2;

	dest = frame_mem + bufinfo.offset_p2 + (bespitch * ypos)/4;
	for(h=0; h < ysize; h++)
	{
		memcpy(dest, cr, xsize);
		cr += stride[1];
		dest += bespitch/2;
	}

	dest = frame_mem + bufinfo.offset_p3 + (bespitch * ypos)/4;
	for(h=0; h < ysize; h++)
	{
		memcpy(dest, cb, xsize);
		cb += stride[2];
		dest += bespitch/2;
	}


}


static void
write_slice_YUV422(uint_8 *y,uint_8 *cr, uint_8 *cb,uint_32 slice_num)
{
	uint_8 *crp, *cbp;
	uint_32 *dest32;
	uint_32 bespitch,h,w;


	bespitch = _config.src_pitch;
	dest32 = (uint_32 *)(vid_data + (bespitch * 16 * slice_num) * 2);

	for(h=0; h < 8; h++)
	{
		cbp = cb;
		crp = cr;
		for(w=0; w < _config.src_width/2; w++)
		{
			*dest32++ = (*y) + ((*cr)<<8) + ((*(y+1))<<16) + ((*cb)<<24);
			y++; y++; cb++; cr++;
		}
		dest32 += (bespitch - _config.src_width) / 2;

		for(w=0; w < _config.src_width/2; w++)
		{
			*dest32++ = (*y) + ((*crp)<<8) + ((*(y+1))<<16) + ((*cbp)<<24);
			y++; y++; cbp++; crp++;
		}
		dest32 += (bespitch - _config.src_width) / 2;
	}
}

//static uint32_t draw_slice(uint8_t *src[], uint32_t slice_num)
static int
draw_slice(uint8_t *src[], int stride[], int w,int h,int x,int y)
{

    if ( vid_data == NULL ) return 0;

    write_slice_YUV420P3(src[0],src[1], src[2],stride,y,w,h);

    //printf("sorry, not syncfb/draw_slice() implemented yet...\n");

#if 0

	if ( conf_palette == VIDEO_PALETTE_YUV422 ) {
		write_slice_YUV422(src[0],src[1], src[2],slice_num);
	} else if ( conf_palette == VIDEO_PALETTE_YUV420P2 ) {
		write_slice_YUV420P2(src[0],src[1], src[2],slice_num);
	} else if ( conf_palette == VIDEO_PALETTE_YUV420P3 ) {
		write_slice_YUV420P3(src[0],src[1], src[2],slice_num);
	}
#endif

	return 0;
}

static void draw_osd(void)
{
}

static void
flip_page(void)
{

//	memset(frame_mem + bufinfo.offset_p2, 0x80, _config.src_width*config.src_height);
	ioctl(f,SYNCFB_COMMIT_BUFFER,&bufinfo);

	if ( dbg_singleframe ) {
		if ( debug_skip_first == 0 ) {
			mp_msg(MSGT_VO,MSGL_INFO, "Press 'anykey' for field 1\n" );
			getchar();
			ioctl(f,SYNCFB_VBI,0);
		}

		if ( debug_skip_first > 0 ) {
			debug_skip_first--;
			// debug_skip_first = 0;
			if ( debug_skip_first == 0 ) {
			ioctl(f,SYNCFB_VBI,0);
			ioctl(f,SYNCFB_VBI,0);
			ioctl(f,SYNCFB_VBI,0);
			}
		}

		if ( debug_skip_first == 0 ) {
			mp_msg(MSGT_VO,MSGL_INFO, "Press 'anykey' for field 2\n" );
			getchar();
			ioctl(f,SYNCFB_VBI,0);
		}
	}

	ioctl(f,SYNCFB_REQUEST_BUFFER,&bufinfo);
	if ( bufinfo.id == -1 ) mp_msg(MSGT_VO,MSGL_INFO, "Got buffer #%d\n", bufinfo.id );

	vid_data = (uint_8 *)(frame_mem + bufinfo.offset);
	if ( bufinfo.id == -1 ) {
		//vid_data = frame_mem;
		vid_data = NULL;
	}
//	printf("Flip %d\n", bufinfo.offset);

}

static int draw_frame(uint8_t *src[])
{
	mp_msg(MSGT_VO,MSGL_INFO, "DRAW FRAME!!!\n");
	if ( conf_palette == VIDEO_PALETTE_YUV422 ) {
		write_frame_YUV422(src[0],src[1], src[2]);
	} else if ( conf_palette == VIDEO_PALETTE_YUV420P2 ) {
		write_frame_YUV420P2(src[0],src[1], src[2]);
	} else if ( conf_palette == VIDEO_PALETTE_YUV420P3 ) {
		write_frame_YUV420P3(src[0],src[1], src[2]);
	}

	flip_page();
	return 0;
}

static int
query_format(uint32_t format)
{
    switch(format){
    case IMGFMT_YV12:
//    case IMGFMT_RGB|24:
//    case IMGFMT_BGR|24:
        return VFCAP_CSP_SUPPORTED;
    }
    return 0;
}

static int config(uint32_t width, uint32_t height, uint32_t d_width, uint32_t d_height, uint32_t flags, char *title, uint32_t format)
{
	uint_32 frame_size;

	f = open("/dev/syncfb",O_RDWR);

	if(f == -1)
	{
		f = open("/dev/mga_vid",O_RDWR);
		if(f == -1)
		{
			mp_msg(MSGT_VO,MSGL_ERR, MSGTR_LIBVO_SYNCFB_CouldntOpen);
			return(-1);
		}
	}

	if (ioctl(f,SYNCFB_GET_CAPS,&sfb_caps)) perror("Error in mga_vid_config ioctl");
	if (ioctl(f,SYNCFB_GET_CONFIG,&config)) perror("Error in mga_vid_config ioctl");

	if (sfb_caps.palettes & (1<<VIDEO_PALETTE_YUV420P3) ) {
		_config.src_palette= VIDEO_PALETTE_YUV420P3;
		mp_msg(MSGT_VO,MSGL_INFO, MSGTR_LIBVO_SYNCFB_UsingPaletteYuv420p3);
	}else if ( sfb_caps.palettes & (1<<VIDEO_PALETTE_YUV420P2) ) {
		_config.src_palette= VIDEO_PALETTE_YUV420P2;
		mp_msg(MSGT_VO,MSGL_INFO, MSGTR_LIBVO_SYNCFB_UsingPaletteYuv420p2);
	} else if ( sfb_caps.palettes & (1<<VIDEO_PALETTE_YUV422) ) {
		_config.src_palette= VIDEO_PALETTE_YUV422;
		mp_msg(MSGT_VO,MSGL_INFO, MSGTR_LIBVO_SYNCFB_UsingPaletteYuv420);
	} else {
		mp_msg(MSGT_VO,MSGL_ERR, MSGTR_LIBVO_SYNCFB_NoSupportedPaletteFound);
		return -1;
	}

	// _config.src_palette= VIDEO_PALETTE_YUV422;

	if ( vo_conf_cinemode ) {
		_config.default_repeat = 3;
	} else {
		_config.default_repeat = 2;
	}

	conf_palette = _config.src_palette;
	if ( vo_conf_deinterlace ) {
		_config.syncfb_mode = SYNCFB_FEATURE_SCALE | SYNCFB_FEATURE_BLOCK_REQUEST | SYNCFB_FEATURE_DEINTERLACE;
		_config.default_repeat = 1;
	} else {
		_config.syncfb_mode = SYNCFB_FEATURE_SCALE | SYNCFB_FEATURE_BLOCK_REQUEST;
	}

	_config.fb_screen_size = (RAM_SIZE-4)*0x100000; //(1280 * 1024 * 32) / 8;
	_config.src_width = width;
	_config.src_height= height;

	_config.image_width = d_width;
	_config.image_height= d_height;
	//_config.image_width = 1024;
	//_config.image_height= 576;

	_config.image_xorg= 0;
	_config.image_yorg= 0;


	mp_msg(MSGT_VO,MSGL_INFO, MSGTR_LIBVO_SYNCFB_BesSourcerSize, width, height);

	ioctl(f,SYNCFB_ON,0);
	if (ioctl(f,SYNCFB_SET_CONFIG,&_config)) perror("Error in mga_vid_config ioctl");

	mp_msg(MSGT_VO,MSGL_INFO, MSGTR_LIBVO_SYNCFB_FramebufferMemory, sfb_caps.memory_size, _config.buffers);

	frame_size = ((width + 31) & ~31) * height + (((width + 31) & ~31) * height) / 2;
	frame_mem = (uint_8*)mmap(0,sfb_caps.memory_size,PROT_WRITE,MAP_SHARED,f,0);

	mp_msg(MSGT_VO,MSGL_INFO, MSGTR_LIBVO_SYNCFB_RequestingFirstBuffer, bufinfo.id );
	ioctl(f,SYNCFB_REQUEST_BUFFER,&bufinfo);
	mp_msg(MSGT_VO,MSGL_INFO, MSGTR_LIBVO_SYNCFB_GotFirstBuffer, bufinfo.id );


	vid_data = (uint_8 *)(frame_mem + bufinfo.offset);

	//clear the buffer
	// memset(frame_mem,0x80,frame_size*2);
  	return 0;
}


static void
uninit(void)
{
	if (ioctl(f,SYNCFB_OFF,0)) perror("Error in OFF ioctl");

}

static void check_events(void)
{
}

static int preinit(const char *arg)
{
    if(arg) 
    {
	mp_msg(MSGT_VO,MSGL_ERR, MSGTR_LIBVO_SYNCFB_UnknownSubdevice,arg);
	return ENOSYS;
    }
    return 0;
}

static int control(uint32_t request, void *data, ...)
{
  switch (request) {
  case VOCTRL_QUERY_FORMAT:
    return query_format(*((uint32_t*)data));
  }
  return VO_NOTIMPL;
}
