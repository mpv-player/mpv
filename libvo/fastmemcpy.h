/*
 This part of code was taken by from Linux-2.4.3 and slightly modified
for MMX2 instruction set. I have done it since linux uses page aligned
blocks but mplayer uses weakly ordered data and original sources can not
speedup their. Only using prefetchnta and movntq together have effect! 
If you have questions please contact with me: Nick Kurshev: nickols_k@mail.ru.
*/
#ifdef HAVE_MMX2
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
		"1: prefetchnta (%0)\n"		/* This set is 28 bytes */
		"   prefetchnta 64(%0)\n"
		"   prefetchnta 128(%0)\n"
		"   prefetchnta 192(%0)\n"
		"   prefetchnta 256(%0)\n"
#if 0		
		"2:  \n"
		".section .fixup, \"ax\"\n"
		"3: movw $0x1AEB, 1b\n"	/* jmp on 26 bytes */
		"   jmp 2b\n"
		".previous\n"
		".section __ex_table,\"a\"\n"
		"	.align 4\n"
		"	.long 1b, 3b\n"
		".previous"
#endif		
		: : "r" (from) );
		
	
	for(; i>0; i--)
	{
		__asm__ __volatile__ (
		"1:  prefetchnta 320(%0)\n"
		"2:  movq (%0), %%mm0\n"
		"  movq 8(%0), %%mm1\n"
		"  movq 16(%0), %%mm2\n"
		"  movq 24(%0), %%mm3\n"
		"  movntq %%mm0, (%1)\n"
		"  movntq %%mm1, 8(%1)\n"
		"  movntq %%mm2, 16(%1)\n"
		"  movntq %%mm3, 24(%1)\n"
		"  movq 32(%0), %%mm0\n"
		"  movq 40(%0), %%mm1\n"
		"  movq 48(%0), %%mm2\n"
		"  movq 56(%0), %%mm3\n"
		"  movntq %%mm0, 32(%1)\n"
		"  movntq %%mm1, 40(%1)\n"
		"  movntq %%mm2, 48(%1)\n"
		"  movntq %%mm3, 56(%1)\n"
#if 0		
		".section .fixup, \"ax\"\n"
		"3: movw $0x05EB, 1b\n"	/* jmp on 5 bytes */
		"   jmp 2b\n"
		".previous\n"
		".section __ex_table,\"a\"\n"
		"	.align 4\n"
		"	.long 1b, 3b\n"
		".previous"
#endif		
		: : "r" (from), "r" (to) : "memory");
		from+=64;
		to+=64;
	}
	        __asm__ __volatile__ ("emms":::"memory");
	}
	/*
	 *	Now do the tail of the block
	 */
	small_memcpy(to, from, len);
	return p;
}
#define memcpy(a,b,c) fast_memcpy(a,b,c)
#endif


