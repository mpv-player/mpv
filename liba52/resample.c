// this code come from a52dec/libao/audio_out_oss.c

// FIXME FIXME FIXME

// a52_resample_init should find the requested converter (from type flags ->
// given number of channels) and set up some function pointers...

// a52_resample() should do the conversion.

#include <inttypes.h>
#include "a52.h"
#include "../config.h"

#ifdef HAVE_MMX
static uint64_t __attribute__((aligned(16))) magicF2W= 0x43c0000043c00000LL;
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

void a52_resample_init(int _flags,int _chans){
    chans=_chans;
    flags=_flags;
}

int a52_resample(float * _f, int16_t * s16)
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
#ifdef HAVE_MMX
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
#else
	for (i = 0; i < 256; i++) {
	    s16[2*i] = convert (f[i]);
	    s16[2*i+1] = convert (f[i+256]);
	}
#endif
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

