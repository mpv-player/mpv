/*
** FAAD2 - Freeware Advanced Audio (AAC) Decoder including SBR decoding
** Copyright (C) 2003-2004 M. Bakker, Ahead Software AG, http://www.nero.com
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
** $Id: sbr_dct.h,v 1.15 2004/09/04 14:56:28 menno Exp $
**/

#ifndef __SBR_DCT_H__
#define __SBR_DCT_H__

#ifdef __cplusplus
extern "C" {
#endif

void dct4_kernel(real_t * in_real, real_t * in_imag, real_t * out_real, real_t * out_imag);

void DCT3_32_unscaled(real_t *y, real_t *x);
void DCT4_32(real_t *y, real_t *x);
void DST4_32(real_t *y, real_t *x);
void DCT2_32_unscaled(real_t *y, real_t *x);
void DCT4_16(real_t *y, real_t *x);
void DCT2_16_unscaled(real_t *y, real_t *x);


#ifdef __cplusplus
}
#endif
#endif

