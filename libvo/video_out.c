/*
 * video_out.c,
 *
 * Copyright (C) Aaron Holtzman - June 2000
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

#include <unistd.h>
//#include <sys/mman.h>

#include "config.h"
#include "video_out.h"

#include "../linux/shmem.h"

//int vo_flags=0;

// currect resolution/bpp on screen:  (should be autodetected by vo_init())
int vo_depthonscreen=0;
int vo_screenwidth=0;
int vo_screenheight=0;

int vo_config_count=0;

// requested resolution/bpp:  (-x -y -bpp options)
int vo_dx=0;
int vo_dy=0;
int vo_dwidth=0;
int vo_dheight=0;
int vo_dbpp=0;

int vo_old_x = 0;
int vo_old_y = 0;
int vo_old_width = 0;
int vo_old_height = 0;

int vo_doublebuffering = 0;
int vo_vsync = 0;
int vo_fs = 0;
int vo_fsmode = 0;
float vo_panscan = 0.0f;

int vo_pts=0; // for hw decoding
float vo_fps=0; // for mp1e rte

char *vo_subdevice = NULL;
int vo_directrendering=0;
/****************************************
*	GAMMA CORRECTION		*
****************************************/
int vo_gamma_brightness=0;
int vo_gamma_saturation=0;
int vo_gamma_contrast=0;
int vo_gamma_hue=0;
int vo_gamma_red_intensity=0;
int vo_gamma_green_intensity=0;
int vo_gamma_blue_intensity=0;
//
// Externally visible list of all vo drivers
//
extern vo_functions_t video_out_mga;
extern vo_functions_t video_out_xmga;
extern vo_functions_t video_out_x11;
extern vo_functions_t video_out_xv;
extern vo_functions_t video_out_gl;
extern vo_functions_t video_out_gl2;
extern vo_functions_t video_out_dga;
extern vo_functions_t video_out_fsdga;
extern vo_functions_t video_out_sdl;
extern vo_functions_t video_out_3dfx;
extern vo_functions_t video_out_tdfxfb;
extern vo_functions_t video_out_null;
//extern vo_functions_t video_out_odivx;
extern vo_functions_t video_out_zr;
extern vo_functions_t video_out_pgm;
extern vo_functions_t video_out_md5;
extern vo_functions_t video_out_syncfb;
extern vo_functions_t video_out_fbdev;
extern vo_functions_t video_out_svga;
extern vo_functions_t video_out_png;
extern vo_functions_t video_out_ggi;
extern vo_functions_t video_out_aa;
extern vo_functions_t video_out_mpegpes;
extern vo_functions_t video_out_yuv4mpeg;
#ifdef HAVE_DXR2
extern vo_functions_t video_out_dxr2;
#endif
extern vo_functions_t video_out_dxr3;
#ifdef HAVE_JPEG
extern vo_functions_t video_out_jpeg;
#endif
#ifdef HAVE_GIF
extern vo_functions_t video_out_gif89a;
#endif
#ifdef HAVE_VESA
extern vo_functions_t video_out_vesa;
#endif
extern vo_functions_t video_out_directfb;
#ifdef CONFIG_VIDIX
extern vo_functions_t video_out_xvidix;
#endif

vo_functions_t* video_out_drivers[] =
{
#ifdef HAVE_XMGA
        &video_out_xmga,
#endif
#ifdef HAVE_MGA
        &video_out_mga,
#endif
#ifdef HAVE_SYNCFB
        &video_out_syncfb,
#endif
#ifdef HAVE_3DFX
        &video_out_3dfx,
#endif
#ifdef HAVE_TDFXFB
        &video_out_tdfxfb,
#endif
#ifdef HAVE_XV
        &video_out_xv,
#endif
#ifdef HAVE_X11
        &video_out_x11,
#endif
#ifdef HAVE_GL
        &video_out_gl,
        &video_out_gl2,
#endif
#ifdef HAVE_DGA
        &video_out_dga,
//        &video_out_fsdga,
#endif
#ifdef HAVE_SDL
        &video_out_sdl,
#endif
#ifdef HAVE_GGI
	&video_out_ggi,
#endif
#ifdef HAVE_FBDEV
	&video_out_fbdev,
#endif
#ifdef HAVE_SVGALIB
	&video_out_svga,
#endif
#ifdef HAVE_AA
	&video_out_aa,
#endif
#ifdef HAVE_DXR2
	&video_out_dxr2,
#endif
#ifdef HAVE_DXR3
	&video_out_dxr3,
#endif
#ifdef HAVE_ZR
	&video_out_zr,
#endif

#ifdef HAVE_PNG
	&video_out_png,
#endif	
#ifdef HAVE_JPEG
	&video_out_jpeg,
#endif
#ifdef HAVE_GIF
	&video_out_gif89a,
#endif
        &video_out_null,
//        &video_out_odivx,
        &video_out_pgm,
        &video_out_md5,
	&video_out_mpegpes,
	&video_out_yuv4mpeg,
#ifdef HAVE_VESA
	&video_out_vesa,
#endif
#ifdef HAVE_DIRECTFB
	&video_out_directfb,
#endif	
#if defined(CONFIG_VIDIX) && defined(HAVE_X11) 
	&video_out_xvidix,
#endif
        NULL
};

#ifdef HAVE_DXR2
extern void vo_dxr2_register_options(void*);
#endif

void libvo_register_options(void* cfg) {
#ifdef HAVE_DXR2
  vo_dxr2_register_options(cfg);
#endif
}
