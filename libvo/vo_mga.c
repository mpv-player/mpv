
//#define memcpy(a,b,c)

/* 
 *    video_out_mga.c
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
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. 
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "video_out.h"
#include "video_out_internal.h"

LIBVO_EXTERN(mga)

#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <linux/fb.h>

#include "drivers/mga_vid.h"
#include "sub.h"
#include "aspect.h"

static vo_info_t vo_info = 
{
	"Matrox G200/G400 overlay (/dev/mga_vid)",
	"mga",
	"Aaron Holtzman <aholtzma@ess.engr.uvic.ca>",
	""
};


#include "mga_common.c"

#define FBDEV	"/dev/fb0"

static uint32_t
config(uint32_t width, uint32_t height, uint32_t d_width, uint32_t d_height, uint32_t fullscreen, char *title, uint32_t format,const vo_tune_info_t *info)
{
char *devname=vo_subdevice?vo_subdevice:"/dev/mga_vid";

	if (f >= 0) mga_uninit();
	if(!vo_screenwidth || !vo_screenheight) {
		int fd;
		struct fb_var_screeninfo fbinfo;

		if(-1 != (fd = open(FBDEV, O_RDONLY))) {
			if(0 == ioctl(fd, FBIOGET_VSCREENINFO, &fbinfo)) {
				if(!vo_screenwidth)   vo_screenwidth = fbinfo.xres;
				if(!vo_screenheight) vo_screenheight = fbinfo.yres;
			} else {
				perror("FBIOGET_VSCREENINFO");
			}
			close(fd);
		} else {
			perror(FBDEV);
		}
	}

	if(vo_screenwidth && vo_screenheight){
		aspect_save_orig(width,height);
		aspect_save_prescale(d_width,d_height);
		aspect_save_screenres(vo_screenwidth,vo_screenheight);
	
		if(fullscreen&0x01) /* -fs */
			aspect(&d_width,&d_height,A_ZOOM);
		else
			aspect(&d_width,&d_height,A_NOZOOM);
		printf("vo_mga aspect(): resized to %dx%d\n",d_width,d_height);
	}


	f = open(devname,O_RDWR);
	if(f == -1)
	{
		printf("Couldn't open %s\n",devname); 
		return(-1);
	}

        switch(format){
        case IMGFMT_YV12:
	    width+=width&1;height+=height&1;
	    mga_vid_config.frame_size = ((width + 31) & ~31) * height + (((width + 31) & ~31) * height) / 2;
            mga_vid_config.format=MGA_VID_FORMAT_I420; break;
        case IMGFMT_I420:
        case IMGFMT_IYUV:
	    width+=width&1;height+=height&1;
	    mga_vid_config.frame_size = ((width + 31) & ~31) * height + (((width + 31) & ~31) * height) / 2;
            mga_vid_config.format=MGA_VID_FORMAT_YV12; break;
        case IMGFMT_YUY2:
	    mga_vid_config.frame_size = ((width + 31) & ~31) * height * 2;
            mga_vid_config.format=MGA_VID_FORMAT_YUY2; break;
        case IMGFMT_UYVY:
	    mga_vid_config.frame_size = ((width + 31) & ~31) * height * 2;
            mga_vid_config.format=MGA_VID_FORMAT_UYVY; break;
        default: 
            printf("mga: invalid output format %0X\n",format);
            return (-1);
        }

        mga_vid_config.colkey_on=0;

	mga_vid_config.src_width = width;
	mga_vid_config.src_height= height;
	mga_vid_config.dest_width = d_width;
	mga_vid_config.dest_height= d_height;
	mga_vid_config.x_org= 0; // (720-mga_vid_config.dest_width)/2;
	mga_vid_config.y_org= 0; // (576-mga_vid_config.dest_height)/2;
	if(vo_screenwidth && vo_screenheight){
		mga_vid_config.x_org=(vo_screenwidth-d_width)/2;
		mga_vid_config.y_org=(vo_screenheight-d_height)/2;
	}
	
	mga_vid_config.version=MGA_VID_VERSION;

  return mga_init();
}

static const vo_info_t*
get_info(void)
{
	return &vo_info;
}

static void
uninit(void)
{
 mga_uninit();
 printf("vo: uninit!\n");
}

static void draw_osd(void)
{
    vo_draw_text(mga_vid_config.src_width,mga_vid_config.src_height,draw_alpha);
}

static void flip_page(void)
{
    vo_mga_flip_page();
}


static void check_events(void)
{
}

