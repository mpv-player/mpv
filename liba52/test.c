
// liba52 sample by A'rpi/ESP-team
// reads ac3 stream form stdin, decodes and downmix to s16 stereo pcm and
// writes it to stdout.  resulting stream playbackable with sox:
//   play -c 2 -r 48000 out.sw

//#define TIMING //needs Pentium or newer 

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>

#include "a52.h"
#include "mm_accel.h"
#include "../cpudetect.h"

static sample_t * samples;
static a52_state_t state;
static uint8_t buf[3840];
static int buf_size=0;

static int16_t out_buf[6*256*6];

void mp_msg_c( int x, const char *format, ... ) // stub for cpudetect.c
{
}

#ifdef TIMING
static inline long long rdtsc()
{
	long long l;
	asm volatile(	"rdtsc\n\t"
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


int main(){
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
    
    samples = a52_init (accel);
    if (samples == NULL) {
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
    if (a52_frame (&state, buf, &flags, &level, bias))
	{ fprintf(stderr,"error at decoding\n"); continue; }
ENDTIMING

    // a52_dynrng (&state, NULL, NULL); // disable dynamic range compensation

STARTTIMING
    a52_resample_init(accel,flags,channels);
    s16 = out_buf;
    for (i = 0; i < 6; i++) {
	if (a52_block (&state, samples))
	    { fprintf(stderr,"error at sampling\n"); break; }
	// float->int + channels interleaving:
	s16+=a52_resample(samples,s16);
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
fprintf(stderr, "%4.4fk cycles ",min/1000.0);
sum=0;
#endif
}
