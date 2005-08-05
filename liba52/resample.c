
// a52_resample_init should find the requested converter (from type flags ->
// given number of channels) and set up some function pointers...

// a52_resample() should do the conversion.

#include <inttypes.h>
#include <stdio.h>
#include "a52.h"
#include "mm_accel.h"
#include "../config.h"
#include "mangle.h"

int (* a52_resample) (float * _f, int16_t * s16)=NULL;

#include "resample_c.c"

#if defined(ARCH_X86) || defined(ARCH_X86_64)
#include "resample_mmx.c"
#endif

#ifdef HAVE_ALTIVEC
#include "resample_altivec.c"
#endif

void* a52_resample_init(uint32_t mm_accel,int flags,int chans){
void* tmp;

#if defined(ARCH_X86) || defined(ARCH_X86_64)
    if(mm_accel&MM_ACCEL_X86_MMX){
	tmp=a52_resample_MMX(flags,chans);
	if(tmp){
	    if(a52_resample==NULL) fprintf(stderr, "Using MMX optimized resampler\n");
	    a52_resample=tmp;
	    return tmp;
	}
    }
#endif
#ifdef HAVE_ALTIVEC
    if(mm_accel&MM_ACCEL_PPC_ALTIVEC){
      tmp=a52_resample_altivec(flags,chans);
      if(tmp){
       if(a52_resample==NULL) fprintf(stderr, "Using AltiVec optimized resampler\n");
       a52_resample=tmp;
       return tmp;
      }
    }
#endif
    
    tmp=a52_resample_C(flags,chans);
    if(tmp){
	if(a52_resample==NULL) fprintf(stderr, "No accelerated resampler found\n");
	a52_resample=tmp;
	return tmp;
    }
    
    fprintf(stderr, "Unimplemented resampler for mode 0x%X -> %d channels conversion - Contact MPlayer developers!\n", flags, chans);
    return NULL;
}
