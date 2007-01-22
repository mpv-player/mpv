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
 *  along with mpeg2dec; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/* All video drivers will want this */
#include "libmpcodecs/vfcap.h"
#include "libmpcodecs/mp_image.h"
#include "geometry.h"

static int control(uint32_t request, void *data, ...);
static int config(uint32_t width, uint32_t height, uint32_t d_width,
		     uint32_t d_height, uint32_t fullscreen, char *title,
		     uint32_t format);
static int draw_frame(uint8_t *src[]);
static int draw_slice(uint8_t *image[], int stride[], int w,int h,int x,int y);
static void draw_osd(void);
static void flip_page(void);
static void check_events(void);
static void uninit(void);
static int query_format(uint32_t format);
static int preinit(const char *);

#define LIBVO_EXTERN(x) vo_functions_t video_out_##x =\
{\
	&info,\
	preinit,\
	config,\
	control,\
	draw_frame,\
	draw_slice,\
     	draw_osd,\
	flip_page,\
	check_events,\
	uninit\
};

#include "osd.h"



