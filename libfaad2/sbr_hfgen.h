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
** $Id: sbr_hfgen.h,v 1.1 2003/07/29 08:20:13 menno Exp $
**/

#ifndef __SBR_HFGEN_H__
#define __SBR_HFGEN_H__

#ifdef __cplusplus
extern "C" {
#endif

void hf_generation(sbr_info *sbr, qmf_t *Xlow,
                   qmf_t *Xhigh
#ifdef SBR_LOW_POWER
                   ,real_t *deg
#endif
                   ,uint8_t ch);

static void calc_prediction_coef(sbr_info *sbr, qmf_t *Xlow,
                                 complex_t *alpha_0, complex_t *alpha_1
#ifdef SBR_LOW_POWER
                                 , real_t *rxx
#endif
                                 );
static void calc_aliasing_degree(sbr_info *sbr, real_t *rxx, real_t *deg);
static void calc_chirp_factors(sbr_info *sbr, uint8_t ch);
static void patch_construction(sbr_info *sbr);

#ifdef __cplusplus
}
#endif
#endif

