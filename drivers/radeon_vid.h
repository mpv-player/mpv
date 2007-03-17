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

#define IMGFMT_IS_RGB(fmt) (((fmt)&IMGFMT_RGB_MASK)==IMGFMT_RGB)
#define IMGFMT_IS_BGR(fmt) (((fmt)&IMGFMT_BGR_MASK)==IMGFMT_BGR)

#define IMGFMT_RGB_DEPTH(fmt) ((fmt)&~IMGFMT_RGB)
#define IMGFMT_BGR_DEPTH(fmt) ((fmt)&~IMGFMT_BGR)


/* Planar YUV Formats */

#define IMGFMT_YVU9 0x39555659
#define IMGFMT_IF09 0x39304649
#define IMGFMT_YV12 0x32315659
#if 0
#define IMGFMT_I420 0x30323449
#define IMGFMT_IYUV 0x56555949
#else
#define IMGFMT_I420 (('I'<<24)|('4'<<16)|('2'<<8)|'0')
#define IMGFMT_IYUV (('I'<<24)|('Y'<<16)|('U'<<8)|'V')
#endif
#define IMGFMT_CLPL 0x4C504C43
#define IMGFMT_Y800 0x30303859
#define IMGFMT_Y8   0x20203859

/* Packed YUV Formats */

#define IMGFMT_IUYV 0x56595549
#define IMGFMT_IY41 0x31435949
#define IMGFMT_IYU1 0x31555949
#define IMGFMT_IYU2 0x32555949
#define IMGFMT_UYNV 0x564E5955
#define IMGFMT_cyuv 0x76757963
#define IMGFMT_Y422 0x32323459
#if 0
#define IMGFMT_YUY2 0x32595559
#define IMGFMT_UYVY 0x59565955
#else
#define IMGFMT_YUY2 (('Y'<<24)|('U'<<16)|('Y'<<8)|'2')
#define IMGFMT_UYVY (('U'<<24)|('Y'<<16)|('V'<<8)|'Y')
#endif
#define IMGFMT_YUNV 0x564E5559
#define IMGFMT_YVYU 0x55595659
#define IMGFMT_Y41P 0x50313459
#define IMGFMT_Y211 0x31313259
#define IMGFMT_Y41T 0x54313459
#define IMGFMT_Y42T 0x54323459
#define IMGFMT_V422 0x32323456
#define IMGFMT_V655 0x35353656
#define IMGFMT_CLJR 0x524A4C43
#define IMGFMT_YUVP 0x50565559
#define IMGFMT_UYVP 0x50565955

/* Compressed Formats. MPlayer's extensions!!! */
#define IMGFMT_MPEGPES (('M'<<24)|('P'<<16)|('E'<<8)|('S'))


#define MGA_VID_CONFIG    _IOR('J', 1, mga_vid_config_t)
#define MGA_VID_ON        _IO ('J', 2)
#define MGA_VID_OFF       _IO ('J', 3)
#define MGA_VID_FSEL	  _IOR('J', 4, int)

#define MGA_VID_VERSION 0x0201

#endif
