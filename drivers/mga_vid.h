/*
 *
 * mga_vid.h
 *
 * Copyright (C) 1999 Aaron Holtzman
 * 
 * Matrox MGA G200/G400 YUV Video Interface module Version 0.1.0
 * 
 * BES == Back End Scaler
 * 
 * This software has been released under the terms of the GNU Public
 * license. See http://www.gnu.org/copyleft/gpl.html for details.
 */

#ifndef __LINUX_MGAVID_H
#define __LINUX_MGAVID_H

//#include <inttypes.h>

typedef struct mga_vid_config_s
{
uint16_t version;
uint16_t card_type;
uint32_t ram_size;
uint32_t src_width;
uint32_t src_height;
uint32_t dest_width;
uint32_t dest_height;
uint32_t x_org;
uint32_t y_org;
uint8_t  colkey_on;
uint8_t  colkey_red;
uint8_t  colkey_green;
uint8_t  colkey_blue;
uint32_t format;
uint32_t frame_size;
uint32_t num_frames;
} mga_vid_config_t;

#define MGA_VID_FORMAT_YV12 0x32315659
#define MGA_VID_FORMAT_IYUV (('I'<<24)|('Y'<<16)|('U'<<8)|'V')
#define MGA_VID_FORMAT_I420 (('I'<<24)|('4'<<16)|('2'<<8)|'0')
#define MGA_VID_FORMAT_YUY2 (('Y'<<24)|('U'<<16)|('Y'<<8)|'2')
#define MGA_VID_FORMAT_UYVY (('U'<<24)|('Y'<<16)|('V'<<8)|'Y')

#define MGA_VID_CONFIG    _IOR('J', 1, mga_vid_config_t)
#define MGA_VID_ON        _IO ('J', 2)
#define MGA_VID_OFF       _IO ('J', 3)
#define MGA_VID_FSEL _IOR('J', 4, int)
#define MGA_VID_GET_LUMA _IOR('J', 5, int)
#define MGA_VID_SET_LUMA _IOR('J', 6, int)

#define MGA_G200 0x1234
#define MGA_G400 0x5678

#define MGA_VID_VERSION 0x0201

#endif
