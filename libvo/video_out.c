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

//
// Externally visible list of all vo drivers
//

int mga_next_frame=0;

extern vo_functions_t video_out_mga;
extern vo_functions_t video_out_xmga;
extern vo_functions_t video_out_x11;
extern vo_functions_t video_out_xv;
extern vo_functions_t video_out_gl;
extern vo_functions_t video_out_dga;
extern vo_functions_t video_out_fsdga;
extern vo_functions_t video_out_sdl;
extern vo_functions_t video_out_3dfx;
extern vo_functions_t video_out_null;
extern vo_functions_t video_out_odivx;
extern vo_functions_t video_out_pgm;
extern vo_functions_t video_out_md5;
extern vo_functions_t video_out_syncfb;

vo_functions_t* video_out_drivers[] =
{
#ifdef HAVE_MGA
#ifdef HAVE_X11
        &video_out_xmga,
#endif
        &video_out_mga,
#endif
#ifdef HAVE_SYNCFB
        &video_out_syncfb,
#endif
#ifdef HAVE_3DFX
        &video_out_3dfx,
#endif
#ifdef HAVE_XV
        &video_out_xv,
#endif
#ifdef HAVE_X11
        &video_out_x11,
#endif
#ifdef HAVE_GL
        &video_out_gl,
#endif
#ifdef HAVE_DGA
        &video_out_dga,
        &video_out_fsdga,
#endif
#ifdef HAVE_SDL
        &video_out_sdl,
#endif
        &video_out_null,
        &video_out_odivx,
        &video_out_pgm,
        &video_out_md5,
        NULL
};


