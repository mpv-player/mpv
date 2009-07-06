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
** $Id: sbr_hfadj.h,v 1.15 2004/09/04 14:56:28 menno Exp $
**/

#ifndef __SBR_HFADJ_H__
#define __SBR_HFADJ_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    real_t G_lim_boost[MAX_L_E][MAX_M];
    real_t Q_M_lim_boost[MAX_L_E][MAX_M];
    real_t S_M_boost[MAX_L_E][MAX_M];
} sbr_hfadj_info;


void hf_adjustment(sbr_info *sbr, qmf_t Xsbr[MAX_NTSRHFG][64]
#ifdef SBR_LOW_POWER
                   ,real_t *deg
#endif
                   ,uint8_t ch);


#ifdef __cplusplus
}
#endif
#endif

