/*
 * motion_comp_altivec.c
 * Copyright (C) 2000-2002 Michel Lespinasse <walken@zoy.org>
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

#ifndef __ALTIVEC__

#include "config.h"

#ifdef ARCH_PPC

#include <inttypes.h>

#include "mpeg2.h"
#include "mpeg2_internal.h"

/*
 * The asm code is generated with:
 *
 * gcc-2.95 -fvec -D__ALTIVEC__ -O9 -fomit-frame-pointer -mregnames -S
 *      motion_comp_altivec.c
 *
 * sed 's/.L/._L/g' motion_comp_altivec.s |
 * awk '{args=""; len=split ($2, arg, ",");
 *      for (i=1; i<=len; i++) { a=arg[i]; if (i<len) a=a",";
 *                               args = args sprintf ("%-6s", a) }
 *      printf ("\t\"\t%-16s%-24s\\n\"\n", $1, args) }' |
 * unexpand -a
 */

static void MC_put_o_16_altivec (uint8_t * dest, const uint8_t * ref,
				 int stride, int height)
{
    asm ("						\n"
	"	srawi		%r6,  %r6,  1		\n"
	"	li		%r9,  15		\n"
	"	addi		%r6,  %r6,  -1		\n"
	"	lvsl		%v12, 0,    %r4		\n"
	"	mtctr		%r6			\n"
	"	lvx		%v1,  0,    %r4		\n"
	"	lvx		%v0,  %r9,  %r4		\n"
	"	add		%r0,  %r5,  %r5		\n"
	"	vperm		%v13, %v1,  %v0,  %v12	\n"
	"	add		%r4,  %r4,  %r5		\n"
	"._L6:						\n"
	"	li		%r9,  15		\n"
	"	lvx		%v1,  0,    %r4		\n"
	"	lvx		%v0,  %r9,  %r4		\n"
	"	stvx		%v13, 0,    %r3		\n"
	"	vperm		%v13, %v1,  %v0,  %v12	\n"
	"	add		%r4,  %r4,  %r5		\n"
	"	lvx		%v1,  0,    %r4		\n"
	"	lvx		%v0,  %r9,  %r4		\n"
	"	stvx		%v13, %r5,  %r3		\n"
	"	vperm		%v13, %v1,  %v0,  %v12	\n"
	"	add		%r4,  %r4,  %r5		\n"
	"	add		%r3,  %r3,  %r0		\n"
	"	bdnz		._L6			\n"
	"	lvx		%v0,  %r9,  %r4		\n"
	"	lvx		%v1,  0,    %r4		\n"
	"	stvx		%v13, 0,    %r3		\n"
	"	vperm		%v13, %v1,  %v0,  %v12	\n"
	"	stvx		%v13, %r5,  %r3		\n"
	 );
}

static void MC_put_o_8_altivec (uint8_t * dest, const uint8_t * ref,
				int stride, int height)
{
    asm ("						\n"
	"	lvsl		%v12, 0,    %r4		\n"
	"	lvsl		%v1,  %r5,  %r4		\n"
	"	vmrghb		%v12, %v12, %v12	\n"
	"	srawi		%r6,  %r6,  1		\n"
	"	li		%r9,  7			\n"
	"	vmrghb		%v1,  %v1,  %v1		\n"
	"	addi		%r6,  %r6,  -1		\n"
	"	vpkuhum		%v10, %v12, %v12	\n"
	"	lvx		%v13, 0,    %r4		\n"
	"	mtctr		%r6			\n"
	"	vpkuhum		%v11, %v1,  %v1		\n"
	"	lvx		%v0,  %r9,  %r4		\n"
	"	add		%r4,  %r4,  %r5		\n"
	"	vperm		%v12, %v13, %v0,  %v10	\n"
	"._L11:						\n"
	"	li		%r9,  7			\n"
	"	lvx		%v0,  %r9,  %r4		\n"
	"	lvx		%v13, 0,    %r4		\n"
	"	stvewx		%v12, 0,    %r3		\n"
	"	li		%r9,  4			\n"
	"	vperm		%v1,  %v13, %v0,  %v11	\n"
	"	stvewx		%v12, %r9,  %r3		\n"
	"	add		%r4,  %r4,  %r5		\n"
	"	li		%r9,  7			\n"
	"	lvx		%v0,  %r9,  %r4		\n"
	"	lvx		%v13, 0,    %r4		\n"
	"	add		%r3,  %r3,  %r5		\n"
	"	stvewx		%v1,  0,    %r3		\n"
	"	vperm		%v12, %v13, %v0,  %v10	\n"
	"	li		%r9,  4			\n"
	"	stvewx		%v1,  %r9,  %r3		\n"
	"	add		%r4,  %r4,  %r5		\n"
	"	add		%r3,  %r3,  %r5		\n"
	"	bdnz		._L11			\n"
	"	li		%r9,  7			\n"
	"	lvx		%v0,  %r9,  %r4		\n"
	"	lvx		%v13, 0,    %r4		\n"
	"	stvewx		%v12, 0,    %r3		\n"
	"	li		%r9,  4			\n"
	"	vperm		%v1,  %v13, %v0,  %v11	\n"
	"	stvewx		%v12, %r9,  %r3		\n"
	"	add		%r3,  %r3,  %r5		\n"
	"	stvewx		%v1,  0,    %r3		\n"
	"	stvewx		%v1,  %r9,  %r3		\n"
	 );
}

static void MC_put_x_16_altivec (uint8_t * dest, const uint8_t * ref,
				 int stride, int height)
{
    asm ("						\n"
	"	lvsl		%v11, 0,    %r4		\n"
	"	vspltisb	%v0,  1			\n"
	"	li		%r9,  16		\n"
	"	lvx		%v12, 0,    %r4		\n"
	"	vaddubm		%v10, %v11, %v0		\n"
	"	lvx		%v13, %r9,  %r4		\n"
	"	srawi		%r6,  %r6,  1		\n"
	"	addi		%r6,  %r6,  -1		\n"
	"	vperm		%v1,  %v12, %v13, %v10	\n"
	"	vperm		%v0,  %v12, %v13, %v11	\n"
	"	mtctr		%r6			\n"
	"	add		%r0,  %r5,  %r5		\n"
	"	add		%r4,  %r4,  %r5		\n"
	"	vavgub		%v0,  %v0,  %v1		\n"
	"._L16:						\n"
	"	li		%r9,  16		\n"
	"	lvx		%v12, 0,    %r4		\n"
	"	lvx		%v13, %r9,  %r4		\n"
	"	stvx		%v0,  0,    %r3		\n"
	"	vperm		%v1,  %v12, %v13, %v10	\n"
	"	add		%r4,  %r4,  %r5		\n"
	"	vperm		%v0,  %v12, %v13, %v11	\n"
	"	lvx		%v12, 0,    %r4		\n"
	"	lvx		%v13, %r9,  %r4		\n"
	"	vavgub		%v0,  %v0,  %v1		\n"
	"	stvx		%v0,  %r5,  %r3		\n"
	"	vperm		%v1,  %v12, %v13, %v10	\n"
	"	add		%r4,  %r4,  %r5		\n"
	"	vperm		%v0,  %v12, %v13, %v11	\n"
	"	add		%r3,  %r3,  %r0		\n"
	"	vavgub		%v0,  %v0,  %v1		\n"
	"	bdnz		._L16			\n"
	"	lvx		%v13, %r9,  %r4		\n"
	"	lvx		%v12, 0,    %r4		\n"
	"	stvx		%v0,  0,    %r3		\n"
	"	vperm		%v1,  %v12, %v13, %v10	\n"
	"	vperm		%v0,  %v12, %v13, %v11	\n"
	"	vavgub		%v0,  %v0,  %v1		\n"
	"	stvx		%v0,  %r5,  %r3		\n"
	 );
}

static void MC_put_x_8_altivec (uint8_t * dest, const uint8_t * ref,
				int stride, int height)
{
    asm ("						\n"
	"	lvsl		%v0,  0,    %r4		\n"
	"	vspltisb	%v13, 1			\n"
	"	lvsl		%v10, %r5,  %r4		\n"
	"	vmrghb		%v0,  %v0,  %v0		\n"
	"	li		%r9,  8			\n"
	"	lvx		%v11, 0,    %r4		\n"
	"	vmrghb		%v10, %v10, %v10	\n"
	"	vpkuhum		%v8,  %v0,  %v0		\n"
	"	lvx		%v12, %r9,  %r4		\n"
	"	srawi		%r6,  %r6,  1		\n"
	"	vpkuhum		%v9,  %v10, %v10	\n"
	"	vaddubm		%v7,  %v8,  %v13	\n"
	"	addi		%r6,  %r6,  -1		\n"
	"	vperm		%v1,  %v11, %v12, %v8	\n"
	"	mtctr		%r6			\n"
	"	vaddubm		%v13, %v9,  %v13	\n"
	"	add		%r4,  %r4,  %r5		\n"
	"	vperm		%v0,  %v11, %v12, %v7	\n"
	"	vavgub		%v0,  %v1,  %v0		\n"
	"._L21:						\n"
	"	li		%r9,  8			\n"
	"	lvx		%v12, %r9,  %r4		\n"
	"	lvx		%v11, 0,    %r4		\n"
	"	stvewx		%v0,  0,    %r3		\n"
	"	li		%r9,  4			\n"
	"	vperm		%v1,  %v11, %v12, %v13	\n"
	"	stvewx		%v0,  %r9,  %r3		\n"
	"	vperm		%v0,  %v11, %v12, %v9	\n"
	"	add		%r4,  %r4,  %r5		\n"
	"	li		%r9,  8			\n"
	"	lvx		%v12, %r9,  %r4		\n"
	"	vavgub		%v10, %v0,  %v1		\n"
	"	lvx		%v11, 0,    %r4		\n"
	"	add		%r3,  %r3,  %r5		\n"
	"	stvewx		%v10, 0,    %r3		\n"
	"	vperm		%v1,  %v11, %v12, %v7	\n"
	"	vperm		%v0,  %v11, %v12, %v8	\n"
	"	li		%r9,  4			\n"
	"	stvewx		%v10, %r9,  %r3		\n"
	"	add		%r4,  %r4,  %r5		\n"
	"	vavgub		%v0,  %v0,  %v1		\n"
	"	add		%r3,  %r3,  %r5		\n"
	"	bdnz		._L21			\n"
	"	li		%r9,  8			\n"
	"	lvx		%v12, %r9,  %r4		\n"
	"	lvx		%v11, 0,    %r4		\n"
	"	stvewx		%v0,  0,    %r3		\n"
	"	li		%r9,  4			\n"
	"	vperm		%v1,  %v11, %v12, %v13	\n"
	"	stvewx		%v0,  %r9,  %r3		\n"
	"	vperm		%v0,  %v11, %v12, %v9	\n"
	"	add		%r3,  %r3,  %r5		\n"
	"	vavgub		%v10, %v0,  %v1		\n"
	"	stvewx		%v10, 0,    %r3		\n"
	"	stvewx		%v10, %r9,  %r3		\n"
	 );
}

static void MC_put_y_16_altivec (uint8_t * dest, const uint8_t * ref,
				 int stride, int height)
{
    asm ("						\n"
	"	li		%r9,  15		\n"
	"	lvsl		%v10, 0,    %r4		\n"
	"	lvx		%v13, 0,    %r4		\n"
	"	lvx		%v1,  %r9,  %r4		\n"
	"	add		%r4,  %r4,  %r5		\n"
	"	vperm		%v12, %v13, %v1,  %v10	\n"
	"	srawi		%r6,  %r6,  1		\n"
	"	lvx		%v13, 0,    %r4		\n"
	"	lvx		%v1,  %r9,  %r4		\n"
	"	addi		%r6,  %r6,  -1		\n"
	"	vperm		%v11, %v13, %v1,  %v10	\n"
	"	mtctr		%r6			\n"
	"	add		%r0,  %r5,  %r5		\n"
	"	add		%r4,  %r4,  %r5		\n"
	"	vavgub		%v0,  %v12, %v11	\n"
	"._L26:						\n"
	"	li		%r9,  15		\n"
	"	lvx		%v13, 0,    %r4		\n"
	"	lvx		%v1,  %r9,  %r4		\n"
	"	stvx		%v0,  0,    %r3		\n"
	"	vperm		%v12, %v13, %v1,  %v10	\n"
	"	add		%r4,  %r4,  %r5		\n"
	"	lvx		%v13, 0,    %r4		\n"
	"	lvx		%v1,  %r9,  %r4		\n"
	"	vavgub		%v0,  %v12, %v11	\n"
	"	stvx		%v0,  %r5,  %r3		\n"
	"	vperm		%v11, %v13, %v1,  %v10	\n"
	"	add		%r4,  %r4,  %r5		\n"
	"	add		%r3,  %r3,  %r0		\n"
	"	vavgub		%v0,  %v12, %v11	\n"
	"	bdnz		._L26			\n"
	"	lvx		%v1,  %r9,  %r4		\n"
	"	lvx		%v13, 0,    %r4		\n"
	"	stvx		%v0,  0,    %r3		\n"
	"	vperm		%v12, %v13, %v1,  %v10	\n"
	"	vavgub		%v0,  %v12, %v11	\n"
	"	stvx		%v0,  %r5,  %r3		\n"
	 );
}

static void MC_put_y_8_altivec (uint8_t * dest, const uint8_t * ref,
				int stride, int height)
{
    asm ("						\n"
	"	lvsl		%v13, 0,    %r4		\n"
	"	lvsl		%v11, %r5,  %r4		\n"
	"	vmrghb		%v13, %v13, %v13	\n"
	"	li		%r9,  7			\n"
	"	lvx		%v12, 0,    %r4		\n"
	"	vmrghb		%v11, %v11, %v11	\n"
	"	lvx		%v1,  %r9,  %r4		\n"
	"	vpkuhum		%v9,  %v13, %v13	\n"
	"	add		%r4,  %r4,  %r5		\n"
	"	vpkuhum		%v10, %v11, %v11	\n"
	"	vperm		%v13, %v12, %v1,  %v9	\n"
	"	srawi		%r6,  %r6,  1		\n"
	"	lvx		%v12, 0,    %r4		\n"
	"	lvx		%v1,  %r9,  %r4		\n"
	"	addi		%r6,  %r6,  -1		\n"
	"	vperm		%v11, %v12, %v1,  %v10	\n"
	"	mtctr		%r6			\n"
	"	add		%r4,  %r4,  %r5		\n"
	"	vavgub		%v0,  %v13, %v11	\n"
	"._L31:						\n"
	"	li		%r9,  7			\n"
	"	lvx		%v1,  %r9,  %r4		\n"
	"	lvx		%v12, 0,    %r4		\n"
	"	stvewx		%v0,  0,    %r3		\n"
	"	li		%r9,  4			\n"
	"	vperm		%v13, %v12, %v1,  %v9	\n"
	"	stvewx		%v0,  %r9,  %r3		\n"
	"	add		%r4,  %r4,  %r5		\n"
	"	vavgub		%v0,  %v13, %v11	\n"
	"	li		%r9,  7			\n"
	"	lvx		%v1,  %r9,  %r4		\n"
	"	lvx		%v12, 0,    %r4		\n"
	"	add		%r3,  %r3,  %r5		\n"
	"	stvewx		%v0,  0,    %r3		\n"
	"	vperm		%v11, %v12, %v1,  %v10	\n"
	"	li		%r9,  4			\n"
	"	stvewx		%v0,  %r9,  %r3		\n"
	"	vavgub		%v0,  %v13, %v11	\n"
	"	add		%r4,  %r4,  %r5		\n"
	"	add		%r3,  %r3,  %r5		\n"
	"	bdnz		._L31			\n"
	"	li		%r9,  7			\n"
	"	lvx		%v1,  %r9,  %r4		\n"
	"	lvx		%v12, 0,    %r4		\n"
	"	stvewx		%v0,  0,    %r3		\n"
	"	li		%r9,  4			\n"
	"	vperm		%v13, %v12, %v1,  %v9	\n"
	"	stvewx		%v0,  %r9,  %r3		\n"
	"	add		%r3,  %r3,  %r5		\n"
	"	vavgub		%v0,  %v13, %v11	\n"
	"	stvewx		%v0,  0,    %r3		\n"
	"	stvewx		%v0,  %r9,  %r3		\n"
	 );
}

static void MC_put_xy_16_altivec (uint8_t * dest, const uint8_t * ref,
				  int stride, int height)
{
    asm ("						\n"
	"	lvsl		%v5,  0,    %r4		\n"
	"	vspltisb	%v3,  1			\n"
	"	li		%r9,  16		\n"
	"	lvx		%v1,  0,    %r4		\n"
	"	vaddubm		%v4,  %v5,  %v3		\n"
	"	lvx		%v0,  %r9,  %r4		\n"
	"	add		%r4,  %r4,  %r5		\n"
	"	vperm		%v10, %v1,  %v0,  %v4	\n"
	"	srawi		%r6,  %r6,  1		\n"
	"	vperm		%v11, %v1,  %v0,  %v5	\n"
	"	addi		%r6,  %r6,  -1		\n"
	"	lvx		%v1,  0,    %r4		\n"
	"	mtctr		%r6			\n"
	"	lvx		%v0,  %r9,  %r4		\n"
	"	vavgub		%v9,  %v11, %v10	\n"
	"	vxor		%v8,  %v11, %v10	\n"
	"	add		%r0,  %r5,  %r5		\n"
	"	vperm		%v10, %v1,  %v0,  %v4	\n"
	"	add		%r4,  %r4,  %r5		\n"
	"	vperm		%v11, %v1,  %v0,  %v5	\n"
	"	vxor		%v6,  %v11, %v10	\n"
	"	vavgub		%v7,  %v11, %v10	\n"
	"	vor		%v0,  %v8,  %v6		\n"
	"	vxor		%v13, %v9,  %v7		\n"
	"	vand		%v0,  %v3,  %v0		\n"
	"	vavgub		%v1,  %v9,  %v7		\n"
	"	vand		%v0,  %v0,  %v13	\n"
	"	vsububm		%v13, %v1,  %v0		\n"
	"._L36:						\n"
	"	li		%r9,  16		\n"
	"	lvx		%v1,  0,    %r4		\n"
	"	lvx		%v0,  %r9,  %r4		\n"
	"	stvx		%v13, 0,    %r3		\n"
	"	vperm		%v10, %v1,  %v0,  %v4	\n"
	"	add		%r4,  %r4,  %r5		\n"
	"	vperm		%v11, %v1,  %v0,  %v5	\n"
	"	lvx		%v1,  0,    %r4		\n"
	"	lvx		%v0,  %r9,  %r4		\n"
	"	vavgub		%v9,  %v11, %v10	\n"
	"	vxor		%v8,  %v11, %v10	\n"
	"	add		%r4,  %r4,  %r5		\n"
	"	vperm		%v10, %v1,  %v0,  %v4	\n"
	"	vavgub		%v12, %v9,  %v7		\n"
	"	vperm		%v11, %v1,  %v0,  %v5	\n"
	"	vor		%v13, %v8,  %v6		\n"
	"	vxor		%v0,  %v9,  %v7		\n"
	"	vxor		%v6,  %v11, %v10	\n"
	"	vand		%v13, %v3,  %v13	\n"
	"	vavgub		%v7,  %v11, %v10	\n"
	"	vor		%v1,  %v8,  %v6		\n"
	"	vand		%v13, %v13, %v0		\n"
	"	vxor		%v0,  %v9,  %v7		\n"
	"	vand		%v1,  %v3,  %v1		\n"
	"	vsububm		%v13, %v12, %v13	\n"
	"	vand		%v1,  %v1,  %v0		\n"
	"	stvx		%v13, %r5,  %r3		\n"
	"	vavgub		%v0,  %v9,  %v7		\n"
	"	add		%r3,  %r3,  %r0		\n"
	"	vsububm		%v13, %v0,  %v1		\n"
	"	bdnz		._L36			\n"
	"	lvx		%v0,  %r9,  %r4		\n"
	"	lvx		%v1,  0,    %r4		\n"
	"	stvx		%v13, 0,    %r3		\n"
	"	vperm		%v10, %v1,  %v0,  %v4	\n"
	"	vperm		%v11, %v1,  %v0,  %v5	\n"
	"	vxor		%v8,  %v11, %v10	\n"
	"	vavgub		%v9,  %v11, %v10	\n"
	"	vor		%v0,  %v8,  %v6		\n"
	"	vxor		%v13, %v9,  %v7		\n"
	"	vand		%v0,  %v3,  %v0		\n"
	"	vavgub		%v1,  %v9,  %v7		\n"
	"	vand		%v0,  %v0,  %v13	\n"
	"	vsububm		%v13, %v1,  %v0		\n"
	"	stvx		%v13, %r5,  %r3		\n"
	 );
}

static void MC_put_xy_8_altivec (uint8_t * dest, const uint8_t * ref,
				 int stride, int height)
{
    asm ("						\n"
	"	lvsl		%v4,  0,    %r4		\n"
	"	vspltisb	%v3,  1			\n"
	"	lvsl		%v5,  %r5,  %r4		\n"
	"	vmrghb		%v4,  %v4,  %v4		\n"
	"	li		%r9,  8			\n"
	"	vmrghb		%v5,  %v5,  %v5		\n"
	"	lvx		%v1,  0,    %r4		\n"
	"	vpkuhum		%v4,  %v4,  %v4		\n"
	"	lvx		%v0,  %r9,  %r4		\n"
	"	vpkuhum		%v5,  %v5,  %v5		\n"
	"	add		%r4,  %r4,  %r5		\n"
	"	vaddubm		%v2,  %v4,  %v3		\n"
	"	vperm		%v11, %v1,  %v0,  %v4	\n"
	"	srawi		%r6,  %r6,  1		\n"
	"	vaddubm		%v19, %v5,  %v3		\n"
	"	addi		%r6,  %r6,  -1		\n"
	"	vperm		%v10, %v1,  %v0,  %v2	\n"
	"	mtctr		%r6			\n"
	"	lvx		%v1,  0,    %r4		\n"
	"	lvx		%v0,  %r9,  %r4		\n"
	"	vavgub		%v9,  %v11, %v10	\n"
	"	vxor		%v8,  %v11, %v10	\n"
	"	add		%r4,  %r4,  %r5		\n"
	"	vperm		%v10, %v1,  %v0,  %v19	\n"
	"	vperm		%v11, %v1,  %v0,  %v5	\n"
	"	vxor		%v6,  %v11, %v10	\n"
	"	vavgub		%v7,  %v11, %v10	\n"
	"	vor		%v0,  %v8,  %v6		\n"
	"	vxor		%v13, %v9,  %v7		\n"
	"	vand		%v0,  %v3,  %v0		\n"
	"	vavgub		%v1,  %v9,  %v7		\n"
	"	vand		%v0,  %v0,  %v13	\n"
	"	vsububm		%v13, %v1,  %v0		\n"
	"._L41:						\n"
	"	li		%r9,  8			\n"
	"	lvx		%v0,  %r9,  %r4		\n"
	"	lvx		%v1,  0,    %r4		\n"
	"	stvewx		%v13, 0,    %r3		\n"
	"	li		%r9,  4			\n"
	"	vperm		%v10, %v1,  %v0,  %v2	\n"
	"	stvewx		%v13, %r9,  %r3		\n"
	"	vperm		%v11, %v1,  %v0,  %v4	\n"
	"	add		%r4,  %r4,  %r5		\n"
	"	li		%r9,  8			\n"
	"	vavgub		%v9,  %v11, %v10	\n"
	"	lvx		%v0,  %r9,  %r4		\n"
	"	vxor		%v8,  %v11, %v10	\n"
	"	lvx		%v1,  0,    %r4		\n"
	"	vavgub		%v12, %v9,  %v7		\n"
	"	vor		%v13, %v8,  %v6		\n"
	"	add		%r3,  %r3,  %r5		\n"
	"	vperm		%v10, %v1,  %v0,  %v19	\n"
	"	li		%r9,  4			\n"
	"	vperm		%v11, %v1,  %v0,  %v5	\n"
	"	vand		%v13, %v3,  %v13	\n"
	"	add		%r4,  %r4,  %r5		\n"
	"	vxor		%v0,  %v9,  %v7		\n"
	"	vxor		%v6,  %v11, %v10	\n"
	"	vavgub		%v7,  %v11, %v10	\n"
	"	vor		%v1,  %v8,  %v6		\n"
	"	vand		%v13, %v13, %v0		\n"
	"	vxor		%v0,  %v9,  %v7		\n"
	"	vand		%v1,  %v3,  %v1		\n"
	"	vsububm		%v13, %v12, %v13	\n"
	"	vand		%v1,  %v1,  %v0		\n"
	"	stvewx		%v13, 0,    %r3		\n"
	"	vavgub		%v0,  %v9,  %v7		\n"
	"	stvewx		%v13, %r9,  %r3		\n"
	"	add		%r3,  %r3,  %r5		\n"
	"	vsububm		%v13, %v0,  %v1		\n"
	"	bdnz		._L41			\n"
	"	li		%r9,  8			\n"
	"	lvx		%v0,  %r9,  %r4		\n"
	"	lvx		%v1,  0,    %r4		\n"
	"	stvewx		%v13, 0,    %r3		\n"
	"	vperm		%v10, %v1,  %v0,  %v2	\n"
	"	li		%r9,  4			\n"
	"	vperm		%v11, %v1,  %v0,  %v4	\n"
	"	stvewx		%v13, %r9,  %r3		\n"
	"	add		%r3,  %r3,  %r5		\n"
	"	vxor		%v8,  %v11, %v10	\n"
	"	vavgub		%v9,  %v11, %v10	\n"
	"	vor		%v0,  %v8,  %v6		\n"
	"	vxor		%v13, %v9,  %v7		\n"
	"	vand		%v0,  %v3,  %v0		\n"
	"	vavgub		%v1,  %v9,  %v7		\n"
	"	vand		%v0,  %v0,  %v13	\n"
	"	vsububm		%v13, %v1,  %v0		\n"
	"	stvewx		%v13, 0,    %r3		\n"
	"	stvewx		%v13, %r9,  %r3		\n"
	 );
}

static void MC_avg_o_16_altivec (uint8_t * dest, const uint8_t * ref,
				 int stride, int height)
{
    asm ("						\n"
	"	li		%r9,  15		\n"
	"	lvx		%v0,  %r9,  %r4		\n"
	"	lvsl		%v11, 0,    %r4		\n"
	"	lvx		%v1,  0,    %r4		\n"
	"	srawi		%r6,  %r6,  1		\n"
	"	addi		%r6,  %r6,  -1		\n"
	"	vperm		%v0,  %v1,  %v0,  %v11	\n"
	"	lvx		%v13, 0,    %r3		\n"
	"	mtctr		%r6			\n"
	"	add		%r9,  %r5,  %r5		\n"
	"	vavgub		%v12, %v13, %v0		\n"
	"	add		%r4,  %r4,  %r5		\n"
	"._L46:						\n"
	"	li		%r11, 15		\n"
	"	lvx		%v1,  0,    %r4		\n"
	"	lvx		%v0,  %r11, %r4		\n"
	"	lvx		%v13, %r5,  %r3		\n"
	"	vperm		%v0,  %v1,  %v0,  %v11	\n"
	"	stvx		%v12, 0,    %r3		\n"
	"	add		%r4,  %r4,  %r5		\n"
	"	vavgub		%v12, %v13, %v0		\n"
	"	lvx		%v1,  0,    %r4		\n"
	"	lvx		%v0,  %r11, %r4		\n"
	"	lvx		%v13, %r9,  %r3		\n"
	"	vperm		%v0,  %v1,  %v0,  %v11	\n"
	"	stvx		%v12, %r5,  %r3		\n"
	"	add		%r4,  %r4,  %r5		\n"
	"	vavgub		%v12, %v13, %v0		\n"
	"	add		%r3,  %r3,  %r9		\n"
	"	bdnz		._L46			\n"
	"	lvx		%v0,  %r11, %r4		\n"
	"	lvx		%v1,  0,    %r4		\n"
	"	lvx		%v13, %r5,  %r3		\n"
	"	vperm		%v0,  %v1,  %v0,  %v11	\n"
	"	stvx		%v12, 0,    %r3		\n"
	"	vavgub		%v12, %v13, %v0		\n"
	"	stvx		%v12, %r5,  %r3		\n"
	 );
}

static void MC_avg_o_8_altivec (uint8_t * dest, const uint8_t * ref,
				int stride, int height)
{
    asm ("						\n"
	"	lvsl		%v12, 0,    %r4		\n"
	"	li		%r9,  7			\n"
	"	vmrghb		%v12, %v12, %v12	\n"
	"	lvsl		%v1,  %r5,  %r4		\n"
	"	lvx		%v13, 0,    %r4		\n"
	"	vpkuhum		%v9,  %v12, %v12	\n"
	"	lvx		%v0,  %r9,  %r4		\n"
	"	srawi		%r6,  %r6,  1		\n"
	"	vmrghb		%v1,  %v1,  %v1		\n"
	"	addi		%r6,  %r6,  -1		\n"
	"	vperm		%v0,  %v13, %v0,  %v9	\n"
	"	lvx		%v11, 0,    %r3		\n"
	"	mtctr		%r6			\n"
	"	vpkuhum		%v10, %v1,  %v1		\n"
	"	add		%r4,  %r4,  %r5		\n"
	"	vavgub		%v12, %v11, %v0		\n"
	"._L51:						\n"
	"	li		%r9,  7			\n"
	"	lvx		%v0,  %r9,  %r4		\n"
	"	lvx		%v13, 0,    %r4		\n"
	"	lvx		%v11, %r5,  %r3		\n"
	"	stvewx		%v12, 0,    %r3		\n"
	"	vperm		%v0,  %v13, %v0,  %v10	\n"
	"	li		%r9,  4			\n"
	"	stvewx		%v12, %r9,  %r3		\n"
	"	vavgub		%v1,  %v11, %v0		\n"
	"	add		%r4,  %r4,  %r5		\n"
	"	li		%r9,  7			\n"
	"	lvx		%v0,  %r9,  %r4		\n"
	"	add		%r3,  %r3,  %r5		\n"
	"	lvx		%v13, 0,    %r4		\n"
	"	lvx		%v11, %r5,  %r3		\n"
	"	stvewx		%v1,  0,    %r3		\n"
	"	vperm		%v0,  %v13, %v0,  %v9	\n"
	"	li		%r9,  4			\n"
	"	stvewx		%v1,  %r9,  %r3		\n"
	"	vavgub		%v12, %v11, %v0		\n"
	"	add		%r4,  %r4,  %r5		\n"
	"	add		%r3,  %r3,  %r5		\n"
	"	bdnz		._L51			\n"
	"	li		%r9,  7			\n"
	"	lvx		%v0,  %r9,  %r4		\n"
	"	lvx		%v13, 0,    %r4		\n"
	"	lvx		%v11, %r5,  %r3		\n"
	"	stvewx		%v12, 0,    %r3		\n"
	"	vperm		%v0,  %v13, %v0,  %v10	\n"
	"	li		%r9,  4			\n"
	"	stvewx		%v12, %r9,  %r3		\n"
	"	vavgub		%v1,  %v11, %v0		\n"
	"	add		%r3,  %r3,  %r5		\n"
	"	stvewx		%v1,  0,    %r3		\n"
	"	stvewx		%v1,  %r9,  %r3		\n"
	 );
}

static void MC_avg_x_16_altivec (uint8_t * dest, const uint8_t * ref,
				 int stride, int height)
{
    asm ("						\n"
	"	lvsl		%v8,  0,    %r4		\n"
	"	vspltisb	%v0,  1			\n"
	"	li		%r9,  16		\n"
	"	lvx		%v12, %r9,  %r4		\n"
	"	vaddubm		%v7,  %v8,  %v0		\n"
	"	lvx		%v11, 0,    %r4		\n"
	"	srawi		%r6,  %r6,  1		\n"
	"	vperm		%v1,  %v11, %v12, %v7	\n"
	"	addi		%r6,  %r6,  -1		\n"
	"	vperm		%v0,  %v11, %v12, %v8	\n"
	"	lvx		%v9,  0,    %r3		\n"
	"	mtctr		%r6			\n"
	"	add		%r9,  %r5,  %r5		\n"
	"	vavgub		%v0,  %v0,  %v1		\n"
	"	add		%r4,  %r4,  %r5		\n"
	"	vavgub		%v10, %v9,  %v0		\n"
	"._L56:						\n"
	"	li		%r11, 16		\n"
	"	lvx		%v11, 0,    %r4		\n"
	"	lvx		%v12, %r11, %r4		\n"
	"	lvx		%v9,  %r5,  %r3		\n"
	"	stvx		%v10, 0,    %r3		\n"
	"	vperm		%v0,  %v11, %v12, %v7	\n"
	"	add		%r4,  %r4,  %r5		\n"
	"	vperm		%v1,  %v11, %v12, %v8	\n"
	"	lvx		%v11, 0,    %r4		\n"
	"	lvx		%v12, %r11, %r4		\n"
	"	vavgub		%v1,  %v1,  %v0		\n"
	"	add		%r4,  %r4,  %r5		\n"
	"	vperm		%v13, %v11, %v12, %v7	\n"
	"	vavgub		%v10, %v9,  %v1		\n"
	"	vperm		%v0,  %v11, %v12, %v8	\n"
	"	lvx		%v9,  %r9,  %r3		\n"
	"	stvx		%v10, %r5,  %r3		\n"
	"	vavgub		%v0,  %v0,  %v13	\n"
	"	add		%r3,  %r3,  %r9		\n"
	"	vavgub		%v10, %v9,  %v0		\n"
	"	bdnz		._L56			\n"
	"	lvx		%v12, %r11, %r4		\n"
	"	lvx		%v11, 0,    %r4		\n"
	"	lvx		%v9,  %r5,  %r3		\n"
	"	vperm		%v1,  %v11, %v12, %v7	\n"
	"	stvx		%v10, 0,    %r3		\n"
	"	vperm		%v0,  %v11, %v12, %v8	\n"
	"	vavgub		%v0,  %v0,  %v1		\n"
	"	vavgub		%v10, %v9,  %v0		\n"
	"	stvx		%v10, %r5,  %r3		\n"
	 );
}

static void MC_avg_x_8_altivec (uint8_t * dest, const uint8_t * ref,
				int stride, int height)
{
    asm ("						\n"
	"	lvsl		%v10, 0,    %r4		\n"
	"	vspltisb	%v13, 1			\n"
	"	li		%r9,  8			\n"
	"	vmrghb		%v10, %v10, %v10	\n"
	"	lvx		%v11, 0,    %r4		\n"
	"	lvx		%v12, %r9,  %r4		\n"
	"	vpkuhum		%v7,  %v10, %v10	\n"
	"	srawi		%r6,  %r6,  1		\n"
	"	lvsl		%v10, %r5,  %r4		\n"
	"	vaddubm		%v6,  %v7,  %v13	\n"
	"	vperm		%v0,  %v11, %v12, %v7	\n"
	"	addi		%r6,  %r6,  -1		\n"
	"	vmrghb		%v10, %v10, %v10	\n"
	"	lvx		%v9,  0,    %r3		\n"
	"	mtctr		%r6			\n"
	"	vperm		%v1,  %v11, %v12, %v6	\n"
	"	add		%r4,  %r4,  %r5		\n"
	"	vpkuhum		%v8,  %v10, %v10	\n"
	"	vavgub		%v0,  %v0,  %v1		\n"
	"	vaddubm		%v13, %v8,  %v13	\n"
	"	vavgub		%v10, %v9,  %v0		\n"
	"._L61:						\n"
	"	li		%r9,  8			\n"
	"	lvx		%v12, %r9,  %r4		\n"
	"	lvx		%v11, 0,    %r4		\n"
	"	lvx		%v9,  %r5,  %r3		\n"
	"	stvewx		%v10, 0,    %r3		\n"
	"	vperm		%v1,  %v11, %v12, %v13	\n"
	"	vperm		%v0,  %v11, %v12, %v8	\n"
	"	li		%r9,  4			\n"
	"	stvewx		%v10, %r9,  %r3		\n"
	"	add		%r4,  %r4,  %r5		\n"
	"	vavgub		%v0,  %v0,  %v1		\n"
	"	li		%r9,  8			\n"
	"	lvx		%v12, %r9,  %r4		\n"
	"	vavgub		%v10, %v9,  %v0		\n"
	"	lvx		%v11, 0,    %r4		\n"
	"	add		%r3,  %r3,  %r5		\n"
	"	vperm		%v1,  %v11, %v12, %v6	\n"
	"	lvx		%v9,  %r5,  %r3		\n"
	"	vperm		%v0,  %v11, %v12, %v7	\n"
	"	stvewx		%v10, 0,    %r3		\n"
	"	li		%r9,  4			\n"
	"	vavgub		%v0,  %v0,  %v1		\n"
	"	stvewx		%v10, %r9,  %r3		\n"
	"	add		%r4,  %r4,  %r5		\n"
	"	add		%r3,  %r3,  %r5		\n"
	"	vavgub		%v10, %v9,  %v0		\n"
	"	bdnz		._L61			\n"
	"	li		%r9,  8			\n"
	"	lvx		%v12, %r9,  %r4		\n"
	"	lvx		%v11, 0,    %r4		\n"
	"	lvx		%v9,  %r5,  %r3		\n"
	"	vperm		%v1,  %v11, %v12, %v13	\n"
	"	stvewx		%v10, 0,    %r3		\n"
	"	vperm		%v0,  %v11, %v12, %v8	\n"
	"	li		%r9,  4			\n"
	"	stvewx		%v10, %r9,  %r3		\n"
	"	vavgub		%v0,  %v0,  %v1		\n"
	"	add		%r3,  %r3,  %r5		\n"
	"	vavgub		%v10, %v9,  %v0		\n"
	"	stvewx		%v10, 0,    %r3		\n"
	"	stvewx		%v10, %r9,  %r3		\n"
	 );
}

static void MC_avg_y_16_altivec (uint8_t * dest, const uint8_t * ref,
				 int stride, int height)
{
    asm ("						\n"
	"	li		%r9,  15		\n"
	"	lvx		%v1,  %r9,  %r4		\n"
	"	lvsl		%v9,  0,    %r4		\n"
	"	lvx		%v13, 0,    %r4		\n"
	"	add		%r4,  %r4,  %r5		\n"
	"	vperm		%v11, %v13, %v1,  %v9	\n"
	"	li		%r11, 15		\n"
	"	lvx		%v13, 0,    %r4		\n"
	"	lvx		%v1,  %r11, %r4		\n"
	"	srawi		%r6,  %r6,  1		\n"
	"	vperm		%v10, %v13, %v1,  %v9	\n"
	"	addi		%r6,  %r6,  -1		\n"
	"	lvx		%v12, 0,    %r3		\n"
	"	mtctr		%r6			\n"
	"	vavgub		%v0,  %v11, %v10	\n"
	"	add		%r9,  %r5,  %r5		\n"
	"	add		%r4,  %r4,  %r5		\n"
	"	vavgub		%v0,  %v12, %v0		\n"
	"._L66:						\n"
	"	li		%r11, 15		\n"
	"	lvx		%v13, 0,    %r4		\n"
	"	lvx		%v1,  %r11, %r4		\n"
	"	lvx		%v12, %r5,  %r3		\n"
	"	vperm		%v11, %v13, %v1,  %v9	\n"
	"	stvx		%v0,  0,    %r3		\n"
	"	add		%r4,  %r4,  %r5		\n"
	"	vavgub		%v0,  %v11, %v10	\n"
	"	lvx		%v13, 0,    %r4		\n"
	"	lvx		%v1,  %r11, %r4		\n"
	"	vavgub		%v0,  %v12, %v0		\n"
	"	add		%r4,  %r4,  %r5		\n"
	"	lvx		%v12, %r9,  %r3		\n"
	"	vperm		%v10, %v13, %v1,  %v9	\n"
	"	stvx		%v0,  %r5,  %r3		\n"
	"	vavgub		%v0,  %v11, %v10	\n"
	"	add		%r3,  %r3,  %r9		\n"
	"	vavgub		%v0,  %v12, %v0		\n"
	"	bdnz		._L66			\n"
	"	lvx		%v1,  %r11, %r4		\n"
	"	lvx		%v13, 0,    %r4		\n"
	"	lvx		%v12, %r5,  %r3		\n"
	"	vperm		%v11, %v13, %v1,  %v9	\n"
	"	stvx		%v0,  0,    %r3		\n"
	"	vavgub		%v0,  %v11, %v10	\n"
	"	vavgub		%v0,  %v12, %v0		\n"
	"	stvx		%v0,  %r5,  %r3		\n"
	 );
}

static void MC_avg_y_8_altivec (uint8_t * dest, const uint8_t * ref,
				int stride, int height)
{
    asm ("						\n"
	"	lvsl		%v12, 0,    %r4		\n"
	"	lvsl		%v9,  %r5,  %r4		\n"
	"	vmrghb		%v12, %v12, %v12	\n"
	"	li		%r9,  7			\n"
	"	lvx		%v11, 0,    %r4		\n"
	"	vmrghb		%v9,  %v9,  %v9		\n"
	"	lvx		%v13, %r9,  %r4		\n"
	"	vpkuhum		%v7,  %v12, %v12	\n"
	"	add		%r4,  %r4,  %r5		\n"
	"	vpkuhum		%v8,  %v9,  %v9		\n"
	"	vperm		%v12, %v11, %v13, %v7	\n"
	"	srawi		%r6,  %r6,  1		\n"
	"	lvx		%v11, 0,    %r4		\n"
	"	lvx		%v13, %r9,  %r4		\n"
	"	addi		%r6,  %r6,  -1		\n"
	"	vperm		%v9,  %v11, %v13, %v8	\n"
	"	lvx		%v10, 0,    %r3		\n"
	"	mtctr		%r6			\n"
	"	add		%r4,  %r4,  %r5		\n"
	"	vavgub		%v0,  %v12, %v9		\n"
	"	vavgub		%v1,  %v10, %v0		\n"
	"._L71:						\n"
	"	li		%r9,  7			\n"
	"	lvx		%v13, %r9,  %r4		\n"
	"	lvx		%v11, 0,    %r4		\n"
	"	lvx		%v10, %r5,  %r3		\n"
	"	stvewx		%v1,  0,    %r3		\n"
	"	vperm		%v12, %v11, %v13, %v7	\n"
	"	li		%r9,  4			\n"
	"	stvewx		%v1,  %r9,  %r3		\n"
	"	vavgub		%v0,  %v12, %v9		\n"
	"	add		%r4,  %r4,  %r5		\n"
	"	li		%r9,  7			\n"
	"	vavgub		%v1,  %v10, %v0		\n"
	"	lvx		%v13, %r9,  %r4		\n"
	"	lvx		%v11, 0,    %r4		\n"
	"	add		%r3,  %r3,  %r5		\n"
	"	vperm		%v9,  %v11, %v13, %v8	\n"
	"	lvx		%v10, %r5,  %r3		\n"
	"	stvewx		%v1,  0,    %r3		\n"
	"	vavgub		%v0,  %v12, %v9		\n"
	"	li		%r9,  4			\n"
	"	stvewx		%v1,  %r9,  %r3		\n"
	"	add		%r4,  %r4,  %r5		\n"
	"	vavgub		%v1,  %v10, %v0		\n"
	"	add		%r3,  %r3,  %r5		\n"
	"	bdnz		._L71			\n"
	"	li		%r9,  7			\n"
	"	lvx		%v13, %r9,  %r4		\n"
	"	lvx		%v11, 0,    %r4		\n"
	"	lvx		%v10, %r5,  %r3		\n"
	"	vperm		%v12, %v11, %v13, %v7	\n"
	"	stvewx		%v1,  0,    %r3		\n"
	"	li		%r9,  4			\n"
	"	vavgub		%v0,  %v12, %v9		\n"
	"	stvewx		%v1,  %r9,  %r3		\n"
	"	add		%r3,  %r3,  %r5		\n"
	"	vavgub		%v1,  %v10, %v0		\n"
	"	stvewx		%v1,  0,    %r3		\n"
	"	stvewx		%v1,  %r9,  %r3		\n"
	 );
}

static void MC_avg_xy_16_altivec (uint8_t * dest, const uint8_t * ref,
				  int stride, int height)
{
    asm ("						\n"
	"	lvsl		%v4,  0,    %r4		\n"
	"	vspltisb	%v2,  1			\n"
	"	li		%r9,  16		\n"
	"	lvx		%v1,  %r9,  %r4		\n"
	"	vaddubm		%v3,  %v4,  %v2		\n"
	"	lvx		%v13, 0,    %r4		\n"
	"	add		%r4,  %r4,  %r5		\n"
	"	vperm		%v10, %v13, %v1,  %v3	\n"
	"	li		%r11, 16		\n"
	"	vperm		%v11, %v13, %v1,  %v4	\n"
	"	srawi		%r6,  %r6,  1		\n"
	"	lvx		%v13, 0,    %r4		\n"
	"	lvx		%v1,  %r11, %r4		\n"
	"	vavgub		%v9,  %v11, %v10	\n"
	"	vxor		%v8,  %v11, %v10	\n"
	"	addi		%r6,  %r6,  -1		\n"
	"	vperm		%v10, %v13, %v1,  %v3	\n"
	"	lvx		%v6,  0,    %r3		\n"
	"	mtctr		%r6			\n"
	"	vperm		%v11, %v13, %v1,  %v4	\n"
	"	add		%r9,  %r5,  %r5		\n"
	"	add		%r4,  %r4,  %r5		\n"
	"	vxor		%v5,  %v11, %v10	\n"
	"	vavgub		%v7,  %v11, %v10	\n"
	"	vor		%v1,  %v8,  %v5		\n"
	"	vxor		%v13, %v9,  %v7		\n"
	"	vand		%v1,  %v2,  %v1		\n"
	"	vavgub		%v0,  %v9,  %v7		\n"
	"	vand		%v1,  %v1,  %v13	\n"
	"	vsububm		%v0,  %v0,  %v1		\n"
	"	vavgub		%v12, %v6,  %v0		\n"
	"._L76:						\n"
	"	li		%r11, 16		\n"
	"	lvx		%v13, 0,    %r4		\n"
	"	lvx		%v1,  %r11, %r4		\n"
	"	lvx		%v6,  %r5,  %r3		\n"
	"	stvx		%v12, 0,    %r3		\n"
	"	vperm		%v10, %v13, %v1,  %v3	\n"
	"	vperm		%v11, %v13, %v1,  %v4	\n"
	"	add		%r4,  %r4,  %r5		\n"
	"	lvx		%v13, 0,    %r4		\n"
	"	lvx		%v1,  %r11, %r4		\n"
	"	vavgub		%v9,  %v11, %v10	\n"
	"	vxor		%v8,  %v11, %v10	\n"
	"	add		%r4,  %r4,  %r5		\n"
	"	vperm		%v10, %v13, %v1,  %v3	\n"
	"	vavgub		%v12, %v9,  %v7		\n"
	"	vperm		%v11, %v13, %v1,  %v4	\n"
	"	vor		%v0,  %v8,  %v5		\n"
	"	vxor		%v13, %v9,  %v7		\n"
	"	vxor		%v5,  %v11, %v10	\n"
	"	vand		%v0,  %v2,  %v0		\n"
	"	vavgub		%v7,  %v11, %v10	\n"
	"	vor		%v1,  %v8,  %v5		\n"
	"	vand		%v0,  %v0,  %v13	\n"
	"	vand		%v1,  %v2,  %v1		\n"
	"	vxor		%v13, %v9,  %v7		\n"
	"	vsububm		%v12, %v12, %v0		\n"
	"	vand		%v1,  %v1,  %v13	\n"
	"	vavgub		%v0,  %v9,  %v7		\n"
	"	vavgub		%v12, %v6,  %v12	\n"
	"	lvx		%v6,  %r9,  %r3		\n"
	"	vsububm		%v0,  %v0,  %v1		\n"
	"	stvx		%v12, %r5,  %r3		\n"
	"	vavgub		%v12, %v6,  %v0		\n"
	"	add		%r3,  %r3,  %r9		\n"
	"	bdnz		._L76			\n"
	"	lvx		%v1,  %r11, %r4		\n"
	"	lvx		%v13, 0,    %r4		\n"
	"	lvx		%v6,  %r5,  %r3		\n"
	"	vperm		%v10, %v13, %v1,  %v3	\n"
	"	stvx		%v12, 0,    %r3		\n"
	"	vperm		%v11, %v13, %v1,  %v4	\n"
	"	vxor		%v8,  %v11, %v10	\n"
	"	vavgub		%v9,  %v11, %v10	\n"
	"	vor		%v0,  %v8,  %v5		\n"
	"	vxor		%v13, %v9,  %v7		\n"
	"	vand		%v0,  %v2,  %v0		\n"
	"	vavgub		%v1,  %v9,  %v7		\n"
	"	vand		%v0,  %v0,  %v13	\n"
	"	vsububm		%v1,  %v1,  %v0		\n"
	"	vavgub		%v12, %v6,  %v1		\n"
	"	stvx		%v12, %r5,  %r3		\n"
	 );
}

static void MC_avg_xy_8_altivec (uint8_t * dest, const uint8_t * ref,
				 int stride, int height)
{
    asm ("						\n"
	"	lvsl		%v2,  0,    %r4		\n"
	"	vspltisb	%v19, 1			\n"
	"	lvsl		%v3,  %r5,  %r4		\n"
	"	vmrghb		%v2,  %v2,  %v2		\n"
	"	li		%r9,  8			\n"
	"	vmrghb		%v3,  %v3,  %v3		\n"
	"	lvx		%v9,  0,    %r4		\n"
	"	vpkuhum		%v2,  %v2,  %v2		\n"
	"	lvx		%v1,  %r9,  %r4		\n"
	"	vpkuhum		%v3,  %v3,  %v3		\n"
	"	add		%r4,  %r4,  %r5		\n"
	"	vaddubm		%v18, %v2,  %v19	\n"
	"	vperm		%v11, %v9,  %v1,  %v2	\n"
	"	srawi		%r6,  %r6,  1		\n"
	"	vaddubm		%v17, %v3,  %v19	\n"
	"	addi		%r6,  %r6,  -1		\n"
	"	vperm		%v10, %v9,  %v1,  %v18	\n"
	"	lvx		%v4,  0,    %r3		\n"
	"	mtctr		%r6			\n"
	"	lvx		%v1,  %r9,  %r4		\n"
	"	lvx		%v9,  0,    %r4		\n"
	"	vavgub		%v8,  %v11, %v10	\n"
	"	vxor		%v7,  %v11, %v10	\n"
	"	add		%r4,  %r4,  %r5		\n"
	"	vperm		%v10, %v9,  %v1,  %v17	\n"
	"	vperm		%v11, %v9,  %v1,  %v3	\n"
	"	vxor		%v5,  %v11, %v10	\n"
	"	vavgub		%v6,  %v11, %v10	\n"
	"	vor		%v1,  %v7,  %v5		\n"
	"	vxor		%v13, %v8,  %v6		\n"
	"	vand		%v1,  %v19, %v1		\n"
	"	vavgub		%v0,  %v8,  %v6		\n"
	"	vand		%v1,  %v1,  %v13	\n"
	"	vsububm		%v0,  %v0,  %v1		\n"
	"	vavgub		%v13, %v4,  %v0		\n"
	"._L81:						\n"
	"	li		%r9,  8			\n"
	"	lvx		%v1,  %r9,  %r4		\n"
	"	lvx		%v9,  0,    %r4		\n"
	"	lvx		%v4,  %r5,  %r3		\n"
	"	stvewx		%v13, 0,    %r3		\n"
	"	vperm		%v10, %v9,  %v1,  %v18	\n"
	"	vperm		%v11, %v9,  %v1,  %v2	\n"
	"	li		%r9,  4			\n"
	"	stvewx		%v13, %r9,  %r3		\n"
	"	vxor		%v7,  %v11, %v10	\n"
	"	add		%r4,  %r4,  %r5		\n"
	"	li		%r9,  8			\n"
	"	vavgub		%v8,  %v11, %v10	\n"
	"	lvx		%v1,  %r9,  %r4		\n"
	"	vor		%v0,  %v7,  %v5		\n"
	"	lvx		%v9,  0,    %r4		\n"
	"	vxor		%v12, %v8,  %v6		\n"
	"	vand		%v0,  %v19, %v0		\n"
	"	add		%r3,  %r3,  %r5		\n"
	"	vperm		%v10, %v9,  %v1,  %v17	\n"
	"	vavgub		%v13, %v8,  %v6		\n"
	"	li		%r9,  4			\n"
	"	vperm		%v11, %v9,  %v1,  %v3	\n"
	"	vand		%v0,  %v0,  %v12	\n"
	"	add		%r4,  %r4,  %r5		\n"
	"	vxor		%v5,  %v11, %v10	\n"
	"	vavgub		%v6,  %v11, %v10	\n"
	"	vor		%v1,  %v7,  %v5		\n"
	"	vsububm		%v13, %v13, %v0		\n"
	"	vxor		%v0,  %v8,  %v6		\n"
	"	vand		%v1,  %v19, %v1		\n"
	"	vavgub		%v13, %v4,  %v13	\n"
	"	vand		%v1,  %v1,  %v0		\n"
	"	lvx		%v4,  %r5,  %r3		\n"
	"	vavgub		%v0,  %v8,  %v6		\n"
	"	stvewx		%v13, 0,    %r3		\n"
	"	stvewx		%v13, %r9,  %r3		\n"
	"	vsububm		%v0,  %v0,  %v1		\n"
	"	add		%r3,  %r3,  %r5		\n"
	"	vavgub		%v13, %v4,  %v0		\n"
	"	bdnz		._L81			\n"
	"	li		%r9,  8			\n"
	"	lvx		%v1,  %r9,  %r4		\n"
	"	lvx		%v9,  0,    %r4		\n"
	"	lvx		%v4,  %r5,  %r3		\n"
	"	vperm		%v10, %v9,  %v1,  %v18	\n"
	"	stvewx		%v13, 0,    %r3		\n"
	"	vperm		%v11, %v9,  %v1,  %v2	\n"
	"	li		%r9,  4			\n"
	"	stvewx		%v13, %r9,  %r3		\n"
	"	vxor		%v7,  %v11, %v10	\n"
	"	add		%r3,  %r3,  %r5		\n"
	"	vavgub		%v8,  %v11, %v10	\n"
	"	vor		%v0,  %v7,  %v5		\n"
	"	vxor		%v13, %v8,  %v6		\n"
	"	vand		%v0,  %v19, %v0		\n"
	"	vavgub		%v1,  %v8,  %v6		\n"
	"	vand		%v0,  %v0,  %v13	\n"
	"	vsububm		%v1,  %v1,  %v0		\n"
	"	vavgub		%v13, %v4,  %v1		\n"
	"	stvewx		%v13, 0,    %r3		\n"
	"	stvewx		%v13, %r9,  %r3		\n"
	 );
}

MPEG2_MC_EXTERN (altivec)

#endif	/* ARCH_PPC */

#else	/* __ALTIVEC__ */

#define vector_s16_t vector signed short
#define vector_u16_t vector unsigned short
#define vector_s8_t vector signed char
#define vector_u8_t vector unsigned char
#define vector_s32_t vector signed int
#define vector_u32_t vector unsigned int

void MC_put_o_16_altivec (unsigned char * dest, const unsigned char * ref,
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

void MC_put_o_8_altivec (unsigned char * dest, const unsigned char * ref,
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

void MC_put_x_16_altivec (unsigned char * dest, const unsigned char * ref,
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

void MC_put_x_8_altivec (unsigned char * dest, const unsigned char * ref,
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

void MC_put_y_16_altivec (unsigned char * dest, const unsigned char * ref,
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

void MC_put_y_8_altivec (unsigned char * dest, const unsigned char * ref,
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

void MC_put_xy_16_altivec (unsigned char * dest, const unsigned char * ref,
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

void MC_put_xy_8_altivec (unsigned char * dest, const unsigned char * ref,
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
void MC_put_xy_8_altivec (unsigned char * dest, const unsigned char * ref,
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

void MC_avg_o_16_altivec (unsigned char * dest, const unsigned char * ref,
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

void MC_avg_o_8_altivec (unsigned char * dest, const unsigned char * ref,
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

void MC_avg_x_16_altivec (unsigned char * dest, const unsigned char * ref,
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

void MC_avg_x_8_altivec (unsigned char * dest, const unsigned char * ref,
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

void MC_avg_y_16_altivec (unsigned char * dest, const unsigned char * ref,
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

void MC_avg_y_8_altivec (unsigned char * dest, const unsigned char * ref,
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

void MC_avg_xy_16_altivec (unsigned char * dest, const unsigned char * ref,
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

void MC_avg_xy_8_altivec (unsigned char * dest, const unsigned char * ref,
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

#endif	/* __ALTIVEC__ */
