/*
 * motion_comp_altivec.c
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

#ifdef ARCH_PPC

#ifdef HAVE_ALTIVEC_H
#include <altivec.h>
#endif
#include <inttypes.h>

#include "mpeg2.h"
#include "attributes.h"
#include "mpeg2_internal.h"

typedef vector signed char vector_s8_t;
typedef vector unsigned char vector_u8_t;
typedef vector signed short vector_s16_t;
typedef vector unsigned short vector_u16_t;
typedef vector signed int vector_s32_t;
typedef vector unsigned int vector_u32_t;

#ifndef COFFEE_BREAK	/* Workarounds for gcc suckage */

static inline vector_u8_t my_vec_ld (int const A, const uint8_t * const B)
{
    return vec_ld (A, (uint8_t *)B);
}
#undef vec_ld
#define vec_ld my_vec_ld

static inline vector_u8_t my_vec_and (vector_u8_t const A, vector_u8_t const B)
{
    return vec_and (A, B);
}
#undef vec_and
#define vec_and my_vec_and

static inline vector_u8_t my_vec_avg (vector_u8_t const A, vector_u8_t const B)
{
    return vec_avg (A, B);
}
#undef vec_avg
#define vec_avg my_vec_avg

#endif

static void MC_put_o_16_altivec (uint8_t * dest, const uint8_t * ref,
				 const int stride, int height)
{
    vector_u8_t perm, ref0, ref1, tmp;

    perm = vec_lvsl (0, ref);

    height = (height >> 1) - 1;

    ref0 = vec_ld (0, ref);
    ref1 = vec_ld (15, ref);
    ref += stride;
    tmp = vec_perm (ref0, ref1, perm);

    do {
	ref0 = vec_ld (0, ref);
	ref1 = vec_ld (15, ref);
	ref += stride;
	vec_st (tmp, 0, dest);
	tmp = vec_perm (ref0, ref1, perm);

	ref0 = vec_ld (0, ref);
	ref1 = vec_ld (15, ref);
	ref += stride;
	vec_st (tmp, stride, dest);
	dest += 2*stride;
	tmp = vec_perm (ref0, ref1, perm);
    } while (--height);

    ref0 = vec_ld (0, ref);
    ref1 = vec_ld (15, ref);
    vec_st (tmp, 0, dest);
    tmp = vec_perm (ref0, ref1, perm);
    vec_st (tmp, stride, dest);
}

static void MC_put_o_8_altivec (uint8_t * dest, const uint8_t * ref,
				const int stride, int height)
{
    vector_u8_t perm0, perm1, tmp0, tmp1, ref0, ref1;

    tmp0 = vec_lvsl (0, ref);
    tmp0 = vec_mergeh (tmp0, tmp0);
    perm0 = vec_pack ((vector_u16_t)tmp0, (vector_u16_t)tmp0);
    tmp1 = vec_lvsl (stride, ref);
    tmp1 = vec_mergeh (tmp1, tmp1);
    perm1 = vec_pack ((vector_u16_t)tmp1, (vector_u16_t)tmp1);

    height = (height >> 1) - 1;

    ref0 = vec_ld (0, ref);
    ref1 = vec_ld (7, ref);
    ref += stride;
    tmp0 = vec_perm (ref0, ref1, perm0);

    do {
	ref0 = vec_ld (0, ref);
	ref1 = vec_ld (7, ref);
	ref += stride;
	vec_ste ((vector_u32_t)tmp0, 0, (unsigned int *)dest);
	vec_ste ((vector_u32_t)tmp0, 4, (unsigned int *)dest);
	dest += stride;
	tmp1 = vec_perm (ref0, ref1, perm1);

	ref0 = vec_ld (0, ref);
	ref1 = vec_ld (7, ref);
	ref += stride;
	vec_ste ((vector_u32_t)tmp1, 0, (unsigned int *)dest);
	vec_ste ((vector_u32_t)tmp1, 4, (unsigned int *)dest);
	dest += stride;
	tmp0 = vec_perm (ref0, ref1, perm0);
    } while (--height);

    ref0 = vec_ld (0, ref);
    ref1 = vec_ld (7, ref);
    vec_ste ((vector_u32_t)tmp0, 0, (unsigned int *)dest);
    vec_ste ((vector_u32_t)tmp0, 4, (unsigned int *)dest);
    dest += stride;
    tmp1 = vec_perm (ref0, ref1, perm1);
    vec_ste ((vector_u32_t)tmp1, 0, (unsigned int *)dest);
    vec_ste ((vector_u32_t)tmp1, 4, (unsigned int *)dest);
}

static void MC_put_x_16_altivec (uint8_t * dest, const uint8_t * ref,
				 const int stride, int height)
{
    vector_u8_t permA, permB, ref0, ref1, tmp;

    permA = vec_lvsl (0, ref);
    permB = vec_add (permA, vec_splat_u8 (1));

    height = (height >> 1) - 1;

    ref0 = vec_ld (0, ref);
    ref1 = vec_ld (16, ref);
    ref += stride;
    tmp = vec_avg (vec_perm (ref0, ref1, permA),
		   vec_perm (ref0, ref1, permB));

    do {
	ref0 = vec_ld (0, ref);
	ref1 = vec_ld (16, ref);
	ref += stride;
	vec_st (tmp, 0, dest);
	tmp = vec_avg (vec_perm (ref0, ref1, permA),
		       vec_perm (ref0, ref1, permB));

	ref0 = vec_ld (0, ref);
	ref1 = vec_ld (16, ref);
	ref += stride;
	vec_st (tmp, stride, dest);
	dest += 2*stride;
	tmp = vec_avg (vec_perm (ref0, ref1, permA),
		       vec_perm (ref0, ref1, permB));
    } while (--height);

    ref0 = vec_ld (0, ref);
    ref1 = vec_ld (16, ref);
    vec_st (tmp, 0, dest);
    tmp = vec_avg (vec_perm (ref0, ref1, permA),
		   vec_perm (ref0, ref1, permB));
    vec_st (tmp, stride, dest);
}

static void MC_put_x_8_altivec (uint8_t * dest, const uint8_t * ref,
				const int stride, int height)
{
    vector_u8_t perm0A, perm0B, perm1A, perm1B, ones, tmp0, tmp1, ref0, ref1;

    ones = vec_splat_u8 (1);
    tmp0 = vec_lvsl (0, ref);
    tmp0 = vec_mergeh (tmp0, tmp0);
    perm0A = vec_pack ((vector_u16_t)tmp0, (vector_u16_t)tmp0);
    perm0B = vec_add (perm0A, ones);
    tmp1 = vec_lvsl (stride, ref);
    tmp1 = vec_mergeh (tmp1, tmp1);
    perm1A = vec_pack ((vector_u16_t)tmp1, (vector_u16_t)tmp1);
    perm1B = vec_add (perm1A, ones);

    height = (height >> 1) - 1;

    ref0 = vec_ld (0, ref);
    ref1 = vec_ld (8, ref);
    ref += stride;
    tmp0 = vec_avg (vec_perm (ref0, ref1, perm0A),
		    vec_perm (ref0, ref1, perm0B));

    do {
	ref0 = vec_ld (0, ref);
	ref1 = vec_ld (8, ref);
	ref += stride;
	vec_ste ((vector_u32_t)tmp0, 0, (unsigned int *)dest);
	vec_ste ((vector_u32_t)tmp0, 4, (unsigned int *)dest);
	dest += stride;
	tmp1 = vec_avg (vec_perm (ref0, ref1, perm1A),
			vec_perm (ref0, ref1, perm1B));

	ref0 = vec_ld (0, ref);
	ref1 = vec_ld (8, ref);
	ref += stride;
	vec_ste ((vector_u32_t)tmp1, 0, (unsigned int *)dest);
	vec_ste ((vector_u32_t)tmp1, 4, (unsigned int *)dest);
	dest += stride;
	tmp0 = vec_avg (vec_perm (ref0, ref1, perm0A),
			vec_perm (ref0, ref1, perm0B));
    } while (--height);

    ref0 = vec_ld (0, ref);
    ref1 = vec_ld (8, ref);
    vec_ste ((vector_u32_t)tmp0, 0, (unsigned int *)dest);
    vec_ste ((vector_u32_t)tmp0, 4, (unsigned int *)dest);
    dest += stride;
    tmp1 = vec_avg (vec_perm (ref0, ref1, perm1A),
		    vec_perm (ref0, ref1, perm1B));
    vec_ste ((vector_u32_t)tmp1, 0, (unsigned int *)dest);
    vec_ste ((vector_u32_t)tmp1, 4, (unsigned int *)dest);
}

static void MC_put_y_16_altivec (uint8_t * dest, const uint8_t * ref,
				 const int stride, int height)
{
    vector_u8_t perm, ref0, ref1, tmp0, tmp1, tmp;

    perm = vec_lvsl (0, ref);

    height = (height >> 1) - 1;

    ref0 = vec_ld (0, ref);
    ref1 = vec_ld (15, ref);
    ref += stride;
    tmp0 = vec_perm (ref0, ref1, perm);
    ref0 = vec_ld (0, ref);
    ref1 = vec_ld (15, ref);
    ref += stride;
    tmp1 = vec_perm (ref0, ref1, perm);
    tmp = vec_avg (tmp0, tmp1);

    do {
	ref0 = vec_ld (0, ref);
	ref1 = vec_ld (15, ref);
	ref += stride;
	vec_st (tmp, 0, dest);
	tmp0 = vec_perm (ref0, ref1, perm);
	tmp = vec_avg (tmp0, tmp1);

	ref0 = vec_ld (0, ref);
	ref1 = vec_ld (15, ref);
	ref += stride;
	vec_st (tmp, stride, dest);
	dest += 2*stride;
	tmp1 = vec_perm (ref0, ref1, perm);
	tmp = vec_avg (tmp0, tmp1);
    } while (--height);

    ref0 = vec_ld (0, ref);
    ref1 = vec_ld (15, ref);
    vec_st (tmp, 0, dest);
    tmp0 = vec_perm (ref0, ref1, perm);
    tmp = vec_avg (tmp0, tmp1);
    vec_st (tmp, stride, dest);
}

static void MC_put_y_8_altivec (uint8_t * dest, const uint8_t * ref,
				const int stride, int height)
{
    vector_u8_t perm0, perm1, tmp0, tmp1, tmp, ref0, ref1;

    tmp0 = vec_lvsl (0, ref);
    tmp0 = vec_mergeh (tmp0, tmp0);
    perm0 = vec_pack ((vector_u16_t)tmp0, (vector_u16_t)tmp0);
    tmp1 = vec_lvsl (stride, ref);
    tmp1 = vec_mergeh (tmp1, tmp1);
    perm1 = vec_pack ((vector_u16_t)tmp1, (vector_u16_t)tmp1);

    height = (height >> 1) - 1;

    ref0 = vec_ld (0, ref);
    ref1 = vec_ld (7, ref);
    ref += stride;
    tmp0 = vec_perm (ref0, ref1, perm0);
    ref0 = vec_ld (0, ref);
    ref1 = vec_ld (7, ref);
    ref += stride;
    tmp1 = vec_perm (ref0, ref1, perm1);
    tmp = vec_avg (tmp0, tmp1);

    do {
	ref0 = vec_ld (0, ref);
	ref1 = vec_ld (7, ref);
	ref += stride;
	vec_ste ((vector_u32_t)tmp, 0, (unsigned int *)dest);
	vec_ste ((vector_u32_t)tmp, 4, (unsigned int *)dest);
	dest += stride;
	tmp0 = vec_perm (ref0, ref1, perm0);
	tmp = vec_avg (tmp0, tmp1);

	ref0 = vec_ld (0, ref);
	ref1 = vec_ld (7, ref);
	ref += stride;
	vec_ste ((vector_u32_t)tmp, 0, (unsigned int *)dest);
	vec_ste ((vector_u32_t)tmp, 4, (unsigned int *)dest);
	dest += stride;
	tmp1 = vec_perm (ref0, ref1, perm1);
	tmp = vec_avg (tmp0, tmp1);
    } while (--height);

    ref0 = vec_ld (0, ref);
    ref1 = vec_ld (7, ref);
    vec_ste ((vector_u32_t)tmp, 0, (unsigned int *)dest);
    vec_ste ((vector_u32_t)tmp, 4, (unsigned int *)dest);
    dest += stride;
    tmp0 = vec_perm (ref0, ref1, perm0);
    tmp = vec_avg (tmp0, tmp1);
    vec_ste ((vector_u32_t)tmp, 0, (unsigned int *)dest);
    vec_ste ((vector_u32_t)tmp, 4, (unsigned int *)dest);
}

static void MC_put_xy_16_altivec (uint8_t * dest, const uint8_t * ref,
				  const int stride, int height)
{
    vector_u8_t permA, permB, ref0, ref1, A, B, avg0, avg1, xor0, xor1, tmp;
    vector_u8_t ones;

    ones = vec_splat_u8 (1);
    permA = vec_lvsl (0, ref);
    permB = vec_add (permA, ones);

    height = (height >> 1) - 1;

    ref0 = vec_ld (0, ref);
    ref1 = vec_ld (16, ref);
    ref += stride;
    A = vec_perm (ref0, ref1, permA);
    B = vec_perm (ref0, ref1, permB);
    avg0 = vec_avg (A, B);
    xor0 = vec_xor (A, B);

    ref0 = vec_ld (0, ref);
    ref1 = vec_ld (16, ref);
    ref += stride;
    A = vec_perm (ref0, ref1, permA);
    B = vec_perm (ref0, ref1, permB);
    avg1 = vec_avg (A, B);
    xor1 = vec_xor (A, B);
    tmp = vec_sub (vec_avg (avg0, avg1),
		   vec_and (vec_and (ones, vec_or (xor0, xor1)),
			    vec_xor (avg0, avg1)));

    do {
	ref0 = vec_ld (0, ref);
	ref1 = vec_ld (16, ref);
	ref += stride;
	vec_st (tmp, 0, dest);
	A = vec_perm (ref0, ref1, permA);
	B = vec_perm (ref0, ref1, permB);
	avg0 = vec_avg (A, B);
	xor0 = vec_xor (A, B);
	tmp = vec_sub (vec_avg (avg0, avg1),
		       vec_and (vec_and (ones, vec_or (xor0, xor1)),
				vec_xor (avg0, avg1)));

	ref0 = vec_ld (0, ref);
	ref1 = vec_ld (16, ref);
	ref += stride;
	vec_st (tmp, stride, dest);
	dest += 2*stride;
	A = vec_perm (ref0, ref1, permA);
	B = vec_perm (ref0, ref1, permB);
	avg1 = vec_avg (A, B);
	xor1 = vec_xor (A, B);
	tmp = vec_sub (vec_avg (avg0, avg1),
		       vec_and (vec_and (ones, vec_or (xor0, xor1)),
				vec_xor (avg0, avg1)));
    } while (--height);

    ref0 = vec_ld (0, ref);
    ref1 = vec_ld (16, ref);
    vec_st (tmp, 0, dest);
    A = vec_perm (ref0, ref1, permA);
    B = vec_perm (ref0, ref1, permB);
    avg0 = vec_avg (A, B);
    xor0 = vec_xor (A, B);
    tmp = vec_sub (vec_avg (avg0, avg1),
		   vec_and (vec_and (ones, vec_or (xor0, xor1)),
			    vec_xor (avg0, avg1)));
    vec_st (tmp, stride, dest);
}

static void MC_put_xy_8_altivec (uint8_t * dest, const uint8_t * ref,
				 const int stride, int height)
{
    vector_u8_t perm0A, perm0B, perm1A, perm1B, ref0, ref1, A, B;
    vector_u8_t avg0, avg1, xor0, xor1, tmp, ones;

    ones = vec_splat_u8 (1);
    perm0A = vec_lvsl (0, ref);
    perm0A = vec_mergeh (perm0A, perm0A);
    perm0A = vec_pack ((vector_u16_t)perm0A, (vector_u16_t)perm0A);
    perm0B = vec_add (perm0A, ones);
    perm1A = vec_lvsl (stride, ref);
    perm1A = vec_mergeh (perm1A, perm1A);
    perm1A = vec_pack ((vector_u16_t)perm1A, (vector_u16_t)perm1A);
    perm1B = vec_add (perm1A, ones);

    height = (height >> 1) - 1;

    ref0 = vec_ld (0, ref);
    ref1 = vec_ld (8, ref);
    ref += stride;
    A = vec_perm (ref0, ref1, perm0A);
    B = vec_perm (ref0, ref1, perm0B);
    avg0 = vec_avg (A, B);
    xor0 = vec_xor (A, B);

    ref0 = vec_ld (0, ref);
    ref1 = vec_ld (8, ref);
    ref += stride;
    A = vec_perm (ref0, ref1, perm1A);
    B = vec_perm (ref0, ref1, perm1B);
    avg1 = vec_avg (A, B);
    xor1 = vec_xor (A, B);
    tmp = vec_sub (vec_avg (avg0, avg1),
		   vec_and (vec_and (ones, vec_or (xor0, xor1)),
			    vec_xor (avg0, avg1)));

    do {
	ref0 = vec_ld (0, ref);
	ref1 = vec_ld (8, ref);
	ref += stride;
	vec_ste ((vector_u32_t)tmp, 0, (unsigned int *)dest);
	vec_ste ((vector_u32_t)tmp, 4, (unsigned int *)dest);
	dest += stride;
	A = vec_perm (ref0, ref1, perm0A);
	B = vec_perm (ref0, ref1, perm0B);
	avg0 = vec_avg (A, B);
	xor0 = vec_xor (A, B);
	tmp = vec_sub (vec_avg (avg0, avg1),
		       vec_and (vec_and (ones, vec_or (xor0, xor1)),
				vec_xor (avg0, avg1)));

	ref0 = vec_ld (0, ref);
	ref1 = vec_ld (8, ref);
	ref += stride;
	vec_ste ((vector_u32_t)tmp, 0, (unsigned int *)dest);
	vec_ste ((vector_u32_t)tmp, 4, (unsigned int *)dest);
	dest += stride;
	A = vec_perm (ref0, ref1, perm1A);
	B = vec_perm (ref0, ref1, perm1B);
	avg1 = vec_avg (A, B);
	xor1 = vec_xor (A, B);
	tmp = vec_sub (vec_avg (avg0, avg1),
		       vec_and (vec_and (ones, vec_or (xor0, xor1)),
				vec_xor (avg0, avg1)));
    } while (--height);

    ref0 = vec_ld (0, ref);
    ref1 = vec_ld (8, ref);
    vec_ste ((vector_u32_t)tmp, 0, (unsigned int *)dest);
    vec_ste ((vector_u32_t)tmp, 4, (unsigned int *)dest);
    dest += stride;
    A = vec_perm (ref0, ref1, perm0A);
    B = vec_perm (ref0, ref1, perm0B);
    avg0 = vec_avg (A, B);
    xor0 = vec_xor (A, B);
    tmp = vec_sub (vec_avg (avg0, avg1),
		   vec_and (vec_and (ones, vec_or (xor0, xor1)),
			    vec_xor (avg0, avg1)));
    vec_ste ((vector_u32_t)tmp, 0, (unsigned int *)dest);
    vec_ste ((vector_u32_t)tmp, 4, (unsigned int *)dest);
}

#if 0
static void MC_put_xy_8_altivec (uint8_t * dest, const uint8_t * ref,
				 const int stride, int height)
{
    vector_u8_t permA, permB, ref0, ref1, A, B, C, D, tmp, zero, ones;
    vector_u16_t splat2, temp;

    ones = vec_splat_u8 (1);
    permA = vec_lvsl (0, ref);
    permB = vec_add (permA, ones);

    zero = vec_splat_u8 (0);
    splat2 = vec_splat_u16 (2);

    do {
	ref0 = vec_ld (0, ref);
	ref1 = vec_ld (8, ref);
	ref += stride;
	A = vec_perm (ref0, ref1, permA);
	B = vec_perm (ref0, ref1, permB);
	ref0 = vec_ld (0, ref);
	ref1 = vec_ld (8, ref);
	C = vec_perm (ref0, ref1, permA);
	D = vec_perm (ref0, ref1, permB);

	temp = vec_add (vec_add ((vector_u16_t)vec_mergeh (zero, A),
				(vector_u16_t)vec_mergeh (zero, B)),
		       vec_add ((vector_u16_t)vec_mergeh (zero, C),
				(vector_u16_t)vec_mergeh (zero, D)));
	temp = vec_sr (vec_add (temp, splat2), splat2);
	tmp = vec_pack (temp, temp);

	vec_st (tmp, 0, dest);
	dest += stride;
	tmp = vec_avg (vec_perm (ref0, ref1, permA),
		       vec_perm (ref0, ref1, permB));
    } while (--height);
}
#endif

static void MC_avg_o_16_altivec (uint8_t * dest, const uint8_t * ref,
				 const int stride, int height)
{
    vector_u8_t perm, ref0, ref1, tmp, prev;

    perm = vec_lvsl (0, ref);

    height = (height >> 1) - 1;

    ref0 = vec_ld (0, ref);
    ref1 = vec_ld (15, ref);
    ref += stride;
    prev = vec_ld (0, dest);
    tmp = vec_avg (prev, vec_perm (ref0, ref1, perm));

    do {
	ref0 = vec_ld (0, ref);
	ref1 = vec_ld (15, ref);
	ref += stride;
	prev = vec_ld (stride, dest);
	vec_st (tmp, 0, dest);
	tmp = vec_avg (prev, vec_perm (ref0, ref1, perm));

	ref0 = vec_ld (0, ref);
	ref1 = vec_ld (15, ref);
	ref += stride;
	prev = vec_ld (2*stride, dest);
	vec_st (tmp, stride, dest);
	dest += 2*stride;
	tmp = vec_avg (prev, vec_perm (ref0, ref1, perm));
    } while (--height);

    ref0 = vec_ld (0, ref);
    ref1 = vec_ld (15, ref);
    prev = vec_ld (stride, dest);
    vec_st (tmp, 0, dest);
    tmp = vec_avg (prev, vec_perm (ref0, ref1, perm));
    vec_st (tmp, stride, dest);
}

static void MC_avg_o_8_altivec (uint8_t * dest, const uint8_t * ref,
				const int stride, int height)
{
    vector_u8_t perm0, perm1, tmp0, tmp1, ref0, ref1, prev;

    tmp0 = vec_lvsl (0, ref);
    tmp0 = vec_mergeh (tmp0, tmp0);
    perm0 = vec_pack ((vector_u16_t)tmp0, (vector_u16_t)tmp0);
    tmp1 = vec_lvsl (stride, ref);
    tmp1 = vec_mergeh (tmp1, tmp1);
    perm1 = vec_pack ((vector_u16_t)tmp1, (vector_u16_t)tmp1);

    height = (height >> 1) - 1;

    ref0 = vec_ld (0, ref);
    ref1 = vec_ld (7, ref);
    ref += stride;
    prev = vec_ld (0, dest);
    tmp0 = vec_avg (prev, vec_perm (ref0, ref1, perm0));

    do {
	ref0 = vec_ld (0, ref);
	ref1 = vec_ld (7, ref);
	ref += stride;
	prev = vec_ld (stride, dest);
	vec_ste ((vector_u32_t)tmp0, 0, (unsigned int *)dest);
	vec_ste ((vector_u32_t)tmp0, 4, (unsigned int *)dest);
	dest += stride;
	tmp1 = vec_avg (prev, vec_perm (ref0, ref1, perm1));

	ref0 = vec_ld (0, ref);
	ref1 = vec_ld (7, ref);
	ref += stride;
	prev = vec_ld (stride, dest);
	vec_ste ((vector_u32_t)tmp1, 0, (unsigned int *)dest);
	vec_ste ((vector_u32_t)tmp1, 4, (unsigned int *)dest);
	dest += stride;
	tmp0 = vec_avg (prev, vec_perm (ref0, ref1, perm0));
    } while (--height);

    ref0 = vec_ld (0, ref);
    ref1 = vec_ld (7, ref);
    prev = vec_ld (stride, dest);
    vec_ste ((vector_u32_t)tmp0, 0, (unsigned int *)dest);
    vec_ste ((vector_u32_t)tmp0, 4, (unsigned int *)dest);
    dest += stride;
    tmp1 = vec_avg (prev, vec_perm (ref0, ref1, perm1));
    vec_ste ((vector_u32_t)tmp1, 0, (unsigned int *)dest);
    vec_ste ((vector_u32_t)tmp1, 4, (unsigned int *)dest);
}

static void MC_avg_x_16_altivec (uint8_t * dest, const uint8_t * ref,
				 const int stride, int height)
{
    vector_u8_t permA, permB, ref0, ref1, tmp, prev;

    permA = vec_lvsl (0, ref);
    permB = vec_add (permA, vec_splat_u8 (1));

    height = (height >> 1) - 1;

    ref0 = vec_ld (0, ref);
    ref1 = vec_ld (16, ref);
    prev = vec_ld (0, dest);
    ref += stride;
    tmp = vec_avg (prev, vec_avg (vec_perm (ref0, ref1, permA),
				  vec_perm (ref0, ref1, permB)));

    do {
	ref0 = vec_ld (0, ref);
	ref1 = vec_ld (16, ref);
	ref += stride;
	prev = vec_ld (stride, dest);
	vec_st (tmp, 0, dest);
	tmp = vec_avg (prev, vec_avg (vec_perm (ref0, ref1, permA),
				      vec_perm (ref0, ref1, permB)));

	ref0 = vec_ld (0, ref);
	ref1 = vec_ld (16, ref);
	ref += stride;
	prev = vec_ld (2*stride, dest);
	vec_st (tmp, stride, dest);
	dest += 2*stride;
	tmp = vec_avg (prev, vec_avg (vec_perm (ref0, ref1, permA),
				      vec_perm (ref0, ref1, permB)));
    } while (--height);

    ref0 = vec_ld (0, ref);
    ref1 = vec_ld (16, ref);
    prev = vec_ld (stride, dest);
    vec_st (tmp, 0, dest);
    tmp = vec_avg (prev, vec_avg (vec_perm (ref0, ref1, permA),
				  vec_perm (ref0, ref1, permB)));
    vec_st (tmp, stride, dest);
}

static void MC_avg_x_8_altivec (uint8_t * dest, const uint8_t * ref,
				const int stride, int height)
{
    vector_u8_t perm0A, perm0B, perm1A, perm1B, ones, tmp0, tmp1, ref0, ref1;
    vector_u8_t prev;

    ones = vec_splat_u8 (1);
    tmp0 = vec_lvsl (0, ref);
    tmp0 = vec_mergeh (tmp0, tmp0);
    perm0A = vec_pack ((vector_u16_t)tmp0, (vector_u16_t)tmp0);
    perm0B = vec_add (perm0A, ones);
    tmp1 = vec_lvsl (stride, ref);
    tmp1 = vec_mergeh (tmp1, tmp1);
    perm1A = vec_pack ((vector_u16_t)tmp1, (vector_u16_t)tmp1);
    perm1B = vec_add (perm1A, ones);

    height = (height >> 1) - 1;

    ref0 = vec_ld (0, ref);
    ref1 = vec_ld (8, ref);
    prev = vec_ld (0, dest);
    ref += stride;
    tmp0 = vec_avg (prev, vec_avg (vec_perm (ref0, ref1, perm0A),
				   vec_perm (ref0, ref1, perm0B)));

    do {
	ref0 = vec_ld (0, ref);
	ref1 = vec_ld (8, ref);
	ref += stride;
	prev = vec_ld (stride, dest);
	vec_ste ((vector_u32_t)tmp0, 0, (unsigned int *)dest);
	vec_ste ((vector_u32_t)tmp0, 4, (unsigned int *)dest);
	dest += stride;
	tmp1 = vec_avg (prev, vec_avg (vec_perm (ref0, ref1, perm1A),
				       vec_perm (ref0, ref1, perm1B)));

	ref0 = vec_ld (0, ref);
	ref1 = vec_ld (8, ref);
	ref += stride;
	prev = vec_ld (stride, dest);
	vec_ste ((vector_u32_t)tmp1, 0, (unsigned int *)dest);
	vec_ste ((vector_u32_t)tmp1, 4, (unsigned int *)dest);
	dest += stride;
	tmp0 = vec_avg (prev, vec_avg (vec_perm (ref0, ref1, perm0A),
				       vec_perm (ref0, ref1, perm0B)));
    } while (--height);

    ref0 = vec_ld (0, ref);
    ref1 = vec_ld (8, ref);
    prev = vec_ld (stride, dest);
    vec_ste ((vector_u32_t)tmp0, 0, (unsigned int *)dest);
    vec_ste ((vector_u32_t)tmp0, 4, (unsigned int *)dest);
    dest += stride;
    tmp1 = vec_avg (prev, vec_avg (vec_perm (ref0, ref1, perm1A),
				   vec_perm (ref0, ref1, perm1B)));
    vec_ste ((vector_u32_t)tmp1, 0, (unsigned int *)dest);
    vec_ste ((vector_u32_t)tmp1, 4, (unsigned int *)dest);
}

static void MC_avg_y_16_altivec (uint8_t * dest, const uint8_t * ref,
				 const int stride, int height)
{
    vector_u8_t perm, ref0, ref1, tmp0, tmp1, tmp, prev;

    perm = vec_lvsl (0, ref);

    height = (height >> 1) - 1;

    ref0 = vec_ld (0, ref);
    ref1 = vec_ld (15, ref);
    ref += stride;
    tmp0 = vec_perm (ref0, ref1, perm);
    ref0 = vec_ld (0, ref);
    ref1 = vec_ld (15, ref);
    ref += stride;
    prev = vec_ld (0, dest);
    tmp1 = vec_perm (ref0, ref1, perm);
    tmp = vec_avg (prev, vec_avg (tmp0, tmp1));

    do {
	ref0 = vec_ld (0, ref);
	ref1 = vec_ld (15, ref);
	ref += stride;
	prev = vec_ld (stride, dest);
	vec_st (tmp, 0, dest);
	tmp0 = vec_perm (ref0, ref1, perm);
	tmp = vec_avg (prev, vec_avg (tmp0, tmp1));

	ref0 = vec_ld (0, ref);
	ref1 = vec_ld (15, ref);
	ref += stride;
	prev = vec_ld (2*stride, dest);
	vec_st (tmp, stride, dest);
	dest += 2*stride;
	tmp1 = vec_perm (ref0, ref1, perm);
	tmp = vec_avg (prev, vec_avg (tmp0, tmp1));
    } while (--height);

    ref0 = vec_ld (0, ref);
    ref1 = vec_ld (15, ref);
    prev = vec_ld (stride, dest);
    vec_st (tmp, 0, dest);
    tmp0 = vec_perm (ref0, ref1, perm);
    tmp = vec_avg (prev, vec_avg (tmp0, tmp1));
    vec_st (tmp, stride, dest);
}

static void MC_avg_y_8_altivec (uint8_t * dest, const uint8_t * ref,
				const int stride, int height)
{
    vector_u8_t perm0, perm1, tmp0, tmp1, tmp, ref0, ref1, prev;

    tmp0 = vec_lvsl (0, ref);
    tmp0 = vec_mergeh (tmp0, tmp0);
    perm0 = vec_pack ((vector_u16_t)tmp0, (vector_u16_t)tmp0);
    tmp1 = vec_lvsl (stride, ref);
    tmp1 = vec_mergeh (tmp1, tmp1);
    perm1 = vec_pack ((vector_u16_t)tmp1, (vector_u16_t)tmp1);

    height = (height >> 1) - 1;

    ref0 = vec_ld (0, ref);
    ref1 = vec_ld (7, ref);
    ref += stride;
    tmp0 = vec_perm (ref0, ref1, perm0);
    ref0 = vec_ld (0, ref);
    ref1 = vec_ld (7, ref);
    ref += stride;
    prev = vec_ld (0, dest);
    tmp1 = vec_perm (ref0, ref1, perm1);
    tmp = vec_avg (prev, vec_avg (tmp0, tmp1));

    do {
	ref0 = vec_ld (0, ref);
	ref1 = vec_ld (7, ref);
	ref += stride;
	prev = vec_ld (stride, dest);
	vec_ste ((vector_u32_t)tmp, 0, (unsigned int *)dest);
	vec_ste ((vector_u32_t)tmp, 4, (unsigned int *)dest);
	dest += stride;
	tmp0 = vec_perm (ref0, ref1, perm0);
	tmp = vec_avg (prev, vec_avg (tmp0, tmp1));

	ref0 = vec_ld (0, ref);
	ref1 = vec_ld (7, ref);
	ref += stride;
	prev = vec_ld (stride, dest);
	vec_ste ((vector_u32_t)tmp, 0, (unsigned int *)dest);
	vec_ste ((vector_u32_t)tmp, 4, (unsigned int *)dest);
	dest += stride;
	tmp1 = vec_perm (ref0, ref1, perm1);
	tmp = vec_avg (prev, vec_avg (tmp0, tmp1));
    } while (--height);

    ref0 = vec_ld (0, ref);
    ref1 = vec_ld (7, ref);
    prev = vec_ld (stride, dest);
    vec_ste ((vector_u32_t)tmp, 0, (unsigned int *)dest);
    vec_ste ((vector_u32_t)tmp, 4, (unsigned int *)dest);
    dest += stride;
    tmp0 = vec_perm (ref0, ref1, perm0);
    tmp = vec_avg (prev, vec_avg (tmp0, tmp1));
    vec_ste ((vector_u32_t)tmp, 0, (unsigned int *)dest);
    vec_ste ((vector_u32_t)tmp, 4, (unsigned int *)dest);
}

static void MC_avg_xy_16_altivec (uint8_t * dest, const uint8_t * ref,
				  const int stride, int height)
{
    vector_u8_t permA, permB, ref0, ref1, A, B, avg0, avg1, xor0, xor1, tmp;
    vector_u8_t ones, prev;

    ones = vec_splat_u8 (1);
    permA = vec_lvsl (0, ref);
    permB = vec_add (permA, ones);

    height = (height >> 1) - 1;

    ref0 = vec_ld (0, ref);
    ref1 = vec_ld (16, ref);
    ref += stride;
    A = vec_perm (ref0, ref1, permA);
    B = vec_perm (ref0, ref1, permB);
    avg0 = vec_avg (A, B);
    xor0 = vec_xor (A, B);

    ref0 = vec_ld (0, ref);
    ref1 = vec_ld (16, ref);
    ref += stride;
    prev = vec_ld (0, dest);
    A = vec_perm (ref0, ref1, permA);
    B = vec_perm (ref0, ref1, permB);
    avg1 = vec_avg (A, B);
    xor1 = vec_xor (A, B);
    tmp = vec_avg (prev, vec_sub (vec_avg (avg0, avg1),
				  vec_and (vec_and (ones, vec_or (xor0, xor1)),
					   vec_xor (avg0, avg1))));

    do {
	ref0 = vec_ld (0, ref);
	ref1 = vec_ld (16, ref);
	ref += stride;
	prev = vec_ld (stride, dest);
	vec_st (tmp, 0, dest);
	A = vec_perm (ref0, ref1, permA);
	B = vec_perm (ref0, ref1, permB);
	avg0 = vec_avg (A, B);
	xor0 = vec_xor (A, B);
	tmp = vec_avg (prev,
		       vec_sub (vec_avg (avg0, avg1),
				vec_and (vec_and (ones, vec_or (xor0, xor1)),
					 vec_xor (avg0, avg1))));

	ref0 = vec_ld (0, ref);
	ref1 = vec_ld (16, ref);
	ref += stride;
	prev = vec_ld (2*stride, dest);
	vec_st (tmp, stride, dest);
	dest += 2*stride;
	A = vec_perm (ref0, ref1, permA);
	B = vec_perm (ref0, ref1, permB);
	avg1 = vec_avg (A, B);
	xor1 = vec_xor (A, B);
	tmp = vec_avg (prev,
		       vec_sub (vec_avg (avg0, avg1),
				vec_and (vec_and (ones, vec_or (xor0, xor1)),
					 vec_xor (avg0, avg1))));
    } while (--height);

    ref0 = vec_ld (0, ref);
    ref1 = vec_ld (16, ref);
    prev = vec_ld (stride, dest);
    vec_st (tmp, 0, dest);
    A = vec_perm (ref0, ref1, permA);
    B = vec_perm (ref0, ref1, permB);
    avg0 = vec_avg (A, B);
    xor0 = vec_xor (A, B);
    tmp = vec_avg (prev, vec_sub (vec_avg (avg0, avg1),
				  vec_and (vec_and (ones, vec_or (xor0, xor1)),
					   vec_xor (avg0, avg1))));
    vec_st (tmp, stride, dest);
}

static void MC_avg_xy_8_altivec (uint8_t * dest, const uint8_t * ref,
				 const int stride, int height)
{
    vector_u8_t perm0A, perm0B, perm1A, perm1B, ref0, ref1, A, B;
    vector_u8_t avg0, avg1, xor0, xor1, tmp, ones, prev;

    ones = vec_splat_u8 (1);
    perm0A = vec_lvsl (0, ref);
    perm0A = vec_mergeh (perm0A, perm0A);
    perm0A = vec_pack ((vector_u16_t)perm0A, (vector_u16_t)perm0A);
    perm0B = vec_add (perm0A, ones);
    perm1A = vec_lvsl (stride, ref);
    perm1A = vec_mergeh (perm1A, perm1A);
    perm1A = vec_pack ((vector_u16_t)perm1A, (vector_u16_t)perm1A);
    perm1B = vec_add (perm1A, ones);

    height = (height >> 1) - 1;

    ref0 = vec_ld (0, ref);
    ref1 = vec_ld (8, ref);
    ref += stride;
    A = vec_perm (ref0, ref1, perm0A);
    B = vec_perm (ref0, ref1, perm0B);
    avg0 = vec_avg (A, B);
    xor0 = vec_xor (A, B);

    ref0 = vec_ld (0, ref);
    ref1 = vec_ld (8, ref);
    ref += stride;
    prev = vec_ld (0, dest);
    A = vec_perm (ref0, ref1, perm1A);
    B = vec_perm (ref0, ref1, perm1B);
    avg1 = vec_avg (A, B);
    xor1 = vec_xor (A, B);
    tmp = vec_avg (prev, vec_sub (vec_avg (avg0, avg1),
				  vec_and (vec_and (ones, vec_or (xor0, xor1)),
					   vec_xor (avg0, avg1))));

    do {
	ref0 = vec_ld (0, ref);
	ref1 = vec_ld (8, ref);
	ref += stride;
	prev = vec_ld (stride, dest);
	vec_ste ((vector_u32_t)tmp, 0, (unsigned int *)dest);
	vec_ste ((vector_u32_t)tmp, 4, (unsigned int *)dest);
	dest += stride;
	A = vec_perm (ref0, ref1, perm0A);
	B = vec_perm (ref0, ref1, perm0B);
	avg0 = vec_avg (A, B);
	xor0 = vec_xor (A, B);
	tmp = vec_avg (prev,
		       vec_sub (vec_avg (avg0, avg1),
				vec_and (vec_and (ones, vec_or (xor0, xor1)),
					 vec_xor (avg0, avg1))));

	ref0 = vec_ld (0, ref);
	ref1 = vec_ld (8, ref);
	ref += stride;
	prev = vec_ld (stride, dest);
	vec_ste ((vector_u32_t)tmp, 0, (unsigned int *)dest);
	vec_ste ((vector_u32_t)tmp, 4, (unsigned int *)dest);
	dest += stride;
	A = vec_perm (ref0, ref1, perm1A);
	B = vec_perm (ref0, ref1, perm1B);
	avg1 = vec_avg (A, B);
	xor1 = vec_xor (A, B);
	tmp = vec_avg (prev,
		       vec_sub (vec_avg (avg0, avg1),
				vec_and (vec_and (ones, vec_or (xor0, xor1)),
					 vec_xor (avg0, avg1))));
    } while (--height);

    ref0 = vec_ld (0, ref);
    ref1 = vec_ld (8, ref);
    prev = vec_ld (stride, dest);
    vec_ste ((vector_u32_t)tmp, 0, (unsigned int *)dest);
    vec_ste ((vector_u32_t)tmp, 4, (unsigned int *)dest);
    dest += stride;
    A = vec_perm (ref0, ref1, perm0A);
    B = vec_perm (ref0, ref1, perm0B);
    avg0 = vec_avg (A, B);
    xor0 = vec_xor (A, B);
    tmp = vec_avg (prev, vec_sub (vec_avg (avg0, avg1),
				  vec_and (vec_and (ones, vec_or (xor0, xor1)),
					   vec_xor (avg0, avg1))));
    vec_ste ((vector_u32_t)tmp, 0, (unsigned int *)dest);
    vec_ste ((vector_u32_t)tmp, 4, (unsigned int *)dest);
}

MPEG2_MC_EXTERN (altivec)

#endif
