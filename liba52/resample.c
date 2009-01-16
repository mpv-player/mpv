/*
 * resample.c
 * Copyright (C) 2001 Árpád Gereöffy
 *
 * This file is part of a52dec, a free ATSC A-52 stream decoder.
 * See http://liba52.sourceforge.net/ for updates.
 *
 * File added for use with MPlayer and not part of original a52dec.
 *
 * a52dec is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * a52dec is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

// a52_resample_init should find the requested converter (from type flags ->
// given number of channels) and set up some function pointers...

// a52_resample() should do the conversion.

#include <inttypes.h>
#include <stdio.h>
#include "a52.h"
#include "mm_accel.h"
#include "config.h"
#include "mangle.h"

int (* a52_resample) (float * _f, int16_t * s16)=NULL;

#include "resample_c.c"

#if ARCH_X86 || ARCH_X86_64
#include "resample_mmx.c"
#endif

#if HAVE_ALTIVEC
#include "resample_altivec.c"
#endif

void* a52_resample_init(uint32_t mm_accel,int flags,int chans){
void* tmp;

#if ARCH_X86 || ARCH_X86_64
    if(mm_accel&MM_ACCEL_X86_MMX){
	tmp=a52_resample_MMX(flags,chans);
	if(tmp){
	    if(a52_resample==NULL) fprintf(stderr, "Using MMX optimized resampler\n");
	    a52_resample=tmp;
	    return tmp;
	}
    }
#endif
#if HAVE_ALTIVEC
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
