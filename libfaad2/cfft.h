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
** $Id: cfft.h,v 1.6 2003/07/29 08:20:12 menno Exp $
**/

#ifndef __CFFT_H__
#define __CFFT_H__

#ifdef __cplusplus
extern "C" {
#endif


void cfftf(cfft_info *cfft, complex_t *c);
void cfftb(cfft_info *cfft, complex_t *c);
cfft_info *cffti(uint16_t n);
void cfftu(cfft_info *cfft);


static void passf2(uint16_t ido, uint16_t l1, complex_t *cc, complex_t *ch,
                   complex_t *wa, int8_t isign);
static void passf3(uint16_t ido, uint16_t l1, complex_t *cc, complex_t *ch,
                   complex_t *wa1, complex_t *wa2, int8_t isign);
static void passf4(uint16_t ido, uint16_t l1, complex_t *cc, complex_t *ch,
                   complex_t *wa1, complex_t *wa2, complex_t *wa3, int8_t isign);
static void passf5(uint16_t ido, uint16_t l1, complex_t *cc, complex_t *ch,
                   complex_t *wa1, complex_t *wa2, complex_t *wa3, complex_t *wa4,
                   int8_t isign);
INLINE void cfftf1(uint16_t n, complex_t *c, complex_t *ch,
                   uint16_t *ifac, complex_t *wa, int8_t isign);
static void cffti1(uint16_t n, complex_t *wa, uint16_t *ifac);


#ifdef __cplusplus
}
#endif
#endif
