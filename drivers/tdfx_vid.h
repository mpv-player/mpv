/*
 * Copyright (C) 2003 Alban Bedel
 *
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef MPLAYER_TDFX_VID_H
#define MPLAYER_TDFX_VID_H

#define TDFX_VID_VERSION 1

#define TDFX_VID_MOVE_2_PACKED  0
#define TDFX_VID_MOVE_2_YUV     1
#define TDFX_VID_MOVE_2_3D      2
#define TDFX_VID_MOVE_2_TEXTURE 3

#define TDFX_VID_SRC_COLORKEY 0x1
#define TDFX_VID_DST_COLORKEY 0x2

#define TDFX_VID_ROP_COPY        0xcc     // src
#define TDFX_VID_ROP_INVERT      0x55     // NOT dst
#define TDFX_VID_ROP_XOR         0x66     // src XOR dst
#define TDFX_VID_ROP_OR		 0xee     // src OR dst

#define TDFX_VID_FORMAT_BGR1  (('B'<<24)|('G'<<16)|('R'<<8)|1)
#define TDFX_VID_FORMAT_BGR8  (('B'<<24)|('G'<<16)|('R'<<8)|8)
#define TDFX_VID_FORMAT_BGR15 (('B'<<24)|('G'<<16)|('R'<<8)|15)
#define TDFX_VID_FORMAT_BGR16 (('B'<<24)|('G'<<16)|('R'<<8)|16)
#define TDFX_VID_FORMAT_BGR24 (('B'<<24)|('G'<<16)|('R'<<8)|24)
#define TDFX_VID_FORMAT_BGR32 (('B'<<24)|('G'<<16)|('R'<<8)|32)

#define TDFX_VID_FORMAT_YUY2 (('2'<<24)|('Y'<<16)|('U'<<8)|'Y')
#define TDFX_VID_FORMAT_UYVY (('Y'<<24)|('V'<<16)|('Y'<<8)|'U')

#define TDFX_VID_FORMAT_YV12 0x32315659
#define TDFX_VID_FORMAT_IYUV (('I'<<24)|('Y'<<16)|('U'<<8)|'V')
#define TDFX_VID_FORMAT_I420 (('I'<<24)|('4'<<16)|('2'<<8)|'0')

#define TDFX_VID_YUV_STRIDE        (1024)
#define TDFX_VID_YUV_PLANE_SIZE    (0x0100000)


typedef struct tdfx_vid_blit_s {
  uint32_t src;
  uint32_t src_stride;
  uint16_t src_x,src_y;
  uint16_t src_w,src_h;
  uint32_t src_format;

  uint32_t  dst;
  uint32_t dst_stride;
  uint16_t dst_x,dst_y;
  uint16_t dst_w,dst_h;
  uint32_t dst_format;

  uint32_t src_colorkey[2];
  uint32_t dst_colorkey[2];

  uint8_t colorkey;
  uint8_t rop[4];
} tdfx_vid_blit_t;

typedef struct tdfx_vid_config_s {
  uint16_t version;
  uint16_t card_type;
  uint32_t ram_size;
  uint16_t screen_width;
  uint16_t screen_height;
  uint16_t screen_stride;
  uint32_t screen_format;
  uint32_t screen_start;
} tdfx_vid_config_t;

typedef struct tdfx_vid_agp_move_s {
  uint16_t move2;
  uint16_t width,height;

  uint32_t src;
  uint32_t src_stride;

  uint32_t dst;
  uint32_t dst_stride;
} tdfx_vid_agp_move_t;

typedef struct tdfx_vid_yuv_s {
  uint32_t base;
  uint16_t stride;
} tdfx_vid_yuv_t;

typedef struct tdfx_vid_overlay_s {
  uint32_t src[2]; // left and right buffer (2 buffer may be NULL)
  uint16_t src_width,src_height;
  uint16_t src_stride;
  uint32_t format;

  uint16_t dst_width,dst_height;
  int16_t dst_x,dst_y;

  uint8_t use_colorkey;
  uint32_t colorkey[2]; // min/max
  uint8_t invert_colorkey;
} tdfx_vid_overlay_t;

#define TDFX_VID_GET_CONFIG _IOR('J', 1, tdfx_vid_config_t)
#define TDFX_VID_AGP_MOVE _IOW('J', 2, tdfx_vid_agp_move_t)
#define TDFX_VID_BLIT _IOW('J', 3, tdfx_vid_blit_t)
#define TDFX_VID_SET_YUV _IOW('J', 4, tdfx_vid_blit_t)
#define TDFX_VID_GET_YUV _IOR('J', 5, tdfx_vid_blit_t)
#define TDFX_VID_BUMP0 _IOW('J', 6, u16)
#define TDFX_VID_SET_OVERLAY _IOW('J', 7, tdfx_vid_overlay_t)
#define TDFX_VID_OVERLAY_ON _IO ('J', 8)
#define TDFX_VID_OVERLAY_OFF _IO ('J', 9)

#endif /* MPLAYER_TDFX_VID_H */
