/*
 * idct_altivec.c
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
#include "attributes.h"

static const int16_t constants[5][8] ATTR_ALIGN(16) = {
    {23170, 13573, 6518, 21895, -23170, -21895, 32, 31},
    {16384, 22725, 21407, 19266, 16384, 19266, 21407, 22725},
    {22725, 31521, 29692, 26722, 22725, 26722, 29692, 31521},
    {21407, 29692, 27969, 25172, 21407, 25172, 27969, 29692},
    {19266, 26722, 25172, 22654, 19266, 22654, 25172, 26722}
};

/*
 * The asm code is generated with:
 *
 * gcc-2.95 -fvec -D__ALTIVEC__	-O9 -fomit-frame-pointer -mregnames -S
 *	idct_altivec.c
 *
 * awk '{args=""; len=split ($2, arg, ",");
 *	for (i=1; i<=len; i++) { a=arg[i]; if (i<len) a=a",";
 *				 args = args sprintf ("%-6s", a) }
 *	printf ("\t\"\t%-16s%-24s\\n\"\n", $1, args) }' idct_altivec.s |
 * unexpand -a
 *
 * I then do some simple trimming on the function prolog/trailers
 */

void mpeg2_idct_copy_altivec (int16_t * block, uint8_t * dest, int stride)
{
    asm ("						\n"
	"#	stwu		%r1,  -128(%r1)		\n"
	"#	mflr		%r0			\n"
	"#	stw		%r0,  132(%r1)		\n"
	"#	addi		%r0,  %r1,  128		\n"
	"#	bl		_savev25		\n"

	"	addi		%r9,  %r3,  112		\n"
	"	vspltish	%v25, 4			\n"
	"	vxor		%v13, %v13, %v13	\n"
	"	lis		%r10, constants@ha	\n"
	"	lvx		%v1,  0,    %r9		\n"
	"	la		%r10, constants@l(%r10) \n"
	"	lvx		%v5,  0,    %r3		\n"
	"	addi		%r9,  %r3,  16		\n"
	"	lvx		%v8,  0,    %r10	\n"
	"	addi		%r11, %r10, 32		\n"
	"	lvx		%v12, 0,    %r9		\n"
	"	lvx		%v6,  0,    %r11	\n"
	"	addi		%r8,  %r3,  48		\n"
	"	vslh		%v1,  %v1,  %v25	\n"
	"	addi		%r9,  %r3,  80		\n"
	"	lvx		%v11, 0,    %r8		\n"
	"	vslh		%v5,  %v5,  %v25	\n"
	"	lvx		%v0,  0,    %r9		\n"
	"	addi		%r11, %r10, 64		\n"
	"	vsplth		%v3,  %v8,  2		\n"
	"	lvx		%v7,  0,    %r11	\n"
	"	addi		%r9,  %r3,  96		\n"
	"	vslh		%v12, %v12, %v25	\n"
	"	vmhraddshs	%v27, %v1,  %v6,  %v13	\n"
	"	addi		%r8,  %r3,  32		\n"
	"	vsplth		%v2,  %v8,  5		\n"
	"	lvx		%v1,  0,    %r9		\n"
	"	vslh		%v11, %v11, %v25	\n"
	"	addi		%r3,  %r3,  64		\n"
	"	lvx		%v9,  0,    %r8		\n"
	"	addi		%r9,  %r10, 48		\n"
	"	vslh		%v0,  %v0,  %v25	\n"
	"	lvx		%v4,  0,    %r9		\n"
	"	vmhraddshs	%v31, %v12, %v6,  %v13	\n"
	"	addi		%r10, %r10, 16		\n"
	"	vmhraddshs	%v30, %v0,  %v7,  %v13	\n"
	"	lvx		%v10, 0,    %r3		\n"
	"	vsplth		%v19, %v8,  3		\n"
	"	vmhraddshs	%v15, %v11, %v7,  %v13	\n"
	"	lvx		%v12, 0,    %r10	\n"
	"	vsplth		%v6,  %v8,  4		\n"
	"	vslh		%v1,  %v1,  %v25	\n"
	"	vsplth		%v11, %v8,  1		\n"
	"	li		%r9,  4			\n"
	"	vslh		%v9,  %v9,  %v25	\n"
	"	vsplth		%v7,  %v8,  0		\n"
	"	vmhraddshs	%v18, %v1,  %v4,  %v13	\n"
	"	vspltw		%v8,  %v8,  3		\n"
	"	vsubshs		%v0,  %v13, %v27	\n"
	"	vmhraddshs	%v1,  %v9,  %v4,  %v13	\n"
	"	vmhraddshs	%v17, %v3,  %v31, %v0	\n"
	"	vmhraddshs	%v4,  %v2,  %v15, %v30	\n"
	"	vslh		%v10, %v10, %v25	\n"
	"	vmhraddshs	%v9,  %v5,  %v12, %v13	\n"
	"	vspltish	%v25, 6			\n"
	"	vmhraddshs	%v5,  %v10, %v12, %v13	\n"
	"	vmhraddshs	%v28, %v19, %v30, %v15	\n"
	"	vmhraddshs	%v27, %v3,  %v27, %v31	\n"
	"	vsubshs		%v0,  %v13, %v18	\n"
	"	vmhraddshs	%v18, %v11, %v18, %v1	\n"
	"	vaddshs		%v30, %v17, %v4		\n"
	"	vmhraddshs	%v12, %v11, %v1,  %v0	\n"
	"	vsubshs		%v4,  %v17, %v4		\n"
	"	vaddshs		%v10, %v9,  %v5		\n"
	"	vsubshs		%v17, %v27, %v28	\n"
	"	vaddshs		%v27, %v27, %v28	\n"
	"	vsubshs		%v1,  %v9,  %v5		\n"
	"	vaddshs		%v28, %v10, %v18	\n"
	"	vsubshs		%v18, %v10, %v18	\n"
	"	vaddshs		%v10, %v1,  %v12	\n"
	"	vsubshs		%v1,  %v1,  %v12	\n"
	"	vsubshs		%v12, %v17, %v4		\n"
	"	vaddshs		%v4,  %v17, %v4		\n"
	"	vmhraddshs	%v5,  %v7,  %v12, %v1	\n"
	"	vmhraddshs	%v26, %v6,  %v4,  %v10	\n"
	"	vmhraddshs	%v29, %v6,  %v12, %v1	\n"
	"	vmhraddshs	%v14, %v7,  %v4,  %v10	\n"
	"	vsubshs		%v12, %v18, %v30	\n"
	"	vaddshs		%v9,  %v28, %v27	\n"
	"	vaddshs		%v16, %v18, %v30	\n"
	"	vsubshs		%v10, %v28, %v27	\n"
	"	vmrglh		%v31, %v9,  %v12	\n"
	"	vmrglh		%v30, %v5,  %v26	\n"
	"	vmrglh		%v15, %v14, %v29	\n"
	"	vmrghh		%v5,  %v5,  %v26	\n"
	"	vmrglh		%v27, %v16, %v10	\n"
	"	vmrghh		%v9,  %v9,  %v12	\n"
	"	vmrghh		%v18, %v16, %v10	\n"
	"	vmrghh		%v1,  %v14, %v29	\n"
	"	vmrglh		%v14, %v9,  %v5		\n"
	"	vmrglh		%v16, %v31, %v30	\n"
	"	vmrglh		%v10, %v15, %v27	\n"
	"	vmrghh		%v9,  %v9,  %v5		\n"
	"	vmrghh		%v26, %v15, %v27	\n"
	"	vmrglh		%v27, %v16, %v10	\n"
	"	vmrghh		%v12, %v1,  %v18	\n"
	"	vmrglh		%v29, %v1,  %v18	\n"
	"	vsubshs		%v0,  %v13, %v27	\n"
	"	vmrghh		%v5,  %v31, %v30	\n"
	"	vmrglh		%v31, %v9,  %v12	\n"
	"	vmrglh		%v30, %v5,  %v26	\n"
	"	vmrglh		%v15, %v14, %v29	\n"
	"	vmhraddshs	%v17, %v3,  %v31, %v0	\n"
	"	vmrghh		%v18, %v16, %v10	\n"
	"	vmhraddshs	%v27, %v3,  %v27, %v31	\n"
	"	vmhraddshs	%v4,  %v2,  %v15, %v30	\n"
	"	vmrghh		%v1,  %v14, %v29	\n"
	"	vmhraddshs	%v28, %v19, %v30, %v15	\n"
	"	vmrghh		%v0,  %v9,  %v12	\n"
	"	vsubshs		%v13, %v13, %v18	\n"
	"	vmrghh		%v5,  %v5,  %v26	\n"
	"	vmhraddshs	%v18, %v11, %v18, %v1	\n"
	"	vaddshs		%v9,  %v0,  %v8		\n"
	"	vaddshs		%v30, %v17, %v4		\n"
	"	vmhraddshs	%v12, %v11, %v1,  %v13	\n"
	"	vsubshs		%v4,  %v17, %v4		\n"
	"	vaddshs		%v10, %v9,  %v5		\n"
	"	vsubshs		%v17, %v27, %v28	\n"
	"	vaddshs		%v27, %v27, %v28	\n"
	"	vsubshs		%v1,  %v9,  %v5		\n"
	"	vaddshs		%v28, %v10, %v18	\n"
	"	vsubshs		%v18, %v10, %v18	\n"
	"	vaddshs		%v10, %v1,  %v12	\n"
	"	vsubshs		%v1,  %v1,  %v12	\n"
	"	vsubshs		%v12, %v17, %v4		\n"
	"	vaddshs		%v4,  %v17, %v4		\n"
	"	vaddshs		%v9,  %v28, %v27	\n"
	"	vmhraddshs	%v14, %v7,  %v4,  %v10	\n"
	"	vsrah		%v9,  %v9,  %v25	\n"
	"	vmhraddshs	%v5,  %v7,  %v12, %v1	\n"
	"	vpkshus		%v0,  %v9,  %v9		\n"
	"	vmhraddshs	%v29, %v6,  %v12, %v1	\n"
	"	stvewx		%v0,  0,    %r4		\n"
	"	vaddshs		%v16, %v18, %v30	\n"
	"	vsrah		%v31, %v14, %v25	\n"
	"	stvewx		%v0,  %r9,  %r4		\n"
	"	add		%r4,  %r4,  %r5		\n"
	"	vsrah		%v15, %v16, %v25	\n"
	"	vpkshus		%v0,  %v31, %v31	\n"
	"	vsrah		%v1,  %v5,  %v25	\n"
	"	stvewx		%v0,  0,    %r4		\n"
	"	vsubshs		%v12, %v18, %v30	\n"
	"	stvewx		%v0,  %r9,  %r4		\n"
	"	vmhraddshs	%v26, %v6,  %v4,  %v10	\n"
	"	vpkshus		%v0,  %v1,  %v1		\n"
	"	add		%r4,  %r4,  %r5		\n"
	"	vsrah		%v5,  %v12, %v25	\n"
	"	stvewx		%v0,  0,    %r4		\n"
	"	vsrah		%v30, %v29, %v25	\n"
	"	stvewx		%v0,  %r9,  %r4		\n"
	"	vsubshs		%v10, %v28, %v27	\n"
	"	vpkshus		%v0,  %v15, %v15	\n"
	"	add		%r4,  %r4,  %r5		\n"
	"	stvewx		%v0,  0,    %r4		\n"
	"	vsrah		%v18, %v26, %v25	\n"
	"	stvewx		%v0,  %r9,  %r4		\n"
	"	vsrah		%v27, %v10, %v25	\n"
	"	vpkshus		%v0,  %v5,  %v5		\n"
	"	add		%r4,  %r4,  %r5		\n"
	"	stvewx		%v0,  0,    %r4		\n"
	"	stvewx		%v0,  %r9,  %r4		\n"
	"	vpkshus		%v0,  %v30, %v30	\n"
	"	add		%r4,  %r4,  %r5		\n"
	"	stvewx		%v0,  0,    %r4		\n"
	"	stvewx		%v0,  %r9,  %r4		\n"
	"	vpkshus		%v0,  %v18, %v18	\n"
	"	add		%r4,  %r4,  %r5		\n"
	"	stvewx		%v0,  0,    %r4		\n"
	"	stvewx		%v0,  %r9,  %r4		\n"
	"	add		%r4,  %r4,  %r5		\n"
	"	vpkshus		%v0,  %v27, %v27	\n"
	"	stvewx		%v0,  0,    %r4		\n"
	"	stvewx		%v0,  %r9,  %r4		\n"

	"#	addi		%r0,  %r1,  128		\n"
	"#	bl		_restv25		\n"
	"#	lwz		%r0,  132(%r1)		\n"
	"#	mtlr		%r0			\n"
	"#	la		%r1,  128(%r1)		\n"

	"	vxor		%v1,  %v1,  %v1		\n"
	"	addi		%r9,  %r3,  16		\n"
	"	stvx		%v1,  0,    %r3		\n"
	"	stvx		%v1,  0,    %r9		\n"
	"	addi		%r11, %r3,  32		\n"
	"	stvx		%v1,  0,    %r11	\n"
	"	addi		%r9,  %r3,  48		\n"
	"	stvx		%v1,  0,    %r9		\n"
	"	addi		%r11, %r3,  -64		\n"
	"	stvx		%v1,  0,    %r11	\n"
	"	addi		%r9,  %r3,  -48		\n"
	"	stvx		%v1,  0,    %r9		\n"
	"	addi		%r11, %r3,  -32		\n"
	"	stvx		%v1,  0,    %r11	\n"
	"	addi		%r3,  %r3,  -16		\n"
	"	stvx		%v1,  0,    %r3		\n"
	 );
}

void mpeg2_idct_add_altivec (int last, int16_t * block,
			     uint8_t * dest, int stride)
{
    asm ("						\n"
	"#	stwu		%r1,  -192(%r1)		\n"
	"#	mflr		%r0			\n"
	"#	stw		%r0,  196(%r1)		\n"
	"#	addi		%r0,  %r1,  192		\n"
	"#	bl		_savev21		\n"

	"	addi		%r9,  %r4,  112		\n"
	"	vspltish	%v21, 4			\n"
	"	vxor		%v1,  %v1,  %v1		\n"
	"	lvx		%v13, 0,    %r9		\n"
	"	lis		%r10, constants@ha	\n"
	"	vspltisw	%v3,  -1		\n"
	"	la		%r10, constants@l(%r10) \n"
	"	lvx		%v5,  0,    %r4		\n"
	"	addi		%r9,  %r4,  16		\n"
	"	lvx		%v8,  0,    %r10	\n"
	"	lvx		%v12, 0,    %r9		\n"
	"	addi		%r11, %r10, 32		\n"
	"	lvx		%v6,  0,    %r11	\n"
	"	addi		%r8,  %r4,  48		\n"
	"	vslh		%v13, %v13, %v21	\n"
	"	addi		%r9,  %r4,  80		\n"
	"	lvx		%v11, 0,    %r8		\n"
	"	vslh		%v5,  %v5,  %v21	\n"
	"	lvx		%v0,  0,    %r9		\n"
	"	addi		%r11, %r10, 64		\n"
	"	vsplth		%v2,  %v8,  2		\n"
	"	lvx		%v7,  0,    %r11	\n"
	"	vslh		%v12, %v12, %v21	\n"
	"	addi		%r9,  %r4,  96		\n"
	"	vmhraddshs	%v24, %v13, %v6,  %v1	\n"
	"	addi		%r8,  %r4,  32		\n"
	"	vsplth		%v17, %v8,  5		\n"
	"	lvx		%v13, 0,    %r9		\n"
	"	vslh		%v11, %v11, %v21	\n"
	"	addi		%r4,  %r4,  64		\n"
	"	lvx		%v10, 0,    %r8		\n"
	"	vslh		%v0,  %v0,  %v21	\n"
	"	addi		%r9,  %r10, 48		\n"
	"	vmhraddshs	%v31, %v12, %v6,  %v1	\n"
	"	lvx		%v4,  0,    %r9		\n"
	"	addi		%r10, %r10, 16		\n"
	"	vmhraddshs	%v26, %v0,  %v7,  %v1	\n"
	"	lvx		%v9,  0,    %r4		\n"
	"	vsplth		%v16, %v8,  3		\n"
	"	vmhraddshs	%v22, %v11, %v7,  %v1	\n"
	"	lvx		%v6,  0,    %r10	\n"
	"	lvsl		%v19, 0,    %r5		\n"
	"	vsubshs		%v12, %v1,  %v24	\n"
	"	lvsl		%v0,  %r6,  %r5		\n"
	"	vsplth		%v11, %v8,  1		\n"
	"	vslh		%v10, %v10, %v21	\n"
	"	vmrghb		%v19, %v3,  %v19	\n"
	"	lvx		%v15, 0,    %r5		\n"
	"	vslh		%v13, %v13, %v21	\n"
	"	vmrghb		%v3,  %v3,  %v0		\n"
	"	li		%r9,  4			\n"
	"	vmhraddshs	%v14, %v2,  %v31, %v12	\n"
	"	vsplth		%v7,  %v8,  0		\n"
	"	vmhraddshs	%v23, %v13, %v4,  %v1	\n"
	"	vsplth		%v18, %v8,  4		\n"
	"	vmhraddshs	%v27, %v10, %v4,  %v1	\n"
	"	vspltw		%v8,  %v8,  3		\n"
	"	vmhraddshs	%v12, %v17, %v22, %v26	\n"
	"	vperm		%v15, %v15, %v1,  %v19	\n"
	"	vslh		%v9,  %v9,  %v21	\n"
	"	vmhraddshs	%v10, %v5,  %v6,  %v1	\n"
	"	vspltish	%v21, 6			\n"
	"	vmhraddshs	%v30, %v9,  %v6,  %v1	\n"
	"	vmhraddshs	%v26, %v16, %v26, %v22	\n"
	"	vmhraddshs	%v24, %v2,  %v24, %v31	\n"
	"	vmhraddshs	%v31, %v11, %v23, %v27	\n"
	"	vsubshs		%v0,  %v1,  %v23	\n"
	"	vaddshs		%v23, %v14, %v12	\n"
	"	vmhraddshs	%v9,  %v11, %v27, %v0	\n"
	"	vsubshs		%v12, %v14, %v12	\n"
	"	vaddshs		%v6,  %v10, %v30	\n"
	"	vsubshs		%v14, %v24, %v26	\n"
	"	vaddshs		%v24, %v24, %v26	\n"
	"	vsubshs		%v13, %v10, %v30	\n"
	"	vaddshs		%v26, %v6,  %v31	\n"
	"	vsubshs		%v31, %v6,  %v31	\n"
	"	vaddshs		%v6,  %v13, %v9		\n"
	"	vsubshs		%v13, %v13, %v9		\n"
	"	vsubshs		%v9,  %v14, %v12	\n"
	"	vaddshs		%v12, %v14, %v12	\n"
	"	vmhraddshs	%v30, %v7,  %v9,  %v13	\n"
	"	vmhraddshs	%v25, %v18, %v12, %v6	\n"
	"	vmhraddshs	%v28, %v18, %v9,  %v13	\n"
	"	vmhraddshs	%v29, %v7,  %v12, %v6	\n"
	"	vaddshs		%v10, %v26, %v24	\n"
	"	vsubshs		%v5,  %v31, %v23	\n"
	"	vsubshs		%v13, %v26, %v24	\n"
	"	vaddshs		%v4,  %v31, %v23	\n"
	"	vmrglh		%v26, %v30, %v25	\n"
	"	vmrglh		%v31, %v10, %v5		\n"
	"	vmrglh		%v22, %v29, %v28	\n"
	"	vmrghh		%v30, %v30, %v25	\n"
	"	vmrglh		%v24, %v4,  %v13	\n"
	"	vmrghh		%v10, %v10, %v5		\n"
	"	vmrghh		%v23, %v4,  %v13	\n"
	"	vmrghh		%v27, %v29, %v28	\n"
	"	vmrglh		%v29, %v10, %v30	\n"
	"	vmrglh		%v4,  %v31, %v26	\n"
	"	vmrglh		%v13, %v22, %v24	\n"
	"	vmrghh		%v10, %v10, %v30	\n"
	"	vmrghh		%v25, %v22, %v24	\n"
	"	vmrglh		%v24, %v4,  %v13	\n"
	"	vmrghh		%v5,  %v27, %v23	\n"
	"	vmrglh		%v28, %v27, %v23	\n"
	"	vsubshs		%v0,  %v1,  %v24	\n"
	"	vmrghh		%v30, %v31, %v26	\n"
	"	vmrglh		%v31, %v10, %v5		\n"
	"	vmrglh		%v26, %v30, %v25	\n"
	"	vmrglh		%v22, %v29, %v28	\n"
	"	vmhraddshs	%v14, %v2,  %v31, %v0	\n"
	"	vmrghh		%v23, %v4,  %v13	\n"
	"	vmhraddshs	%v24, %v2,  %v24, %v31	\n"
	"	vmhraddshs	%v12, %v17, %v22, %v26	\n"
	"	vmrghh		%v27, %v29, %v28	\n"
	"	vmhraddshs	%v26, %v16, %v26, %v22	\n"
	"	vmrghh		%v0,  %v10, %v5		\n"
	"	vmhraddshs	%v31, %v11, %v23, %v27	\n"
	"	vmrghh		%v30, %v30, %v25	\n"
	"	vsubshs		%v13, %v1,  %v23	\n"
	"	vaddshs		%v10, %v0,  %v8		\n"
	"	vaddshs		%v23, %v14, %v12	\n"
	"	vsubshs		%v12, %v14, %v12	\n"
	"	vaddshs		%v6,  %v10, %v30	\n"
	"	vsubshs		%v14, %v24, %v26	\n"
	"	vmhraddshs	%v9,  %v11, %v27, %v13	\n"
	"	vaddshs		%v24, %v24, %v26	\n"
	"	vaddshs		%v26, %v6,  %v31	\n"
	"	vsubshs		%v13, %v10, %v30	\n"
	"	vaddshs		%v10, %v26, %v24	\n"
	"	vsubshs		%v31, %v6,  %v31	\n"
	"	vaddshs		%v6,  %v13, %v9		\n"
	"	vsrah		%v10, %v10, %v21	\n"
	"	vsubshs		%v13, %v13, %v9		\n"
	"	vaddshs		%v0,  %v15, %v10	\n"
	"	vsubshs		%v9,  %v14, %v12	\n"
	"	vaddshs		%v12, %v14, %v12	\n"
	"	vpkshus		%v15, %v0,  %v0		\n"
	"	stvewx		%v15, 0,    %r5		\n"
	"	vaddshs		%v4,  %v31, %v23	\n"
	"	vmhraddshs	%v29, %v7,  %v12, %v6	\n"
	"	stvewx		%v15, %r9,  %r5		\n"
	"	add		%r5,  %r5,  %r6		\n"
	"	vsubshs		%v5,  %v31, %v23	\n"
	"	lvx		%v15, 0,    %r5		\n"
	"	vmhraddshs	%v30, %v7,  %v9,  %v13	\n"
	"	vsrah		%v22, %v4,  %v21	\n"
	"	vperm		%v15, %v15, %v1,  %v3	\n"
	"	vmhraddshs	%v28, %v18, %v9,  %v13	\n"
	"	vsrah		%v31, %v29, %v21	\n"
	"	vsubshs		%v13, %v26, %v24	\n"
	"	vaddshs		%v0,  %v15, %v31	\n"
	"	vsrah		%v27, %v30, %v21	\n"
	"	vpkshus		%v15, %v0,  %v0		\n"
	"	vsrah		%v30, %v5,  %v21	\n"
	"	stvewx		%v15, 0,    %r5		\n"
	"	vsrah		%v26, %v28, %v21	\n"
	"	stvewx		%v15, %r9,  %r5		\n"
	"	vmhraddshs	%v25, %v18, %v12, %v6	\n"
	"	add		%r5,  %r5,  %r6		\n"
	"	vsrah		%v24, %v13, %v21	\n"
	"	lvx		%v15, 0,    %r5		\n"
	"	vperm		%v15, %v15, %v1,  %v19	\n"
	"	vsrah		%v23, %v25, %v21	\n"
	"	vaddshs		%v0,  %v15, %v27	\n"
	"	vpkshus		%v15, %v0,  %v0		\n"
	"	stvewx		%v15, 0,    %r5		\n"
	"	stvewx		%v15, %r9,  %r5		\n"
	"	add		%r5,  %r5,  %r6		\n"
	"	lvx		%v15, 0,    %r5		\n"
	"	vperm		%v15, %v15, %v1,  %v3	\n"
	"	vaddshs		%v0,  %v15, %v22	\n"
	"	vpkshus		%v15, %v0,  %v0		\n"
	"	stvewx		%v15, 0,    %r5		\n"
	"	stvewx		%v15, %r9,  %r5		\n"
	"	add		%r5,  %r5,  %r6		\n"
	"	lvx		%v15, 0,    %r5		\n"
	"	vperm		%v15, %v15, %v1,  %v19	\n"
	"	vaddshs		%v0,  %v15, %v30	\n"
	"	vpkshus		%v15, %v0,  %v0		\n"
	"	stvewx		%v15, 0,    %r5		\n"
	"	stvewx		%v15, %r9,  %r5		\n"
	"	add		%r5,  %r5,  %r6		\n"
	"	lvx		%v15, 0,    %r5		\n"
	"	vperm		%v15, %v15, %v1,  %v3	\n"
	"	vaddshs		%v0,  %v15, %v26	\n"
	"	vpkshus		%v15, %v0,  %v0		\n"
	"	stvewx		%v15, 0,    %r5		\n"
	"	stvewx		%v15, %r9,  %r5		\n"
	"	add		%r5,  %r5,  %r6		\n"
	"	lvx		%v15, 0,    %r5		\n"
	"	vperm		%v15, %v15, %v1,  %v19	\n"
	"	vaddshs		%v0,  %v15, %v23	\n"
	"	vpkshus		%v15, %v0,  %v0		\n"
	"	stvewx		%v15, 0,    %r5		\n"
	"	stvewx		%v15, %r9,  %r5		\n"
	"	add		%r5,  %r5,  %r6		\n"
	"	lvx		%v15, 0,    %r5		\n"
	"	vperm		%v15, %v15, %v1,  %v3	\n"
	"	vaddshs		%v0,  %v15, %v24	\n"
	"	vpkshus		%v15, %v0,  %v0		\n"
	"	stvewx		%v15, 0,    %r5		\n"
	"	stvewx		%v15, %r9,  %r5		\n"

	"#	addi		%r0,  %r1,  192		\n"
	"#	bl		_restv21		\n"
	"#	lwz		%r0,  196(%r1)		\n"
	"#	mtlr		%r0			\n"
	"#	la		%r1,  192(%r1)		\n"

	"	addi		%r9,  %r4,  16		\n"
	"	stvx		%v1,  0,    %r4		\n"
	"	stvx		%v1,  0,    %r9		\n"
	"	addi		%r11, %r4,  32		\n"
	"	stvx		%v1,  0,    %r11	\n"
	"	addi		%r9,  %r4,  48		\n"
	"	stvx		%v1,  0,    %r9		\n"
	"	addi		%r11, %r4,  -64		\n"
	"	stvx		%v1,  0,    %r11	\n"
	"	addi		%r9,  %r4,  -48		\n"
	"	stvx		%v1,  0,    %r9		\n"
	"	addi		%r11, %r4,  -32		\n"
	"	stvx		%v1,  0,    %r11	\n"
	"	addi		%r4,  %r4,  -16		\n"
	"	stvx		%v1,  0,    %r4		\n"
	 );
}

void mpeg2_idct_altivec_init (void)
{
    extern uint8_t mpeg2_scan_norm[64];
    extern uint8_t mpeg2_scan_alt[64];
    int i, j;

    i = constants[0][0];	/* just pretending - keeps gcc happy */

    /* the altivec idct uses a transposed input, so we patch scan tables */
    for (i = 0; i < 64; i++) {
	j = mpeg2_scan_norm[i];
	mpeg2_scan_norm[i] = (j >> 3) | ((j & 7) << 3);
	j = mpeg2_scan_alt[i];
	mpeg2_scan_alt[i] = (j >> 3) | ((j & 7) << 3);
    }
}

#endif	/* ARCH_PPC */

#else	/* __ALTIVEC__ */

#define vector_s16_t vector signed short
#define vector_u16_t vector unsigned short
#define vector_s8_t vector signed char
#define vector_u8_t vector unsigned char
#define vector_s32_t vector signed int
#define vector_u32_t vector unsigned int

#define IDCT_HALF					\
    /* 1st stage */					\
    t1 = vec_mradds (a1, vx7, vx1 );			\
    t8 = vec_mradds (a1, vx1, vec_subs (zero, vx7));	\
    t7 = vec_mradds (a2, vx5, vx3);			\
    t3 = vec_mradds (ma2, vx3, vx5);			\
							\
    /* 2nd stage */					\
    t5 = vec_adds (vx0, vx4);				\
    t0 = vec_subs (vx0, vx4);				\
    t2 = vec_mradds (a0, vx6, vx2);			\
    t4 = vec_mradds (a0, vx2, vec_subs (zero, vx6));	\
    t6 = vec_adds (t8, t3);				\
    t3 = vec_subs (t8, t3);				\
    t8 = vec_subs (t1, t7);				\
    t1 = vec_adds (t1, t7);				\
							\
    /* 3rd stage */					\
    t7 = vec_adds (t5, t2);				\
    t2 = vec_subs (t5, t2);				\
    t5 = vec_adds (t0, t4);				\
    t0 = vec_subs (t0, t4);				\
    t4 = vec_subs (t8, t3);				\
    t3 = vec_adds (t8, t3);				\
							\
    /* 4th stage */					\
    vy0 = vec_adds (t7, t1);				\
    vy7 = vec_subs (t7, t1);				\
    vy1 = vec_mradds (c4, t3, t5);			\
    vy6 = vec_mradds (mc4, t3, t5);			\
    vy2 = vec_mradds (c4, t4, t0);			\
    vy5 = vec_mradds (mc4, t4, t0);			\
    vy3 = vec_adds (t2, t6);				\
    vy4 = vec_subs (t2, t6);

#define IDCT								\
    vector_s16_t vx0, vx1, vx2, vx3, vx4, vx5, vx6, vx7;		\
    vector_s16_t vy0, vy1, vy2, vy3, vy4, vy5, vy6, vy7;		\
    vector_s16_t a0, a1, a2, ma2, c4, mc4, zero, bias;			\
    vector_s16_t t0, t1, t2, t3, t4, t5, t6, t7, t8;			\
    vector_u16_t shift;							\
									\
    c4 = vec_splat (constants[0], 0);					\
    a0 = vec_splat (constants[0], 1);					\
    a1 = vec_splat (constants[0], 2);					\
    a2 = vec_splat (constants[0], 3);					\
    mc4 = vec_splat (constants[0], 4);					\
    ma2 = vec_splat (constants[0], 5);					\
    bias = (vector_s16_t)vec_splat ((vector_s32_t)constants[0], 3);	\
									\
    zero = vec_splat_s16 (0);						\
    shift = vec_splat_u16 (4);						\
									\
    vx0 = vec_mradds (vec_sl (block[0], shift), constants[1], zero);	\
    vx1 = vec_mradds (vec_sl (block[1], shift), constants[2], zero);	\
    vx2 = vec_mradds (vec_sl (block[2], shift), constants[3], zero);	\
    vx3 = vec_mradds (vec_sl (block[3], shift), constants[4], zero);	\
    vx4 = vec_mradds (vec_sl (block[4], shift), constants[1], zero);	\
    vx5 = vec_mradds (vec_sl (block[5], shift), constants[4], zero);	\
    vx6 = vec_mradds (vec_sl (block[6], shift), constants[3], zero);	\
    vx7 = vec_mradds (vec_sl (block[7], shift), constants[2], zero);	\
									\
    IDCT_HALF								\
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
    vx0 = vec_adds (vec_mergeh (vy0, vy4), bias);			\
    vx1 = vec_mergel (vy0, vy4);					\
    vx2 = vec_mergeh (vy1, vy5);					\
    vx3 = vec_mergel (vy1, vy5);					\
    vx4 = vec_mergeh (vy2, vy6);					\
    vx5 = vec_mergel (vy2, vy6);					\
    vx6 = vec_mergeh (vy3, vy7);					\
    vx7 = vec_mergel (vy3, vy7);					\
									\
    IDCT_HALF								\
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

static const vector_s16_t constants[5] = {
    (vector_s16_t)(23170, 13573, 6518, 21895, -23170, -21895, 32, 31),
    (vector_s16_t)(16384, 22725, 21407, 19266, 16384, 19266, 21407, 22725),
    (vector_s16_t)(22725, 31521, 29692, 26722, 22725, 26722, 29692, 31521),
    (vector_s16_t)(21407, 29692, 27969, 25172, 21407, 25172, 27969, 29692),
    (vector_s16_t)(19266, 26722, 25172, 22654, 19266, 22654, 25172, 26722)
};

void mpeg2_idct_copy_altivec (vector_s16_t * const block, unsigned char * dest,
			      const int stride)
{
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

    memset (block, 0, 64 * sizeof (signed short));
}

void mpeg2_idct_add_altivec (const int last, vector_s16_t * const block,
			     unsigned char * dest, const int stride)
{
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

    memset (block, 0, 64 * sizeof (signed short));
}

#endif	/* __ALTIVEC__ */
