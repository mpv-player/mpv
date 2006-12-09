/*
 * cpu_accel.c
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
 *
 * Modified for use with MPlayer, see libmpeg-0.4.1.diff for the exact changes.
 * detailed changelog at http://svn.mplayerhq.hu/mplayer/trunk/
 * $Id$
 */

#include "config.h"
#include "cpudetect.h"

#include <inttypes.h>

#include "mpeg2.h"
#include "attributes.h"
#include "mpeg2_internal.h"

#ifdef ACCEL_DETECT
#if defined(ARCH_X86) || defined(ARCH_X86_64)

/* MPlayer imports libmpeg2 as decoder, which detects MMX / 3DNow! 
 * instructions via assembly. However, it is regarded as duplicaed work
 * in MPlayer, so that we enforce to use MPlayer's implementation.
 */
#define USE_MPLAYER_CPUDETECT

static inline uint32_t arch_accel (void)
{
#if !defined(USE_MPLAYER_CPUDETECT)
    uint32_t eax, ebx, ecx, edx;
    int AMD;
    uint32_t caps;

#if defined(__x86_64__) || (!defined(PIC) && !defined(__PIC__))
#define cpuid(op,eax,ebx,ecx,edx)	\
    __asm__ ("cpuid"			\
	     : "=a" (eax),		\
	       "=b" (ebx),		\
	       "=c" (ecx),		\
	       "=d" (edx)		\
	     : "a" (op)			\
	     : "cc")
#else  /* PIC version : save ebx (not needed on x86_64) */
#define cpuid(op,eax,ebx,ecx,edx)	\
    __asm__ ("pushl %%ebx\n\t"		\
	     "cpuid\n\t"		\
	     "movl %%ebx,%1\n\t"	\
	     "popl %%ebx"		\
	     : "=a" (eax),		\
	       "=r" (ebx),		\
	       "=c" (ecx),		\
	       "=d" (edx)		\
	     : "a" (op)			\
	     : "cc")
#endif

#ifndef __x86_64__ /* x86_64 supports the cpuid op */
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
#endif

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
#else /* USE_MPLAYER_CPUDETECT: Use MPlayer's cpu capability property */
    caps = 0;
    if (gCpuCaps.hasMMX)
        caps |= MPEG2_ACCEL_X86_MMX;
    if (gCpuCaps.hasSSE2)
	caps |= MPEG2_ACCEL_X86_SSE2;
    if (gCpuCaps.hasMMX2)
	caps |= MPEG2_ACCEL_X86_MMXEXT;
    if (gCpuCaps.has3DNow)
	caps |= MPEG2_ACCEL_X86_3DNOW;

    return caps;

#endif /* USE_MPLAYER_CPUDETECT */
}
#endif /* ARCH_X86 || ARCH_X86_64 */

#if defined(ARCH_PPC) || (defined(ARCH_SPARC) && defined(HAVE_VIS))
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

#ifdef ARCH_PPC
static uint32_t arch_accel (void)
{
    static RETSIGTYPE (* oldsig) (int);

    oldsig = signal (SIGILL, sigill_handler);
    if (sigsetjmp (jmpbuf, 1)) {
	signal (SIGILL, oldsig);
	return 0;
    }

    canjump = 1;

#if defined( __APPLE_CC__ ) && defined( __APPLE_ALTIVEC__ ) /* apple */
#define VAND(a,b,c) "vand v" #a ",v" #b ",v" #c "\n\t"
#else			/* gnu */
#define VAND(a,b,c) "vand " #a "," #b "," #c "\n\t"
#endif
    asm volatile ("mtspr 256, %0\n\t"
		  VAND (0, 0, 0)
		  :
		  : "r" (-1));

    canjump = 0;

    signal (SIGILL, oldsig);
    return MPEG2_ACCEL_PPC_ALTIVEC;
}
#endif /* ARCH_PPC */

#ifdef ARCH_SPARC
static uint32_t arch_accel (void)
{
    static RETSIGTYPE (* oldsig) (int);

    oldsig = signal (SIGILL, sigill_handler);
    if (sigsetjmp (jmpbuf, 1)) {
	signal (SIGILL, oldsig);
	return 0;
    }

    canjump = 1;

    /* pdist %f0, %f0, %f0 */
    __asm__ __volatile__(".word\t0x81b007c0");

    canjump = 0;

    if (sigsetjmp (jmpbuf, 1)) {
	signal (SIGILL, oldsig);
	return MPEG2_ACCEL_SPARC_VIS;
    }

    canjump = 1;

    /* edge8n %g0, %g0, %g0 */
    __asm__ __volatile__(".word\t0x81b00020");

    canjump = 0;

    signal (SIGILL, oldsig);
    return MPEG2_ACCEL_SPARC_VIS | MPEG2_ACCEL_SPARC_VIS2;
}
#endif /* ARCH_SPARC */
#endif /* ARCH_PPC || ARCH_SPARC */

#ifdef ARCH_ALPHA
static uint32_t arch_accel (void)
{
#ifdef CAN_COMPILE_ALPHA_MVI
    uint64_t no_mvi;

    asm volatile ("amask %1, %0"
		  : "=r" (no_mvi)
		  : "rI" (256));	/* AMASK_MVI */
    return no_mvi ? MPEG2_ACCEL_ALPHA : (MPEG2_ACCEL_ALPHA |
					 MPEG2_ACCEL_ALPHA_MVI);
#else
    return MPEG2_ACCEL_ALPHA;
#endif
}
#endif /* ARCH_ALPHA */
#endif /* ACCEL_DETECT */

uint32_t mpeg2_detect_accel (void)
{
    uint32_t accel;

    accel = 0;
#ifdef ACCEL_DETECT
#if defined (ARCH_X86) || defined (ARCH_X86_64) || defined (ARCH_PPC) || defined (ARCH_ALPHA) || defined (ARCH_SPARC)
    accel = arch_accel ();
#endif
#endif
    return accel;
}
