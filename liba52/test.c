
// liba52 sample by A'rpi/ESP-team
// reads ac3 stream form stdin, decodes and downmix to s16 stereo pcm and
// writes it to stdout.  resulting stream playbackable with sox:
//   play -c 2 -r 48000 out.sw

//#define TIMING //needs Pentium or newer 

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include "a52.h"

static sample_t * samples;
static a52_state_t state;
static uint8_t buf[3840];
static int buf_size=0;

static int16_t out_buf[6*256*6];

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


static inline int16_t convert (int32_t i)
{
    if (i > 0x43c07fff)
	return 32767;
    else if (i < 0x43bf8000)
	return -32768;
    else
	return i - 0x43c00000;
}

static inline void float_to_int (float * _f, int16_t * s16, int flags)
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
}


int main(){
int accel=0;
int sample_rate=0;
int bit_rate=0;
#ifdef TIMING
long long t, sum=0;
#endif

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
    flags=A52_STEREO; // A52_DOLBY // A52_2F2R // A52_3F2R | A52_LFE
    flags |= A52_ADJUST_LEVEL;
STARTTIMING
    if (a52_frame (&state, buf, &flags, &level, bias))
	{ fprintf(stderr,"error at decoding\n"); continue; }
ENDTIMING

    // a52_dynrng (&state, NULL, NULL); // disable dynamic range compensation

    s16 = out_buf;
    for (i = 0; i < 6; i++) {
	int32_t * f = (int32_t *) samples;
	int i;
STARTTIMING
	if (a52_block (&state, samples))
	    { fprintf(stderr,"error at sampling\n"); break; }
ENDTIMING
	// resample to STEREO/DOLBY:
	for (i = 0; i < 256; i++) {
	    s16[2*i] = convert (f[i]);
	    s16[2*i+1] = convert (f[i+256]);
	}
	s16+=2*i;
    }
    fwrite(out_buf,6*256*2*2,1,stdout);

}

eof:

#ifdef TIMING
fprintf(stderr, "%4.4fm cycles\n",sum/1000000.0);
#endif

}
