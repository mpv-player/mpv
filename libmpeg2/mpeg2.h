/*
 * mpeg2.h
 *
 * Copyright (C) Aaron Holtzman <aholtzma@ess.engr.uvic.ca> - Mar 2000
 *
 * This file is part of mpeg2dec, a free MPEG-2 video stream decoder.
 *	
 * mpeg2dec is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 * 
 * mpeg2dec is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with GNU Make; see the file COPYING. If not, write to
 * the Free Software Foundation, 
 *
 */

#ifdef __OMS__
#include <oms/plugin/output_video.h>
#ifndef vo_functions_t
#define vo_functions_t plugin_output_video_t
#endif
#else
//FIXME normally I wouldn't nest includes, but we'll leave this here until I get
//another chance to move things around
#include "video_out.h"
#endif

#include <inttypes.h>
#ifdef __OMS__
#include <oms/accel.h>
#else
#include "mm_accel.h"
#endif

//config flags
#define MPEG2_MLIB_ENABLE MM_ACCEL_MLIB
#define MPEG2_MMX_ENABLE MM_ACCEL_X86_MMX
#define MPEG2_3DNOW_ENABLE MM_ACCEL_X86_3DNOW
#define MPEG2_SSE_ENABLE MM_ACCEL_X86_MMXEXT

//typedef struct mpeg2_config_s {
//    //Bit flags that enable various things
//    uint32_t flags;
//} mpeg2_config_t;

void mpeg2_init (void);
//void mpeg2_allocate_image_buffers (picture_t * picture);
int mpeg2_decode_data (vo_functions_t *, uint8_t * data_start, uint8_t * data_end);
//void mpeg2_close (vo_functions_t *);
void mpeg2_drop (int flag);
