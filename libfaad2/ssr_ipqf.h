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
** $Id: ssr_ipqf.h,v 1.3 2003/07/29 08:20:14 menno Exp $
**/

#ifndef __SSR_IPQF_H__
#define __SSR_IPQF_H__

#ifdef __cplusplus
extern "C" {
#endif

void ssr_ipqf(ssr_info *ssr, real_t *in_data, real_t *out_data,
              real_t buffer[SSR_BANDS][96/4],
              uint16_t frame_len, uint8_t bands);


#ifdef __cplusplus
}
#endif
#endif
