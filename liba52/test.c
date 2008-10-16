/*
 * liba52 sample by A'rpi/ESP-team
 * Reads an AC-3 stream from stdin, decodes and downmixes to s16 stereo PCM
 * and writes it to stdout. The resulting stream is playable with sox:
 *   play -c2 -r48000 -sw -fs out.sw
 *
 * Copyright (C) 2001 Árpád Gereöffy
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

//#define TIMING //needs Pentium or newer 

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>

#include "a52.h"
#include "mm_accel.h"
#include "cpudetect.h"

static a52_state_t *state;
static uint8_t buf[3840];
static int buf_size=0;

static int16_t out_buf[6*256*6];

void mp_msg( int x, const char *format, ... ) // stub for cpudetect.c
{
}

#ifdef TIMING
static inline long long rdtsc()
{
	long long l;
	__asm__ volatile("rdtsc\n\t"
		: "=A" (l)
	);
//	printf("%d\n", int(l/1000));
	return l;
}

#define STARTTIMING t=rdtsc();
#define ENDTIMING sum+=rdtsc()-t; t=rdtsc();
#else
#define STARTTIMING ;
#define ENDTIMING ;
#endif


int main(void){
int accel=0;
int sample_rate=0;
int bit_rate=0;
#ifdef TIMING
long long t, sum=0, min=256*256*256*64;
#endif

    FILE *temp= stdout;
    stdout= stderr; //EVIL HACK FIXME
    GetCpuCaps(&gCpuCaps);
    stdout= temp;
//    gCpuCaps.hasMMX=0;
//    gCpuCaps.hasSSE=0;
    if(gCpuCaps.hasMMX) 	accel |= MM_ACCEL_X86_MMX;
    if(gCpuCaps.hasMMX2) 	accel |= MM_ACCEL_X86_MMXEXT;
    if(gCpuCaps.hasSSE) 	accel |= MM_ACCEL_X86_SSE;
    if(gCpuCaps.has3DNow) 	accel |= MM_ACCEL_X86_3DNOW;
//    if(gCpuCaps.has3DNowExt) 	accel |= MM_ACCEL_X86_3DNOWEXT;
    
    state = a52_init (accel);
    if (state == NULL) {
	fprintf (stderr, "A52 init failed\n");
	return 1;
    }

while(1){
    int length,i;
    int16_t *s16;
    sample_t level=1, bias=384;
    int flags=0;
    int channels=0;

    while(buf_size<7){
	int c=getchar();
	if(c<0) goto eof;
	buf[buf_size++]=c;
    }
STARTTIMING
    length = a52_syncinfo (buf, &flags, &sample_rate, &bit_rate);
ENDTIMING
    if(!length){
	// bad file => resync
	memcpy(buf,buf+1,6);
	--buf_size;
	continue;
    }
    fprintf(stderr,"sync. %d bytes  0x%X  %d Hz  %d kbit\n",length,flags,sample_rate,bit_rate);
    while(buf_size<length){
	buf[buf_size++]=getchar();
    }
    
    buf_size=0;

    // decode:
    flags=A52_STEREO; //A52_STEREO; //A52_DOLBY; //A52_STEREO; // A52_DOLBY // A52_2F2R // A52_3F2R | A52_LFE
    channels=2;
    
    flags |= A52_ADJUST_LEVEL;
STARTTIMING
    if (a52_frame (state, buf, &flags, &level, bias))
	{ fprintf(stderr,"error at decoding\n"); continue; }
ENDTIMING

    // a52_dynrng (state, NULL, NULL); // disable dynamic range compensation

STARTTIMING
    a52_resample_init(accel,flags,channels);
    s16 = out_buf;
    for (i = 0; i < 6; i++) {
	if (a52_block (state))
	    { fprintf(stderr,"error at sampling\n"); break; }
	// float->int + channels interleaving:
	s16+=a52_resample(a52_samples(state),s16);
ENDTIMING
    }
#ifdef TIMING
if(sum<min) min=sum;
sum=0;
#endif
    fwrite(out_buf,6*256*2*channels,1,stdout);

}

eof:
#ifdef TIMING
fprintf(stderr, "%4.4fk cycles\n",min/1000.0);
sum=0;
#endif
return 0;
}
