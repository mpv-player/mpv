/*
** FAAD2 - Freeware Advanced Audio (AAC) Decoder including SBR decoding
** Copyright (C) 2003 M. Bakker, Ahead Software AG, http://www.nero.com
**  
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
** 
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
** 
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software 
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
**
** Any non-GPL usage of this software or parts of this software is strictly
** forbidden.
**
** Commercial non-GPL licensing of this software is possible.
** For more info contact Ahead Software through Mpeg4AAClicense@nero.com.
**
** $Id: rvlc.h,v 1.2 2003/07/29 08:20:13 menno Exp $
**/

#ifndef __RVLC_SCF_H__
#define __RVLC_SCF_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    int8_t index;
    uint8_t len;
    uint32_t cw;
} rvlc_huff_table;


#define ESC_VAL 7


uint8_t rvlc_scale_factor_data(ic_stream *ics, bitfile *ld);
uint8_t rvlc_decode_scale_factors(ic_stream *ics, bitfile *ld);

static uint8_t rvlc_decode_sf_forward(ic_stream *ics,
                                      bitfile *ld_sf,
                                      bitfile *ld_esc,
                                      uint8_t *is_used);
static uint8_t rvlc_decode_sf_reverse(ic_stream *ics,
                                      bitfile *ld_sf,
                                      bitfile *ld_esc,
                                      uint8_t is_used);
static int8_t rvlc_huffman_sf(bitfile *ld_sf, bitfile *ld_esc,
                              int8_t direction);
static int8_t rvlc_huffman_esc(bitfile *ld_esc, int8_t direction);


#ifdef __cplusplus
}
#endif
#endif
