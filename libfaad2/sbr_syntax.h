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
** $Id: sbr_syntax.h,v 1.7 2003/09/09 18:09:52 menno Exp $
**/

#ifndef __SBR_SYNTAX_H__
#define __SBR_SYNTAX_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "bits.h"

#ifdef DRM
# define T_HFGEN_DRM 32
# define T_HFADJ_DRM 0
#endif
#define T_HFGEN 8
#define T_HFADJ 2

#define EXT_SBR_DATA     13
#define EXT_SBR_DATA_CRC 14

#define FIXFIX 0
#define FIXVAR 1
#define VARFIX 2
#define VARVAR 3

#define LO_RES 0
#define HI_RES 1

#define NO_TIME_SLOTS_960 15
#define NO_TIME_SLOTS 16
#define RATE              2

#define NOISE_FLOOR_OFFSET 6.0


uint8_t sbr_extension_data(bitfile *ld, sbr_info *sbr, uint8_t id_aac);
static void sbr_header(bitfile *ld, sbr_info *sbr, uint8_t id_aac);
static uint8_t sbr_data(bitfile *ld, sbr_info *sbr, uint8_t id_aac);
static uint8_t sbr_single_channel_element(bitfile *ld, sbr_info *sbr);
static uint8_t sbr_channel_pair_element(bitfile *ld, sbr_info *sbr);
static uint8_t sbr_grid(bitfile *ld, sbr_info *sbr, uint8_t ch);
static void sbr_dtdf(bitfile *ld, sbr_info *sbr, uint8_t ch);
static void invf_mode(bitfile *ld, sbr_info *sbr, uint8_t ch);
static void sinusoidal_coding(bitfile *ld, sbr_info *sbr, uint8_t ch);

#ifdef __cplusplus
}
#endif
#endif /* __SBR_SYNTAX_H__ */

