
#ifndef __MPLAYER_MEMCPY
#define __MPLAYER_MEMCPY

/*
 This part of code was taken by from Linux-2.4.3 and slightly modified
for MMX2 instruction set. I have done it since linux uses page aligned
blocks but mplayer uses weakly ordered data and original sources can not
speedup their. Only using prefetchnta and movntq together have effect! 
If you have questions please contact with me: Nick Kurshev: nickols_k@mail.ru.
*/

// 3dnow memcpy support from kernel 2.4.2
//  by Pontscho/fresh!mindworkz

#if defined( HAVE_MMX2 ) || defined( HAVE_3DNOW )

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

        if(len >= 0x200) /* 512-byte blocks */
	{
  	  p = to;
	  i = len >> 6; /* len/64 */
	  len&=63;
	  
	__asm__ __volatile__ (
#if defined( HAVE_3DNOW ) && !defined( HAVE_MMX2 )
	        "prefetch (%0)\n"
	        "prefetch 64(%0)\n"
	        "prefetch 128(%0)\n"
        	"prefetch 192(%0)\n"
        	"prefetch 256(%0)\n"
#else
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
	for(; i>0; i--)
	{
		__asm__ __volatile__ (
#if defined( HAVE_3DNOW ) && !defined( HAVE_MMX2 )
        	"prefetch 320(%0)\n"
#else
		"prefetchnta 320(%0)\n"
#endif
#ifdef HAVE_SSE /* Only P3 (may be Cyrix3) */
		"movups (%0), %%xmm0\n"
		"movups 16(%0), %%xmm1\n"
		"movntps %%xmm0, (%1)\n"
		"movntps %%xmm1, 16(%1)\n"
		"movups 32(%0), %%xmm0\n"
		"movups 48(%0), %%xmm1\n"
		"movntps %%xmm0, 32(%1)\n"
		"movntps %%xmm1, 48(%1)\n"
#else /* Only K7 (may be other) */
#if defined( HAVE_3DNOW ) && !defined( HAVE_MMX2 )
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
#else
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
#endif
		:: "r" (from), "r" (to) : "memory");
		from+=64;
		to+=64;
	}
#if defined( HAVE_3DNOW ) && !defined( HAVE_MMX2 )
		__asm__ __volatile__ ("femms":::"memory");
#else
		__asm__ __volatile__ ("emms":::"memory");
#endif
	}
	/*
	 *	Now do the tail of the block
	 */
#if 0
	small_memcpy(to, from, len);
#else
        __asm__ __volatile__ (
                "shrl $1,%%ecx\n"
                "jnc 1f\n"
                "movsb\n"
                "1:\n"
                "shrl $1,%%ecx\n"
                "jnc 2f\n"
                "movsw\n"
                "2:\n"
                "rep ; movsl\n"
        	::"D" (to), "S" (from),"c" (len)
        	: "memory");
#endif
	return p;
}
#define memcpy(a,b,c) fast_memcpy(a,b,c)

#endif


