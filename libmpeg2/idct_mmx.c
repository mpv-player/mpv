/*
 * idct_mmx.c
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

#if ARCH_X86 || ARCH_X86_64

#include <inttypes.h>

#include "mpeg2.h"
#include "attributes.h"
#include "mpeg2_internal.h"
#include "mmx.h"

#define ROW_SHIFT 15
#define COL_SHIFT 6

#define round(bias) ((int)(((bias)+0.5) * (1<<ROW_SHIFT)))
#define rounder(bias) {round (bias), round (bias)}
#define rounder_sse2(bias) {round (bias), round (bias), round (bias), round (bias)}


#if 0
/* C row IDCT - it is just here to document the MMXEXT and MMX versions */
static inline void idct_row (int16_t * row, int offset,
			     int16_t * table, int32_t * rounder)
{
    int C1, C2, C3, C4, C5, C6, C7;
    int a0, a1, a2, a3, b0, b1, b2, b3;

    row += offset;

    C1 = table[1];
    C2 = table[2];
    C3 = table[3];
    C4 = table[4];
    C5 = table[5];
    C6 = table[6];
    C7 = table[7];

    a0 = C4*row[0] + C2*row[2] + C4*row[4] + C6*row[6] + *rounder;
    a1 = C4*row[0] + C6*row[2] - C4*row[4] - C2*row[6] + *rounder;
    a2 = C4*row[0] - C6*row[2] - C4*row[4] + C2*row[6] + *rounder;
    a3 = C4*row[0] - C2*row[2] + C4*row[4] - C6*row[6] + *rounder;

    b0 = C1*row[1] + C3*row[3] + C5*row[5] + C7*row[7];
    b1 = C3*row[1] - C7*row[3] - C1*row[5] - C5*row[7];
    b2 = C5*row[1] - C1*row[3] + C7*row[5] + C3*row[7];
    b3 = C7*row[1] - C5*row[3] + C3*row[5] - C1*row[7];

    row[0] = (a0 + b0) >> ROW_SHIFT;
    row[1] = (a1 + b1) >> ROW_SHIFT;
    row[2] = (a2 + b2) >> ROW_SHIFT;
    row[3] = (a3 + b3) >> ROW_SHIFT;
    row[4] = (a3 - b3) >> ROW_SHIFT;
    row[5] = (a2 - b2) >> ROW_SHIFT;
    row[6] = (a1 - b1) >> ROW_SHIFT;
    row[7] = (a0 - b0) >> ROW_SHIFT;
}
#endif


/* SSE2 row IDCT */
#define sse2_table(c1,c2,c3,c4,c5,c6,c7) {  c4,  c2,  c4,  c6,   \
					    c4, -c6,  c4, -c2,   \
					    c4,  c6, -c4, -c2,   \
					   -c4,  c2,  c4, -c6,   \
					    c1,  c3,  c3, -c7,   \
					    c5, -c1,  c7, -c5,   \
					    c5,  c7, -c1, -c5,   \
					    c7,  c3,  c3, -c1 }

#define SSE2_IDCT_2ROW(table, row1, row2, round1, round2) do {               \
    /* no scheduling: trust in out of order execution */                     \
    /* based on Intel AP-945 */                                              \
    /* (http://cache-www.intel.com/cd/00/00/01/76/17680_w_idct.pdf) */       \
                                                                             \
    /* input */                      /* 1: row1= x7 x5 x3 x1  x6 x4 x2 x0 */ \
    pshufd_r2r   (row1, xmm1, 0);    /* 1: xmm1= x2 x0 x2 x0  x2 x0 x2 x0 */ \
    pmaddwd_m2r  (table[0], xmm1);   /* 1: xmm1= x2*C + x0*C ...          */ \
    pshufd_r2r   (row1, xmm3, 0xaa); /* 1: xmm3= x3 x1 x3 x1  x3 x1 x3 x1 */ \
    pmaddwd_m2r  (table[2*8], xmm3); /* 1: xmm3= x3*C + x1*C ...          */ \
    pshufd_r2r   (row1, xmm2, 0x55); /* 1: xmm2= x6 x4 x6 x4  x6 x4 x6 x4 */ \
    pshufd_r2r   (row1, row1, 0xff); /* 1: row1= x7 x5 x7 x5  x7 x5 x7 x5 */ \
    pmaddwd_m2r  (table[1*8], xmm2); /* 1: xmm2= x6*C + x4*C ...          */ \
    paddd_m2r    (round1, xmm1);     /* 1: xmm1= x2*C + x0*C + round ...  */ \
    pmaddwd_m2r  (table[3*8], row1); /* 1: row1= x7*C + x5*C ...          */ \
    pshufd_r2r   (row2, xmm5, 0);    /*    2:                             */ \
    pshufd_r2r   (row2, xmm6, 0x55); /*    2:                             */ \
    pmaddwd_m2r  (table[0], xmm5);   /*    2:                             */ \
    paddd_r2r    (xmm2, xmm1);       /* 1: xmm1= a[]                      */ \
    movdqa_r2r   (xmm1, xmm2);       /* 1: xmm2= a[]                      */ \
    pshufd_r2r   (row2, xmm7, 0xaa); /*    2:                             */ \
    pmaddwd_m2r  (table[1*8], xmm6); /*    2:                             */ \
    paddd_r2r    (xmm3, row1);       /* 1: row1= b[]= 7*C+5*C+3*C+1*C ... */ \
    pshufd_r2r   (row2, row2, 0xff); /*    2:                             */ \
    psubd_r2r    (row1, xmm2);       /* 1: xmm2= a[] - b[]                */ \
    pmaddwd_m2r  (table[2*8], xmm7); /*    2:                             */ \
    paddd_r2r    (xmm1, row1);       /* 1: row1= a[] + b[]                */ \
    psrad_i2r    (ROW_SHIFT, xmm2);  /* 1: xmm2= result 4...7             */ \
    paddd_m2r    (round2, xmm5);     /*    2:                             */ \
    pmaddwd_m2r  (table[3*8], row2); /*    2:                             */ \
    paddd_r2r    (xmm6, xmm5);       /*    2:                             */ \
    movdqa_r2r   (xmm5, xmm6);       /*    2:                             */ \
    psrad_i2r    (ROW_SHIFT, row1);  /* 1: row1= result 0...4             */ \
    pshufd_r2r   (xmm2, xmm2, 0x1b); /* 1: [0 1 2 3] -> [3 2 1 0]         */ \
    packssdw_r2r (xmm2, row1);       /* 1: row1= result[]                 */ \
    paddd_r2r    (xmm7, row2);       /*    2:                             */ \
    psubd_r2r    (row2, xmm6);       /*    2:                             */ \
    paddd_r2r    (xmm5, row2);       /*    2:                             */ \
    psrad_i2r    (ROW_SHIFT, xmm6);  /*    2:                             */ \
    psrad_i2r    (ROW_SHIFT, row2);  /*    2:                             */ \
    pshufd_r2r   (xmm6, xmm6, 0x1b); /*    2:                             */ \
    packssdw_r2r (xmm6, row2);       /*    2:                             */ \
} while (0)


/* MMXEXT row IDCT */

#define mmxext_table(c1,c2,c3,c4,c5,c6,c7)	{  c4,  c2, -c4, -c2,	\
						   c4,  c6,  c4,  c6,	\
						   c1,  c3, -c1, -c5,	\
						   c5,  c7,  c3, -c7,	\
						   c4, -c6,  c4, -c6,	\
						  -c4,  c2,  c4, -c2,	\
						   c5, -c1,  c3, -c1,	\
						   c7,  c3,  c7, -c5 }

static inline void mmxext_row_head (int16_t * const row, const int offset,
				    const int16_t * const table)
{
    movq_m2r (*(row+offset), mm2);	/* mm2 = x6 x4 x2 x0 */

    movq_m2r (*(row+offset+4), mm5);	/* mm5 = x7 x5 x3 x1 */
    movq_r2r (mm2, mm0);		/* mm0 = x6 x4 x2 x0 */

    movq_m2r (*table, mm3);		/* mm3 = -C2 -C4 C2 C4 */
    movq_r2r (mm5, mm6);		/* mm6 = x7 x5 x3 x1 */

    movq_m2r (*(table+4), mm4);		/* mm4 = C6 C4 C6 C4 */
    pmaddwd_r2r (mm0, mm3);		/* mm3 = -C4*x4-C2*x6 C4*x0+C2*x2 */

    pshufw_r2r (mm2, mm2, 0x4e);	/* mm2 = x2 x0 x6 x4 */
}

static inline void mmxext_row (const int16_t * const table,
			       const int32_t * const rounder)
{
    movq_m2r (*(table+8), mm1);		/* mm1 = -C5 -C1 C3 C1 */
    pmaddwd_r2r (mm2, mm4);		/* mm4 = C4*x0+C6*x2 C4*x4+C6*x6 */

    pmaddwd_m2r (*(table+16), mm0);	/* mm0 = C4*x4-C6*x6 C4*x0-C6*x2 */
    pshufw_r2r (mm6, mm6, 0x4e);	/* mm6 = x3 x1 x7 x5 */

    movq_m2r (*(table+12), mm7);	/* mm7 = -C7 C3 C7 C5 */
    pmaddwd_r2r (mm5, mm1);		/* mm1 = -C1*x5-C5*x7 C1*x1+C3*x3 */

    paddd_m2r (*rounder, mm3);		/* mm3 += rounder */
    pmaddwd_r2r (mm6, mm7);		/* mm7 = C3*x1-C7*x3 C5*x5+C7*x7 */

    pmaddwd_m2r (*(table+20), mm2);	/* mm2 = C4*x0-C2*x2 -C4*x4+C2*x6 */
    paddd_r2r (mm4, mm3);		/* mm3 = a1 a0 + rounder */

    pmaddwd_m2r (*(table+24), mm5);	/* mm5 = C3*x5-C1*x7 C5*x1-C1*x3 */
    movq_r2r (mm3, mm4);		/* mm4 = a1 a0 + rounder */

    pmaddwd_m2r (*(table+28), mm6);	/* mm6 = C7*x1-C5*x3 C7*x5+C3*x7 */
    paddd_r2r (mm7, mm1);		/* mm1 = b1 b0 */

    paddd_m2r (*rounder, mm0);		/* mm0 += rounder */
    psubd_r2r (mm1, mm3);		/* mm3 = a1-b1 a0-b0 + rounder */

    psrad_i2r (ROW_SHIFT, mm3);		/* mm3 = y6 y7 */
    paddd_r2r (mm4, mm1);		/* mm1 = a1+b1 a0+b0 + rounder */

    paddd_r2r (mm2, mm0);		/* mm0 = a3 a2 + rounder */
    psrad_i2r (ROW_SHIFT, mm1);		/* mm1 = y1 y0 */

    paddd_r2r (mm6, mm5);		/* mm5 = b3 b2 */
    movq_r2r (mm0, mm4);		/* mm4 = a3 a2 + rounder */

    paddd_r2r (mm5, mm0);		/* mm0 = a3+b3 a2+b2 + rounder */
    psubd_r2r (mm5, mm4);		/* mm4 = a3-b3 a2-b2 + rounder */
}

static inline void mmxext_row_tail (int16_t * const row, const int store)
{
    psrad_i2r (ROW_SHIFT, mm0);		/* mm0 = y3 y2 */

    psrad_i2r (ROW_SHIFT, mm4);		/* mm4 = y4 y5 */

    packssdw_r2r (mm0, mm1);		/* mm1 = y3 y2 y1 y0 */

    packssdw_r2r (mm3, mm4);		/* mm4 = y6 y7 y4 y5 */

    movq_r2m (mm1, *(row+store));	/* save y3 y2 y1 y0 */
    pshufw_r2r (mm4, mm4, 0xb1);	/* mm4 = y7 y6 y5 y4 */

    /* slot */

    movq_r2m (mm4, *(row+store+4));	/* save y7 y6 y5 y4 */
}

static inline void mmxext_row_mid (int16_t * const row, const int store,
				   const int offset,
				   const int16_t * const table)
{
    movq_m2r (*(row+offset), mm2);	/* mm2 = x6 x4 x2 x0 */
    psrad_i2r (ROW_SHIFT, mm0);		/* mm0 = y3 y2 */

    movq_m2r (*(row+offset+4), mm5);	/* mm5 = x7 x5 x3 x1 */
    psrad_i2r (ROW_SHIFT, mm4);		/* mm4 = y4 y5 */

    packssdw_r2r (mm0, mm1);		/* mm1 = y3 y2 y1 y0 */
    movq_r2r (mm5, mm6);		/* mm6 = x7 x5 x3 x1 */

    packssdw_r2r (mm3, mm4);		/* mm4 = y6 y7 y4 y5 */
    movq_r2r (mm2, mm0);		/* mm0 = x6 x4 x2 x0 */

    movq_r2m (mm1, *(row+store));	/* save y3 y2 y1 y0 */
    pshufw_r2r (mm4, mm4, 0xb1);	/* mm4 = y7 y6 y5 y4 */

    movq_m2r (*table, mm3);		/* mm3 = -C2 -C4 C2 C4 */
    movq_r2m (mm4, *(row+store+4));	/* save y7 y6 y5 y4 */

    pmaddwd_r2r (mm0, mm3);		/* mm3 = -C4*x4-C2*x6 C4*x0+C2*x2 */

    movq_m2r (*(table+4), mm4);		/* mm4 = C6 C4 C6 C4 */
    pshufw_r2r (mm2, mm2, 0x4e);	/* mm2 = x2 x0 x6 x4 */
}


/* MMX row IDCT */

#define mmx_table(c1,c2,c3,c4,c5,c6,c7)	{  c4,  c2,  c4,  c6,	\
					   c4,  c6, -c4, -c2,	\
					   c1,  c3,  c3, -c7,	\
					   c5,  c7, -c1, -c5,	\
					   c4, -c6,  c4, -c2,	\
					  -c4,  c2,  c4, -c6,	\
					   c5, -c1,  c7, -c5,	\
					   c7,  c3,  c3, -c1 }

static inline void mmx_row_head (int16_t * const row, const int offset,
				 const int16_t * const table)
{
    movq_m2r (*(row+offset), mm2);	/* mm2 = x6 x4 x2 x0 */

    movq_m2r (*(row+offset+4), mm5);	/* mm5 = x7 x5 x3 x1 */
    movq_r2r (mm2, mm0);		/* mm0 = x6 x4 x2 x0 */

    movq_m2r (*table, mm3);		/* mm3 = C6 C4 C2 C4 */
    movq_r2r (mm5, mm6);		/* mm6 = x7 x5 x3 x1 */

    punpckldq_r2r (mm0, mm0);		/* mm0 = x2 x0 x2 x0 */

    movq_m2r (*(table+4), mm4);		/* mm4 = -C2 -C4 C6 C4 */
    pmaddwd_r2r (mm0, mm3);		/* mm3 = C4*x0+C6*x2 C4*x0+C2*x2 */

    movq_m2r (*(table+8), mm1);		/* mm1 = -C7 C3 C3 C1 */
    punpckhdq_r2r (mm2, mm2);		/* mm2 = x6 x4 x6 x4 */
}

static inline void mmx_row (const int16_t * const table,
			    const int32_t * const rounder)
{
    pmaddwd_r2r (mm2, mm4);		/* mm4 = -C4*x4-C2*x6 C4*x4+C6*x6 */
    punpckldq_r2r (mm5, mm5);		/* mm5 = x3 x1 x3 x1 */

    pmaddwd_m2r (*(table+16), mm0);	/* mm0 = C4*x0-C2*x2 C4*x0-C6*x2 */
    punpckhdq_r2r (mm6, mm6);		/* mm6 = x7 x5 x7 x5 */

    movq_m2r (*(table+12), mm7);	/* mm7 = -C5 -C1 C7 C5 */
    pmaddwd_r2r (mm5, mm1);		/* mm1 = C3*x1-C7*x3 C1*x1+C3*x3 */

    paddd_m2r (*rounder, mm3);		/* mm3 += rounder */
    pmaddwd_r2r (mm6, mm7);		/* mm7 = -C1*x5-C5*x7 C5*x5+C7*x7 */

    pmaddwd_m2r (*(table+20), mm2);	/* mm2 = C4*x4-C6*x6 -C4*x4+C2*x6 */
    paddd_r2r (mm4, mm3);		/* mm3 = a1 a0 + rounder */

    pmaddwd_m2r (*(table+24), mm5);	/* mm5 = C7*x1-C5*x3 C5*x1-C1*x3 */
    movq_r2r (mm3, mm4);		/* mm4 = a1 a0 + rounder */

    pmaddwd_m2r (*(table+28), mm6);	/* mm6 = C3*x5-C1*x7 C7*x5+C3*x7 */
    paddd_r2r (mm7, mm1);		/* mm1 = b1 b0 */

    paddd_m2r (*rounder, mm0);		/* mm0 += rounder */
    psubd_r2r (mm1, mm3);		/* mm3 = a1-b1 a0-b0 + rounder */

    psrad_i2r (ROW_SHIFT, mm3);		/* mm3 = y6 y7 */
    paddd_r2r (mm4, mm1);		/* mm1 = a1+b1 a0+b0 + rounder */

    paddd_r2r (mm2, mm0);		/* mm0 = a3 a2 + rounder */
    psrad_i2r (ROW_SHIFT, mm1);		/* mm1 = y1 y0 */

    paddd_r2r (mm6, mm5);		/* mm5 = b3 b2 */
    movq_r2r (mm0, mm7);		/* mm7 = a3 a2 + rounder */

    paddd_r2r (mm5, mm0);		/* mm0 = a3+b3 a2+b2 + rounder */
    psubd_r2r (mm5, mm7);		/* mm7 = a3-b3 a2-b2 + rounder */
}

static inline void mmx_row_tail (int16_t * const row, const int store)
{
    psrad_i2r (ROW_SHIFT, mm0);		/* mm0 = y3 y2 */

    psrad_i2r (ROW_SHIFT, mm7);		/* mm7 = y4 y5 */

    packssdw_r2r (mm0, mm1);		/* mm1 = y3 y2 y1 y0 */

    packssdw_r2r (mm3, mm7);		/* mm7 = y6 y7 y4 y5 */

    movq_r2m (mm1, *(row+store));	/* save y3 y2 y1 y0 */
    movq_r2r (mm7, mm4);		/* mm4 = y6 y7 y4 y5 */

    pslld_i2r (16, mm7);		/* mm7 = y7 0 y5 0 */

    psrld_i2r (16, mm4);		/* mm4 = 0 y6 0 y4 */

    por_r2r (mm4, mm7);			/* mm7 = y7 y6 y5 y4 */

    /* slot */

    movq_r2m (mm7, *(row+store+4));	/* save y7 y6 y5 y4 */
}

static inline void mmx_row_mid (int16_t * const row, const int store,
				const int offset, const int16_t * const table)
{
    movq_m2r (*(row+offset), mm2);	/* mm2 = x6 x4 x2 x0 */
    psrad_i2r (ROW_SHIFT, mm0);		/* mm0 = y3 y2 */

    movq_m2r (*(row+offset+4), mm5);	/* mm5 = x7 x5 x3 x1 */
    psrad_i2r (ROW_SHIFT, mm7);		/* mm7 = y4 y5 */

    packssdw_r2r (mm0, mm1);		/* mm1 = y3 y2 y1 y0 */
    movq_r2r (mm5, mm6);		/* mm6 = x7 x5 x3 x1 */

    packssdw_r2r (mm3, mm7);		/* mm7 = y6 y7 y4 y5 */
    movq_r2r (mm2, mm0);		/* mm0 = x6 x4 x2 x0 */

    movq_r2m (mm1, *(row+store));	/* save y3 y2 y1 y0 */
    movq_r2r (mm7, mm1);		/* mm1 = y6 y7 y4 y5 */

    punpckldq_r2r (mm0, mm0);		/* mm0 = x2 x0 x2 x0 */
    psrld_i2r (16, mm7);		/* mm7 = 0 y6 0 y4 */

    movq_m2r (*table, mm3);		/* mm3 = C6 C4 C2 C4 */
    pslld_i2r (16, mm1);		/* mm1 = y7 0 y5 0 */

    movq_m2r (*(table+4), mm4);		/* mm4 = -C2 -C4 C6 C4 */
    por_r2r (mm1, mm7);			/* mm7 = y7 y6 y5 y4 */

    movq_m2r (*(table+8), mm1);		/* mm1 = -C7 C3 C3 C1 */
    punpckhdq_r2r (mm2, mm2);		/* mm2 = x6 x4 x6 x4 */

    movq_r2m (mm7, *(row+store+4));	/* save y7 y6 y5 y4 */
    pmaddwd_r2r (mm0, mm3);		/* mm3 = C4*x0+C6*x2 C4*x0+C2*x2 */
}


#if 0
/* C column IDCT - it is just here to document the MMXEXT and MMX versions */
static inline void idct_col (int16_t * col, int offset)
{
/* multiplication - as implemented on mmx */
#define F(c,x) (((c) * (x)) >> 16)

/* saturation - it helps us handle torture test cases */
#define S(x) (((x)>32767) ? 32767 : ((x)<-32768) ? -32768 : (x))

    int16_t x0, x1, x2, x3, x4, x5, x6, x7;
    int16_t y0, y1, y2, y3, y4, y5, y6, y7;
    int16_t a0, a1, a2, a3, b0, b1, b2, b3;
    int16_t u04, v04, u26, v26, u17, v17, u35, v35, u12, v12;

    col += offset;

    x0 = col[0*8];
    x1 = col[1*8];
    x2 = col[2*8];
    x3 = col[3*8];
    x4 = col[4*8];
    x5 = col[5*8];
    x6 = col[6*8];
    x7 = col[7*8];

    u04 = S (x0 + x4);
    v04 = S (x0 - x4);
    u26 = S (F (T2, x6) + x2);
    v26 = S (F (T2, x2) - x6);

    a0 = S (u04 + u26);
    a1 = S (v04 + v26);
    a2 = S (v04 - v26);
    a3 = S (u04 - u26);

    u17 = S (F (T1, x7) + x1);
    v17 = S (F (T1, x1) - x7);
    u35 = S (F (T3, x5) + x3);
    v35 = S (F (T3, x3) - x5);

    b0 = S (u17 + u35);
    b3 = S (v17 - v35);
    u12 = S (u17 - u35);
    v12 = S (v17 + v35);
    u12 = S (2 * F (C4, u12));
    v12 = S (2 * F (C4, v12));
    b1 = S (u12 + v12);
    b2 = S (u12 - v12);

    y0 = S (a0 + b0) >> COL_SHIFT;
    y1 = S (a1 + b1) >> COL_SHIFT;
    y2 = S (a2 + b2) >> COL_SHIFT;
    y3 = S (a3 + b3) >> COL_SHIFT;

    y4 = S (a3 - b3) >> COL_SHIFT;
    y5 = S (a2 - b2) >> COL_SHIFT;
    y6 = S (a1 - b1) >> COL_SHIFT;
    y7 = S (a0 - b0) >> COL_SHIFT;

    col[0*8] = y0;
    col[1*8] = y1;
    col[2*8] = y2;
    col[3*8] = y3;
    col[4*8] = y4;
    col[5*8] = y5;
    col[6*8] = y6;
    col[7*8] = y7;
}
#endif


#define T1 13036
#define T2 27146
#define T3 43790
#define C4 23170


/* SSE2 column IDCT */
static inline void sse2_idct_col (int16_t * const col)
{
    /* Almost identical to mmxext version:  */
    /* just do both 4x8 columns in paraller */

    static const short t1_vector[] ATTR_ALIGN(16) = {T1,T1,T1,T1,T1,T1,T1,T1};
    static const short t2_vector[] ATTR_ALIGN(16) = {T2,T2,T2,T2,T2,T2,T2,T2};
    static const short t3_vector[] ATTR_ALIGN(16) = {T3,T3,T3,T3,T3,T3,T3,T3};
    static const short c4_vector[] ATTR_ALIGN(16) = {C4,C4,C4,C4,C4,C4,C4,C4};

#if defined(__x86_64__)

    /* INPUT: block in xmm8 ... xmm15 */

    movdqa_m2r (*t1_vector, xmm0);	/* xmm0  = T1 */
    movdqa_r2r (xmm9, xmm1);		/* xmm1  = x1 */

    movdqa_r2r (xmm0, xmm2);		/* xmm2  = T1 */
    pmulhw_r2r (xmm1, xmm0);		/* xmm0  = T1*x1 */

    movdqa_m2r (*t3_vector, xmm5);	/* xmm5  = T3 */
    pmulhw_r2r (xmm15, xmm2);		/* xmm2  = T1*x7 */

    movdqa_r2r (xmm5, xmm7);		/* xmm7  = T3-1 */
    psubsw_r2r (xmm15, xmm0);		/* xmm0  = v17 */

    movdqa_m2r (*t2_vector, xmm9);	/* xmm9  = T2 */
    pmulhw_r2r (xmm11, xmm5);		/* xmm5  = (T3-1)*x3 */

    paddsw_r2r (xmm2, xmm1);		/* xmm1  = u17 */
    pmulhw_r2r (xmm13, xmm7);		/* xmm7  = (T3-1)*x5 */

    movdqa_r2r (xmm9, xmm2);		/* xmm2  = T2 */
    paddsw_r2r (xmm11, xmm5);		/* xmm5  = T3*x3 */

    pmulhw_r2r (xmm10, xmm9);   	/* xmm9  = T2*x2 */
    paddsw_r2r (xmm13, xmm7);		/* xmm7  = T3*x5 */

    psubsw_r2r (xmm13, xmm5);		/* xmm5  = v35 */
    paddsw_r2r (xmm11, xmm7);		/* xmm7  = u35 */

    movdqa_r2r (xmm0, xmm6);		/* xmm6  = v17 */
    pmulhw_r2r (xmm14, xmm2);		/* xmm2  = T2*x6 */

    psubsw_r2r (xmm5, xmm0);		/* xmm0  = b3 */
    psubsw_r2r (xmm14, xmm9);		/* xmm9  = v26 */

    paddsw_r2r (xmm6, xmm5);		/* xmm5  = v12 */
    movdqa_r2r (xmm0, xmm11);		/* xmm11 = b3 */

    movdqa_r2r (xmm1, xmm6);		/* xmm6  = u17 */
    paddsw_r2r (xmm10, xmm2);		/* xmm2  = u26 */

    paddsw_r2r (xmm7, xmm6);		/* xmm6  = b0 */
    psubsw_r2r (xmm7, xmm1);		/* xmm1  = u12 */

    movdqa_r2r (xmm1, xmm7);		/* xmm7  = u12 */
    paddsw_r2r (xmm5, xmm1);		/* xmm1  = u12+v12 */

    movdqa_m2r (*c4_vector, xmm0);	/* xmm0  = C4/2 */
    psubsw_r2r (xmm5, xmm7);		/* xmm7  = u12-v12 */

    movdqa_r2r (xmm6, xmm4);		/* xmm4  = b0 */
    pmulhw_r2r (xmm0, xmm1);		/* xmm1  = b1/2 */

    movdqa_r2r (xmm9, xmm6);		/* xmm6  = v26 */
    pmulhw_r2r (xmm0, xmm7);		/* xmm7  = b2/2 */

    movdqa_r2r (xmm8, xmm10);		/* xmm10 = x0 */
    movdqa_r2r (xmm8, xmm0);		/* xmm0  = x0 */

    psubsw_r2r (xmm12, xmm10);		/* xmm10 = v04 */
    paddsw_r2r (xmm12, xmm0);		/* xmm0  = u04 */

    paddsw_r2r (xmm10, xmm9);		/* xmm9  = a1 */
    movdqa_r2r (xmm0, xmm8);		/* xmm8  = u04 */

    psubsw_r2r (xmm6, xmm10);		/* xmm10 = a2 */
    paddsw_r2r (xmm2, xmm8);		/* xmm5  = a0 */

    paddsw_r2r (xmm1, xmm1);		/* xmm1  = b1 */
    psubsw_r2r (xmm2, xmm0);		/* xmm0  = a3 */

    paddsw_r2r (xmm7, xmm7);		/* xmm7  = b2 */
    movdqa_r2r (xmm10, xmm13);		/* xmm13 = a2 */

    movdqa_r2r (xmm9, xmm14);		/* xmm14 = a1 */
    paddsw_r2r (xmm7, xmm10);		/* xmm10 = a2+b2 */

    psraw_i2r (COL_SHIFT,xmm10);	/* xmm10 = y2 */
    paddsw_r2r (xmm1, xmm9);		/* xmm9  = a1+b1 */

    psraw_i2r (COL_SHIFT, xmm9);	/* xmm9  = y1 */
    psubsw_r2r (xmm1, xmm14);		/* xmm14 = a1-b1 */

    psubsw_r2r (xmm7, xmm13);		/* xmm13 = a2-b2 */
    psraw_i2r (COL_SHIFT,xmm14);	/* xmm14 = y6 */

    movdqa_r2r (xmm8, xmm15);		/* xmm15 = a0 */
    psraw_i2r (COL_SHIFT,xmm13);	/* xmm13 = y5 */

    paddsw_r2r (xmm4, xmm8);		/* xmm8  = a0+b0 */
    psubsw_r2r (xmm4, xmm15);		/* xmm15 = a0-b0 */

    psraw_i2r (COL_SHIFT, xmm8);	/* xmm8  = y0 */
    movdqa_r2r (xmm0, xmm12);		/* xmm12 = a3 */

    psubsw_r2r (xmm11, xmm12);		/* xmm12 = a3-b3 */
    psraw_i2r (COL_SHIFT,xmm15);	/* xmm15 = y7 */

    paddsw_r2r (xmm0, xmm11);		/* xmm11 = a3+b3 */
    psraw_i2r (COL_SHIFT,xmm12);	/* xmm12 = y4 */

    psraw_i2r (COL_SHIFT,xmm11);	/* xmm11 = y3 */

    /* OUTPUT: block in xmm8 ... xmm15 */

#else
    movdqa_m2r (*t1_vector, xmm0);	/* xmm0 = T1 */

    movdqa_m2r (*(col+1*8), xmm1);	/* xmm1 = x1 */
    movdqa_r2r (xmm0, xmm2);		/* xmm2 = T1 */

    movdqa_m2r (*(col+7*8), xmm4);	/* xmm4 = x7 */
    pmulhw_r2r (xmm1, xmm0);		/* xmm0 = T1*x1 */

    movdqa_m2r (*t3_vector, xmm5);	/* xmm5 = T3 */
    pmulhw_r2r (xmm4, xmm2);		/* xmm2 = T1*x7 */

    movdqa_m2r (*(col+5*8), xmm6);	/* xmm6 = x5 */
    movdqa_r2r (xmm5, xmm7);		/* xmm7 = T3-1 */

    movdqa_m2r (*(col+3*8), xmm3);	/* xmm3 = x3 */
    psubsw_r2r (xmm4, xmm0);		/* xmm0 = v17 */

    movdqa_m2r (*t2_vector, xmm4);	/* xmm4 = T2 */
    pmulhw_r2r (xmm3, xmm5);		/* xmm5 = (T3-1)*x3 */

    paddsw_r2r (xmm2, xmm1);		/* xmm1 = u17 */
    pmulhw_r2r (xmm6, xmm7);		/* xmm7 = (T3-1)*x5 */

    /* slot */

    movdqa_r2r (xmm4, xmm2);		/* xmm2 = T2 */
    paddsw_r2r (xmm3, xmm5);		/* xmm5 = T3*x3 */

    pmulhw_m2r (*(col+2*8), xmm4);	/* xmm4 = T2*x2 */
    paddsw_r2r (xmm6, xmm7);		/* xmm7 = T3*x5 */

    psubsw_r2r (xmm6, xmm5);		/* xmm5 = v35 */
    paddsw_r2r (xmm3, xmm7);		/* xmm7 = u35 */

    movdqa_m2r (*(col+6*8), xmm3);	/* xmm3 = x6 */
    movdqa_r2r (xmm0, xmm6);		/* xmm6 = v17 */

    pmulhw_r2r (xmm3, xmm2);		/* xmm2 = T2*x6 */
    psubsw_r2r (xmm5, xmm0);		/* xmm0 = b3 */

    psubsw_r2r (xmm3, xmm4);		/* xmm4 = v26 */
    paddsw_r2r (xmm6, xmm5);		/* xmm5 = v12 */

    movdqa_r2m (xmm0, *(col+3*8));	/* save b3 in scratch0 */
    movdqa_r2r (xmm1, xmm6);		/* xmm6 = u17 */

    paddsw_m2r (*(col+2*8), xmm2);	/* xmm2 = u26 */
    paddsw_r2r (xmm7, xmm6);		/* xmm6 = b0 */

    psubsw_r2r (xmm7, xmm1);		/* xmm1 = u12 */
    movdqa_r2r (xmm1, xmm7);		/* xmm7 = u12 */

    movdqa_m2r (*(col+0*8), xmm3);	/* xmm3 = x0 */
    paddsw_r2r (xmm5, xmm1);		/* xmm1 = u12+v12 */

    movdqa_m2r (*c4_vector, xmm0);	/* xmm0 = C4/2 */
    psubsw_r2r (xmm5, xmm7);		/* xmm7 = u12-v12 */

    movdqa_r2m (xmm6, *(col+5*8));	/* save b0 in scratch1 */
    pmulhw_r2r (xmm0, xmm1);		/* xmm1 = b1/2 */

    movdqa_r2r (xmm4, xmm6);		/* xmm6 = v26 */
    pmulhw_r2r (xmm0, xmm7);		/* xmm7 = b2/2 */

    movdqa_m2r (*(col+4*8), xmm5);	/* xmm5 = x4 */
    movdqa_r2r (xmm3, xmm0);		/* xmm0 = x0 */

    psubsw_r2r (xmm5, xmm3);		/* xmm3 = v04 */
    paddsw_r2r (xmm5, xmm0);		/* xmm0 = u04 */

    paddsw_r2r (xmm3, xmm4);		/* xmm4 = a1 */
    movdqa_r2r (xmm0, xmm5);		/* xmm5 = u04 */

    psubsw_r2r (xmm6, xmm3);		/* xmm3 = a2 */
    paddsw_r2r (xmm2, xmm5);		/* xmm5 = a0 */

    paddsw_r2r (xmm1, xmm1);		/* xmm1 = b1 */
    psubsw_r2r (xmm2, xmm0);		/* xmm0 = a3 */

    paddsw_r2r (xmm7, xmm7);		/* xmm7 = b2 */
    movdqa_r2r (xmm3, xmm2);		/* xmm2 = a2 */

    movdqa_r2r (xmm4, xmm6);		/* xmm6 = a1 */
    paddsw_r2r (xmm7, xmm3);		/* xmm3 = a2+b2 */

    psraw_i2r (COL_SHIFT, xmm3);	/* xmm3 = y2 */
    paddsw_r2r (xmm1, xmm4);		/* xmm4 = a1+b1 */

    psraw_i2r (COL_SHIFT, xmm4);	/* xmm4 = y1 */
    psubsw_r2r (xmm1, xmm6);		/* xmm6 = a1-b1 */

    movdqa_m2r (*(col+5*8), xmm1);	/* xmm1 = b0 */
    psubsw_r2r (xmm7, xmm2);		/* xmm2 = a2-b2 */

    psraw_i2r (COL_SHIFT, xmm6);	/* xmm6 = y6 */
    movdqa_r2r (xmm5, xmm7);		/* xmm7 = a0 */

    movdqa_r2m (xmm4, *(col+1*8));	/* save y1 */
    psraw_i2r (COL_SHIFT, xmm2);	/* xmm2 = y5 */

    movdqa_r2m (xmm3, *(col+2*8));	/* save y2 */
    paddsw_r2r (xmm1, xmm5);		/* xmm5 = a0+b0 */

    movdqa_m2r (*(col+3*8), xmm4);	/* xmm4 = b3 */
    psubsw_r2r (xmm1, xmm7);		/* xmm7 = a0-b0 */

    psraw_i2r (COL_SHIFT, xmm5);	/* xmm5 = y0 */
    movdqa_r2r (xmm0, xmm3);		/* xmm3 = a3 */

    movdqa_r2m (xmm2, *(col+5*8));	/* save y5 */
    psubsw_r2r (xmm4, xmm3);		/* xmm3 = a3-b3 */

    psraw_i2r (COL_SHIFT, xmm7);	/* xmm7 = y7 */
    paddsw_r2r (xmm0, xmm4);		/* xmm4 = a3+b3 */

    movdqa_r2m (xmm5, *(col+0*8));	/* save y0 */
    psraw_i2r (COL_SHIFT, xmm3);	/* xmm3 = y4 */

    movdqa_r2m (xmm6, *(col+6*8));	/* save y6 */
    psraw_i2r (COL_SHIFT, xmm4);	/* xmm4 = y3 */

    movdqa_r2m (xmm7, *(col+7*8));	/* save y7 */

    movdqa_r2m (xmm3, *(col+4*8));	/* save y4 */

    movdqa_r2m (xmm4, *(col+3*8));	/* save y3 */
#endif
}


/* MMX column IDCT */
static inline void idct_col (int16_t * const col, const int offset)
{
    static const short t1_vector[] ATTR_ALIGN(8) = {T1,T1,T1,T1};
    static const short t2_vector[] ATTR_ALIGN(8) = {T2,T2,T2,T2};
    static const short t3_vector[] ATTR_ALIGN(8) = {T3,T3,T3,T3};
    static const short c4_vector[] ATTR_ALIGN(8) = {C4,C4,C4,C4};

    /* column code adapted from peter gubanov */
    /* http://www.elecard.com/peter/idct.shtml */

    movq_m2r (*t1_vector, mm0);		/* mm0 = T1 */

    movq_m2r (*(col+offset+1*8), mm1);	/* mm1 = x1 */
    movq_r2r (mm0, mm2);		/* mm2 = T1 */

    movq_m2r (*(col+offset+7*8), mm4);	/* mm4 = x7 */
    pmulhw_r2r (mm1, mm0);		/* mm0 = T1*x1 */

    movq_m2r (*t3_vector, mm5);		/* mm5 = T3 */
    pmulhw_r2r (mm4, mm2);		/* mm2 = T1*x7 */

    movq_m2r (*(col+offset+5*8), mm6);	/* mm6 = x5 */
    movq_r2r (mm5, mm7);		/* mm7 = T3-1 */

    movq_m2r (*(col+offset+3*8), mm3);	/* mm3 = x3 */
    psubsw_r2r (mm4, mm0);		/* mm0 = v17 */

    movq_m2r (*t2_vector, mm4);		/* mm4 = T2 */
    pmulhw_r2r (mm3, mm5);		/* mm5 = (T3-1)*x3 */

    paddsw_r2r (mm2, mm1);		/* mm1 = u17 */
    pmulhw_r2r (mm6, mm7);		/* mm7 = (T3-1)*x5 */

    /* slot */

    movq_r2r (mm4, mm2);		/* mm2 = T2 */
    paddsw_r2r (mm3, mm5);		/* mm5 = T3*x3 */

    pmulhw_m2r (*(col+offset+2*8), mm4);/* mm4 = T2*x2 */
    paddsw_r2r (mm6, mm7);		/* mm7 = T3*x5 */

    psubsw_r2r (mm6, mm5);		/* mm5 = v35 */
    paddsw_r2r (mm3, mm7);		/* mm7 = u35 */

    movq_m2r (*(col+offset+6*8), mm3);	/* mm3 = x6 */
    movq_r2r (mm0, mm6);		/* mm6 = v17 */

    pmulhw_r2r (mm3, mm2);		/* mm2 = T2*x6 */
    psubsw_r2r (mm5, mm0);		/* mm0 = b3 */

    psubsw_r2r (mm3, mm4);		/* mm4 = v26 */
    paddsw_r2r (mm6, mm5);		/* mm5 = v12 */

    movq_r2m (mm0, *(col+offset+3*8));	/* save b3 in scratch0 */
    movq_r2r (mm1, mm6);		/* mm6 = u17 */

    paddsw_m2r (*(col+offset+2*8), mm2);/* mm2 = u26 */
    paddsw_r2r (mm7, mm6);		/* mm6 = b0 */

    psubsw_r2r (mm7, mm1);		/* mm1 = u12 */
    movq_r2r (mm1, mm7);		/* mm7 = u12 */

    movq_m2r (*(col+offset+0*8), mm3);	/* mm3 = x0 */
    paddsw_r2r (mm5, mm1);		/* mm1 = u12+v12 */

    movq_m2r (*c4_vector, mm0);		/* mm0 = C4/2 */
    psubsw_r2r (mm5, mm7);		/* mm7 = u12-v12 */

    movq_r2m (mm6, *(col+offset+5*8));	/* save b0 in scratch1 */
    pmulhw_r2r (mm0, mm1);		/* mm1 = b1/2 */

    movq_r2r (mm4, mm6);		/* mm6 = v26 */
    pmulhw_r2r (mm0, mm7);		/* mm7 = b2/2 */

    movq_m2r (*(col+offset+4*8), mm5);	/* mm5 = x4 */
    movq_r2r (mm3, mm0);		/* mm0 = x0 */

    psubsw_r2r (mm5, mm3);		/* mm3 = v04 */
    paddsw_r2r (mm5, mm0);		/* mm0 = u04 */

    paddsw_r2r (mm3, mm4);		/* mm4 = a1 */
    movq_r2r (mm0, mm5);		/* mm5 = u04 */

    psubsw_r2r (mm6, mm3);		/* mm3 = a2 */
    paddsw_r2r (mm2, mm5);		/* mm5 = a0 */

    paddsw_r2r (mm1, mm1);		/* mm1 = b1 */
    psubsw_r2r (mm2, mm0);		/* mm0 = a3 */

    paddsw_r2r (mm7, mm7);		/* mm7 = b2 */
    movq_r2r (mm3, mm2);		/* mm2 = a2 */

    movq_r2r (mm4, mm6);		/* mm6 = a1 */
    paddsw_r2r (mm7, mm3);		/* mm3 = a2+b2 */

    psraw_i2r (COL_SHIFT, mm3);		/* mm3 = y2 */
    paddsw_r2r (mm1, mm4);		/* mm4 = a1+b1 */

    psraw_i2r (COL_SHIFT, mm4);		/* mm4 = y1 */
    psubsw_r2r (mm1, mm6);		/* mm6 = a1-b1 */

    movq_m2r (*(col+offset+5*8), mm1);	/* mm1 = b0 */
    psubsw_r2r (mm7, mm2);		/* mm2 = a2-b2 */

    psraw_i2r (COL_SHIFT, mm6);		/* mm6 = y6 */
    movq_r2r (mm5, mm7);		/* mm7 = a0 */

    movq_r2m (mm4, *(col+offset+1*8));	/* save y1 */
    psraw_i2r (COL_SHIFT, mm2);		/* mm2 = y5 */

    movq_r2m (mm3, *(col+offset+2*8));	/* save y2 */
    paddsw_r2r (mm1, mm5);		/* mm5 = a0+b0 */

    movq_m2r (*(col+offset+3*8), mm4);	/* mm4 = b3 */
    psubsw_r2r (mm1, mm7);		/* mm7 = a0-b0 */

    psraw_i2r (COL_SHIFT, mm5);		/* mm5 = y0 */
    movq_r2r (mm0, mm3);		/* mm3 = a3 */

    movq_r2m (mm2, *(col+offset+5*8));	/* save y5 */
    psubsw_r2r (mm4, mm3);		/* mm3 = a3-b3 */

    psraw_i2r (COL_SHIFT, mm7);		/* mm7 = y7 */
    paddsw_r2r (mm0, mm4);		/* mm4 = a3+b3 */

    movq_r2m (mm5, *(col+offset+0*8));	/* save y0 */
    psraw_i2r (COL_SHIFT, mm3);		/* mm3 = y4 */

    movq_r2m (mm6, *(col+offset+6*8));	/* save y6 */
    psraw_i2r (COL_SHIFT, mm4);		/* mm4 = y3 */

    movq_r2m (mm7, *(col+offset+7*8));	/* save y7 */

    movq_r2m (mm3, *(col+offset+4*8));	/* save y4 */

    movq_r2m (mm4, *(col+offset+3*8));	/* save y3 */
}


static const int32_t rounder0[] ATTR_ALIGN(8) =
    rounder ((1 << (COL_SHIFT - 1)) - 0.5);
static const int32_t rounder4[] ATTR_ALIGN(8) = rounder (0);
static const int32_t rounder1[] ATTR_ALIGN(8) =
    rounder (1.25683487303);	/* C1*(C1/C4+C1+C7)/2 */
static const int32_t rounder7[] ATTR_ALIGN(8) =
    rounder (-0.25);		/* C1*(C7/C4+C7-C1)/2 */
static const int32_t rounder2[] ATTR_ALIGN(8) =
    rounder (0.60355339059);	/* C2 * (C6+C2)/2 */
static const int32_t rounder6[] ATTR_ALIGN(8) =
    rounder (-0.25);		/* C2 * (C6-C2)/2 */
static const int32_t rounder3[] ATTR_ALIGN(8) =
    rounder (0.087788325588);	/* C3*(-C3/C4+C3+C5)/2 */
static const int32_t rounder5[] ATTR_ALIGN(8) =
    rounder (-0.441341716183);	/* C3*(-C5/C4+C5-C3)/2 */


#define declare_idct(idct,table,idct_row_head,idct_row,idct_row_tail,idct_row_mid)	\
static inline void idct (int16_t * const block)				\
{									\
    static const int16_t table04[] ATTR_ALIGN(16) =			\
	table (22725, 21407, 19266, 16384, 12873,  8867, 4520);		\
    static const int16_t table17[] ATTR_ALIGN(16) =			\
	table (31521, 29692, 26722, 22725, 17855, 12299, 6270);		\
    static const int16_t table26[] ATTR_ALIGN(16) =			\
	table (29692, 27969, 25172, 21407, 16819, 11585, 5906);		\
    static const int16_t table35[] ATTR_ALIGN(16) =			\
	table (26722, 25172, 22654, 19266, 15137, 10426, 5315);		\
									\
    idct_row_head (block, 0*8, table04);				\
    idct_row (table04, rounder0);					\
    idct_row_mid (block, 0*8, 4*8, table04);				\
    idct_row (table04, rounder4);					\
    idct_row_mid (block, 4*8, 1*8, table17);				\
    idct_row (table17, rounder1);					\
    idct_row_mid (block, 1*8, 7*8, table17);				\
    idct_row (table17, rounder7);					\
    idct_row_mid (block, 7*8, 2*8, table26);				\
    idct_row (table26, rounder2);					\
    idct_row_mid (block, 2*8, 6*8, table26);				\
    idct_row (table26, rounder6);					\
    idct_row_mid (block, 6*8, 3*8, table35);				\
    idct_row (table35, rounder3);					\
    idct_row_mid (block, 3*8, 5*8, table35);				\
    idct_row (table35, rounder5);					\
    idct_row_tail (block, 5*8);						\
									\
    idct_col (block, 0);						\
    idct_col (block, 4);						\
}

static inline void sse2_idct (int16_t * const block)
{
    static const int16_t table04[] ATTR_ALIGN(16) =
	sse2_table (22725, 21407, 19266, 16384, 12873,  8867, 4520);
    static const int16_t table17[] ATTR_ALIGN(16) =
	sse2_table (31521, 29692, 26722, 22725, 17855, 12299, 6270);
    static const int16_t table26[] ATTR_ALIGN(16) =
	sse2_table (29692, 27969, 25172, 21407, 16819, 11585, 5906);
    static const int16_t table35[] ATTR_ALIGN(16) =
	sse2_table (26722, 25172, 22654, 19266, 15137, 10426, 5315);

    static const int32_t rounder0_128[] ATTR_ALIGN(16) =
	rounder_sse2 ((1 << (COL_SHIFT - 1)) - 0.5);
    static const int32_t rounder4_128[] ATTR_ALIGN(16) = rounder_sse2 (0);
    static const int32_t rounder1_128[] ATTR_ALIGN(16) =
	rounder_sse2 (1.25683487303);	/* C1*(C1/C4+C1+C7)/2 */
    static const int32_t rounder7_128[] ATTR_ALIGN(16) =
	rounder_sse2 (-0.25);		/* C1*(C7/C4+C7-C1)/2 */
    static const int32_t rounder2_128[] ATTR_ALIGN(16) =
	rounder_sse2 (0.60355339059);	/* C2 * (C6+C2)/2 */
    static const int32_t rounder6_128[] ATTR_ALIGN(16) =
	rounder_sse2 (-0.25);		/* C2 * (C6-C2)/2 */
    static const int32_t rounder3_128[] ATTR_ALIGN(16) =
	rounder_sse2 (0.087788325588);	/* C3*(-C3/C4+C3+C5)/2 */
    static const int32_t rounder5_128[] ATTR_ALIGN(16) =
	rounder_sse2 (-0.441341716183);	/* C3*(-C5/C4+C5-C3)/2 */

#if defined(__x86_64__)
    movdqa_m2r (block[0*8], xmm8);
    movdqa_m2r (block[4*8], xmm12);
    SSE2_IDCT_2ROW (table04,  xmm8, xmm12, *rounder0_128, *rounder4_128);

    movdqa_m2r (block[1*8], xmm9);
    movdqa_m2r (block[7*8], xmm15);
    SSE2_IDCT_2ROW (table17,  xmm9, xmm15, *rounder1_128, *rounder7_128);

    movdqa_m2r (block[2*8], xmm10);
    movdqa_m2r (block[6*8], xmm14);
    SSE2_IDCT_2ROW (table26, xmm10, xmm14, *rounder2_128, *rounder6_128);

    movdqa_m2r (block[3*8], xmm11);
    movdqa_m2r (block[5*8], xmm13);
    SSE2_IDCT_2ROW (table35, xmm11, xmm13, *rounder3_128, *rounder5_128);

    /* OUTPUT: block in xmm8 ... xmm15 */

#else
    movdqa_m2r (block[0*8], xmm0);
    movdqa_m2r (block[4*8], xmm4);
    SSE2_IDCT_2ROW (table04, xmm0, xmm4, *rounder0_128, *rounder4_128);
    movdqa_r2m (xmm0, block[0*8]);
    movdqa_r2m (xmm4, block[4*8]);

    movdqa_m2r (block[1*8], xmm0);
    movdqa_m2r (block[7*8], xmm4);
    SSE2_IDCT_2ROW (table17, xmm0, xmm4, *rounder1_128, *rounder7_128);
    movdqa_r2m (xmm0, block[1*8]);
    movdqa_r2m (xmm4, block[7*8]);

    movdqa_m2r (block[2*8], xmm0);
    movdqa_m2r (block[6*8], xmm4);
    SSE2_IDCT_2ROW (table26, xmm0, xmm4, *rounder2_128, *rounder6_128);
    movdqa_r2m (xmm0, block[2*8]);
    movdqa_r2m (xmm4, block[6*8]);

    movdqa_m2r (block[3*8], xmm0);
    movdqa_m2r (block[5*8], xmm4);
    SSE2_IDCT_2ROW (table35, xmm0, xmm4, *rounder3_128, *rounder5_128);
    movdqa_r2m (xmm0, block[3*8]);
    movdqa_r2m (xmm4, block[5*8]);
#endif

    sse2_idct_col (block);
}

static void sse2_block_copy (int16_t * const block, uint8_t * dest,
			     const int stride)
{
#if defined(__x86_64__)
    /* INPUT: block in xmm8 ... xmm15 */
    packuswb_r2r (xmm8, xmm8);
    packuswb_r2r (xmm9, xmm9);
    movq_r2m (xmm8,  *(dest+0*stride));
    packuswb_r2r (xmm10, xmm10);
    movq_r2m (xmm9,  *(dest+1*stride));
    packuswb_r2r (xmm11, xmm11);
    movq_r2m (xmm10, *(dest+2*stride));
    packuswb_r2r (xmm12, xmm12);
    movq_r2m (xmm11, *(dest+3*stride));
    packuswb_r2r (xmm13, xmm13);
    movq_r2m (xmm12, *(dest+4*stride));
    packuswb_r2r (xmm14, xmm14);
    movq_r2m (xmm13, *(dest+5*stride));
    packuswb_r2r (xmm15, xmm15);
    movq_r2m (xmm14, *(dest+6*stride));
    movq_r2m (xmm15, *(dest+7*stride));
#else
    movdqa_m2r (*(block+0*8), xmm0);
    movdqa_m2r (*(block+1*8), xmm1);
    movdqa_m2r (*(block+2*8), xmm2);
    packuswb_r2r (xmm0, xmm0);
    movdqa_m2r (*(block+3*8), xmm3);
    packuswb_r2r (xmm1, xmm1);
    movdqa_m2r (*(block+4*8), xmm4);
    packuswb_r2r (xmm2, xmm2);
    movdqa_m2r (*(block+5*8), xmm5);
    packuswb_r2r (xmm3, xmm3);
    movdqa_m2r (*(block+6*8), xmm6);
    packuswb_r2r (xmm4, xmm4);
    movdqa_m2r (*(block+7*8), xmm7);
    movq_r2m (xmm0, *(dest+0*stride));
    packuswb_r2r (xmm5, xmm5);
    movq_r2m (xmm1, *(dest+1*stride));
    packuswb_r2r (xmm6, xmm6);
    movq_r2m (xmm2, *(dest+2*stride));
    packuswb_r2r (xmm7, xmm7);
    movq_r2m (xmm3, *(dest+3*stride));
    movq_r2m (xmm4, *(dest+4*stride));
    movq_r2m (xmm5, *(dest+5*stride));
    movq_r2m (xmm6, *(dest+6*stride));
    movq_r2m (xmm7, *(dest+7*stride));
#endif
}

#define COPY_MMX(offset,r0,r1,r2)	\
do {					\
    movq_m2r (*(block+offset), r0);	\
    dest += stride;			\
    movq_m2r (*(block+offset+4), r1);	\
    movq_r2m (r2, *dest);		\
    packuswb_r2r (r1, r0);		\
} while (0)

static inline void block_copy (int16_t * const block, uint8_t * dest,
			       const int stride)
{
    movq_m2r (*(block+0*8), mm0);
    movq_m2r (*(block+0*8+4), mm1);
    movq_m2r (*(block+1*8), mm2);
    packuswb_r2r (mm1, mm0);
    movq_m2r (*(block+1*8+4), mm3);
    movq_r2m (mm0, *dest);
    packuswb_r2r (mm3, mm2);
    COPY_MMX (2*8, mm0, mm1, mm2);
    COPY_MMX (3*8, mm2, mm3, mm0);
    COPY_MMX (4*8, mm0, mm1, mm2);
    COPY_MMX (5*8, mm2, mm3, mm0);
    COPY_MMX (6*8, mm0, mm1, mm2);
    COPY_MMX (7*8, mm2, mm3, mm0);
    movq_r2m (mm2, *(dest+stride));
}

#define ADD_SSE2_2ROW(op, block0, block1)\
do {					\
    movq_m2r (*(dest), xmm1);		\
    movq_m2r (*(dest+stride), xmm2);	\
    punpcklbw_r2r (xmm0, xmm1);		\
    punpcklbw_r2r (xmm0, xmm2);		\
    paddsw_##op (block0, xmm1);		\
    paddsw_##op (block1, xmm2);		\
    packuswb_r2r (xmm1, xmm1);		\
    packuswb_r2r (xmm2, xmm2);		\
    movq_r2m (xmm1, *(dest));		\
    movq_r2m (xmm2, *(dest+stride));	\
    dest += 2*stride;			\
} while (0)

static void sse2_block_add (int16_t * const block, uint8_t * dest,
			    const int stride)
{
    pxor_r2r(xmm0, xmm0);
#if defined(__x86_64__)
    /* INPUT: block in xmm8 ... xmm15 */
    ADD_SSE2_2ROW(r2r, xmm8, xmm9);
    ADD_SSE2_2ROW(r2r, xmm10, xmm11);
    ADD_SSE2_2ROW(r2r, xmm12, xmm13);
    ADD_SSE2_2ROW(r2r, xmm14, xmm15);
#else
    ADD_SSE2_2ROW(m2r, *(block+0*8), *(block+1*8));
    ADD_SSE2_2ROW(m2r, *(block+2*8), *(block+3*8));
    ADD_SSE2_2ROW(m2r, *(block+4*8), *(block+5*8));
    ADD_SSE2_2ROW(m2r, *(block+6*8), *(block+7*8));
#endif
}

#define ADD_MMX(offset,r1,r2,r3,r4)	\
do {					\
    movq_m2r (*(dest+2*stride), r1);	\
    packuswb_r2r (r4, r3);		\
    movq_r2r (r1, r2);			\
    dest += stride;			\
    movq_r2m (r3, *dest);		\
    punpcklbw_r2r (mm0, r1);		\
    paddsw_m2r (*(block+offset), r1);	\
    punpckhbw_r2r (mm0, r2);		\
    paddsw_m2r (*(block+offset+4), r2);	\
} while (0)

static inline void block_add (int16_t * const block, uint8_t * dest,
			      const int stride)
{
    movq_m2r (*dest, mm1);
    pxor_r2r (mm0, mm0);
    movq_m2r (*(dest+stride), mm3);
    movq_r2r (mm1, mm2);
    punpcklbw_r2r (mm0, mm1);
    movq_r2r (mm3, mm4);
    paddsw_m2r (*(block+0*8), mm1);
    punpckhbw_r2r (mm0, mm2);
    paddsw_m2r (*(block+0*8+4), mm2);
    punpcklbw_r2r (mm0, mm3);
    paddsw_m2r (*(block+1*8), mm3);
    packuswb_r2r (mm2, mm1);
    punpckhbw_r2r (mm0, mm4);
    movq_r2m (mm1, *dest);
    paddsw_m2r (*(block+1*8+4), mm4);
    ADD_MMX (2*8, mm1, mm2, mm3, mm4);
    ADD_MMX (3*8, mm3, mm4, mm1, mm2);
    ADD_MMX (4*8, mm1, mm2, mm3, mm4);
    ADD_MMX (5*8, mm3, mm4, mm1, mm2);
    ADD_MMX (6*8, mm1, mm2, mm3, mm4);
    ADD_MMX (7*8, mm3, mm4, mm1, mm2);
    packuswb_r2r (mm4, mm3);
    movq_r2m (mm3, *(dest+stride));
}


static inline void sse2_block_zero (int16_t * const block)
{
    pxor_r2r (xmm0, xmm0);
    movdqa_r2m (xmm0, *(block+0*8));
    movdqa_r2m (xmm0, *(block+1*8));
    movdqa_r2m (xmm0, *(block+2*8));
    movdqa_r2m (xmm0, *(block+3*8));
    movdqa_r2m (xmm0, *(block+4*8));
    movdqa_r2m (xmm0, *(block+5*8));
    movdqa_r2m (xmm0, *(block+6*8));
    movdqa_r2m (xmm0, *(block+7*8));
}

static inline void block_zero (int16_t * const block)
{
    pxor_r2r (mm0, mm0);
    movq_r2m (mm0, *(block+0*4));
    movq_r2m (mm0, *(block+1*4));
    movq_r2m (mm0, *(block+2*4));
    movq_r2m (mm0, *(block+3*4));
    movq_r2m (mm0, *(block+4*4));
    movq_r2m (mm0, *(block+5*4));
    movq_r2m (mm0, *(block+6*4));
    movq_r2m (mm0, *(block+7*4));
    movq_r2m (mm0, *(block+8*4));
    movq_r2m (mm0, *(block+9*4));
    movq_r2m (mm0, *(block+10*4));
    movq_r2m (mm0, *(block+11*4));
    movq_r2m (mm0, *(block+12*4));
    movq_r2m (mm0, *(block+13*4));
    movq_r2m (mm0, *(block+14*4));
    movq_r2m (mm0, *(block+15*4));
}


#define CPU_MMXEXT 0
#define CPU_MMX 1

#define dup4(reg)			\
do {					\
    if (cpu != CPU_MMXEXT) {		\
	punpcklwd_r2r (reg, reg);	\
	punpckldq_r2r (reg, reg);	\
    } else				\
	pshufw_r2r (reg, reg, 0x00);	\
} while (0)

static inline void block_add_DC (int16_t * const block, uint8_t * dest,
				 const int stride, const int cpu)
{
    movd_v2r ((block[0] + 64) >> 7, mm0);
    pxor_r2r (mm1, mm1);
    movq_m2r (*dest, mm2);
    dup4 (mm0);
    psubsw_r2r (mm0, mm1);
    packuswb_r2r (mm0, mm0);
    paddusb_r2r (mm0, mm2);
    packuswb_r2r (mm1, mm1);
    movq_m2r (*(dest + stride), mm3);
    psubusb_r2r (mm1, mm2);
    block[0] = 0;
    paddusb_r2r (mm0, mm3);
    movq_r2m (mm2, *dest);
    psubusb_r2r (mm1, mm3);
    movq_m2r (*(dest + 2*stride), mm2);
    dest += stride;
    movq_r2m (mm3, *dest);
    paddusb_r2r (mm0, mm2);
    movq_m2r (*(dest + 2*stride), mm3);
    psubusb_r2r (mm1, mm2);
    dest += stride;
    paddusb_r2r (mm0, mm3);
    movq_r2m (mm2, *dest);
    psubusb_r2r (mm1, mm3);
    movq_m2r (*(dest + 2*stride), mm2);
    dest += stride;
    movq_r2m (mm3, *dest);
    paddusb_r2r (mm0, mm2);
    movq_m2r (*(dest + 2*stride), mm3);
    psubusb_r2r (mm1, mm2);
    dest += stride;
    paddusb_r2r (mm0, mm3);
    movq_r2m (mm2, *dest);
    psubusb_r2r (mm1, mm3);
    movq_m2r (*(dest + 2*stride), mm2);
    dest += stride;
    movq_r2m (mm3, *dest);
    paddusb_r2r (mm0, mm2);
    movq_m2r (*(dest + 2*stride), mm3);
    psubusb_r2r (mm1, mm2);
    block[63] = 0;
    paddusb_r2r (mm0, mm3);
    movq_r2m (mm2, *(dest + stride));
    psubusb_r2r (mm1, mm3);
    movq_r2m (mm3, *(dest + 2*stride));
}

void mpeg2_idct_copy_sse2 (int16_t * const block, uint8_t * const dest,
			   const int stride)
{
    sse2_idct (block);
    sse2_block_copy (block, dest, stride);
    sse2_block_zero (block);
}

void mpeg2_idct_add_sse2 (const int last, int16_t * const block,
			  uint8_t * const dest, const int stride)
{
    if (last != 129 || (block[0] & (7 << 4)) == (4 << 4)) {
	sse2_idct (block);
	sse2_block_add (block, dest, stride);
	sse2_block_zero (block);
    } else
	block_add_DC (block, dest, stride, CPU_MMXEXT);
}


declare_idct (mmxext_idct, mmxext_table,
	      mmxext_row_head, mmxext_row, mmxext_row_tail, mmxext_row_mid)

void mpeg2_idct_copy_mmxext (int16_t * const block, uint8_t * const dest,
			     const int stride)
{
    mmxext_idct (block);
    block_copy (block, dest, stride);
    block_zero (block);
}

void mpeg2_idct_add_mmxext (const int last, int16_t * const block,
			    uint8_t * const dest, const int stride)
{
    if (last != 129 || (block[0] & (7 << 4)) == (4 << 4)) {
	mmxext_idct (block);
	block_add (block, dest, stride);
	block_zero (block);
    } else
	block_add_DC (block, dest, stride, CPU_MMXEXT);
}


declare_idct (mmx_idct, mmx_table,
	      mmx_row_head, mmx_row, mmx_row_tail, mmx_row_mid)

void mpeg2_idct_copy_mmx (int16_t * const block, uint8_t * const dest,
			  const int stride)
{
    mmx_idct (block);
    block_copy (block, dest, stride);
    block_zero (block);
}

void mpeg2_idct_add_mmx (const int last, int16_t * const block,
			 uint8_t * const dest, const int stride)
{
    if (last != 129 || (block[0] & (7 << 4)) == (4 << 4)) {
	mmx_idct (block);
	block_add (block, dest, stride);
	block_zero (block);
    } else
	block_add_DC (block, dest, stride, CPU_MMX);
}


void mpeg2_idct_mmx_init (void)
{
    int i, j;

    /* the mmx/mmxext idct uses a reordered input, so we patch scan tables */

    for (i = 0; i < 64; i++) {
	j = mpeg2_scan_norm[i];
	mpeg2_scan_norm[i] = (j & 0x38) | ((j & 6) >> 1) | ((j & 1) << 2);
	j = mpeg2_scan_alt[i];
	mpeg2_scan_alt[i] = (j & 0x38) | ((j & 6) >> 1) | ((j & 1) << 2);
    }
}

#endif
