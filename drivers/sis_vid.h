/*
 *
 * sis_vid.h
 *
 * Copyright (C) 2000 Aaron Holtzman
 * 
 * YUV Framebuffer driver for SiS 6326 cards
 * 
 * This software has been released under the terms of the GNU Public
 * license. See http://www.gnu.org/copyleft/gpl.html for details.
 */

#include <inttypes.h>

typedef struct mga_vid_config_s
{
uint32_t card_type;
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
} mga_vid_config_t;

#define MGA_VID_CONFIG    _IOR('J', 1, mga_vid_config_t)
#define MGA_VID_ON        _IO ('J', 2)
#define MGA_VID_OFF       _IO ('J', 3)
#define MGA_VID_FSEL _IOR('J', 4, int)

#define MGA_G200 0x1234
#define MGA_G400 0x5678
