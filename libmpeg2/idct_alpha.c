/*
 * idct_alpha.c
 * Copyright (C) 2002-2003 Falk Hueffner <falk@debian.org>
 * Copyright (C) 2000-2003 Michel Lespinasse <walken@zoy.org>
 * Copyright (C) 1999-2000 Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
 *
 * This file is part of mpeg2dec, a free MPEG-2 video stream decoder.
 * See http://libmpeg2.sourceforge.net/ for updates.
 *
 * mpeg2dec is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpeg2dec is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "config.h"

#if ARCH_ALPHA

#include <stdlib.h>
#include <inttypes.h>

#include "mpeg2.h"
#include "attributes.h"
#include "mpeg2_internal.h"
#include "alpha_asm.h"

#define W1 2841 /* 2048 * sqrt (2) * cos (1 * pi / 16) */
#define W2 2676 /* 2048 * sqrt (2) * cos (2 * pi / 16) */
#define W3 2408 /* 2048 * sqrt (2) * cos (3 * pi / 16) */
#define W5 1609 /* 2048 * sqrt (2) * cos (5 * pi / 16) */
#define W6 1108 /* 2048 * sqrt (2) * cos (6 * pi / 16) */
#define W7 565  /* 2048 * sqrt (2) * cos (7 * pi / 16) */

extern uint8_t mpeg2_clip[3840 * 2 + 256];
#define CLIP(i) ((mpeg2_clip + 3840)[i])

#if 0
#define BUTTERFLY(t0,t1,W0,W1,d0,d1)	\
do {					\
    t0 = W0 * d0 + W1 * d1;			\
    t1 = W0 * d1 - W1 * d0;			\
} while (0)
#else
#define BUTTERFLY(t0,t1,W0,W1,d0,d1)	\
do {					\
    int_fast32_t tmp = W0 * (d0 + d1);	\
    t0 = tmp + (W1 - W0) * d1;		\
    t1 = tmp - (W1 + W0) * d0;		\
} while (0)
#endif

static inline void idct_row (int16_t * const block)
{
    uint64_t l, r;
    int_fast32_t d0, d1, d2, d3;
    int_fast32_t a0, a1, a2, a3, b0, b1, b2, b3;
    int_fast32_t t0, t1, t2, t3;

    l = ldq (block);
    r = ldq (block + 4);

    /* shortcut */
    if (likely (!((l & ~0xffffUL) | r))) {
	uint64_t tmp = (uint16_t) (l >> 1);
	tmp |= tmp << 16;
	tmp |= tmp << 32;
	((int32_t *)block)[0] = tmp;
	((int32_t *)block)[1] = tmp;
	((int32_t *)block)[2] = tmp;
	((int32_t *)block)[3] = tmp;
	return;
    }

    d0 = (sextw (l) << 11) + 2048;
    d1 = sextw (extwl (l, 2));
    d2 = sextw (extwl (l, 4)) << 11;
    d3 = sextw (extwl (l, 6));
    t0 = d0 + d2;
    t1 = d0 - d2;
    BUTTERFLY (t2, t3, W6, W2, d3, d1);
    a0 = t0 + t2;
    a1 = t1 + t3;
    a2 = t1 - t3;
    a3 = t0 - t2;

    d0 = sextw (r);
    d1 = sextw (extwl (r, 2));
    d2 = sextw (extwl (r, 4));
    d3 = sextw (extwl (r, 6));
    BUTTERFLY (t0, t1, W7, W1, d3, d0);
    BUTTERFLY (t2, t3, W3, W5, d1, d2);
    b0 = t0 + t2;
    b3 = t1 + t3;
    t0 -= t2;
    t1 -= t3;
    b1 = ((t0 + t1) >> 8) * 181;
    b2 = ((t0 - t1) >> 8) * 181;

    block[0] = (a0 + b0) >> 12;
    block[1] = (a1 + b1) >> 12;
    block[2] = (a2 + b2) >> 12;
    block[3] = (a3 + b3) >> 12;
    block[4] = (a3 - b3) >> 12;
    block[5] = (a2 - b2) >> 12;
    block[6] = (a1 - b1) >> 12;
    block[7] = (a0 - b0) >> 12;
}

static inline void idct_col (int16_t * const block)
{
    int_fast32_t d0, d1, d2, d3;
    int_fast32_t a0, a1, a2, a3, b0, b1, b2, b3;
    int_fast32_t t0, t1, t2, t3;

    d0 = (block[8*0] << 11) + 65536;
    d1 = block[8*1];
    d2 = block[8*2] << 11;
    d3 = block[8*3];
    t0 = d0 + d2;
    t1 = d0 - d2;
    BUTTERFLY (t2, t3, W6, W2, d3, d1);
    a0 = t0 + t2;
    a1 = t1 + t3;
    a2 = t1 - t3;
    a3 = t0 - t2;

    d0 = block[8*4];
    d1 = block[8*5];
    d2 = block[8*6];
    d3 = block[8*7];
    BUTTERFLY (t0, t1, W7, W1, d3, d0);
    BUTTERFLY (t2, t3, W3, W5, d1, d2);
    b0 = t0 + t2;
    b3 = t1 + t3;
    t0 -= t2;
    t1 -= t3;
    b1 = ((t0 + t1) >> 8) * 181;
    b2 = ((t0 - t1) >> 8) * 181;

    block[8*0] = (a0 + b0) >> 17;
    block[8*1] = (a1 + b1) >> 17;
    block[8*2] = (a2 + b2) >> 17;
    block[8*3] = (a3 + b3) >> 17;
    block[8*4] = (a3 - b3) >> 17;
    block[8*5] = (a2 - b2) >> 17;
    block[8*6] = (a1 - b1) >> 17;
    block[8*7] = (a0 - b0) >> 17;
}

void mpeg2_idct_copy_mvi (int16_t * block, uint8_t * dest, const int stride)
{
    uint64_t clampmask;
    int i;

    for (i = 0; i < 8; i++)
	idct_row (block + 8 * i);

    for (i = 0; i < 8; i++)
	idct_col (block + i);

    clampmask = zap (-1, 0xaa);	/* 0x00ff00ff00ff00ff */
    do {
	uint64_t shorts0, shorts1;

	shorts0 = ldq (block);
	shorts0 = maxsw4 (shorts0, 0);
	shorts0 = minsw4 (shorts0, clampmask);
	stl (pkwb (shorts0), dest);

	shorts1 = ldq (block + 4);
	shorts1 = maxsw4 (shorts1, 0);
	shorts1 = minsw4 (shorts1, clampmask);
	stl (pkwb (shorts1), dest + 4);

	stq (0, block);
	stq (0, block + 4);

	dest += stride;
	block += 8;
    } while (--i);
}

void mpeg2_idct_add_mvi (const int last, int16_t * block,
			 uint8_t * dest, const int stride)
{
    uint64_t clampmask;
    uint64_t signmask;
    int i;

    if (last != 129 || (block[0] & (7 << 4)) == (4 << 4)) {
	for (i = 0; i < 8; i++)
	    idct_row (block + 8 * i);
	for (i = 0; i < 8; i++)
	    idct_col (block + i);
	clampmask = zap (-1, 0xaa);	/* 0x00ff00ff00ff00ff */
	signmask = zap (-1, 0x33);
	signmask ^= signmask >> 1;	/* 0x8000800080008000 */

	do {
	    uint64_t shorts0, pix0, signs0;
	    uint64_t shorts1, pix1, signs1;

	    shorts0 = ldq (block);
	    shorts1 = ldq (block + 4);

	    pix0 = unpkbw (ldl (dest));
	    /* signed subword add (MMX paddw).  */
	    signs0 = shorts0 & signmask;
	    shorts0 &= ~signmask;
	    shorts0 += pix0;
	    shorts0 ^= signs0;
	    /* clamp. */
	    shorts0 = maxsw4 (shorts0, 0);
	    shorts0 = minsw4 (shorts0, clampmask);	

	    /* next 4.  */
	    pix1 = unpkbw (ldl (dest + 4));
	    signs1 = shorts1 & signmask;
	    shorts1 &= ~signmask;
	    shorts1 += pix1;
	    shorts1 ^= signs1;
	    shorts1 = maxsw4 (shorts1, 0);
	    shorts1 = minsw4 (shorts1, clampmask);

	    stl (pkwb (shorts0), dest);
	    stl (pkwb (shorts1), dest + 4);
	    stq (0, block);
	    stq (0, block + 4);

	    dest += stride;
	    block += 8;
	} while (--i);
    } else {
	int DC;
	uint64_t p0, p1, p2, p3, p4, p5, p6, p7;
	uint64_t DCs;

	DC = (block[0] + 64) >> 7;
	block[0] = block[63] = 0;

	p0 = ldq (dest + 0 * stride);
	p1 = ldq (dest + 1 * stride);
	p2 = ldq (dest + 2 * stride);
	p3 = ldq (dest + 3 * stride);
	p4 = ldq (dest + 4 * stride);
	p5 = ldq (dest + 5 * stride);
	p6 = ldq (dest + 6 * stride);
	p7 = ldq (dest + 7 * stride);

	if (DC > 0) {
	    DCs = BYTE_VEC (likely (DC <= 255) ? DC : 255);
	    p0 += minub8 (DCs, ~p0);
	    p1 += minub8 (DCs, ~p1);
	    p2 += minub8 (DCs, ~p2);
	    p3 += minub8 (DCs, ~p3);
	    p4 += minub8 (DCs, ~p4);
	    p5 += minub8 (DCs, ~p5);
	    p6 += minub8 (DCs, ~p6);
	    p7 += minub8 (DCs, ~p7);
	} else {
	    DCs = BYTE_VEC (likely (-DC <= 255) ? -DC : 255);
	    p0 -= minub8 (DCs, p0);
	    p1 -= minub8 (DCs, p1);
	    p2 -= minub8 (DCs, p2);
	    p3 -= minub8 (DCs, p3);
	    p4 -= minub8 (DCs, p4);
	    p5 -= minub8 (DCs, p5);
	    p6 -= minub8 (DCs, p6);
	    p7 -= minub8 (DCs, p7);
	}

	stq (p0, dest + 0 * stride);
	stq (p1, dest + 1 * stride);
	stq (p2, dest + 2 * stride);
	stq (p3, dest + 3 * stride);
	stq (p4, dest + 4 * stride);
	stq (p5, dest + 5 * stride);
	stq (p6, dest + 6 * stride);
	stq (p7, dest + 7 * stride);
    }
}

void mpeg2_idct_copy_alpha (int16_t * block, uint8_t * dest, const int stride)
{
    int i;

    for (i = 0; i < 8; i++)
	idct_row (block + 8 * i);
    for (i = 0; i < 8; i++)
	idct_col (block + i);
    do {
	dest[0] = CLIP (block[0]);
	dest[1] = CLIP (block[1]);
	dest[2] = CLIP (block[2]);
	dest[3] = CLIP (block[3]);
	dest[4] = CLIP (block[4]);
	dest[5] = CLIP (block[5]);
	dest[6] = CLIP (block[6]);
	dest[7] = CLIP (block[7]);

	stq(0, block);
	stq(0, block + 4);

	dest += stride;
	block += 8;
    } while (--i);
}

void mpeg2_idct_add_alpha (const int last, int16_t * block,
			   uint8_t * dest, const int stride)
{
    int i;

    if (last != 129 || (block[0] & (7 << 4)) == (4 << 4)) {
	for (i = 0; i < 8; i++)
	    idct_row (block + 8 * i);
	for (i = 0; i < 8; i++)
	    idct_col (block + i);
	do {
	    dest[0] = CLIP (block[0] + dest[0]);
	    dest[1] = CLIP (block[1] + dest[1]);
	    dest[2] = CLIP (block[2] + dest[2]);
	    dest[3] = CLIP (block[3] + dest[3]);
	    dest[4] = CLIP (block[4] + dest[4]);
	    dest[5] = CLIP (block[5] + dest[5]);
	    dest[6] = CLIP (block[6] + dest[6]);
	    dest[7] = CLIP (block[7] + dest[7]);

	    stq(0, block);
	    stq(0, block + 4);

	    dest += stride;
	    block += 8;
	} while (--i);
    } else {
	int DC;

	DC = (block[0] + 64) >> 7;
	block[0] = block[63] = 0;
	i = 8;
	do {
	    dest[0] = CLIP (DC + dest[0]);
	    dest[1] = CLIP (DC + dest[1]);
	    dest[2] = CLIP (DC + dest[2]);
	    dest[3] = CLIP (DC + dest[3]);
	    dest[4] = CLIP (DC + dest[4]);
	    dest[5] = CLIP (DC + dest[5]);
	    dest[6] = CLIP (DC + dest[6]);
	    dest[7] = CLIP (DC + dest[7]);
	    dest += stride;
	} while (--i);
    }
}

void mpeg2_idct_alpha_init (void)
{
    int i, j;

    for (i = 0; i < 64; i++) {
	j = mpeg2_scan_norm[i];
	mpeg2_scan_norm[i] = ((j & 0x36) >> 1) | ((j & 0x09) << 2);
	j = mpeg2_scan_alt[i];
	mpeg2_scan_alt[i] = ((j & 0x36) >> 1) | ((j & 0x09) << 2);
    }
}

#endif /* ARCH_ALPHA */
