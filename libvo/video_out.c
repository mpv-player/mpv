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
#include <sys/mman.h>

#include "config.h"
#include "video_out.h"

#include "../linux/shmem.h"

// currect resolution/bpp on screen:  (should be autodetected by vo_init())
int vo_depthonscreen=0;
int vo_screenwidth=0;
int vo_screenheight=0;

// requested resolution/bpp:  (-x -y -bpp options)
int vo_dwidth=0;
int vo_dheight=0;
int vo_dbpp=0;
int vo_doublebuffering = 0;
int vo_fsmode = 0;

int vo_pts=0; // for hw decoding

char *vo_subdevice = NULL;

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
extern vo_functions_t video_out_pgm;
extern vo_functions_t video_out_md5;
extern vo_functions_t video_out_syncfb;
extern vo_functions_t video_out_fbdev;
extern vo_functions_t video_out_svga;
extern vo_functions_t video_out_png;
extern vo_functions_t video_out_ggi;
extern vo_functions_t video_out_aa;
extern vo_functions_t video_out_mpegpes;
extern vo_functions_t video_out_dxr3;
#ifdef TARGET_LINUX
extern vo_functions_t video_out_vesa;
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
#ifdef HAVE_DXR3
	&video_out_dxr3,
#endif

#ifdef HAVE_PNG
	&video_out_png,
#endif	
        &video_out_null,
//        &video_out_odivx,
        &video_out_pgm,
        &video_out_md5,
	&video_out_mpegpes,
#if defined( ARCH_X86 ) && defined( TARGET_LINUX )
	&video_out_vesa,
#endif
        NULL
};

#include "sub.c"
