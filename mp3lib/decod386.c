/*
 * Modified for use with MPlayer, for details see the changelog at
 * http://svn.mplayerhq.hu/mplayer/trunk/
 * $Id$
 */

/*
 * Mpeg Layer-1,2,3 audio decoder
 * ------------------------------
 * copyright (c) 1995,1996,1997 by Michael Hipp, All rights reserved.
 * See also 'README'
 *
 * slighlty optimized for machines without autoincrement/decrement.
 * The performance is highly compiler dependend. Maybe
 * the decode.c version for 'normal' processor may be faster
 * even for Intel processors.
 */


#include "config.h"

#if 0
 /* old WRITE_SAMPLE */
   /* is portable */
#define WRITE_SAMPLE(samples,sum,clip) {			\
  if( (sum) > 32767.0) { *(samples) = 0x7fff; (clip)++; }	\
  else if( (sum) < -32768.0) { *(samples) = -0x8000; (clip)++; }\
  else { *(samples) = sum;  }					\
}
#else
 /* new WRITE_SAMPLE */

/*
 * should be the same as the "old WRITE_SAMPLE" macro above, but uses
 * some tricks to avoid double->int conversions and floating point compares.
 *
 * Here's how it works:
 * ((((65536.0 * 65536.0 * 16)+(65536.0 * 0.5))* 65536.0)) is
 * 0x0010000080000000LL in hex.  It computes 0x0010000080000000LL + sum
 * as a double IEEE fp value and extracts the low-order 32-bits from the
 * IEEE fp representation stored in memory.  The 2^56 bit in the constant
 * is intended to force the bits of "sum" into the least significant bits
 * of the double mantissa.  After an integer substraction of 0x80000000
 * we have the original double value "sum" converted to an 32-bit int value.
 *
 * (Is that really faster than the clean and simple old version of the macro?)
 */

/*
 * On a SPARC cpu, we fetch the low-order 32-bit from the second 32-bit
 * word of the double fp value stored in memory.  On an x86 cpu, we fetch it
 * from the first 32-bit word.
 * I'm not sure if the WORDS_BIGENDIAN feature test covers all possible memory
 * layouts of double floating point values an all cpu architectures.  If
 * it doesn't work for you, just enable the "old WRITE_SAMPLE" macro.
 */
#ifdef WORDS_BIGENDIAN
#define	MANTISSA_OFFSET	1
#else
#define	MANTISSA_OFFSET	0
#endif

   /* sizeof(int) == 4 */
#define WRITE_SAMPLE(samples,sum,clip) { \
  union { double dtemp; int itemp[2]; } u; int v; \
  u.dtemp = ((((65536.0 * 65536.0 * 16)+(65536.0 * 0.5))* 65536.0)) + (sum);\
  v = u.itemp[MANTISSA_OFFSET] - 0x80000000; \
  if( v > 32767) { *(samples) = 0x7fff; (clip)++; } \
  else if( v < -32768) { *(samples) = -0x8000; (clip)++; } \
  else { *(samples) = v; } \
}
#endif


/*
#define WRITE_SAMPLE(samples,sum,clip) { \
  double dtemp; int v;                    \
  dtemp = ((((65536.0 * 65536.0 * 16)+(65536.0 * 0.5))* 65536.0)) + (sum);\
  v = ((*(int *)&dtemp) - 0x80000000); \
  if( v > 32767) { *(samples) = 0x7fff; (clip)++; } \
  else if( v < -32768) { *(samples) = -0x8000; (clip)++; } \
  else { *(samples) = v; } \
}
*/

static int synth_1to1(real *bandPtr,int channel,unsigned char *out,int *pnt);

static int synth_1to1_mono2stereo(real *bandPtr,unsigned char *samples,int *pnt)
{
  int i,ret;

  ret = synth_1to1(bandPtr,0,samples,pnt);
  samples = samples + *pnt - 128;

  for(i=0;i<32;i++) {
    ((short *)samples)[1] = ((short *)samples)[0];
    samples+=4;
  }

  return ret;
}

static synth_func_t synth_func;

#if HAVE_ALTIVEC
#define dct64_base(a,b,c) if(gCpuCaps.hasAltiVec) dct64_altivec(a,b,c); else dct64(a,b,c)
#else /* HAVE_ALTIVEC */
#define dct64_base(a,b,c) dct64(a,b,c)
#endif /* HAVE_ALTIVEC */
 
static int synth_1to1(real *bandPtr,int channel,unsigned char *out,int *pnt)
{
  static real buffs[2][2][0x110];
  static const int step = 2;
  static int bo = 1;
  short *samples = (short *) (out + *pnt);
  real *b0,(*buf)[0x110];
  int clip = 0;
  int bo1;

  *pnt += 128;

/* optimized for x86 */
#if ARCH_X86
  if ( synth_func )
   {
//    printf("Calling %p, bandPtr=%p channel=%d samples=%p\n",synth_func,bandPtr,channel,samples);
    // FIXME: synth_func() may destroy EBP, don't rely on stack contents!!!
    return (*synth_func)( bandPtr,channel,samples);
   }
#endif
  if(!channel) {     /* channel=0 */
    bo--;
    bo &= 0xf;
    buf = buffs[0];
  }
  else {
    samples++;
    buf = buffs[1];
  }

  if(bo & 0x1) {
    b0 = buf[0];
    bo1 = bo;
    dct64_base(buf[1]+((bo+1)&0xf),buf[0]+bo,bandPtr);
  }
  else {
    b0 = buf[1];
    bo1 = bo+1;
    dct64_base(buf[0]+bo,buf[1]+bo+1,bandPtr);
  }

  {
    register int j;
    real *window = mp3lib_decwin + 16 - bo1;

    for (j=16;j;j--,b0+=0x10,window+=0x20,samples+=step)
    {
      real sum;
      sum  = window[0x0] * b0[0x0];
      sum -= window[0x1] * b0[0x1];
      sum += window[0x2] * b0[0x2];
      sum -= window[0x3] * b0[0x3];
      sum += window[0x4] * b0[0x4];
      sum -= window[0x5] * b0[0x5];
      sum += window[0x6] * b0[0x6];
      sum -= window[0x7] * b0[0x7];
      sum += window[0x8] * b0[0x8];
      sum -= window[0x9] * b0[0x9];
      sum += window[0xA] * b0[0xA];
      sum -= window[0xB] * b0[0xB];
      sum += window[0xC] * b0[0xC];
      sum -= window[0xD] * b0[0xD];
      sum += window[0xE] * b0[0xE];
      sum -= window[0xF] * b0[0xF];

      WRITE_SAMPLE(samples,sum,clip);
    }

    {
      real sum;
      sum  = window[0x0] * b0[0x0];
      sum += window[0x2] * b0[0x2];
      sum += window[0x4] * b0[0x4];
      sum += window[0x6] * b0[0x6];
      sum += window[0x8] * b0[0x8];
      sum += window[0xA] * b0[0xA];
      sum += window[0xC] * b0[0xC];
      sum += window[0xE] * b0[0xE];
      WRITE_SAMPLE(samples,sum,clip);
      b0-=0x10,window-=0x20,samples+=step;
    }
    window += bo1<<1;

    for (j=15;j;j--,b0-=0x10,window-=0x20,samples+=step)
    {
      real sum;
      sum = -window[-0x1] * b0[0x0];
      sum -= window[-0x2] * b0[0x1];
      sum -= window[-0x3] * b0[0x2];
      sum -= window[-0x4] * b0[0x3];
      sum -= window[-0x5] * b0[0x4];
      sum -= window[-0x6] * b0[0x5];
      sum -= window[-0x7] * b0[0x6];
      sum -= window[-0x8] * b0[0x7];
      sum -= window[-0x9] * b0[0x8];
      sum -= window[-0xA] * b0[0x9];
      sum -= window[-0xB] * b0[0xA];
      sum -= window[-0xC] * b0[0xB];
      sum -= window[-0xD] * b0[0xC];
      sum -= window[-0xE] * b0[0xD];
      sum -= window[-0xF] * b0[0xE];
      sum -= window[-0x0] * b0[0xF];

      WRITE_SAMPLE(samples,sum,clip);
    }
  }

  return clip;

}

#ifdef CONFIG_FAKE_MONO
static int synth_1to1_l(real *bandPtr,int channel,unsigned char *out,int *pnt)
{
  int i,ret;

  ret = synth_1to1(bandPtr,channel,out,pnt);
  out = out + *pnt - 128;

  for(i=0;i<32;i++) {
    ((short *)out)[1] = ((short *)out)[0];
    out+=4;
  }

  return ret;
}

static int synth_1to1_r(real *bandPtr,int channel,unsigned char *out,int *pnt)
{
  int i,ret;

  ret = synth_1to1(bandPtr,channel,out,pnt);
  out = out + *pnt - 128;

  for(i=0;i<32;i++) {
    ((short *)out)[0] = ((short *)out)[1];
    out+=4;
  }

  return ret;
}
#endif
