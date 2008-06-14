/*
 * Matrox MGA G200/G400 YUV Video Interface module Version 0.1.0
 * BES == Back End Scaler
 *
 * Copyright (C) 1999 Aaron Holtzman
 *
 * This file is part of mga_vid.
 *
 * mga_vid is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mga_vid is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mga_vid; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef MGA_VID_H
#define MGA_VID_H

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
uint32_t capabilities;
} mga_vid_config_t;

/* supported FOURCCs */
#define MGA_VID_FORMAT_YV12 0x32315659
#define MGA_VID_FORMAT_IYUV (('I'<<24)|('Y'<<16)|('U'<<8)|'V')
#define MGA_VID_FORMAT_I420 (('I'<<24)|('4'<<16)|('2'<<8)|'0')
#define MGA_VID_FORMAT_YUY2 (('Y'<<24)|('U'<<16)|('Y'<<8)|'2')
#define MGA_VID_FORMAT_UYVY (('U'<<24)|('Y'<<16)|('V'<<8)|'Y')

/* ioctl commands */
#define MGA_VID_GET_VERSION  _IOR ('J', 1, uint32_t)
#define MGA_VID_CONFIG       _IOWR('J', 2, mga_vid_config_t)
#define MGA_VID_ON           _IO  ('J', 3)
#define MGA_VID_OFF          _IO  ('J', 4)
#define MGA_VID_FSEL         _IOW ('J', 5, uint32_t)
#define MGA_VID_GET_LUMA     _IOR ('J', 6, uint32_t)
#define MGA_VID_SET_LUMA     _IOW ('J', 7, uint32_t)

/* card identifiers */
#define MGA_G200 0x1234
#define MGA_G400 0x5678
// currently unused, G450 are mapped to MGA_G400
// #define MGA_G450 0x9ABC
#define MGA_G550 0xDEF0

/* version of the mga_vid_config struct */
#define MGA_VID_VERSION 0x0202

#endif /* MGA_VID_H */
