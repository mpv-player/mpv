/*
 * idct_altivec.c
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

#if defined( HAVE_ALTIVEC_H ) && !defined( __APPLE_ALTIVEC__ ) && (__GNUC__ * 100 + __GNUC_MINOR__ < 303)
/* work around gcc <3.3 vec_mergel bug */
static inline vector_s16_t my_vec_mergel (vector_s16_t const A,
					  vector_s16_t const B)
{
    static const vector_u8_t mergel = {
	0x08, 0x09, 0x18, 0x19, 0x0a, 0x0b, 0x1a, 0x1b,
	0x0c, 0x0d, 0x1c, 0x1d, 0x0e, 0x0f, 0x1e, 0x1f
    };
    return vec_perm (A, B, mergel);
}
#undef vec_mergel
#define vec_mergel my_vec_mergel
#endif

#if defined( __APPLE_CC__ ) && defined( __APPLE_ALTIVEC__ ) /* apple */
#define VEC_S16(a,b,c,d,e,f,g,h) (vector_s16_t) (a, b, c, d, e, f, g, h)
#else			/* gnu */
#define VEC_S16(a,b,c,d,e,f,g,h) {a, b, c, d, e, f, g, h}
#endif

static const vector_s16_t constants ATTR_ALIGN(16) =
    VEC_S16 (23170, 13573, 6518, 21895, -23170, -21895, 32, 31);
static const vector_s16_t constants_1 ATTR_ALIGN(16) =
    VEC_S16 (16384, 22725, 21407, 19266, 16384, 19266, 21407, 22725);
static const vector_s16_t constants_2 ATTR_ALIGN(16) =
    VEC_S16 (16069, 22289, 20995, 18895, 16069, 18895, 20995, 22289);
static const vector_s16_t constants_3 ATTR_ALIGN(16) =
    VEC_S16 (21407, 29692, 27969, 25172, 21407, 25172, 27969, 29692);
static const vector_s16_t constants_4 ATTR_ALIGN(16) =
    VEC_S16 (13623, 18895, 17799, 16019, 13623, 16019, 17799, 18895);

#define IDCT								\
    vector_s16_t vx0, vx1, vx2, vx3, vx4, vx5, vx6, vx7;		\
    vector_s16_t vy0, vy1, vy2, vy3, vy4, vy5, vy6, vy7;		\
    vector_s16_t a0, a1, a2, ma2, c4, mc4, zero, bias;			\
    vector_s16_t t0, t1, t2, t3, t4, t5, t6, t7, t8;			\
    vector_u16_t shift;							\
									\
    c4 = vec_splat (constants, 0);					\
    a0 = vec_splat (constants, 1);					\
    a1 = vec_splat (constants, 2);					\
    a2 = vec_splat (constants, 3);					\
    mc4 = vec_splat (constants, 4);					\
    ma2 = vec_splat (constants, 5);					\
    bias = (vector_s16_t)vec_splat ((vector_s32_t)constants, 3);	\
									\
    zero = vec_splat_s16 (0);						\
									\
    vx0 = vec_adds (block[0], block[4]);				\
    vx4 = vec_subs (block[0], block[4]);				\
    t5 = vec_mradds (vx0, constants_1, zero);				\
    t0 = vec_mradds (vx4, constants_1, zero);				\
									\
    vx1 = vec_mradds (a1, block[7], block[1]);				\
    vx7 = vec_mradds (a1, block[1], vec_subs (zero, block[7]));		\
    t1 = vec_mradds (vx1, constants_2, zero);				\
    t8 = vec_mradds (vx7, constants_2, zero);				\
									\
    vx2 = vec_mradds (a0, block[6], block[2]);				\
    vx6 = vec_mradds (a0, block[2], vec_subs (zero, block[6]));		\
    t2 = vec_mradds (vx2, constants_3, zero);				\
    t4 = vec_mradds (vx6, constants_3, zero);				\
									\
    vx3 = vec_mradds (block[3], constants_4, zero);			\
    vx5 = vec_mradds (block[5], constants_4, zero);			\
    t7 = vec_mradds (a2, vx5, vx3);					\
    t3 = vec_mradds (ma2, vx3, vx5);					\
									\
    t6 = vec_adds (t8, t3);						\
    t3 = vec_subs (t8, t3);						\
    t8 = vec_subs (t1, t7);						\
    t1 = vec_adds (t1, t7);						\
    t6 = vec_mradds (a0, t6, t6);	/* a0+1 == 2*c4 */		\
    t1 = vec_mradds (a0, t1, t1);	/* a0+1 == 2*c4 */		\
									\
    t7 = vec_adds (t5, t2);						\
    t2 = vec_subs (t5, t2);						\
    t5 = vec_adds (t0, t4);						\
    t0 = vec_subs (t0, t4);						\
    t4 = vec_subs (t8, t3);						\
    t3 = vec_adds (t8, t3);						\
									\
    vy0 = vec_adds (t7, t1);						\
    vy7 = vec_subs (t7, t1);						\
    vy1 = vec_adds (t5, t3);						\
    vy6 = vec_subs (t5, t3);						\
    vy2 = vec_adds (t0, t4);						\
    vy5 = vec_subs (t0, t4);						\
    vy3 = vec_adds (t2, t6);						\
    vy4 = vec_subs (t2, t6);						\
									\
    vx0 = vec_mergeh (vy0, vy4);					\
    vx1 = vec_mergel (vy0, vy4);					\
    vx2 = vec_mergeh (vy1, vy5);					\
    vx3 = vec_mergel (vy1, vy5);					\
    vx4 = vec_mergeh (vy2, vy6);					\
    vx5 = vec_mergel (vy2, vy6);					\
    vx6 = vec_mergeh (vy3, vy7);					\
    vx7 = vec_mergel (vy3, vy7);					\
									\
    vy0 = vec_mergeh (vx0, vx4);					\
    vy1 = vec_mergel (vx0, vx4);					\
    vy2 = vec_mergeh (vx1, vx5);					\
    vy3 = vec_mergel (vx1, vx5);					\
    vy4 = vec_mergeh (vx2, vx6);					\
    vy5 = vec_mergel (vx2, vx6);					\
    vy6 = vec_mergeh (vx3, vx7);					\
    vy7 = vec_mergel (vx3, vx7);					\
									\
    vx0 = vec_mergeh (vy0, vy4);					\
    vx1 = vec_mergel (vy0, vy4);					\
    vx2 = vec_mergeh (vy1, vy5);					\
    vx3 = vec_mergel (vy1, vy5);					\
    vx4 = vec_mergeh (vy2, vy6);					\
    vx5 = vec_mergel (vy2, vy6);					\
    vx6 = vec_mergeh (vy3, vy7);					\
    vx7 = vec_mergel (vy3, vy7);					\
									\
    vx0 = vec_adds (vx0, bias);						\
    t5 = vec_adds (vx0, vx4);						\
    t0 = vec_subs (vx0, vx4);						\
									\
    t1 = vec_mradds (a1, vx7, vx1);					\
    t8 = vec_mradds (a1, vx1, vec_subs (zero, vx7));			\
									\
    t2 = vec_mradds (a0, vx6, vx2);					\
    t4 = vec_mradds (a0, vx2, vec_subs (zero, vx6));			\
									\
    t7 = vec_mradds (a2, vx5, vx3);					\
    t3 = vec_mradds (ma2, vx3, vx5);					\
									\
    t6 = vec_adds (t8, t3);						\
    t3 = vec_subs (t8, t3);						\
    t8 = vec_subs (t1, t7);						\
    t1 = vec_adds (t1, t7);						\
									\
    t7 = vec_adds (t5, t2);						\
    t2 = vec_subs (t5, t2);						\
    t5 = vec_adds (t0, t4);						\
    t0 = vec_subs (t0, t4);						\
    t4 = vec_subs (t8, t3);						\
    t3 = vec_adds (t8, t3);						\
									\
    vy0 = vec_adds (t7, t1);						\
    vy7 = vec_subs (t7, t1);						\
    vy1 = vec_mradds (c4, t3, t5);					\
    vy6 = vec_mradds (mc4, t3, t5);					\
    vy2 = vec_mradds (c4, t4, t0);					\
    vy5 = vec_mradds (mc4, t4, t0);					\
    vy3 = vec_adds (t2, t6);						\
    vy4 = vec_subs (t2, t6);						\
									\
    shift = vec_splat_u16 (6);						\
    vx0 = vec_sra (vy0, shift);						\
    vx1 = vec_sra (vy1, shift);						\
    vx2 = vec_sra (vy2, shift);						\
    vx3 = vec_sra (vy3, shift);						\
    vx4 = vec_sra (vy4, shift);						\
    vx5 = vec_sra (vy5, shift);						\
    vx6 = vec_sra (vy6, shift);						\
    vx7 = vec_sra (vy7, shift);

void mpeg2_idct_copy_altivec (int16_t * const _block, uint8_t * dest,
			      const int stride)
{
    vector_s16_t * const block = (vector_s16_t *)_block;
    vector_u8_t tmp;

    IDCT

#define COPY(dest,src)						\
    tmp = vec_packsu (src, src);				\
    vec_ste ((vector_u32_t)tmp, 0, (unsigned int *)dest);	\
    vec_ste ((vector_u32_t)tmp, 4, (unsigned int *)dest);

    COPY (dest, vx0)	dest += stride;
    COPY (dest, vx1)	dest += stride;
    COPY (dest, vx2)	dest += stride;
    COPY (dest, vx3)	dest += stride;
    COPY (dest, vx4)	dest += stride;
    COPY (dest, vx5)	dest += stride;
    COPY (dest, vx6)	dest += stride;
    COPY (dest, vx7)

    block[0] = block[1] = block[2] = block[3] = zero;
    block[4] = block[5] = block[6] = block[7] = zero;
}

void mpeg2_idct_add_altivec (const int last, int16_t * const _block,
			     uint8_t * dest, const int stride)
{
    vector_s16_t * const block = (vector_s16_t *)_block;
    vector_u8_t tmp;
    vector_s16_t tmp2, tmp3;
    vector_u8_t perm0;
    vector_u8_t perm1;
    vector_u8_t p0, p1, p;

    IDCT

    p0 = vec_lvsl (0, dest);
    p1 = vec_lvsl (stride, dest);
    p = vec_splat_u8 (-1);
    perm0 = vec_mergeh (p, p0);
    perm1 = vec_mergeh (p, p1);

#define ADD(dest,src,perm)						\
    /* *(uint64_t *)&tmp = *(uint64_t *)dest; */			\
    tmp = vec_ld (0, dest);						\
    tmp2 = (vector_s16_t)vec_perm (tmp, (vector_u8_t)zero, perm);	\
    tmp3 = vec_adds (tmp2, src);					\
    tmp = vec_packsu (tmp3, tmp3);					\
    vec_ste ((vector_u32_t)tmp, 0, (unsigned int *)dest);		\
    vec_ste ((vector_u32_t)tmp, 4, (unsigned int *)dest);

    ADD (dest, vx0, perm0)	dest += stride;
    ADD (dest, vx1, perm1)	dest += stride;
    ADD (dest, vx2, perm0)	dest += stride;
    ADD (dest, vx3, perm1)	dest += stride;
    ADD (dest, vx4, perm0)	dest += stride;
    ADD (dest, vx5, perm1)	dest += stride;
    ADD (dest, vx6, perm0)	dest += stride;
    ADD (dest, vx7, perm1)

    block[0] = block[1] = block[2] = block[3] = zero;
    block[4] = block[5] = block[6] = block[7] = zero;
}

void mpeg2_idct_altivec_init (void)
{
    extern uint8_t mpeg2_scan_norm[64];
    extern uint8_t mpeg2_scan_alt[64];
    int i, j;

    /* the altivec idct uses a transposed input, so we patch scan tables */
    for (i = 0; i < 64; i++) {
	j = mpeg2_scan_norm[i];
	mpeg2_scan_norm[i] = (j >> 3) | ((j & 7) << 3);
	j = mpeg2_scan_alt[i];
	mpeg2_scan_alt[i] = (j >> 3) | ((j & 7) << 3);
    }
}

#endif
