/*
 *  video_out_internal.h
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

#include "../libmpcodecs/vfcap.h"
#include "../libmpcodecs/mp_image.h"

static uint32_t control(uint32_t request, void *data, ...);
static uint32_t config(uint32_t width, uint32_t height, uint32_t d_width,
		     uint32_t d_height, uint32_t fullscreen, char *title,
		     uint32_t format,const vo_tune_info_t *);
static const vo_info_t* get_info(void);
static uint32_t draw_frame(uint8_t *src[]);
static uint32_t draw_slice(uint8_t *image[], int stride[], int w,int h,int x,int y);
static void draw_osd(void);
static void flip_page(void);
static void check_events(void);
static void uninit(void);
static uint32_t query_format(uint32_t format);
static uint32_t preinit(const char *);
static void query_vaa(vo_vaa_t *);

#define LIBVO_EXTERN(x) vo_functions_t video_out_##x =\
{\
	preinit,\
	config,\
	control,\
	get_info,\
	draw_frame,\
	draw_slice,\
     	draw_osd,\
	flip_page,\
	check_events,\
	uninit\
};

#include "osd.h"



