// this code come from a52dec/libao/audio_out_oss.c

// FIXME FIXME FIXME

// a52_resample_init should find the requested converter (from type flags ->
// given number of channels) and set up some function pointers...

// a52_resample() should do the conversion.

// MMX optimizations from Michael Niedermayer (michaelni@gmx.at) (under GPL)

/* optimization TODO / NOTES 
    movntq is slightly faster (0.5% with the current test.c benchmark) 
	(but thats just test.c so that needs to be testd in reallity)
	and it would mean (C / MMX2 / MMX / 3DNOW) versions 
*/

#include <inttypes.h>
#include <stdio.h>
#include "a52.h"
#include "mm_accel.h"
#include "../config.h"

int (* a52_resample) (float * _f, int16_t * s16)=NULL;

#ifdef ARCH_X86
static uint64_t __attribute__((aligned(8))) magicF2W= 0x43c0000043c00000LL;
static uint64_t __attribute__((aligned(8))) wm1010= 0xFFFF0000FFFF0000LL;
static uint64_t __attribute__((aligned(8))) wm0101= 0x0000FFFF0000FFFFLL;
static uint64_t __attribute__((aligned(8))) wm1100= 0xFFFFFFFF00000000LL;
#endif

static inline int16_t convert (int32_t i)
{
    if (i > 0x43c07fff)
	return 32767;
    else if (i < 0x43bf8000)
	return -32768;
    else
	return i - 0x43c00000;
}

static int chans=2;
static int flags=0;

int a52_resample_C(float * _f, int16_t * s16)
{
    int i;
    int32_t * f = (int32_t *) _f;

    switch (flags) {
    case A52_MONO:
	for (i = 0; i < 256; i++) {
	    s16[5*i] = s16[5*i+1] = s16[5*i+2] = s16[5*i+3] = 0;
	    s16[5*i+4] = convert (f[i]);
	}
	break;
    case A52_CHANNEL:
    case A52_STEREO:
    case A52_DOLBY:
	for (i = 0; i < 256; i++) {
	    s16[2*i] = convert (f[i]);
	    s16[2*i+1] = convert (f[i+256]);
	}
	break;
    case A52_3F:
	for (i = 0; i < 256; i++) {
	    s16[5*i] = convert (f[i]);
	    s16[5*i+1] = convert (f[i+512]);
	    s16[5*i+2] = s16[5*i+3] = 0;
	    s16[5*i+4] = convert (f[i+256]);
	}
	break;
    case A52_2F2R:
	for (i = 0; i < 256; i++) {
	    s16[4*i] = convert (f[i]);
	    s16[4*i+1] = convert (f[i+256]);
	    s16[4*i+2] = convert (f[i+512]);
	    s16[4*i+3] = convert (f[i+768]);
	}
	break;
    case A52_3F2R:
	for (i = 0; i < 256; i++) {
	    s16[5*i] = convert (f[i]);
	    s16[5*i+1] = convert (f[i+512]);
	    s16[5*i+2] = convert (f[i+768]);
	    s16[5*i+3] = convert (f[i+1024]);
	    s16[5*i+4] = convert (f[i+256]);
	}
	break;
    case A52_MONO | A52_LFE:
	for (i = 0; i < 256; i++) {
	    s16[6*i] = s16[6*i+1] = s16[6*i+2] = s16[6*i+3] = 0;
	    s16[6*i+4] = convert (f[i+256]);
	    s16[6*i+5] = convert (f[i]);
	}
	break;
    case A52_CHANNEL | A52_LFE:
    case A52_STEREO | A52_LFE:
    case A52_DOLBY | A52_LFE:
	for (i = 0; i < 256; i++) {
	    s16[6*i] = convert (f[i+256]);
	    s16[6*i+1] = convert (f[i+512]);
	    s16[6*i+2] = s16[6*i+3] = s16[6*i+4] = 0;
	    s16[6*i+5] = convert (f[i]);
	}
	break;
    case A52_3F | A52_LFE:
	for (i = 0; i < 256; i++) {
	    s16[6*i] = convert (f[i+256]);
	    s16[6*i+1] = convert (f[i+768]);
	    s16[6*i+2] = s16[6*i+3] = 0;
	    s16[6*i+4] = convert (f[i+512]);
	    s16[6*i+5] = convert (f[i]);
	}
	break;
    case A52_2F2R | A52_LFE:
	for (i = 0; i < 256; i++) {
	    s16[6*i] = convert (f[i+256]);
	    s16[6*i+1] = convert (f[i+512]);
	    s16[6*i+2] = convert (f[i+768]);
	    s16[6*i+3] = convert (f[i+1024]);
	    s16[6*i+4] = 0;
	    s16[6*i+5] = convert (f[i]);
	}
	break;
    case A52_3F2R | A52_LFE:
	for (i = 0; i < 256; i++) {
	    s16[6*i] = convert (f[i+256]);
	    s16[6*i+1] = convert (f[i+768]);
	    s16[6*i+2] = convert (f[i+1024]);
	    s16[6*i+3] = convert (f[i+1280]);
	    s16[6*i+4] = convert (f[i+512]);
	    s16[6*i+5] = convert (f[i]);
	}
	break;
    }
    return chans*256;
}

#ifdef ARCH_X86
int a52_resample_MMX(float * _f, int16_t * s16)
{
    int i;
    int32_t * f = (int32_t *) _f;

    switch (flags) {
    case A52_MONO:
	asm volatile(
		"movl $-512, %%esi		\n\t"
		"movq magicF2W, %%mm7		\n\t"
		"movq wm1100, %%mm3		\n\t"
		"movq wm0101, %%mm4		\n\t"
		"movq wm1010, %%mm5		\n\t"
		"pxor %%mm6, %%mm6		\n\t"
		"1:				\n\t"
		"movq (%1, %%esi, 2), %%mm0	\n\t"
		"movq 8(%1, %%esi, 2), %%mm1	\n\t"
		"leal (%%esi, %%esi, 4), %%edi	\n\t"
		"psubd %%mm7, %%mm0		\n\t"
		"psubd %%mm7, %%mm1		\n\t"
		"packssdw %%mm1, %%mm0		\n\t"
		"movq %%mm0, %%mm1		\n\t"
		"pand %%mm4, %%mm0		\n\t"
		"pand %%mm5, %%mm1		\n\t"
		"movq %%mm6, (%0, %%edi)	\n\t" // 0 0 0 0
		"movd %%mm0, 8(%0, %%edi)	\n\t" // A 0
		"pand %%mm3, %%mm0		\n\t"
		"movd %%mm6, 12(%0, %%edi)	\n\t" // 0 0
		"movd %%mm1, 16(%0, %%edi)	\n\t" // 0 B
		"pand %%mm3, %%mm1		\n\t"
		"movd %%mm6, 20(%0, %%edi)	\n\t" // 0 0
		"movq %%mm0, 24(%0, %%edi)	\n\t" // 0 0 C 0
		"movq %%mm1, 32(%0, %%edi)	\n\t" // 0 0 0 B
		"addl $8, %%esi			\n\t"
		" jnz 1b			\n\t"
		"emms				\n\t"
		:: "r" (s16+1280), "r" (f+256)
		:"%esi", "%edi", "memory"
	);
	break;
    case A52_CHANNEL:
    case A52_STEREO:
    case A52_DOLBY:
/* benchmark scores are 0.3% better with SSE but we would need to set bias=0 and premultiply it
#ifdef HAVE_SSE
	asm volatile(
		"movl $-1024, %%esi		\n\t"
		"1:				\n\t"
		"cvtps2pi (%1, %%esi), %%mm0	\n\t"
		"cvtps2pi 1024(%1, %%esi), %%mm2\n\t"
		"movq %%mm0, %%mm1		\n\t"
		"punpcklwd %%mm2, %%mm0		\n\t"
		"punpckhwd %%mm2, %%mm1		\n\t"
		"movq %%mm0, (%0, %%esi)	\n\t"
		"movq %%mm1, 8(%0, %%esi)	\n\t"
		"addl $16, %%esi		\n\t"
		" jnz 1b			\n\t"
		"emms				\n\t"
		:: "r" (s16+512), "r" (f+256)
		:"%esi", "memory"
	);*/
	asm volatile(
		"movl $-1024, %%esi		\n\t"
		"movq magicF2W, %%mm7		\n\t"
		"1:				\n\t"
		"movq (%1, %%esi), %%mm0	\n\t"
		"movq 8(%1, %%esi), %%mm1	\n\t"
		"movq 1024(%1, %%esi), %%mm2	\n\t"
		"movq 1032(%1, %%esi), %%mm3	\n\t"
		"psubd %%mm7, %%mm0		\n\t"
		"psubd %%mm7, %%mm1		\n\t"
		"psubd %%mm7, %%mm2		\n\t"
		"psubd %%mm7, %%mm3		\n\t"
		"packssdw %%mm1, %%mm0		\n\t"
		"packssdw %%mm3, %%mm2		\n\t"
		"movq %%mm0, %%mm1		\n\t"
		"punpcklwd %%mm2, %%mm0		\n\t"
		"punpckhwd %%mm2, %%mm1		\n\t"
		"movq %%mm0, (%0, %%esi)	\n\t"
		"movq %%mm1, 8(%0, %%esi)	\n\t"
		"addl $16, %%esi		\n\t"
		" jnz 1b			\n\t"
		"emms				\n\t"
		:: "r" (s16+512), "r" (f+256)
		:"%esi", "memory"
	);
	break;
    case A52_3F: 
	asm volatile(
		"movl $-1024, %%esi		\n\t"
		"movq magicF2W, %%mm7		\n\t"
		"pxor %%mm6, %%mm6		\n\t"
		"movq %%mm7, %%mm5		\n\t"
		"punpckldq %%mm6, %%mm5		\n\t"
		"1:				\n\t"
		"movd (%1, %%esi), %%mm0	\n\t"
		"punpckldq 2048(%1, %%esi), %%mm0\n\t"
		"movd 1024(%1, %%esi), %%mm1	\n\t"
		"punpckldq 4(%1, %%esi), %%mm1	\n\t"
		"movd 2052(%1, %%esi), %%mm2	\n\t"
		"movq %%mm7, %%mm3		\n\t"
		"punpckldq 1028(%1, %%esi), %%mm3\n\t"
		"movd 8(%1, %%esi), %%mm4	\n\t"
		"punpckldq 2056(%1, %%esi), %%mm4\n\t"
		"leal (%%esi, %%esi, 4), %%edi	\n\t"
		"sarl $1, %%edi			\n\t"
		"psubd %%mm7, %%mm0		\n\t"
		"psubd %%mm7, %%mm1		\n\t"
		"psubd %%mm5, %%mm2		\n\t"
		"psubd %%mm7, %%mm3		\n\t"
		"psubd %%mm7, %%mm4		\n\t"
		"packssdw %%mm6, %%mm0		\n\t"
		"packssdw %%mm2, %%mm1		\n\t"
		"packssdw %%mm4, %%mm3		\n\t"
		"movq %%mm0, (%0, %%edi)	\n\t"
		"movq %%mm1, 8(%0, %%edi)	\n\t"
		"movq %%mm3, 16(%0, %%edi)	\n\t"
		
		"movd 1032(%1, %%esi), %%mm1	\n\t"
		"punpckldq 12(%1, %%esi), %%mm1\n\t"
		"movd 2060(%1, %%esi), %%mm2	\n\t"
		"movq %%mm7, %%mm3		\n\t"
		"punpckldq 1036(%1, %%esi), %%mm3\n\t"
		"pxor %%mm0, %%mm0		\n\t"
		"psubd %%mm7, %%mm1		\n\t"
		"psubd %%mm5, %%mm2		\n\t"
		"psubd %%mm7, %%mm3		\n\t"
		"packssdw %%mm1, %%mm0		\n\t"
		"packssdw %%mm3, %%mm2		\n\t"
		"movq %%mm0, 24(%0, %%edi)	\n\t"
		"movq %%mm2, 32(%0, %%edi)	\n\t"
				
		"addl $16, %%esi		\n\t"
		" jnz 1b			\n\t"
		"emms				\n\t"
		:: "r" (s16+1280), "r" (f+256)
		:"%esi", "%edi", "memory"
	);
	break;
    case A52_2F2R:
	asm volatile(
		"movl $-1024, %%esi		\n\t"
		"movq magicF2W, %%mm7		\n\t"
		"1:				\n\t"
		"movq (%1, %%esi), %%mm0	\n\t"
		"movq 8(%1, %%esi), %%mm1	\n\t"
		"movq 1024(%1, %%esi), %%mm2	\n\t"
		"movq 1032(%1, %%esi), %%mm3	\n\t"
		"psubd %%mm7, %%mm0		\n\t"
		"psubd %%mm7, %%mm1		\n\t"
		"psubd %%mm7, %%mm2		\n\t"
		"psubd %%mm7, %%mm3		\n\t"
		"packssdw %%mm1, %%mm0		\n\t"
		"packssdw %%mm3, %%mm2		\n\t"
		"movq 2048(%1, %%esi), %%mm3	\n\t"
		"movq 2056(%1, %%esi), %%mm4	\n\t"
		"movq 3072(%1, %%esi), %%mm5	\n\t"
		"movq 3080(%1, %%esi), %%mm6	\n\t"
		"psubd %%mm7, %%mm3		\n\t"
		"psubd %%mm7, %%mm4		\n\t"
		"psubd %%mm7, %%mm5		\n\t"
		"psubd %%mm7, %%mm6		\n\t"
		"packssdw %%mm4, %%mm3		\n\t"
		"packssdw %%mm6, %%mm5		\n\t"
		"movq %%mm0, %%mm1		\n\t"
		"movq %%mm3, %%mm4		\n\t"
		"punpcklwd %%mm2, %%mm0		\n\t"
		"punpckhwd %%mm2, %%mm1		\n\t"
		"punpcklwd %%mm5, %%mm3		\n\t"
		"punpckhwd %%mm5, %%mm4		\n\t"
		"movq %%mm0, %%mm2		\n\t"
		"movq %%mm1, %%mm5		\n\t"
		"punpckldq %%mm3, %%mm0		\n\t"
		"punpckhdq %%mm3, %%mm2		\n\t"
		"punpckldq %%mm4, %%mm1		\n\t"
		"punpckhdq %%mm4, %%mm5		\n\t"
		"movq %%mm0, (%0, %%esi,2)	\n\t"
		"movq %%mm2, 8(%0, %%esi,2)	\n\t"
		"movq %%mm1, 16(%0, %%esi,2)	\n\t"
		"movq %%mm5, 24(%0, %%esi,2)	\n\t"
		"addl $16, %%esi		\n\t"
		" jnz 1b			\n\t"
		"emms				\n\t"
		:: "r" (s16+1024), "r" (f+256)
		:"%esi", "memory"
	);
	break;
    case A52_3F2R: 
	asm volatile(
		"movl $-1024, %%esi		\n\t"
		"movq magicF2W, %%mm7		\n\t"
		"1:				\n\t"
		"movd (%1, %%esi), %%mm0	\n\t"
		"punpckldq 2048(%1, %%esi), %%mm0\n\t"
		"movd 3072(%1, %%esi), %%mm1	\n\t"
		"punpckldq 4096(%1, %%esi), %%mm1\n\t"
		"movd 1024(%1, %%esi), %%mm2	\n\t"
		"punpckldq 4(%1, %%esi), %%mm2	\n\t"
		"movd 2052(%1, %%esi), %%mm3	\n\t"
		"punpckldq 3076(%1, %%esi), %%mm3\n\t"
		"movd 4100(%1, %%esi), %%mm4	\n\t"
		"punpckldq 1028(%1, %%esi), %%mm4\n\t"
		"movd 8(%1, %%esi), %%mm5	\n\t"
		"punpckldq 2056(%1, %%esi), %%mm5\n\t"
		"leal (%%esi, %%esi, 4), %%edi	\n\t"
		"sarl $1, %%edi			\n\t"
		"psubd %%mm7, %%mm0		\n\t"
		"psubd %%mm7, %%mm1		\n\t"
		"psubd %%mm7, %%mm2		\n\t"
		"psubd %%mm7, %%mm3		\n\t"
		"psubd %%mm7, %%mm4		\n\t"
		"psubd %%mm7, %%mm5		\n\t"
		"packssdw %%mm1, %%mm0		\n\t"
		"packssdw %%mm3, %%mm2		\n\t"
		"packssdw %%mm5, %%mm4		\n\t"
		"movq %%mm0, (%0, %%edi)	\n\t"
		"movq %%mm2, 8(%0, %%edi)	\n\t"
		"movq %%mm4, 16(%0, %%edi)	\n\t"
		
		"movd 3080(%1, %%esi), %%mm0	\n\t"
		"punpckldq 4104(%1, %%esi), %%mm0\n\t"
		"movd 1032(%1, %%esi), %%mm1	\n\t"
		"punpckldq 12(%1, %%esi), %%mm1\n\t"
		"movd 2060(%1, %%esi), %%mm2	\n\t"
		"punpckldq 3084(%1, %%esi), %%mm2\n\t"
		"movd 4108(%1, %%esi), %%mm3	\n\t"
		"punpckldq 1036(%1, %%esi), %%mm3\n\t"
		"psubd %%mm7, %%mm0		\n\t"
		"psubd %%mm7, %%mm1		\n\t"
		"psubd %%mm7, %%mm2		\n\t"
		"psubd %%mm7, %%mm3		\n\t"
		"packssdw %%mm1, %%mm0		\n\t"
		"packssdw %%mm3, %%mm2		\n\t"
		"movq %%mm0, 24(%0, %%edi)	\n\t"
		"movq %%mm2, 32(%0, %%edi)	\n\t"
				
		"addl $16, %%esi		\n\t"
		" jnz 1b			\n\t"
		"emms				\n\t"
		:: "r" (s16+1280), "r" (f+256)
		:"%esi", "%edi", "memory"
	);
	break;
    case A52_MONO | A52_LFE:
	asm volatile(
		"movl $-1024, %%esi		\n\t"
		"movq magicF2W, %%mm7		\n\t"
		"pxor %%mm6, %%mm6		\n\t"
		"1:				\n\t"
		"movq 1024(%1, %%esi), %%mm0	\n\t"
		"movq 1032(%1, %%esi), %%mm1	\n\t"
		"movq (%1, %%esi), %%mm2	\n\t"
		"movq 8(%1, %%esi), %%mm3	\n\t"
		"psubd %%mm7, %%mm0		\n\t"
		"psubd %%mm7, %%mm1		\n\t"
		"psubd %%mm7, %%mm2		\n\t"
		"psubd %%mm7, %%mm3		\n\t"
		"packssdw %%mm1, %%mm0		\n\t"
		"packssdw %%mm3, %%mm2		\n\t"
		"movq %%mm0, %%mm1		\n\t"
		"punpcklwd %%mm2, %%mm0		\n\t"
		"punpckhwd %%mm2, %%mm1		\n\t"
		"leal (%%esi, %%esi, 2), %%edi	\n\t"
		"movq %%mm6, (%0, %%edi)	\n\t"
		"movd %%mm0, 8(%0, %%edi)	\n\t"
		"punpckhdq %%mm0, %%mm0		\n\t"
		"movq %%mm6, 12(%0, %%edi)	\n\t"
		"movd %%mm0, 20(%0, %%edi)	\n\t"
		"movq %%mm6, 24(%0, %%edi)	\n\t"
		"movd %%mm1, 32(%0, %%edi)	\n\t"
		"punpckhdq %%mm1, %%mm1		\n\t"
		"movq %%mm6, 36(%0, %%edi)	\n\t"
		"movd %%mm1, 44(%0, %%edi)	\n\t"
		"addl $16, %%esi		\n\t"
		" jnz 1b			\n\t"
		"emms				\n\t"
		:: "r" (s16+1536), "r" (f+256)
		:"%esi", "%edi", "memory"
	);
	break;
    case A52_CHANNEL | A52_LFE:
    case A52_STEREO | A52_LFE:
    case A52_DOLBY | A52_LFE:
	asm volatile(
		"movl $-1024, %%esi		\n\t"
		"movq magicF2W, %%mm7		\n\t"
		"pxor %%mm6, %%mm6		\n\t"
		"1:				\n\t"
		"movq 1024(%1, %%esi), %%mm0	\n\t"
		"movq 2048(%1, %%esi), %%mm1	\n\t"
		"movq (%1, %%esi), %%mm5	\n\t" 
		"psubd %%mm7, %%mm0		\n\t"
		"psubd %%mm7, %%mm1		\n\t"
		"psubd %%mm7, %%mm5		\n\t"
		"leal (%%esi, %%esi, 2), %%edi	\n\t"
		
		"pxor %%mm4, %%mm4		\n\t"
		"packssdw %%mm5, %%mm0		\n\t" // FfAa
		"packssdw %%mm4, %%mm1		\n\t" // 00Bb
		"punpckhwd %%mm0, %%mm4		\n\t" // F0f0
		"punpcklwd %%mm1, %%mm0		\n\t" // BAba
		"movq %%mm0, %%mm1		\n\t" // BAba
		"punpckldq %%mm4, %%mm3		\n\t" // f0XX
		"punpckldq %%mm6, %%mm0		\n\t" // 00ba
		"punpckhdq %%mm1, %%mm3		\n\t" // BAf0
		
		"movq %%mm0, (%0, %%edi)	\n\t" // 00ba
		"punpckhdq %%mm4, %%mm0		\n\t" // F000
		"movq %%mm3, 8(%0, %%edi)	\n\t" // BAf0
		"movq %%mm0, 16(%0, %%edi)	\n\t" // F000
		"addl $8, %%esi			\n\t"
		" jnz 1b			\n\t"
		"emms				\n\t"
		:: "r" (s16+1536), "r" (f+256)
		:"%esi", "%edi", "memory"
	);
	break;
    case A52_3F | A52_LFE:
	asm volatile(
		"movl $-1024, %%esi		\n\t"
		"movq magicF2W, %%mm7		\n\t"
		"pxor %%mm6, %%mm6		\n\t"
		"1:				\n\t"
		"movq 1024(%1, %%esi), %%mm0	\n\t"
		"movq 3072(%1, %%esi), %%mm1	\n\t"
		"movq 2048(%1, %%esi), %%mm4	\n\t"
		"movq (%1, %%esi), %%mm5	\n\t" 
		"psubd %%mm7, %%mm0		\n\t"
		"psubd %%mm7, %%mm1		\n\t"
		"psubd %%mm7, %%mm4		\n\t"
		"psubd %%mm7, %%mm5		\n\t"
		"leal (%%esi, %%esi, 2), %%edi	\n\t"
		
		"packssdw %%mm4, %%mm0		\n\t" // EeAa
		"packssdw %%mm5, %%mm1		\n\t" // FfBb
		"movq %%mm0, %%mm2		\n\t" // EeAa
		"punpcklwd %%mm1, %%mm0		\n\t" // BAba
		"punpckhwd %%mm1, %%mm2		\n\t" // FEfe
		"movq %%mm0, %%mm1		\n\t" // BAba
		"punpckldq %%mm6, %%mm0		\n\t" // 00ba
		"punpckhdq %%mm1, %%mm1		\n\t" // BABA
		
		"movq %%mm0, (%0, %%edi)	\n\t"
		"punpckhdq %%mm2, %%mm0		\n\t" // FE00
		"punpckldq %%mm1, %%mm2		\n\t" // BAfe
		"movq %%mm2, 8(%0, %%edi)	\n\t"
		"movq %%mm0, 16(%0, %%edi)	\n\t"
		"addl $8, %%esi			\n\t"
		" jnz 1b			\n\t"
		"emms				\n\t"
		:: "r" (s16+1536), "r" (f+256)
		:"%esi", "%edi", "memory"
	);
	break;
    case A52_2F2R | A52_LFE:
	asm volatile(
		"movl $-1024, %%esi		\n\t"
		"movq magicF2W, %%mm7		\n\t"
//		"pxor %%mm6, %%mm6		\n\t"
		"1:				\n\t"
		"movq 1024(%1, %%esi), %%mm0	\n\t"
		"movq 2048(%1, %%esi), %%mm1	\n\t"
		"movq 3072(%1, %%esi), %%mm2	\n\t"
		"movq 4096(%1, %%esi), %%mm3	\n\t"
		"movq (%1, %%esi), %%mm5	\n\t" 
		"psubd %%mm7, %%mm0		\n\t"
		"psubd %%mm7, %%mm1		\n\t"
		"psubd %%mm7, %%mm2		\n\t"
		"psubd %%mm7, %%mm3		\n\t"
		"psubd %%mm7, %%mm5		\n\t"
		"leal (%%esi, %%esi, 2), %%edi	\n\t"
		
		"packssdw %%mm2, %%mm0		\n\t" // CcAa
		"packssdw %%mm3, %%mm1		\n\t" // DdBb
		"packssdw %%mm5, %%mm5		\n\t" // FfFf
		"movq %%mm0, %%mm2		\n\t" // CcAa
		"punpcklwd %%mm1, %%mm0		\n\t" // BAba
		"punpckhwd %%mm1, %%mm2		\n\t" // DCdc
		"pxor %%mm4, %%mm4		\n\t" // 0000
		"punpcklwd %%mm5, %%mm4		\n\t" // F0f0
		"movq %%mm0, %%mm1		\n\t" // BAba
		"movq %%mm4, %%mm3		\n\t" // F0f0
		"punpckldq %%mm2, %%mm0		\n\t" // dcba
		"punpckhdq %%mm1, %%mm1		\n\t" // BABA
		"punpckldq %%mm1, %%mm4		\n\t" // BAf0
		"punpckhdq %%mm3, %%mm2		\n\t" // F0DC
		
		"movq %%mm0, (%0, %%edi)	\n\t"
		"movq %%mm4, 8(%0, %%edi)	\n\t"
		"movq %%mm2, 16(%0, %%edi)	\n\t"
		"addl $8, %%esi			\n\t"
		" jnz 1b			\n\t"
		"emms				\n\t"
		:: "r" (s16+1536), "r" (f+256)
		:"%esi", "%edi", "memory"
	);
	break;
    case A52_3F2R | A52_LFE:
	asm volatile(
		"movl $-1024, %%esi		\n\t"
		"movq magicF2W, %%mm7		\n\t"
//		"pxor %%mm6, %%mm6		\n\t"
		"1:				\n\t"
		"movq 1024(%1, %%esi), %%mm0	\n\t"
		"movq 3072(%1, %%esi), %%mm1	\n\t"
		"movq 4096(%1, %%esi), %%mm2	\n\t"
		"movq 5120(%1, %%esi), %%mm3	\n\t"
		"movq 2048(%1, %%esi), %%mm4	\n\t"
		"movq (%1, %%esi), %%mm5	\n\t" 
		"psubd %%mm7, %%mm0		\n\t"
		"psubd %%mm7, %%mm1		\n\t"
		"psubd %%mm7, %%mm2		\n\t"
		"psubd %%mm7, %%mm3		\n\t"
		"psubd %%mm7, %%mm4		\n\t"
		"psubd %%mm7, %%mm5		\n\t"
		"leal (%%esi, %%esi, 2), %%edi	\n\t"
		
		"packssdw %%mm2, %%mm0		\n\t" // CcAa
		"packssdw %%mm3, %%mm1		\n\t" // DdBb
		"packssdw %%mm4, %%mm4		\n\t" // EeEe
		"packssdw %%mm5, %%mm5		\n\t" // FfFf
		"movq %%mm0, %%mm2		\n\t" // CcAa
		"punpcklwd %%mm1, %%mm0		\n\t" // BAba
		"punpckhwd %%mm1, %%mm2		\n\t" // DCdc
		"punpcklwd %%mm5, %%mm4		\n\t" // FEfe
		"movq %%mm0, %%mm1		\n\t" // BAba
		"movq %%mm4, %%mm3		\n\t" // FEfe
		"punpckldq %%mm2, %%mm0		\n\t" // dcba
		"punpckhdq %%mm1, %%mm1		\n\t" // BABA
		"punpckldq %%mm1, %%mm4		\n\t" // BAfe
		"punpckhdq %%mm3, %%mm2		\n\t" // FEDC
		
		"movq %%mm0, (%0, %%edi)	\n\t"
		"movq %%mm4, 8(%0, %%edi)	\n\t"
		"movq %%mm2, 16(%0, %%edi)	\n\t"
		"addl $8, %%esi			\n\t"
		" jnz 1b			\n\t"
		"emms				\n\t"
		:: "r" (s16+1536), "r" (f+256)
		:"%esi", "%edi", "memory"
	);
	break;
    }
    return chans*256;
}
#endif //arch_x86

void a52_resample_init(uint32_t mm_accel,int _flags,int _chans){
    chans=_chans;
    flags=_flags;

    if(a52_resample==NULL) // only once please ;)
    {
	    if(mm_accel & MM_ACCEL_X86_MMX) 	fprintf(stderr, "Using MMX optimized resampler\n");
	    else				fprintf(stderr, "No accelerated resampler found\n");
    }
    
#ifdef ARCH_X86
    if(mm_accel & MM_ACCEL_X86_MMX) a52_resample= a52_resample_MMX;
#else
    if(0);
#endif
    else		a52_resample= a52_resample_C;
}

