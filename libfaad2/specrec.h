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
** $Id: specrec.h,v 1.15 2003/09/24 08:05:45 menno Exp $
**/

#ifndef __SPECREC_H__
#define __SPECREC_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "syntax.h"

uint8_t window_grouping_info(faacDecHandle hDecoder, ic_stream *ics);
void quant_to_spec(ic_stream *ics, real_t *spec_data, uint16_t frame_len);
void inverse_quantization(real_t *x_invquant, int16_t *x_quant, uint16_t frame_len);
void apply_scalefactors(faacDecHandle hDecoder, ic_stream *ics, real_t *x_invquant,
                        uint16_t frame_len);
#ifndef FIXED_POINT
void build_tables(real_t *pow2_table);
#endif

#ifdef __cplusplus
}
#endif
#endif
