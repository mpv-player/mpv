/*
 * motion_comp_alpha.c
 * Copyright (C) 2002 Falk Hueffner <falk@debian.org>
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

#ifdef ARCH_ALPHA

#include <inttypes.h>

#include "mpeg2.h"
#include "mpeg2_internal.h"
#include "alpha_asm.h"

static inline uint64_t avg2(uint64_t a, uint64_t b)
{
    return (a | b) - (((a ^ b) & BYTE_VEC(0xfe)) >> 1);    
}

// Load two unaligned quadwords from addr. This macro only works if
// addr is actually unaligned.
#define ULOAD16(ret_l, ret_r, addr)			\
    do {						\
	uint64_t _l = ldq_u(addr +  0);			\
	uint64_t _m = ldq_u(addr +  8);			\
	uint64_t _r = ldq_u(addr + 16);			\
	ret_l = extql(_l, addr) | extqh(_m, addr);	\
	ret_r = extql(_m, addr) | extqh(_r, addr);	\
    } while (0)

// Load two aligned quadwords from addr.
#define ALOAD16(ret_l, ret_r, addr)			\
    do {						\
	ret_l = ldq(addr);				\
	ret_r = ldq(addr + 8);				\
    } while (0)

#define OP8(LOAD, LOAD16, STORE)		\
    do {					\
	STORE(LOAD(pixels), block);		\
	pixels += line_size;			\
	block += line_size;			\
    } while (--h)

#define OP16(LOAD, LOAD16, STORE)		\
    do {					\
	uint64_t l, r;				\
	LOAD16(l, r, pixels);			\
	STORE(l, block);			\
	STORE(r, block + 8);			\
	pixels += line_size;			\
	block += line_size;			\
    } while (--h)

#define OP8_X2(LOAD, LOAD16, STORE)			\
    do {						\
	uint64_t p0, p1;				\
							\
	p0 = LOAD(pixels);				\
	p1 = p0 >> 8 | ((uint64_t) pixels[8] << 56);	\
	STORE(avg2(p0, p1), block);			\
	pixels += line_size;				\
	block += line_size;				\
    } while (--h)

#define OP16_X2(LOAD, LOAD16, STORE)				\
    do {							\
	uint64_t p0, p1;					\
								\
	LOAD16(p0, p1, pixels);					\
	STORE(avg2(p0, p0 >> 8 | p1 << 56), block);		\
	STORE(avg2(p1, p1 >> 8 | (uint64_t) pixels[16] << 56),	\
	      block + 8);					\
	pixels += line_size;					\
	block += line_size;					\
    } while (--h)

#define OP8_Y2(LOAD, LOAD16, STORE)		\
    do {					\
	uint64_t p0, p1;			\
	p0 = LOAD(pixels);			\
	pixels += line_size;			\
	p1 = LOAD(pixels);			\
	do {					\
	    uint64_t av = avg2(p0, p1);		\
	    if (--h == 0) line_size = 0;	\
	    pixels += line_size;		\
	    p0 = p1;				\
	    p1 = LOAD(pixels);			\
	    STORE(av, block);			\
	    block += line_size;			\
	} while (h);				\
    } while (0)

#define OP16_Y2(LOAD, LOAD16, STORE)		\
    do {					\
	uint64_t p0l, p0r, p1l, p1r;		\
	LOAD16(p0l, p0r, pixels);		\
	pixels += line_size;			\
	LOAD16(p1l, p1r, pixels);		\
	do {					\
	    uint64_t avl, avr;			\
	    if (--h == 0) line_size = 0;	\
	    avl = avg2(p0l, p1l);		\
	    avr = avg2(p0r, p1r);		\
	    p0l = p1l;				\
	    p0r = p1r;				\
	    pixels += line_size;		\
	    LOAD16(p1l, p1r, pixels);		\
	    STORE(avl, block);			\
	    STORE(avr, block + 8);		\
	    block += line_size;			\
	} while (h);				\
    } while (0)

#define OP8_XY2(LOAD, LOAD16, STORE)				\
    do {							\
	uint64_t pl, ph;					\
	uint64_t p1 = LOAD(pixels);				\
	uint64_t p2 = p1 >> 8 | ((uint64_t) pixels[8] << 56);	\
								\
	ph = ((p1 & ~BYTE_VEC(0x03)) >> 2)			\
	   + ((p2 & ~BYTE_VEC(0x03)) >> 2);			\
	pl = (p1 & BYTE_VEC(0x03))				\
	   + (p2 & BYTE_VEC(0x03));				\
								\
	do {							\
	    uint64_t npl, nph;					\
								\
	    pixels += line_size;				\
	    p1 = LOAD(pixels);					\
	    p2 = (p1 >> 8) | ((uint64_t) pixels[8] << 56);	\
	    nph = ((p1 & ~BYTE_VEC(0x03)) >> 2)			\
	        + ((p2 & ~BYTE_VEC(0x03)) >> 2);		\
	    npl = (p1 & BYTE_VEC(0x03))				\
	        + (p2 & BYTE_VEC(0x03));			\
								\
	    STORE(ph + nph					\
		  + (((pl + npl + BYTE_VEC(0x02)) >> 2)		\
		     & BYTE_VEC(0x03)), block);			\
								\
	    block += line_size;					\
            pl = npl;						\
	    ph = nph;						\
	} while (--h);						\
    } while (0)

#define OP16_XY2(LOAD, LOAD16, STORE)				\
    do {							\
	uint64_t p0, p1, p2, p3, pl_l, ph_l, pl_r, ph_r;	\
	LOAD16(p0, p2, pixels);					\
	p1 = p0 >> 8 | (p2 << 56);				\
	p3 = p2 >> 8 | ((uint64_t) pixels[16] << 56);		\
								\
	ph_l = ((p0 & ~BYTE_VEC(0x03)) >> 2)			\
	     + ((p1 & ~BYTE_VEC(0x03)) >> 2);			\
	pl_l = (p0 & BYTE_VEC(0x03))				\
	     + (p1 & BYTE_VEC(0x03));				\
	ph_r = ((p2 & ~BYTE_VEC(0x03)) >> 2)			\
	     + ((p3 & ~BYTE_VEC(0x03)) >> 2);			\
	pl_r = (p2 & BYTE_VEC(0x03))				\
	     + (p3 & BYTE_VEC(0x03));				\
								\
	do {							\
	    uint64_t npl_l, nph_l, npl_r, nph_r;		\
								\
	    pixels += line_size;				\
	    LOAD16(p0, p2, pixels);				\
	    p1 = p0 >> 8 | (p2 << 56);				\
	    p3 = p2 >> 8 | ((uint64_t) pixels[16] << 56);	\
	    nph_l = ((p0 & ~BYTE_VEC(0x03)) >> 2)		\
		  + ((p1 & ~BYTE_VEC(0x03)) >> 2);		\
	    npl_l = (p0 & BYTE_VEC(0x03))			\
		  + (p1 & BYTE_VEC(0x03));			\
	    nph_r = ((p2 & ~BYTE_VEC(0x03)) >> 2)		\
		  + ((p3 & ~BYTE_VEC(0x03)) >> 2);		\
	    npl_r = (p2 & BYTE_VEC(0x03))			\
		  + (p3 & BYTE_VEC(0x03));			\
								\
	    STORE(ph_l + nph_l					\
		  + (((pl_l + npl_l + BYTE_VEC(0x02)) >> 2)	\
		     & BYTE_VEC(0x03)), block);			\
	    STORE(ph_r + nph_r					\
		  + (((pl_r + npl_r + BYTE_VEC(0x02)) >> 2)	\
		     & BYTE_VEC(0x03)), block + 8);		\
								\
	    block += line_size;					\
	    pl_l = npl_l;					\
	    ph_l = nph_l;					\
	    pl_r = npl_r;					\
	    ph_r = nph_r;					\
	} while (--h);						\
    } while (0)

#define MAKE_OP(OPNAME, SIZE, SUFF, OPKIND, STORE)			\
static void MC_ ## OPNAME ## _ ## SUFF ## _ ## SIZE ## _alpha		\
	(uint8_t *restrict block, const uint8_t *restrict pixels,	\
	 int line_size, int h)						\
{									\
    if ((uint64_t) pixels & 0x7) {					\
	OPKIND(uldq, ULOAD16, STORE);					\
    } else {								\
	OPKIND(ldq, ALOAD16, STORE);					\
    }									\
}

#define PIXOP(OPNAME, STORE)			\
    MAKE_OP(OPNAME, 8,  o,  OP8,      STORE);	\
    MAKE_OP(OPNAME, 8,  x,  OP8_X2,   STORE);	\
    MAKE_OP(OPNAME, 8,  y,  OP8_Y2,   STORE);	\
    MAKE_OP(OPNAME, 8,  xy, OP8_XY2,  STORE);	\
    MAKE_OP(OPNAME, 16, o,  OP16,     STORE);	\
    MAKE_OP(OPNAME, 16, x,  OP16_X2,  STORE);	\
    MAKE_OP(OPNAME, 16, y,  OP16_Y2,  STORE);	\
    MAKE_OP(OPNAME, 16, xy, OP16_XY2, STORE);

#define STORE(l, b) stq(l, b)
PIXOP(put, STORE);

#undef STORE
#define STORE(l, b) stq(avg2(l, ldq(b)), b);
PIXOP(avg, STORE);

mpeg2_mc_t mpeg2_mc_alpha = {
    { MC_put_o_16_alpha, MC_put_x_16_alpha,
      MC_put_y_16_alpha, MC_put_xy_16_alpha,
      MC_put_o_8_alpha, MC_put_x_8_alpha,
      MC_put_y_8_alpha, MC_put_xy_8_alpha },
    { MC_avg_o_16_alpha, MC_avg_x_16_alpha,
      MC_avg_y_16_alpha, MC_avg_xy_16_alpha,
      MC_avg_o_8_alpha, MC_avg_x_8_alpha,
      MC_avg_y_8_alpha, MC_avg_xy_8_alpha }
};

#endif
