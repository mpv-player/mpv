/*
 *
 * radeon_vid.h
 *
 * Copyright (C) 2001 Nick Kurshev
 * 
 * BES YUV Framebuffer driver for Radeon cards
 * 
 * This software has been released under the terms of the GNU Public
 * license. See http://www.gnu.org/copyleft/gpl.html for details.
 *
 * This file is partly based on mga_vid and sis_vid stuff from
 * mplayer's package.
 */

#ifndef __RADEON_VID_INCLUDED
#define __RADEON_VID_INCLUDED

typedef struct mga_vid_config_s
{
uint16_t version;
uint16_t card_type;
uint32_t ram_size;
uint32_t src_width;
uint32_t src_height;
uint32_t dest_width;
uint32_t dest_height;
uint32_t x_org;          /* dest x */
uint32_t y_org;          /* dest y */
uint8_t  colkey_on;
uint8_t  colkey_red;
uint8_t  colkey_green;
uint8_t  colkey_blue;
uint32_t format;
uint32_t frame_size;
uint32_t num_frames;
} mga_vid_config_t;

#define IMGFMT_RGB_MASK 0xFFFFFF00
#define IMGFMT_RGB (('R'<<24)|('G'<<16)|('B'<<8))
#define IMGFMT_RGB8  (IMGFMT_RGB|8)
#define IMGFMT_RGB15 (IMGFMT_RGB|15)
#define IMGFMT_RGB16 (IMGFMT_RGB|16)
#define IMGFMT_RGB24 (IMGFMT_RGB|24)
#define IMGFMT_RGB32 (IMGFMT_RGB|32)

#define IMGFMT_BGR_MASK 0xFFFFFF00
#define IMGFMT_BGR (('B'<<24)|('G'<<16)|('R'<<8))
#define IMGFMT_BGR8 (IMGFMT_BGR|8)
#define IMGFMT_BGR15 (IMGFMT_BGR|15)
#define IMGFMT_BGR16 (IMGFMT_BGR|16)
#define IMGFMT_BGR24 (IMGFMT_BGR|24)
#define IMGFMT_BGR32 (IMGFMT_BGR|32)

#define IMGFMT_YV12 0x32315659
#define IMGFMT_IYUV (('I'<<24)|('Y'<<16)|('U'<<8)|'V')
#define IMGFMT_I420 (('I'<<24)|('4'<<16)|('2'<<8)|'0')
#define IMGFMT_YUY2 (('Y'<<24)|('U'<<16)|('Y'<<8)|'2')
#define IMGFMT_UYVY (('U'<<24)|('Y'<<16)|('V'<<8)|'Y')
#define IMGFMT_YVU9 0x39555659

#define MGA_VID_CONFIG    _IOR('J', 1, mga_vid_config_t)
#define MGA_VID_ON        _IO ('J', 2)
#define MGA_VID_OFF       _IO ('J', 3)
#define MGA_VID_FSEL _IOR('J', 4, int)

#define MGA_VID_VERSION 0x0201

#endif