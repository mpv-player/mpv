/*
** FAAD - Freeware Advanced Audio Decoder
** Copyright (C) 2002 M. Bakker
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
** Initially modified for use with MPlayer by Arpad Gereöffy on 2003/08/30
** $Id: ssr_ipqf.h,v 1.3 2004/06/02 22:59:03 diego Exp $
** detailed CVS changelog at http://www.mplayerhq.hu/cgi-bin/cvsweb.cgi/main/
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
