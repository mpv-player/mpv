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
** $Id: huffman.h,v 1.1 2003/08/30 22:30:21 arpi Exp $
**/

#ifndef __HUFFMAN_H__
#define __HUFFMAN_H__

#ifdef __cplusplus
extern "C" {
#endif


static INLINE void huffman_sign_bits(bitfile *ld, int16_t *sp, uint8_t len);
static INLINE int16_t huffman_getescape(bitfile *ld, int16_t sp);
static uint8_t huffman_2step_quad(uint8_t cb, bitfile *ld, int16_t *sp);
static uint8_t huffman_2step_quad_sign(uint8_t cb, bitfile *ld, int16_t *sp);
static uint8_t huffman_2step_pair(uint8_t cb, bitfile *ld, int16_t *sp);
static huffman_2step_pair_sign(uint8_t cb, bitfile *ld, int16_t *sp);
static uint8_t huffman_binary_quad(uint8_t cb, bitfile *ld, int16_t *sp);
static uint8_t huffman_binary_quad_sign(uint8_t cb, bitfile *ld, int16_t *sp);
static uint8_t huffman_binary_pair(uint8_t cb, bitfile *ld, int16_t *sp);
static uint8_t huffman_binary_pair_sign(uint8_t cb, bitfile *ld, int16_t *sp);
static int16_t huffman_codebook(uint8_t i);

int8_t huffman_scale_factor(bitfile *ld);
uint8_t huffman_spectral_data(uint8_t cb, bitfile *ld, int16_t *sp);
#ifdef ERROR_RESILIENCE
int8_t huffman_spectral_data_2(uint8_t cb, bits_t *ld, int16_t *sp);
#endif

#ifdef __cplusplus
}
#endif
#endif
