#ifndef __MPLAYER_MEMCPY
#define __MPLAYER_MEMCPY

/*
 This part of code was taken by from Linux-2.4.3 and slightly modified
for MMX2, SSE instruction set. I have done it since linux uses page aligned
blocks but mplayer uses weakly ordered data and original sources can not
speedup them. Only using PREFETCHNTA and MOVNTQ together have effect!

From IA-32 Intel Architecture Software Developer's Manual Volume 1,
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

#if defined( HAVE_MMX2 ) || defined( HAVE_3DNOW )

#undef HAVE_K6_2PLUS
#if !defined( HAVE_MMX2) && defined( HAVE_3DNOW)
#define HAVE_K6_2PLUS
#endif

/* for small memory blocks (<256 bytes) this version is faster */
#define small_memcpy(to,from,n)\
{\
__asm__ __volatile__(\
	"rep ; movsb\n"\
	::"D" (to), "S" (from),"c" (n)\
	: "memory");\
}

inline static void * fast_memcpy(void * to, const void * from, unsigned len)
{
	void *p;
	int i;

#ifdef HAVE_SSE /* Only P3 (may be Cyrix3) */
//        printf("fastmemcpy_pre(0x%X,0x%X,0x%X)\n",to,from,len);
        // Align dest to 16-byte boundary:
        if((unsigned long)to&15){
          int len2=16-((unsigned long)to&15);
          if(len>len2){
            len-=len2;
            __asm__ __volatile__(
	    "rep ; movsb\n"
	    :"=D" (to), "=S" (from)
            : "D" (to), "S" (from),"c" (len2)
	    : "memory");
          }
        }
//        printf("fastmemcpy(0x%X,0x%X,0x%X)\n",to,from,len);
#endif

        if(len >= 0x200) /* 512-byte blocks */
	{
  	  p = to;
	  i = len >> 6; /* len/64 */
	  len&=63;
	  
	__asm__ __volatile__ (
#ifdef HAVE_K6_2PLUS
	        "prefetch (%0)\n"
	        "prefetch 64(%0)\n"
	        "prefetch 128(%0)\n"
        	"prefetch 192(%0)\n"
        	"prefetch 256(%0)\n"
#else /* K7, P3, CyrixIII */
		"prefetchnta (%0)\n"
		"prefetchnta 64(%0)\n"
		"prefetchnta 128(%0)\n"
		"prefetchnta 192(%0)\n"
		"prefetchnta 256(%0)\n"
#endif
		: : "r" (from) );
        /*
           This algorithm is top effective when the code consequently
           reads and writes blocks which have size of cache line.
           Size of cache line is processor-dependent.
           It will, however, be a minimum of 32 bytes on any processors.
           It would be better to have a number of instructions which
           perform reading and writing to be multiple to a number of
           processor's decoders, but it's not always possible.
        */
#ifdef HAVE_SSE /* Only P3 (may be Cyrix3) */
	if(((unsigned long)from) & 15)
	/* if SRC is misaligned */
	for(; i>0; i--)
	{
		__asm__ __volatile__ (
		"prefetchnta 320(%0)\n"
		"movups (%0), %%xmm0\n"
		"movups 16(%0), %%xmm1\n"
		"movntps %%xmm0, (%1)\n"
		"movntps %%xmm1, 16(%1)\n"
		"movups 32(%0), %%xmm0\n"
		"movups 48(%0), %%xmm1\n"
		"movntps %%xmm0, 32(%1)\n"
		"movntps %%xmm1, 48(%1)\n"
		:: "r" (from), "r" (to) : "memory");
		from+=64;
		to+=64;
	}
	else 
	/*
	   Only if SRC is aligned on 16-byte boundary.
	   It allows to use movaps instead of movups, which required data
	   to be aligned or a general-protection exception (#GP) is generated.
	*/
	for(; i>0; i--)
	{
		__asm__ __volatile__ (
		"prefetchnta 320(%0)\n"
		"movaps (%0), %%xmm0\n"
		"movaps 16(%0), %%xmm1\n"
		"movntps %%xmm0, (%1)\n"
		"movntps %%xmm1, 16(%1)\n"
		"movaps 32(%0), %%xmm0\n"
		"movaps 48(%0), %%xmm1\n"
		"movntps %%xmm0, 32(%1)\n"
		"movntps %%xmm1, 48(%1)\n"
		:: "r" (from), "r" (to) : "memory");
		from+=64;
		to+=64;
	}
#else
	for(; i>0; i--)
	{
		__asm__ __volatile__ (
#ifdef HAVE_K6_2PLUS
        	"prefetch 320(%0)\n"
#else
		"prefetchnta 320(%0)\n"
#endif
#ifdef HAVE_K6_2PLUS
        	"movq (%0), %%mm0\n"
        	"movq 8(%0), %%mm1\n"
        	"movq 16(%0), %%mm2\n"
        	"movq 24(%0), %%mm3\n"
        	"movq %%mm0, (%1)\n"
        	"movq %%mm1, 8(%1)\n"
        	"movq %%mm2, 16(%1)\n"
        	"movq %%mm3, 24(%1)\n"
        	"movq 32(%0), %%mm0\n"
        	"movq 40(%0), %%mm1\n"
        	"movq 48(%0), %%mm2\n"
        	"movq 56(%0), %%mm3\n"
        	"movq %%mm0, 32(%1)\n"
        	"movq %%mm1, 40(%1)\n"
        	"movq %%mm2, 48(%1)\n"
        	"movq %%mm3, 56(%1)\n"
#else /* K7 */
		"movq (%0), %%mm0\n"
		"movq 8(%0), %%mm1\n"
		"movq 16(%0), %%mm2\n"
		"movq 24(%0), %%mm3\n"
		"movntq %%mm0, (%1)\n"
		"movntq %%mm1, 8(%1)\n"
		"movntq %%mm2, 16(%1)\n"
		"movntq %%mm3, 24(%1)\n"
		"movq 32(%0), %%mm0\n"
		"movq 40(%0), %%mm1\n"
		"movq 48(%0), %%mm2\n"
		"movq 56(%0), %%mm3\n"
		"movntq %%mm0, 32(%1)\n"
		"movntq %%mm1, 40(%1)\n"
		"movntq %%mm2, 48(%1)\n"
		"movntq %%mm3, 56(%1)\n"
#endif
		:: "r" (from), "r" (to) : "memory");
		from+=64;
		to+=64;
	}
#endif /* Have SSE */
#ifdef HAVE_K6_2PLUS
                /* On K6 femms is fatser of emms.
		   On K7 femms is directly mapped on emms. */
		__asm__ __volatile__ ("femms":::"memory");
#else /* K7, P3, CyrixIII */
                /* since movntq is weakly-ordered, a "sfence"
		 * is needed to become ordered again. */
		__asm__ __volatile__ ("sfence":::"memory");
#ifndef HAVE_SSE		
		/* enables to use FPU */
		__asm__ __volatile__ ("emms":::"memory");
#endif		
#endif
	}
	/*
	 *	Now do the tail of the block
	 */
	small_memcpy(to, from, len);
	return p;
}
#define memcpy(a,b,c) fast_memcpy(a,b,c)
#undef small_memcpy

#endif

#endif
