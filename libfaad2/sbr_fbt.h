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
** Initially modified for use with MPlayer by Arpad Gereöffy on 2003/08/30
** $Id: sbr_fbt.h,v 1.3 2004/06/02 22:59:03 diego Exp $
** detailed CVS changelog at http://www.mplayerhq.hu/cgi-bin/cvsweb.cgi/main/
**/

#ifndef __SBR_FBT_H__
#define __SBR_FBT_H__

#ifdef __cplusplus
extern "C" {
#endif

uint8_t qmf_start_channel(uint8_t bs_start_freq, uint8_t bs_samplerate_mode,
                           uint32_t sample_rate);
uint8_t qmf_stop_channel(uint8_t bs_stop_freq, uint32_t sample_rate,
                          uint8_t k0);
uint8_t master_frequency_table_fs0(sbr_info *sbr, uint8_t k0, uint8_t k2,
                                   uint8_t bs_alter_scale);
uint8_t master_frequency_table(sbr_info *sbr, uint8_t k0, uint8_t k2,
                               uint8_t bs_freq_scale, uint8_t bs_alter_scale);
uint8_t derived_frequency_table(sbr_info *sbr, uint8_t bs_xover_band,
                                uint8_t k2);
void limiter_frequency_table(sbr_info *sbr);


#ifdef __cplusplus
}
#endif
#endif

