/*
 * cpu_accel.c
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

#include "config.h"

#include <inttypes.h>

#include "mpeg2.h"

#ifdef ACCEL_DETECT
#ifdef ARCH_X86
static inline uint32_t arch_accel (void)
{
    uint32_t eax, ebx, ecx, edx;
    int AMD;
    uint32_t caps;

#ifndef PIC
#define cpuid(op,eax,ebx,ecx,edx)	\
    __asm__ ("cpuid"			\
	     : "=a" (eax),		\
	       "=b" (ebx),		\
	       "=c" (ecx),		\
	       "=d" (edx)		\
	     : "a" (op)			\
	     : "cc")
#else	/* PIC version : save ebx */
#define cpuid(op,eax,ebx,ecx,edx)	\
    __asm__ ("push %%ebx\n\t"		\
	     "cpuid\n\t"		\
	     "movl %%ebx,%1\n\t"	\
	     "pop %%ebx"		\
	     : "=a" (eax),		\
	       "=r" (ebx),		\
	       "=c" (ecx),		\
	       "=d" (edx)		\
	     : "a" (op)			\
	     : "cc")
#endif

    __asm__ ("pushf\n\t"
	     "pushf\n\t"
	     "pop %0\n\t"
	     "movl %0,%1\n\t"
	     "xorl $0x200000,%0\n\t"
	     "push %0\n\t"
	     "popf\n\t"
	     "pushf\n\t"
	     "pop %0\n\t"
	     "popf"
	     : "=r" (eax),
	       "=r" (ebx)
	     :
	     : "cc");

    if (eax == ebx)		/* no cpuid */
	return 0;

    cpuid (0x00000000, eax, ebx, ecx, edx);
    if (!eax)			/* vendor string only */
	return 0;

    AMD = (ebx == 0x68747541) && (ecx == 0x444d4163) && (edx == 0x69746e65);

    cpuid (0x00000001, eax, ebx, ecx, edx);
    if (! (edx & 0x00800000))	/* no MMX */
	return 0;

    caps = MPEG2_ACCEL_X86_MMX;
    if (edx & 0x02000000)	/* SSE - identical to AMD MMX extensions */
	caps = MPEG2_ACCEL_X86_MMX | MPEG2_ACCEL_X86_MMXEXT;

    cpuid (0x80000000, eax, ebx, ecx, edx);
    if (eax < 0x80000001)	/* no extended capabilities */
	return caps;

    cpuid (0x80000001, eax, ebx, ecx, edx);

    if (edx & 0x80000000)
	caps |= MPEG2_ACCEL_X86_3DNOW;

    if (AMD && (edx & 0x00400000))	/* AMD MMX extensions */
	caps |= MPEG2_ACCEL_X86_MMXEXT;

    return caps;
}
#endif /* ARCH_X86 */

#ifdef ARCH_PPC
#include <signal.h>
#include <setjmp.h>

static sigjmp_buf jmpbuf;
static volatile sig_atomic_t canjump = 0;

static RETSIGTYPE sigill_handler (int sig)
{
    if (!canjump) {
	signal (sig, SIG_DFL);
	raise (sig);
    }

    canjump = 0;
    siglongjmp (jmpbuf, 1);
}

static inline uint32_t arch_accel (void)
{
    signal (SIGILL, sigill_handler);
    if (sigsetjmp (jmpbuf, 1)) {
	signal (SIGILL, SIG_DFL);
	return 0;
    }

    canjump = 1;

    asm volatile ("mtspr 256, %0\n\t"
		  "vand %%v0, %%v0, %%v0"
		  :
		  : "r" (-1));

    signal (SIGILL, SIG_DFL);
    return MPEG2_ACCEL_PPC_ALTIVEC;
}
#endif /* ARCH_PPC */

#ifdef ARCH_ALPHA
static inline uint32_t arch_accel (void)
{
    uint64_t no_mvi;

    asm volatile ("amask %1, %0"
		  : "=r" (no_mvi)
		  : "rI" (256));	/* AMASK_MVI */
    return no_mvi ? MPEG2_ACCEL_ALPHA : (MPEG2_ACCEL_ALPHA |
					 MPEG2_ACCEL_ALPHA_MVI);
}
#endif /* ARCH_ALPHA */
#endif

uint32_t mpeg2_detect_accel (void)
{
    uint32_t accel;

    accel = 0;
#ifdef ACCEL_DETECT
#ifdef LIBMPEG2_MLIB
    accel = MPEG2_ACCEL_MLIB;
#endif
#if defined (ARCH_X86) || defined (ARCH_PPC) || defined (ARCH_ALPHA)
    accel |= arch_accel ();
#endif
#endif
    return accel;
}
