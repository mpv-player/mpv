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
** $Id: sbr_huff.h,v 1.5 2003/07/29 08:20:13 menno Exp $
**/

#ifndef __SBR_HUFF_H__
#define __SBR_HUFF_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef const int8_t (*sbr_huff_tab)[2];

int16_t sbr_huff_dec(bitfile *ld, sbr_huff_tab t_huff);

const int8_t t_huffman_env_1_5dB[120][2];
const int8_t f_huffman_env_1_5dB[120][2];
const int8_t t_huffman_env_bal_1_5dB[48][2];
const int8_t f_huffman_env_bal_1_5dB[48][2];
const int8_t t_huffman_env_3_0dB[62][2];
const int8_t f_huffman_env_3_0dB[62][2];
const int8_t t_huffman_env_bal_3_0dB[24][2];
const int8_t f_huffman_env_bal_3_0dB[24][2];
const int8_t t_huffman_noise_3_0dB[62][2];
const int8_t t_huffman_noise_bal_3_0dB[24][2];

#ifdef __cplusplus
}
#endif
#endif

