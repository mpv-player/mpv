/*
 * sse.h
 * Copyright (C) 1999 R. Fisher
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


typedef	union {
	float			sf[4];	/* Single-precision (32-bit) value */
} ATTR_ALIGN(16) sse_t;	/* On a 16 byte (128-bit) boundary */


#define	sse_i2r(op, imm, reg) \
	__asm__ __volatile__ (#op " %0, %%" #reg \
			      : /* nothing */ \
			      : "X" (imm) )

#define	sse_m2r(op, mem, reg) \
	__asm__ __volatile__ (#op " %0, %%" #reg \
			      : /* nothing */ \
			      : "X" (mem))

#define	sse_r2m(op, reg, mem) \
	__asm__ __volatile__ (#op " %%" #reg ", %0" \
			      : "=X" (mem) \
			      : /* nothing */ )

#define	sse_r2r(op, regs, regd) \
	__asm__ __volatile__ (#op " %" #regs ", %" #regd)

#define	sse_r2ri(op, regs, regd, imm) \
	__asm__ __volatile__ (#op " %0, %%" #regs ", %%" #regd \
			      : /* nothing */ \
			      : "X" (imm) )

#define	sse_m2ri(op, mem, reg, subop) \
	__asm__ __volatile__ (#op " %0, %%" #reg ", " #subop \
			      : /* nothing */ \
			      : "X" (mem))


#define	movaps_m2r(var, reg)	sse_m2r(movaps, var, reg)
#define	movaps_r2m(reg, var)	sse_r2m(movaps, reg, var)
#define	movaps_r2r(regs, regd)	sse_r2r(movaps, regs, regd)

#define	movntps_r2m(xmmreg, var)	sse_r2m(movntps, xmmreg, var)

#define	movups_m2r(var, reg)	sse_m2r(movups, var, reg)
#define	movups_r2m(reg, var)	sse_r2m(movups, reg, var)
#define	movups_r2r(regs, regd)	sse_r2r(movups, regs, regd)

#define	movhlps_r2r(regs, regd)	sse_r2r(movhlps, regs, regd)

#define	movlhps_r2r(regs, regd)	sse_r2r(movlhps, regs, regd)

#define	movhps_m2r(var, reg)	sse_m2r(movhps, var, reg)
#define	movhps_r2m(reg, var)	sse_r2m(movhps, reg, var)

#define	movlps_m2r(var, reg)	sse_m2r(movlps, var, reg)
#define	movlps_r2m(reg, var)	sse_r2m(movlps, reg, var)

#define	movss_m2r(var, reg)	sse_m2r(movss, var, reg)
#define	movss_r2m(reg, var)	sse_r2m(movss, reg, var)
#define	movss_r2r(regs, regd)	sse_r2r(movss, regs, regd)

#define	shufps_m2r(var, reg, index)	sse_m2ri(shufps, var, reg, index)
#define	shufps_r2r(regs, regd, index)	sse_r2ri(shufps, regs, regd, index)

#define	cvtpi2ps_m2r(var, xmmreg)	sse_m2r(cvtpi2ps, var, xmmreg)
#define	cvtpi2ps_r2r(mmreg, xmmreg)	sse_r2r(cvtpi2ps, mmreg, xmmreg)

#define	cvtps2pi_m2r(var, mmreg)	sse_m2r(cvtps2pi, var, mmreg)
#define	cvtps2pi_r2r(xmmreg, mmreg)	sse_r2r(cvtps2pi, mmreg, xmmreg)

#define	cvttps2pi_m2r(var, mmreg)	sse_m2r(cvttps2pi, var, mmreg)
#define	cvttps2pi_r2r(xmmreg, mmreg)	sse_r2r(cvttps2pi, mmreg, xmmreg)

#define	cvtsi2ss_m2r(var, xmmreg)	sse_m2r(cvtsi2ss, var, xmmreg)
#define	cvtsi2ss_r2r(reg, xmmreg)	sse_r2r(cvtsi2ss, reg, xmmreg)

#define	cvtss2si_m2r(var, reg)		sse_m2r(cvtss2si, var, reg)
#define	cvtss2si_r2r(xmmreg, reg)	sse_r2r(cvtss2si, xmmreg, reg)

#define	cvttss2si_m2r(var, reg)		sse_m2r(cvtss2si, var, reg)
#define	cvttss2si_r2r(xmmreg, reg)	sse_r2r(cvtss2si, xmmreg, reg)

#define	movmskps(xmmreg, reg) \
	__asm__ __volatile__ ("movmskps %" #xmmreg ", %" #reg)

#define	addps_m2r(var, reg)		sse_m2r(addps, var, reg)
#define	addps_r2r(regs, regd)		sse_r2r(addps, regs, regd)

#define	addss_m2r(var, reg)		sse_m2r(addss, var, reg)
#define	addss_r2r(regs, regd)		sse_r2r(addss, regs, regd)

#define	subps_m2r(var, reg)		sse_m2r(subps, var, reg)
#define	subps_r2r(regs, regd)		sse_r2r(subps, regs, regd)

#define	subss_m2r(var, reg)		sse_m2r(subss, var, reg)
#define	subss_r2r(regs, regd)		sse_r2r(subss, regs, regd)

#define	mulps_m2r(var, reg)		sse_m2r(mulps, var, reg)
#define	mulps_r2r(regs, regd)		sse_r2r(mulps, regs, regd)

#define	mulss_m2r(var, reg)		sse_m2r(mulss, var, reg)
#define	mulss_r2r(regs, regd)		sse_r2r(mulss, regs, regd)

#define	divps_m2r(var, reg)		sse_m2r(divps, var, reg)
#define	divps_r2r(regs, regd)		sse_r2r(divps, regs, regd)

#define	divss_m2r(var, reg)		sse_m2r(divss, var, reg)
#define	divss_r2r(regs, regd)		sse_r2r(divss, regs, regd)

#define	rcpps_m2r(var, reg)		sse_m2r(rcpps, var, reg)
#define	rcpps_r2r(regs, regd)		sse_r2r(rcpps, regs, regd)

#define	rcpss_m2r(var, reg)		sse_m2r(rcpss, var, reg)
#define	rcpss_r2r(regs, regd)		sse_r2r(rcpss, regs, regd)

#define	rsqrtps_m2r(var, reg)		sse_m2r(rsqrtps, var, reg)
#define	rsqrtps_r2r(regs, regd)		sse_r2r(rsqrtps, regs, regd)

#define	rsqrtss_m2r(var, reg)		sse_m2r(rsqrtss, var, reg)
#define	rsqrtss_r2r(regs, regd)		sse_r2r(rsqrtss, regs, regd)

#define	sqrtps_m2r(var, reg)		sse_m2r(sqrtps, var, reg)
#define	sqrtps_r2r(regs, regd)		sse_r2r(sqrtps, regs, regd)

#define	sqrtss_m2r(var, reg)		sse_m2r(sqrtss, var, reg)
#define	sqrtss_r2r(regs, regd)		sse_r2r(sqrtss, regs, regd)

#define	andps_m2r(var, reg)		sse_m2r(andps, var, reg)
#define	andps_r2r(regs, regd)		sse_r2r(andps, regs, regd)

#define	andnps_m2r(var, reg)		sse_m2r(andnps, var, reg)
#define	andnps_r2r(regs, regd)		sse_r2r(andnps, regs, regd)

#define	orps_m2r(var, reg)		sse_m2r(orps, var, reg)
#define	orps_r2r(regs, regd)		sse_r2r(orps, regs, regd)

#define	xorps_m2r(var, reg)		sse_m2r(xorps, var, reg)
#define	xorps_r2r(regs, regd)		sse_r2r(xorps, regs, regd)

#define	maxps_m2r(var, reg)		sse_m2r(maxps, var, reg)
#define	maxps_r2r(regs, regd)		sse_r2r(maxps, regs, regd)

#define	maxss_m2r(var, reg)		sse_m2r(maxss, var, reg)
#define	maxss_r2r(regs, regd)		sse_r2r(maxss, regs, regd)

#define	minps_m2r(var, reg)		sse_m2r(minps, var, reg)
#define	minps_r2r(regs, regd)		sse_r2r(minps, regs, regd)

#define	minss_m2r(var, reg)		sse_m2r(minss, var, reg)
#define	minss_r2r(regs, regd)		sse_r2r(minss, regs, regd)

#define	cmpps_m2r(var, reg, op)		sse_m2ri(cmpps, var, reg, op)
#define	cmpps_r2r(regs, regd, op)	sse_r2ri(cmpps, regs, regd, op)

#define	cmpeqps_m2r(var, reg)		sse_m2ri(cmpps, var, reg, 0)
#define	cmpeqps_r2r(regs, regd)		sse_r2ri(cmpps, regs, regd, 0)

#define	cmpltps_m2r(var, reg)		sse_m2ri(cmpps, var, reg, 1)
#define	cmpltps_r2r(regs, regd)		sse_r2ri(cmpps, regs, regd, 1)

#define	cmpleps_m2r(var, reg)		sse_m2ri(cmpps, var, reg, 2)
#define	cmpleps_r2r(regs, regd)		sse_r2ri(cmpps, regs, regd, 2)

#define	cmpunordps_m2r(var, reg)	sse_m2ri(cmpps, var, reg, 3)
#define	cmpunordps_r2r(regs, regd)	sse_r2ri(cmpps, regs, regd, 3)

#define	cmpneqps_m2r(var, reg)		sse_m2ri(cmpps, var, reg, 4)
#define	cmpneqps_r2r(regs, regd)	sse_r2ri(cmpps, regs, regd, 4)

#define	cmpnltps_m2r(var, reg)		sse_m2ri(cmpps, var, reg, 5)
#define	cmpnltps_r2r(regs, regd)	sse_r2ri(cmpps, regs, regd, 5)

#define	cmpnleps_m2r(var, reg)		sse_m2ri(cmpps, var, reg, 6)
#define	cmpnleps_r2r(regs, regd)	sse_r2ri(cmpps, regs, regd, 6)

#define	cmpordps_m2r(var, reg)		sse_m2ri(cmpps, var, reg, 7)
#define	cmpordps_r2r(regs, regd)	sse_r2ri(cmpps, regs, regd, 7)

#define	cmpss_m2r(var, reg, op)		sse_m2ri(cmpss, var, reg, op)
#define	cmpss_r2r(regs, regd, op)	sse_r2ri(cmpss, regs, regd, op)

#define	cmpeqss_m2r(var, reg)		sse_m2ri(cmpss, var, reg, 0)
#define	cmpeqss_r2r(regs, regd)		sse_r2ri(cmpss, regs, regd, 0)

#define	cmpltss_m2r(var, reg)		sse_m2ri(cmpss, var, reg, 1)
#define	cmpltss_r2r(regs, regd)		sse_r2ri(cmpss, regs, regd, 1)

#define	cmpless_m2r(var, reg)		sse_m2ri(cmpss, var, reg, 2)
#define	cmpless_r2r(regs, regd)		sse_r2ri(cmpss, regs, regd, 2)

#define	cmpunordss_m2r(var, reg)	sse_m2ri(cmpss, var, reg, 3)
#define	cmpunordss_r2r(regs, regd)	sse_r2ri(cmpss, regs, regd, 3)

#define	cmpneqss_m2r(var, reg)		sse_m2ri(cmpss, var, reg, 4)
#define	cmpneqss_r2r(regs, regd)	sse_r2ri(cmpss, regs, regd, 4)

#define	cmpnltss_m2r(var, reg)		sse_m2ri(cmpss, var, reg, 5)
#define	cmpnltss_r2r(regs, regd)	sse_r2ri(cmpss, regs, regd, 5)

#define	cmpnless_m2r(var, reg)		sse_m2ri(cmpss, var, reg, 6)
#define	cmpnless_r2r(regs, regd)	sse_r2ri(cmpss, regs, regd, 6)

#define	cmpordss_m2r(var, reg)		sse_m2ri(cmpss, var, reg, 7)
#define	cmpordss_r2r(regs, regd)	sse_r2ri(cmpss, regs, regd, 7)

#define	comiss_m2r(var, reg)		sse_m2r(comiss, var, reg)
#define	comiss_r2r(regs, regd)		sse_r2r(comiss, regs, regd)

#define	ucomiss_m2r(var, reg)		sse_m2r(ucomiss, var, reg)
#define	ucomiss_r2r(regs, regd)		sse_r2r(ucomiss, regs, regd)

#define	unpcklps_m2r(var, reg)		sse_m2r(unpcklps, var, reg)
#define	unpcklps_r2r(regs, regd)	sse_r2r(unpcklps, regs, regd)

#define	unpckhps_m2r(var, reg)		sse_m2r(unpckhps, var, reg)
#define	unpckhps_r2r(regs, regd)	sse_r2r(unpckhps, regs, regd)

#define	fxrstor(mem) \
	__asm__ __volatile__ ("fxrstor %0" \
			      : /* nothing */ \
			      : "X" (mem))

#define	fxsave(mem) \
	__asm__ __volatile__ ("fxsave %0" \
			      : /* nothing */ \
			      : "X" (mem))

#define	stmxcsr(mem) \
	__asm__ __volatile__ ("stmxcsr %0" \
			      : /* nothing */ \
			      : "X" (mem))

#define	ldmxcsr(mem) \
	__asm__ __volatile__ ("ldmxcsr %0" \
			      : /* nothing */ \
			      : "X" (mem))

