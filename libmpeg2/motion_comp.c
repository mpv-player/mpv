/*
 * motion_comp.c
 * Copyright (C) 1999-2001 Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
 *
 * This file is part of mpeg2dec, a free MPEG-2 video stream decoder.
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

#include <stdio.h>
#include <inttypes.h>

#include "mpeg2_internal.h"
#include "mm_accel.h"

mc_functions_t mc_functions;

void motion_comp_init (void)
{

#ifdef ARCH_X86
    if (config.flags & MM_ACCEL_X86_MMXEXT) {
	printf ("libmpeg2: Using MMXEXT for motion compensation\n");
	mc_functions = mc_functions_mmxext;
    } else if (config.flags & MM_ACCEL_X86_3DNOW) {
	printf ("libmpeg2: Using 3DNOW for motion compensation\n");
	mc_functions = mc_functions_3dnow;
    } else if (config.flags & MM_ACCEL_X86_MMX) {
	printf ("libmpeg2: Using MMX for motion compensation\n");
	mc_functions = mc_functions_mmx;
    } else
#endif
#ifdef LIBMPEG2_MLIB
    if (config.flags & MM_ACCEL_MLIB) {
	printf ("libmpeg2: Using mlib for motion compensation\n");
	mc_functions = mc_functions_mlib;
    } else
#endif
    {
	printf ("libmpeg2: No accelerated motion compensation found\n");
	mc_functions = mc_functions_c;
    }
}

#define avg2(a,b) ((a+b+1)>>1)
#define avg4(a,b,c,d) ((a+b+c+d+2)>>2)

#define predict_(i) (ref[i])
#define predict_x(i) (avg2 (ref[i], ref[i+1]))
#define predict_y(i) (avg2 (ref[i], (ref+stride)[i]))
#define predict_xy(i) (avg4 (ref[i], ref[i+1], (ref+stride)[i], (ref+stride)[i+1]))

#define put(predictor,i) dest[i] = predictor (i)
#define avg(predictor,i) dest[i] = avg2 (predictor (i), dest[i])

/* mc function template */

#define MC_FUNC(op,xy)						\
static void MC_##op##_##xy##16_c (uint8_t * dest, uint8_t * ref,\
				 int stride, int height)	\
{								\
    do {							\
	op (predict_##xy, 0);					\
	op (predict_##xy, 1);					\
	op (predict_##xy, 2);					\
	op (predict_##xy, 3);					\
	op (predict_##xy, 4);					\
	op (predict_##xy, 5);					\
	op (predict_##xy, 6);					\
	op (predict_##xy, 7);					\
	op (predict_##xy, 8);					\
	op (predict_##xy, 9);					\
	op (predict_##xy, 10);					\
	op (predict_##xy, 11);					\
	op (predict_##xy, 12);					\
	op (predict_##xy, 13);					\
	op (predict_##xy, 14);					\
	op (predict_##xy, 15);					\
	ref += stride;						\
	dest += stride;						\
    } while (--height);						\
}								\
static void MC_##op##_##xy##8_c (uint8_t * dest, uint8_t * ref,	\
				int stride, int height)		\
{								\
    do {							\
	op (predict_##xy, 0);					\
	op (predict_##xy, 1);					\
	op (predict_##xy, 2);					\
	op (predict_##xy, 3);					\
	op (predict_##xy, 4);					\
	op (predict_##xy, 5);					\
	op (predict_##xy, 6);					\
	op (predict_##xy, 7);					\
	ref += stride;						\
	dest += stride;						\
    } while (--height);						\
}

/* definitions of the actual mc functions */

MC_FUNC (put,)
MC_FUNC (avg,)
MC_FUNC (put,x)
MC_FUNC (avg,x)
MC_FUNC (put,y)
MC_FUNC (avg,y)
MC_FUNC (put,xy)
MC_FUNC (avg,xy)

MOTION_COMP_EXTERN (c)
