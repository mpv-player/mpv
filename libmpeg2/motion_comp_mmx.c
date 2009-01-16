/*
 * motion_comp_mmx.c
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

#define CPU_MMXEXT 0
#define CPU_3DNOW 1


/* MMX code - needs a rewrite */

/*
 * Motion Compensation frequently needs to average values using the
 * formula (x+y+1)>>1. Both MMXEXT and 3Dnow include one instruction
 * to compute this, but it's been left out of classic MMX.
 *
 * We need to be careful of overflows when doing this computation.
 * Rather than unpacking data to 16-bits, which reduces parallelism,
 * we use the following formulas:
 *
 * (x+y)>>1 == (x&y)+((x^y)>>1)
 * (x+y+1)>>1 == (x|y)-((x^y)>>1)
 */

/* some rounding constants */
static mmx_t mask1 = {0xfefefefefefefefeLL};
static mmx_t round4 = {0x0002000200020002LL};

/*
 * This code should probably be compiled with loop unrolling
 * (ie, -funroll-loops in gcc)becuase some of the loops
 * use a small static number of iterations. This was written
 * with the assumption the compiler knows best about when
 * unrolling will help
 */

static inline void mmx_zero_reg (void)
{
    /* load 0 into mm0 */
    pxor_r2r (mm0, mm0);
}

static inline void mmx_average_2_U8 (uint8_t * dest, const uint8_t * src1,
				     const uint8_t * src2)
{
    /* *dest = (*src1 + *src2 + 1)/ 2; */

    movq_m2r (*src1, mm1);	/* load 8 src1 bytes */
    movq_r2r (mm1, mm2);	/* copy 8 src1 bytes */

    movq_m2r (*src2, mm3);	/* load 8 src2 bytes */
    movq_r2r (mm3, mm4);	/* copy 8 src2 bytes */

    pxor_r2r (mm1, mm3);	/* xor src1 and src2 */
    pand_m2r (mask1, mm3);	/* mask lower bits */
    psrlq_i2r (1, mm3);		/* /2 */
    por_r2r (mm2, mm4);		/* or src1 and src2 */
    psubb_r2r (mm3, mm4);	/* subtract subresults */
    movq_r2m (mm4, *dest);	/* store result in dest */
}

static inline void mmx_interp_average_2_U8 (uint8_t * dest,
					    const uint8_t * src1,
					    const uint8_t * src2)
{
    /* *dest = (*dest + (*src1 + *src2 + 1)/ 2 + 1)/ 2; */

    movq_m2r (*dest, mm1);	/* load 8 dest bytes */
    movq_r2r (mm1, mm2);	/* copy 8 dest bytes */

    movq_m2r (*src1, mm3);	/* load 8 src1 bytes */
    movq_r2r (mm3, mm4);	/* copy 8 src1 bytes */

    movq_m2r (*src2, mm5);	/* load 8 src2 bytes */
    movq_r2r (mm5, mm6);	/* copy 8 src2 bytes */

    pxor_r2r (mm3, mm5);	/* xor src1 and src2 */
    pand_m2r (mask1, mm5);	/* mask lower bits */
    psrlq_i2r (1, mm5);		/* /2 */
    por_r2r (mm4, mm6);		/* or src1 and src2 */
    psubb_r2r (mm5, mm6);	/* subtract subresults */
    movq_r2r (mm6, mm5);	/* copy subresult */

    pxor_r2r (mm1, mm5);	/* xor srcavg and dest */
    pand_m2r (mask1, mm5);	/* mask lower bits */
    psrlq_i2r (1, mm5);		/* /2 */
    por_r2r (mm2, mm6);		/* or srcavg and dest */
    psubb_r2r (mm5, mm6);	/* subtract subresults */
    movq_r2m (mm6, *dest);	/* store result in dest */
}

static inline void mmx_average_4_U8 (uint8_t * dest, const uint8_t * src1,
				     const uint8_t * src2,
				     const uint8_t * src3,
				     const uint8_t * src4)
{
    /* *dest = (*src1 + *src2 + *src3 + *src4 + 2)/ 4; */

    movq_m2r (*src1, mm1);	/* load 8 src1 bytes */
    movq_r2r (mm1, mm2);	/* copy 8 src1 bytes */

    punpcklbw_r2r (mm0, mm1);	/* unpack low src1 bytes */
    punpckhbw_r2r (mm0, mm2);	/* unpack high src1 bytes */

    movq_m2r (*src2, mm3);	/* load 8 src2 bytes */
    movq_r2r (mm3, mm4);	/* copy 8 src2 bytes */

    punpcklbw_r2r (mm0, mm3);	/* unpack low src2 bytes */
    punpckhbw_r2r (mm0, mm4);	/* unpack high src2 bytes */

    paddw_r2r (mm3, mm1);	/* add lows */
    paddw_r2r (mm4, mm2);	/* add highs */

    /* now have partials in mm1 and mm2 */

    movq_m2r (*src3, mm3);	/* load 8 src3 bytes */
    movq_r2r (mm3, mm4);	/* copy 8 src3 bytes */

    punpcklbw_r2r (mm0, mm3);	/* unpack low src3 bytes */
    punpckhbw_r2r (mm0, mm4);	/* unpack high src3 bytes */

    paddw_r2r (mm3, mm1);	/* add lows */
    paddw_r2r (mm4, mm2);	/* add highs */

    movq_m2r (*src4, mm5);	/* load 8 src4 bytes */
    movq_r2r (mm5, mm6);	/* copy 8 src4 bytes */

    punpcklbw_r2r (mm0, mm5);	/* unpack low src4 bytes */
    punpckhbw_r2r (mm0, mm6);	/* unpack high src4 bytes */

    paddw_r2r (mm5, mm1);	/* add lows */
    paddw_r2r (mm6, mm2);	/* add highs */

    /* now have subtotal in mm1 and mm2 */

    paddw_m2r (round4, mm1);
    psraw_i2r (2, mm1);		/* /4 */
    paddw_m2r (round4, mm2);
    psraw_i2r (2, mm2);		/* /4 */

    packuswb_r2r (mm2, mm1);	/* pack (w/ saturation) */
    movq_r2m (mm1, *dest);	/* store result in dest */
}

static inline void mmx_interp_average_4_U8 (uint8_t * dest,
					    const uint8_t * src1,
					    const uint8_t * src2,
					    const uint8_t * src3,
					    const uint8_t * src4)
{
    /* *dest = (*dest + (*src1 + *src2 + *src3 + *src4 + 2)/ 4 + 1)/ 2; */

    movq_m2r (*src1, mm1);	/* load 8 src1 bytes */
    movq_r2r (mm1, mm2);	/* copy 8 src1 bytes */

    punpcklbw_r2r (mm0, mm1);	/* unpack low src1 bytes */
    punpckhbw_r2r (mm0, mm2);	/* unpack high src1 bytes */

    movq_m2r (*src2, mm3);	/* load 8 src2 bytes */
    movq_r2r (mm3, mm4);	/* copy 8 src2 bytes */

    punpcklbw_r2r (mm0, mm3);	/* unpack low src2 bytes */
    punpckhbw_r2r (mm0, mm4);	/* unpack high src2 bytes */

    paddw_r2r (mm3, mm1);	/* add lows */
    paddw_r2r (mm4, mm2);	/* add highs */

    /* now have partials in mm1 and mm2 */

    movq_m2r (*src3, mm3);	/* load 8 src3 bytes */
    movq_r2r (mm3, mm4);	/* copy 8 src3 bytes */

    punpcklbw_r2r (mm0, mm3);	/* unpack low src3 bytes */
    punpckhbw_r2r (mm0, mm4);	/* unpack high src3 bytes */

    paddw_r2r (mm3, mm1);	/* add lows */
    paddw_r2r (mm4, mm2);	/* add highs */

    movq_m2r (*src4, mm5);	/* load 8 src4 bytes */
    movq_r2r (mm5, mm6);	/* copy 8 src4 bytes */

    punpcklbw_r2r (mm0, mm5);	/* unpack low src4 bytes */
    punpckhbw_r2r (mm0, mm6);	/* unpack high src4 bytes */

    paddw_r2r (mm5, mm1);	/* add lows */
    paddw_r2r (mm6, mm2);	/* add highs */

    paddw_m2r (round4, mm1);
    psraw_i2r (2, mm1);		/* /4 */
    paddw_m2r (round4, mm2);
    psraw_i2r (2, mm2);		/* /4 */

    /* now have subtotal/4 in mm1 and mm2 */

    movq_m2r (*dest, mm3);	/* load 8 dest bytes */
    movq_r2r (mm3, mm4);	/* copy 8 dest bytes */

    packuswb_r2r (mm2, mm1);	/* pack (w/ saturation) */
    movq_r2r (mm1,mm2);		/* copy subresult */

    pxor_r2r (mm1, mm3);	/* xor srcavg and dest */
    pand_m2r (mask1, mm3);	/* mask lower bits */
    psrlq_i2r (1, mm3);		/* /2 */
    por_r2r (mm2, mm4);		/* or srcavg and dest */
    psubb_r2r (mm3, mm4);	/* subtract subresults */
    movq_r2m (mm4, *dest);	/* store result in dest */
}

/*-----------------------------------------------------------------------*/

static inline void MC_avg_mmx (const int width, int height, uint8_t * dest,
			       const uint8_t * ref, const int stride)
{
    mmx_zero_reg ();

    do {
	mmx_average_2_U8 (dest, dest, ref);

	if (width == 16)
	    mmx_average_2_U8 (dest+8, dest+8, ref+8);

	dest += stride;
	ref += stride;
    } while (--height);
}

static void MC_avg_o_16_mmx (uint8_t * dest, const uint8_t * ref,
			     int stride, int height)
{
    MC_avg_mmx (16, height, dest, ref, stride);
}

static void MC_avg_o_8_mmx (uint8_t * dest, const uint8_t * ref,
			    int stride, int height)
{
    MC_avg_mmx (8, height, dest, ref, stride);
}

/*-----------------------------------------------------------------------*/

static inline void MC_put_mmx (const int width, int height, uint8_t * dest,
			       const uint8_t * ref, const int stride)
{
    mmx_zero_reg ();

    do {
	movq_m2r (* ref, mm1);	/* load 8 ref bytes */
	movq_r2m (mm1,* dest);	/* store 8 bytes at curr */

	if (width == 16)
	    {
		movq_m2r (* (ref+8), mm1);	/* load 8 ref bytes */
		movq_r2m (mm1,* (dest+8));	/* store 8 bytes at curr */
	    }

	dest += stride;
	ref += stride;
    } while (--height);
}

static void MC_put_o_16_mmx (uint8_t * dest, const uint8_t * ref,
			     int stride, int height)
{
    MC_put_mmx (16, height, dest, ref, stride);
}

static void MC_put_o_8_mmx (uint8_t * dest, const uint8_t * ref,
			    int stride, int height)
{
    MC_put_mmx (8, height, dest, ref, stride);
}

/*-----------------------------------------------------------------------*/

/* Half pixel interpolation in the x direction */
static inline void MC_avg_x_mmx (const int width, int height, uint8_t * dest,
				 const uint8_t * ref, const int stride)
{
    mmx_zero_reg ();

    do {
	mmx_interp_average_2_U8 (dest, ref, ref+1);

	if (width == 16)
	    mmx_interp_average_2_U8 (dest+8, ref+8, ref+9);

	dest += stride;
	ref += stride;
    } while (--height);
}

static void MC_avg_x_16_mmx (uint8_t * dest, const uint8_t * ref,
			     int stride, int height)
{
    MC_avg_x_mmx (16, height, dest, ref, stride);
}

static void MC_avg_x_8_mmx (uint8_t * dest, const uint8_t * ref,
			    int stride, int height)
{
    MC_avg_x_mmx (8, height, dest, ref, stride);
}

/*-----------------------------------------------------------------------*/

static inline void MC_put_x_mmx (const int width, int height, uint8_t * dest,
				 const uint8_t * ref, const int stride)
{
    mmx_zero_reg ();

    do {
	mmx_average_2_U8 (dest, ref, ref+1);

	if (width == 16)
	    mmx_average_2_U8 (dest+8, ref+8, ref+9);

	dest += stride;
	ref += stride;
    } while (--height);
}

static void MC_put_x_16_mmx (uint8_t * dest, const uint8_t * ref,
			     int stride, int height)
{
    MC_put_x_mmx (16, height, dest, ref, stride);
}

static void MC_put_x_8_mmx (uint8_t * dest, const uint8_t * ref,
			    int stride, int height)
{
    MC_put_x_mmx (8, height, dest, ref, stride);
}

/*-----------------------------------------------------------------------*/

static inline void MC_avg_xy_mmx (const int width, int height, uint8_t * dest,
				  const uint8_t * ref, const int stride)
{
    const uint8_t * ref_next = ref + stride;

    mmx_zero_reg ();

    do {
	mmx_interp_average_4_U8 (dest, ref, ref+1, ref_next, ref_next+1);

	if (width == 16)
	    mmx_interp_average_4_U8 (dest+8, ref+8, ref+9,
				     ref_next+8, ref_next+9);

	dest += stride;
	ref += stride;
	ref_next += stride;
    } while (--height);
}

static void MC_avg_xy_16_mmx (uint8_t * dest, const uint8_t * ref,
			      int stride, int height)
{
    MC_avg_xy_mmx (16, height, dest, ref, stride);
}

static void MC_avg_xy_8_mmx (uint8_t * dest, const uint8_t * ref,
			     int stride, int height)
{
    MC_avg_xy_mmx (8, height, dest, ref, stride);
}

/*-----------------------------------------------------------------------*/

static inline void MC_put_xy_mmx (const int width, int height, uint8_t * dest,
				  const uint8_t * ref, const int stride)
{
    const uint8_t * ref_next = ref + stride;

    mmx_zero_reg ();

    do {
	mmx_average_4_U8 (dest, ref, ref+1, ref_next, ref_next+1);

	if (width == 16)
	    mmx_average_4_U8 (dest+8, ref+8, ref+9, ref_next+8, ref_next+9);

	dest += stride;
	ref += stride;
	ref_next += stride;
    } while (--height);
}

static void MC_put_xy_16_mmx (uint8_t * dest, const uint8_t * ref,
			      int stride, int height)
{
    MC_put_xy_mmx (16, height, dest, ref, stride);
}

static void MC_put_xy_8_mmx (uint8_t * dest, const uint8_t * ref,
			     int stride, int height)
{
    MC_put_xy_mmx (8, height, dest, ref, stride);
}

/*-----------------------------------------------------------------------*/

static inline void MC_avg_y_mmx (const int width, int height, uint8_t * dest,
				 const uint8_t * ref, const int stride)
{
    const uint8_t * ref_next = ref + stride;

    mmx_zero_reg ();

    do {
	mmx_interp_average_2_U8 (dest, ref, ref_next);

	if (width == 16)
	    mmx_interp_average_2_U8 (dest+8, ref+8, ref_next+8);

	dest += stride;
	ref += stride;
	ref_next += stride;
    } while (--height);
}

static void MC_avg_y_16_mmx (uint8_t * dest, const uint8_t * ref,
			     int stride, int height)
{
    MC_avg_y_mmx (16, height, dest, ref, stride);
}

static void MC_avg_y_8_mmx (uint8_t * dest, const uint8_t * ref,
			    int stride, int height)
{
    MC_avg_y_mmx (8, height, dest, ref, stride);
}

/*-----------------------------------------------------------------------*/

static inline void MC_put_y_mmx (const int width, int height, uint8_t * dest,
				 const uint8_t * ref, const int stride)
{
    const uint8_t * ref_next = ref + stride;

    mmx_zero_reg ();

    do {
	mmx_average_2_U8 (dest, ref, ref_next);

	if (width == 16)
	    mmx_average_2_U8 (dest+8, ref+8, ref_next+8);

	dest += stride;
	ref += stride;
	ref_next += stride;
    } while (--height);
}

static void MC_put_y_16_mmx (uint8_t * dest, const uint8_t * ref,
			     int stride, int height)
{
    MC_put_y_mmx (16, height, dest, ref, stride);
}

static void MC_put_y_8_mmx (uint8_t * dest, const uint8_t * ref,
			    int stride, int height)
{
    MC_put_y_mmx (8, height, dest, ref, stride);
}


MPEG2_MC_EXTERN (mmx)







/* CPU_MMXEXT/CPU_3DNOW adaptation layer */

#define pavg_r2r(src,dest)		\
do {					\
    if (cpu == CPU_MMXEXT)		\
	pavgb_r2r (src, dest);		\
    else				\
	pavgusb_r2r (src, dest);	\
} while (0)

#define pavg_m2r(src,dest)		\
do {					\
    if (cpu == CPU_MMXEXT)		\
	pavgb_m2r (src, dest);		\
    else				\
	pavgusb_m2r (src, dest);	\
} while (0)


/* CPU_MMXEXT code */


static inline void MC_put1_8 (int height, uint8_t * dest, const uint8_t * ref,
			      const int stride)
{
    do {
	movq_m2r (*ref, mm0);
	movq_r2m (mm0, *dest);
	ref += stride;
	dest += stride;
    } while (--height);
}

static inline void MC_put1_16 (int height, uint8_t * dest, const uint8_t * ref,
			       const int stride)
{
    do {
	movq_m2r (*ref, mm0);
	movq_m2r (*(ref+8), mm1);
	ref += stride;
	movq_r2m (mm0, *dest);
	movq_r2m (mm1, *(dest+8));
	dest += stride;
    } while (--height);
}

static inline void MC_avg1_8 (int height, uint8_t * dest, const uint8_t * ref,
			      const int stride, const int cpu)
{
    do {
	movq_m2r (*ref, mm0);
	pavg_m2r (*dest, mm0);
	ref += stride;
	movq_r2m (mm0, *dest);
	dest += stride;
    } while (--height);
}

static inline void MC_avg1_16 (int height, uint8_t * dest, const uint8_t * ref,
			       const int stride, const int cpu)
{
    do {
	movq_m2r (*ref, mm0);
	movq_m2r (*(ref+8), mm1);
	pavg_m2r (*dest, mm0);
	pavg_m2r (*(dest+8), mm1);
	movq_r2m (mm0, *dest);
	ref += stride;
	movq_r2m (mm1, *(dest+8));
	dest += stride;
    } while (--height);
}

static inline void MC_put2_8 (int height, uint8_t * dest, const uint8_t * ref,
			      const int stride, const int offset,
			      const int cpu)
{
    do {
	movq_m2r (*ref, mm0);
	pavg_m2r (*(ref+offset), mm0);
	ref += stride;
	movq_r2m (mm0, *dest);
	dest += stride;
    } while (--height);
}

static inline void MC_put2_16 (int height, uint8_t * dest, const uint8_t * ref,
			       const int stride, const int offset,
			       const int cpu)
{
    do {
	movq_m2r (*ref, mm0);
	movq_m2r (*(ref+8), mm1);
	pavg_m2r (*(ref+offset), mm0);
	pavg_m2r (*(ref+offset+8), mm1);
	movq_r2m (mm0, *dest);
	ref += stride;
	movq_r2m (mm1, *(dest+8));
	dest += stride;
    } while (--height);
}

static inline void MC_avg2_8 (int height, uint8_t * dest, const uint8_t * ref,
			      const int stride, const int offset,
			      const int cpu)
{
    do {
	movq_m2r (*ref, mm0);
	pavg_m2r (*(ref+offset), mm0);
	pavg_m2r (*dest, mm0);
	ref += stride;
	movq_r2m (mm0, *dest);
	dest += stride;
    } while (--height);
}

static inline void MC_avg2_16 (int height, uint8_t * dest, const uint8_t * ref,
			       const int stride, const int offset,
			       const int cpu)
{
    do {
	movq_m2r (*ref, mm0);
	movq_m2r (*(ref+8), mm1);
	pavg_m2r (*(ref+offset), mm0);
	pavg_m2r (*(ref+offset+8), mm1);
	pavg_m2r (*dest, mm0);
	pavg_m2r (*(dest+8), mm1);
	ref += stride;
	movq_r2m (mm0, *dest);
	movq_r2m (mm1, *(dest+8));
	dest += stride;
    } while (--height);
}

static mmx_t mask_one = {0x0101010101010101LL};

static inline void MC_put4_8 (int height, uint8_t * dest, const uint8_t * ref,
			      const int stride, const int cpu)
{
    movq_m2r (*ref, mm0);
    movq_m2r (*(ref+1), mm1);
    movq_r2r (mm0, mm7);
    pxor_r2r (mm1, mm7);
    pavg_r2r (mm1, mm0);
    ref += stride;

    do {
	movq_m2r (*ref, mm2);
	movq_r2r (mm0, mm5);

	movq_m2r (*(ref+1), mm3);
	movq_r2r (mm2, mm6);

	pxor_r2r (mm3, mm6);
	pavg_r2r (mm3, mm2);

	por_r2r (mm6, mm7);
	pxor_r2r (mm2, mm5);

	pand_r2r (mm5, mm7);
	pavg_r2r (mm2, mm0);

	pand_m2r (mask_one, mm7);

	psubusb_r2r (mm7, mm0);

	ref += stride;
	movq_r2m (mm0, *dest);
	dest += stride;

	movq_r2r (mm6, mm7);	/* unroll ! */
	movq_r2r (mm2, mm0);	/* unroll ! */
    } while (--height);
}

static inline void MC_put4_16 (int height, uint8_t * dest, const uint8_t * ref,
			       const int stride, const int cpu)
{
    do {
	movq_m2r (*ref, mm0);
	movq_m2r (*(ref+stride+1), mm1);
	movq_r2r (mm0, mm7);
	movq_m2r (*(ref+1), mm2);
	pxor_r2r (mm1, mm7);
	movq_m2r (*(ref+stride), mm3);
	movq_r2r (mm2, mm6);
	pxor_r2r (mm3, mm6);
	pavg_r2r (mm1, mm0);
	pavg_r2r (mm3, mm2);
	por_r2r (mm6, mm7);
	movq_r2r (mm0, mm6);
	pxor_r2r (mm2, mm6);
	pand_r2r (mm6, mm7);
	pand_m2r (mask_one, mm7);
	pavg_r2r (mm2, mm0);
	psubusb_r2r (mm7, mm0);
	movq_r2m (mm0, *dest);

	movq_m2r (*(ref+8), mm0);
	movq_m2r (*(ref+stride+9), mm1);
	movq_r2r (mm0, mm7);
	movq_m2r (*(ref+9), mm2);
	pxor_r2r (mm1, mm7);
	movq_m2r (*(ref+stride+8), mm3);
	movq_r2r (mm2, mm6);
	pxor_r2r (mm3, mm6);
	pavg_r2r (mm1, mm0);
	pavg_r2r (mm3, mm2);
	por_r2r (mm6, mm7);
	movq_r2r (mm0, mm6);
	pxor_r2r (mm2, mm6);
	pand_r2r (mm6, mm7);
	pand_m2r (mask_one, mm7);
	pavg_r2r (mm2, mm0);
	psubusb_r2r (mm7, mm0);
	ref += stride;
	movq_r2m (mm0, *(dest+8));
	dest += stride;
    } while (--height);
}

static inline void MC_avg4_8 (int height, uint8_t * dest, const uint8_t * ref,
			      const int stride, const int cpu)
{
    do {
	movq_m2r (*ref, mm0);
	movq_m2r (*(ref+stride+1), mm1);
	movq_r2r (mm0, mm7);
	movq_m2r (*(ref+1), mm2);
	pxor_r2r (mm1, mm7);
	movq_m2r (*(ref+stride), mm3);
	movq_r2r (mm2, mm6);
	pxor_r2r (mm3, mm6);
	pavg_r2r (mm1, mm0);
	pavg_r2r (mm3, mm2);
	por_r2r (mm6, mm7);
	movq_r2r (mm0, mm6);
	pxor_r2r (mm2, mm6);
	pand_r2r (mm6, mm7);
	pand_m2r (mask_one, mm7);
	pavg_r2r (mm2, mm0);
	psubusb_r2r (mm7, mm0);
	movq_m2r (*dest, mm1);
	pavg_r2r (mm1, mm0);
	ref += stride;
	movq_r2m (mm0, *dest);
	dest += stride;
    } while (--height);
}

static inline void MC_avg4_16 (int height, uint8_t * dest, const uint8_t * ref,
			       const int stride, const int cpu)
{
    do {
	movq_m2r (*ref, mm0);
	movq_m2r (*(ref+stride+1), mm1);
	movq_r2r (mm0, mm7);
	movq_m2r (*(ref+1), mm2);
	pxor_r2r (mm1, mm7);
	movq_m2r (*(ref+stride), mm3);
	movq_r2r (mm2, mm6);
	pxor_r2r (mm3, mm6);
	pavg_r2r (mm1, mm0);
	pavg_r2r (mm3, mm2);
	por_r2r (mm6, mm7);
	movq_r2r (mm0, mm6);
	pxor_r2r (mm2, mm6);
	pand_r2r (mm6, mm7);
	pand_m2r (mask_one, mm7);
	pavg_r2r (mm2, mm0);
	psubusb_r2r (mm7, mm0);
	movq_m2r (*dest, mm1);
	pavg_r2r (mm1, mm0);
	movq_r2m (mm0, *dest);

	movq_m2r (*(ref+8), mm0);
	movq_m2r (*(ref+stride+9), mm1);
	movq_r2r (mm0, mm7);
	movq_m2r (*(ref+9), mm2);
	pxor_r2r (mm1, mm7);
	movq_m2r (*(ref+stride+8), mm3);
	movq_r2r (mm2, mm6);
	pxor_r2r (mm3, mm6);
	pavg_r2r (mm1, mm0);
	pavg_r2r (mm3, mm2);
	por_r2r (mm6, mm7);
	movq_r2r (mm0, mm6);
	pxor_r2r (mm2, mm6);
	pand_r2r (mm6, mm7);
	pand_m2r (mask_one, mm7);
	pavg_r2r (mm2, mm0);
	psubusb_r2r (mm7, mm0);
	movq_m2r (*(dest+8), mm1);
	pavg_r2r (mm1, mm0);
	ref += stride;
	movq_r2m (mm0, *(dest+8));
	dest += stride;
    } while (--height);
}

static void MC_avg_o_16_mmxext (uint8_t * dest, const uint8_t * ref,
				int stride, int height)
{
    MC_avg1_16 (height, dest, ref, stride, CPU_MMXEXT);
}

static void MC_avg_o_8_mmxext (uint8_t * dest, const uint8_t * ref,
			       int stride, int height)
{
    MC_avg1_8 (height, dest, ref, stride, CPU_MMXEXT);
}

static void MC_put_o_16_mmxext (uint8_t * dest, const uint8_t * ref,
				int stride, int height)
{
    MC_put1_16 (height, dest, ref, stride);
}

static void MC_put_o_8_mmxext (uint8_t * dest, const uint8_t * ref,
			       int stride, int height)
{
    MC_put1_8 (height, dest, ref, stride);
}

static void MC_avg_x_16_mmxext (uint8_t * dest, const uint8_t * ref,
				int stride, int height)
{
    MC_avg2_16 (height, dest, ref, stride, 1, CPU_MMXEXT);
}

static void MC_avg_x_8_mmxext (uint8_t * dest, const uint8_t * ref,
			       int stride, int height)
{
    MC_avg2_8 (height, dest, ref, stride, 1, CPU_MMXEXT);
}

static void MC_put_x_16_mmxext (uint8_t * dest, const uint8_t * ref,
				int stride, int height)
{
    MC_put2_16 (height, dest, ref, stride, 1, CPU_MMXEXT);
}

static void MC_put_x_8_mmxext (uint8_t * dest, const uint8_t * ref,
			       int stride, int height)
{
    MC_put2_8 (height, dest, ref, stride, 1, CPU_MMXEXT);
}

static void MC_avg_y_16_mmxext (uint8_t * dest, const uint8_t * ref,
				int stride, int height)
{
    MC_avg2_16 (height, dest, ref, stride, stride, CPU_MMXEXT);
}

static void MC_avg_y_8_mmxext (uint8_t * dest, const uint8_t * ref,
			       int stride, int height)
{
    MC_avg2_8 (height, dest, ref, stride, stride, CPU_MMXEXT);
}

static void MC_put_y_16_mmxext (uint8_t * dest, const uint8_t * ref,
				int stride, int height)
{
    MC_put2_16 (height, dest, ref, stride, stride, CPU_MMXEXT);
}

static void MC_put_y_8_mmxext (uint8_t * dest, const uint8_t * ref,
			       int stride, int height)
{
    MC_put2_8 (height, dest, ref, stride, stride, CPU_MMXEXT);
}

static void MC_avg_xy_16_mmxext (uint8_t * dest, const uint8_t * ref,
				 int stride, int height)
{
    MC_avg4_16 (height, dest, ref, stride, CPU_MMXEXT);
}

static void MC_avg_xy_8_mmxext (uint8_t * dest, const uint8_t * ref,
				int stride, int height)
{
    MC_avg4_8 (height, dest, ref, stride, CPU_MMXEXT);
}

static void MC_put_xy_16_mmxext (uint8_t * dest, const uint8_t * ref,
				 int stride, int height)
{
    MC_put4_16 (height, dest, ref, stride, CPU_MMXEXT);
}

static void MC_put_xy_8_mmxext (uint8_t * dest, const uint8_t * ref,
				int stride, int height)
{
    MC_put4_8 (height, dest, ref, stride, CPU_MMXEXT);
}


MPEG2_MC_EXTERN (mmxext)



static void MC_avg_o_16_3dnow (uint8_t * dest, const uint8_t * ref,
			       int stride, int height)
{
    MC_avg1_16 (height, dest, ref, stride, CPU_3DNOW);
}

static void MC_avg_o_8_3dnow (uint8_t * dest, const uint8_t * ref,
			      int stride, int height)
{
    MC_avg1_8 (height, dest, ref, stride, CPU_3DNOW);
}

static void MC_put_o_16_3dnow (uint8_t * dest, const uint8_t * ref,
			       int stride, int height)
{
    MC_put1_16 (height, dest, ref, stride);
}

static void MC_put_o_8_3dnow (uint8_t * dest, const uint8_t * ref,
			      int stride, int height)
{
    MC_put1_8 (height, dest, ref, stride);
}

static void MC_avg_x_16_3dnow (uint8_t * dest, const uint8_t * ref,
			       int stride, int height)
{
    MC_avg2_16 (height, dest, ref, stride, 1, CPU_3DNOW);
}

static void MC_avg_x_8_3dnow (uint8_t * dest, const uint8_t * ref,
			      int stride, int height)
{
    MC_avg2_8 (height, dest, ref, stride, 1, CPU_3DNOW);
}

static void MC_put_x_16_3dnow (uint8_t * dest, const uint8_t * ref,
			       int stride, int height)
{
    MC_put2_16 (height, dest, ref, stride, 1, CPU_3DNOW);
}

static void MC_put_x_8_3dnow (uint8_t * dest, const uint8_t * ref,
			      int stride, int height)
{
    MC_put2_8 (height, dest, ref, stride, 1, CPU_3DNOW);
}

static void MC_avg_y_16_3dnow (uint8_t * dest, const uint8_t * ref,
			       int stride, int height)
{
    MC_avg2_16 (height, dest, ref, stride, stride, CPU_3DNOW);
}

static void MC_avg_y_8_3dnow (uint8_t * dest, const uint8_t * ref,
			      int stride, int height)
{
    MC_avg2_8 (height, dest, ref, stride, stride, CPU_3DNOW);
}

static void MC_put_y_16_3dnow (uint8_t * dest, const uint8_t * ref,
			       int stride, int height)
{
    MC_put2_16 (height, dest, ref, stride, stride, CPU_3DNOW);
}

static void MC_put_y_8_3dnow (uint8_t * dest, const uint8_t * ref,
			      int stride, int height)
{
    MC_put2_8 (height, dest, ref, stride, stride, CPU_3DNOW);
}

static void MC_avg_xy_16_3dnow (uint8_t * dest, const uint8_t * ref,
				int stride, int height)
{
    MC_avg4_16 (height, dest, ref, stride, CPU_3DNOW);
}

static void MC_avg_xy_8_3dnow (uint8_t * dest, const uint8_t * ref,
			       int stride, int height)
{
    MC_avg4_8 (height, dest, ref, stride, CPU_3DNOW);
}

static void MC_put_xy_16_3dnow (uint8_t * dest, const uint8_t * ref,
				int stride, int height)
{
    MC_put4_16 (height, dest, ref, stride, CPU_3DNOW);
}

static void MC_put_xy_8_3dnow (uint8_t * dest, const uint8_t * ref,
			       int stride, int height)
{
    MC_put4_8 (height, dest, ref, stride, CPU_3DNOW);
}


MPEG2_MC_EXTERN (3dnow)

#endif
