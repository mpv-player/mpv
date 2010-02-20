/*
 * aclib - advanced C library ;)
 * functions which improve and expand the standard C library
 *
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#if !HAVE_SSE2
/*
   P3 processor has only one SSE decoder so can execute only 1 sse insn per
   cpu clock, but it has 3 mmx decoders (include load/store unit)
   and executes 3 mmx insns per cpu clock.
   P4 processor has some chances, but after reading:
   http://www.emulators.com/pentium4.htm
   I have doubts. Anyway SSE2 version of this code can be written better.
*/
#undef HAVE_SSE
#define HAVE_SSE 0
#endif


/*
 This part of code was taken by me from Linux-2.4.3 and slightly modified
for MMX, MMX2, SSE instruction set. I have done it since linux uses page aligned
blocks but mplayer uses weakly ordered data and original sources can not
speedup them. Only using PREFETCHNTA and MOVNTQ together have effect!

>From IA-32 Intel Architecture Software Developer's Manual Volume 1,

Order Number 245470:
"10.4.6. Cacheability Control, Prefetch, and Memory Ordering Instructions"

Data referenced by a program can be temporal (data will be used again) or
non-temporal (data will be referenced once and not reused in the immediate
future). To make efficient use of the processor's caches, it is generally
desirable to cache temporal data and not cache non-temporal data. Overloading
the processor's caches with non-temporal data is sometimes referred to as
"polluting the caches".
The non-temporal data is written to memory with Write-Combining semantics.

The PREFETCHh instructions permits a program to load data into the processor
at a suggested cache level, so that it is closer to the processors load and
store unit when it is needed. If the data is already present in a level of
the cache hierarchy that is closer to the processor, the PREFETCHh instruction
will not result in any data movement.
But we should you PREFETCHNTA: Non-temporal data fetch data into location
close to the processor, minimizing cache pollution.

The MOVNTQ (store quadword using non-temporal hint) instruction stores
packed integer data from an MMX register to memory, using a non-temporal hint.
The MOVNTPS (store packed single-precision floating-point values using
non-temporal hint) instruction stores packed floating-point data from an
XMM register to memory, using a non-temporal hint.

The SFENCE (Store Fence) instruction controls write ordering by creating a
fence for memory store operations. This instruction guarantees that the results
of every store instruction that precedes the store fence in program order is
globally visible before any store instruction that follows the fence. The
SFENCE instruction provides an efficient way of ensuring ordering between
procedures that produce weakly-ordered data and procedures that consume that
data.

If you have questions please contact with me: Nick Kurshev: nickols_k@mail.ru.
*/

// 3dnow memcpy support from kernel 2.4.2
//  by Pontscho/fresh!mindworkz


#undef HAVE_ONLY_MMX1
#if HAVE_MMX && !HAVE_MMX2 && !HAVE_AMD3DNOW && !HAVE_SSE
/*  means: mmx v.1. Note: Since we added alignment of destinition it speedups
    of memory copying on PentMMX, Celeron-1 and P2 upto 12% versus
    standard (non MMX-optimized) version.
    Note: on K6-2+ it speedups memory copying upto 25% and
          on K7 and P3 about 500% (5 times). */
#define HAVE_ONLY_MMX1
#endif


#undef HAVE_K6_2PLUS
#if !HAVE_MMX2 && HAVE_AMD3DNOW
#define HAVE_K6_2PLUS
#endif

/* for small memory blocks (<256 bytes) this version is faster */
#define small_memcpy(to,from,n)\
{\
register x86_reg dummy;\
__asm__ volatile(\
	"rep; movsb"\
	:"=&D"(to), "=&S"(from), "=&c"(dummy)\
/* It's most portable way to notify compiler */\
/* that edi, esi and ecx are clobbered in asm block. */\
/* Thanks to A'rpi for hint!!! */\
        :"0" (to), "1" (from),"2" (n)\
	: "memory");\
}

#undef MMREG_SIZE
#if HAVE_SSE
#define MMREG_SIZE 16
#else
#define MMREG_SIZE 64 //8
#endif

#undef PREFETCH
#undef EMMS

#if HAVE_MMX2
#define PREFETCH "prefetchnta"
#elif HAVE_AMD3DNOW
#define PREFETCH  "prefetch"
#else
#define PREFETCH " # nop"
#endif

/* On K6 femms is faster of emms. On K7 femms is directly mapped on emms. */
#if HAVE_AMD3DNOW
#define EMMS     "femms"
#else
#define EMMS     "emms"
#endif

#undef MOVNTQ
#if HAVE_MMX2
#define MOVNTQ "movntq"
#else
#define MOVNTQ "movq"
#endif

#undef MIN_LEN
#ifdef HAVE_ONLY_MMX1
#define MIN_LEN 0x800  /* 2K blocks */
#else
#define MIN_LEN 0x40  /* 64-byte blocks */
#endif

static void * RENAME(fast_memcpy)(void * to, const void * from, size_t len)
{
	void *retval;
	size_t i;
	retval = to;
#ifdef STATISTICS
	{
		static int freq[33];
		static int t=0;
		int i;
		for(i=0; len>(1<<i); i++);
		freq[i]++;
		t++;
		if(1024*1024*1024 % t == 0)
			for(i=0; i<32; i++)
				printf("freq < %8d %4d\n", 1<<i, freq[i]);
	}
#endif
#ifndef HAVE_ONLY_MMX1
        /* PREFETCH has effect even for MOVSB instruction ;) */
	__asm__ volatile (
	        PREFETCH" (%0)\n"
	        PREFETCH" 64(%0)\n"
	        PREFETCH" 128(%0)\n"
        	PREFETCH" 192(%0)\n"
        	PREFETCH" 256(%0)\n"
		: : "r" (from) );
#endif
        if(len >= MIN_LEN)
	{
	  register x86_reg delta;
          /* Align destinition to MMREG_SIZE -boundary */
          delta = ((intptr_t)to)&(MMREG_SIZE-1);
          if(delta)
	  {
	    delta=MMREG_SIZE-delta;
	    len -= delta;
	    small_memcpy(to, from, delta);
	  }
	  i = len >> 6; /* len/64 */
	  len&=63;
        /*
           This algorithm is top effective when the code consequently
           reads and writes blocks which have size of cache line.
           Size of cache line is processor-dependent.
           It will, however, be a minimum of 32 bytes on any processors.
           It would be better to have a number of instructions which
           perform reading and writing to be multiple to a number of
           processor's decoders, but it's not always possible.
        */
#if HAVE_SSE /* Only P3 (may be Cyrix3) */
	if(((intptr_t)from) & 15)
	/* if SRC is misaligned */
	for(; i>0; i--)
	{
		__asm__ volatile (
		PREFETCH" 320(%0)\n"
		"movups (%0), %%xmm0\n"
		"movups 16(%0), %%xmm1\n"
		"movups 32(%0), %%xmm2\n"
		"movups 48(%0), %%xmm3\n"
		"movntps %%xmm0, (%1)\n"
		"movntps %%xmm1, 16(%1)\n"
		"movntps %%xmm2, 32(%1)\n"
		"movntps %%xmm3, 48(%1)\n"
		:: "r" (from), "r" (to) : "memory");
		from=((const unsigned char *) from)+64;
		to=((unsigned char *)to)+64;
	}
	else
	/*
	   Only if SRC is aligned on 16-byte boundary.
	   It allows to use movaps instead of movups, which required data
	   to be aligned or a general-protection exception (#GP) is generated.
	*/
	for(; i>0; i--)
	{
		__asm__ volatile (
		PREFETCH" 320(%0)\n"
		"movaps (%0), %%xmm0\n"
		"movaps 16(%0), %%xmm1\n"
		"movaps 32(%0), %%xmm2\n"
		"movaps 48(%0), %%xmm3\n"
		"movntps %%xmm0, (%1)\n"
		"movntps %%xmm1, 16(%1)\n"
		"movntps %%xmm2, 32(%1)\n"
		"movntps %%xmm3, 48(%1)\n"
		:: "r" (from), "r" (to) : "memory");
		from=((const unsigned char *)from)+64;
		to=((unsigned char *)to)+64;
	}
#else
	// Align destination at BLOCK_SIZE boundary
	for(; ((intptr_t)to & (BLOCK_SIZE-1)) && i>0; i--)
	{
		__asm__ volatile (
#ifndef HAVE_ONLY_MMX1
        	PREFETCH" 320(%0)\n"
#endif
		"movq (%0), %%mm0\n"
		"movq 8(%0), %%mm1\n"
		"movq 16(%0), %%mm2\n"
		"movq 24(%0), %%mm3\n"
		"movq 32(%0), %%mm4\n"
		"movq 40(%0), %%mm5\n"
		"movq 48(%0), %%mm6\n"
		"movq 56(%0), %%mm7\n"
		MOVNTQ" %%mm0, (%1)\n"
		MOVNTQ" %%mm1, 8(%1)\n"
		MOVNTQ" %%mm2, 16(%1)\n"
		MOVNTQ" %%mm3, 24(%1)\n"
		MOVNTQ" %%mm4, 32(%1)\n"
		MOVNTQ" %%mm5, 40(%1)\n"
		MOVNTQ" %%mm6, 48(%1)\n"
		MOVNTQ" %%mm7, 56(%1)\n"
		:: "r" (from), "r" (to) : "memory");
		from=((const unsigned char *)from)+64;
		to=((unsigned char *)to)+64;
	}

//	printf(" %d %d\n", (int)from&1023, (int)to&1023);
	// Pure Assembly cuz gcc is a bit unpredictable ;)
	if(i>=BLOCK_SIZE/64)
		__asm__ volatile(
			"xor %%"REG_a", %%"REG_a"	\n\t"
			ASMALIGN(4)
			"1:			\n\t"
				"movl (%0, %%"REG_a"), %%ecx 	\n\t"
				"movl 32(%0, %%"REG_a"), %%ecx 	\n\t"
				"movl 64(%0, %%"REG_a"), %%ecx 	\n\t"
				"movl 96(%0, %%"REG_a"), %%ecx 	\n\t"
				"add $128, %%"REG_a"		\n\t"
				"cmp %3, %%"REG_a"		\n\t"
				" jb 1b				\n\t"

			"xor %%"REG_a", %%"REG_a"	\n\t"

				ASMALIGN(4)
				"2:			\n\t"
				"movq (%0, %%"REG_a"), %%mm0\n"
				"movq 8(%0, %%"REG_a"), %%mm1\n"
				"movq 16(%0, %%"REG_a"), %%mm2\n"
				"movq 24(%0, %%"REG_a"), %%mm3\n"
				"movq 32(%0, %%"REG_a"), %%mm4\n"
				"movq 40(%0, %%"REG_a"), %%mm5\n"
				"movq 48(%0, %%"REG_a"), %%mm6\n"
				"movq 56(%0, %%"REG_a"), %%mm7\n"
				MOVNTQ" %%mm0, (%1, %%"REG_a")\n"
				MOVNTQ" %%mm1, 8(%1, %%"REG_a")\n"
				MOVNTQ" %%mm2, 16(%1, %%"REG_a")\n"
				MOVNTQ" %%mm3, 24(%1, %%"REG_a")\n"
				MOVNTQ" %%mm4, 32(%1, %%"REG_a")\n"
				MOVNTQ" %%mm5, 40(%1, %%"REG_a")\n"
				MOVNTQ" %%mm6, 48(%1, %%"REG_a")\n"
				MOVNTQ" %%mm7, 56(%1, %%"REG_a")\n"
				"add $64, %%"REG_a"		\n\t"
				"cmp %3, %%"REG_a"		\n\t"
				"jb 2b				\n\t"

#if CONFUSION_FACTOR > 0
	// a few percent speedup on out of order executing CPUs
			"mov %5, %%"REG_a"		\n\t"
				"2:			\n\t"
				"movl (%0), %%ecx	\n\t"
				"movl (%0), %%ecx	\n\t"
				"movl (%0), %%ecx	\n\t"
				"movl (%0), %%ecx	\n\t"
				"dec %%"REG_a"		\n\t"
				" jnz 2b		\n\t"
#endif

			"xor %%"REG_a", %%"REG_a"	\n\t"
			"add %3, %0		\n\t"
			"add %3, %1		\n\t"
			"sub %4, %2		\n\t"
			"cmp %4, %2		\n\t"
			" jae 1b		\n\t"
				: "+r" (from), "+r" (to), "+r" (i)
				: "r" ((x86_reg)BLOCK_SIZE), "i" (BLOCK_SIZE/64), "i" ((x86_reg)CONFUSION_FACTOR)
				: "%"REG_a, "%ecx"
		);

	for(; i>0; i--)
	{
		__asm__ volatile (
#ifndef HAVE_ONLY_MMX1
        	PREFETCH" 320(%0)\n"
#endif
		"movq (%0), %%mm0\n"
		"movq 8(%0), %%mm1\n"
		"movq 16(%0), %%mm2\n"
		"movq 24(%0), %%mm3\n"
		"movq 32(%0), %%mm4\n"
		"movq 40(%0), %%mm5\n"
		"movq 48(%0), %%mm6\n"
		"movq 56(%0), %%mm7\n"
		MOVNTQ" %%mm0, (%1)\n"
		MOVNTQ" %%mm1, 8(%1)\n"
		MOVNTQ" %%mm2, 16(%1)\n"
		MOVNTQ" %%mm3, 24(%1)\n"
		MOVNTQ" %%mm4, 32(%1)\n"
		MOVNTQ" %%mm5, 40(%1)\n"
		MOVNTQ" %%mm6, 48(%1)\n"
		MOVNTQ" %%mm7, 56(%1)\n"
		:: "r" (from), "r" (to) : "memory");
		from=((const unsigned char *)from)+64;
		to=((unsigned char *)to)+64;
	}

#endif /* Have SSE */
#if HAVE_MMX2
                /* since movntq is weakly-ordered, a "sfence"
		 * is needed to become ordered again. */
		__asm__ volatile ("sfence":::"memory");
#endif
#if !HAVE_SSE
		/* enables to use FPU */
		__asm__ volatile (EMMS:::"memory");
#endif
	}
	/*
	 *	Now do the tail of the block
	 */
	if(len) small_memcpy(to, from, len);
	return retval;
}

/**
 * special copy routine for mem -> agp/pci copy (based upon fast_memcpy)
 */
static void * RENAME(mem2agpcpy)(void * to, const void * from, size_t len)
{
	void *retval;
	size_t i;
	retval = to;
#ifdef STATISTICS
	{
		static int freq[33];
		static int t=0;
		int i;
		for(i=0; len>(1<<i); i++);
		freq[i]++;
		t++;
		if(1024*1024*1024 % t == 0)
			for(i=0; i<32; i++)
				printf("mem2agp freq < %8d %4d\n", 1<<i, freq[i]);
	}
#endif
        if(len >= MIN_LEN)
	{
	  register x86_reg delta;
          /* Align destinition to MMREG_SIZE -boundary */
          delta = ((intptr_t)to)&7;
          if(delta)
	  {
	    delta=8-delta;
	    len -= delta;
	    small_memcpy(to, from, delta);
	  }
	  i = len >> 6; /* len/64 */
	  len &= 63;
        /*
           This algorithm is top effective when the code consequently
           reads and writes blocks which have size of cache line.
           Size of cache line is processor-dependent.
           It will, however, be a minimum of 32 bytes on any processors.
           It would be better to have a number of instructions which
           perform reading and writing to be multiple to a number of
           processor's decoders, but it's not always possible.
        */
	for(; i>0; i--)
	{
		__asm__ volatile (
        	PREFETCH" 320(%0)\n"
		"movq (%0), %%mm0\n"
		"movq 8(%0), %%mm1\n"
		"movq 16(%0), %%mm2\n"
		"movq 24(%0), %%mm3\n"
		"movq 32(%0), %%mm4\n"
		"movq 40(%0), %%mm5\n"
		"movq 48(%0), %%mm6\n"
		"movq 56(%0), %%mm7\n"
		MOVNTQ" %%mm0, (%1)\n"
		MOVNTQ" %%mm1, 8(%1)\n"
		MOVNTQ" %%mm2, 16(%1)\n"
		MOVNTQ" %%mm3, 24(%1)\n"
		MOVNTQ" %%mm4, 32(%1)\n"
		MOVNTQ" %%mm5, 40(%1)\n"
		MOVNTQ" %%mm6, 48(%1)\n"
		MOVNTQ" %%mm7, 56(%1)\n"
		:: "r" (from), "r" (to) : "memory");
		from=((const unsigned char *)from)+64;
		to=((unsigned char *)to)+64;
	}
#if HAVE_MMX2
                /* since movntq is weakly-ordered, a "sfence"
		 * is needed to become ordered again. */
		__asm__ volatile ("sfence":::"memory");
#endif
		/* enables to use FPU */
		__asm__ volatile (EMMS:::"memory");
	}
	/*
	 *	Now do the tail of the block
	 */
	if(len) small_memcpy(to, from, len);
	return retval;
}
