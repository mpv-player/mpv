/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggVorbis 'TREMOR' CODEC SOURCE CODE.   *
 *                                                                  *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE OggVorbis 'TREMOR' SOURCE CODE IS (C) COPYRIGHT 1994-2002    *
 * BY THE Xiph.Org FOUNDATION http://www.xiph.org/                  *
 *                                                                  *
 ********************************************************************

 function: miscellaneous math and prototypes

 ********************************************************************/

#ifndef _V_RANDOM_H_
#define _V_RANDOM_H_
#include "ivorbiscodec.h"
#include "codec_internal.h"
#include "os_types.h"

#include "asm_arm.h"

#ifndef _V_WIDE_MATH
#define _V_WIDE_MATH

#ifndef  _LOW_ACCURACY_
/* 64 bit multiply */

#include <sys/types.h>
#include <stdlib.h>
#include "config.h"

#if !HAVE_BIGENDIAN
union magic {
  struct {
    ogg_int32_t lo;
    ogg_int32_t hi;
  } halves;
  ogg_int64_t whole;
};
#else
union magic {
  struct {
    ogg_int32_t hi;
    ogg_int32_t lo;
  } halves;
  ogg_int64_t whole;
};
#endif

static inline ogg_int32_t MULT32(ogg_int32_t x, ogg_int32_t y) {
  union magic magic;
  magic.whole = (ogg_int64_t)x * y;
  return magic.halves.hi;
}

static inline ogg_int32_t MULT31(ogg_int32_t x, ogg_int32_t y) {
  return MULT32(x,y)<<1;
}

static inline ogg_int32_t MULT31_SHIFT15(ogg_int32_t x, ogg_int32_t y) {
  union magic magic;
  magic.whole  = (ogg_int64_t)x * y;
  return ((ogg_uint32_t)(magic.halves.lo)>>15) | ((magic.halves.hi)<<17);
}

#else
/* 32 bit multiply, more portable but less accurate */

/*
 * Note: Precision is biased towards the first argument therefore ordering
 * is important.  Shift values were chosen for the best sound quality after
 * many listening tests.
 */

/*
 * For MULT32 and MULT31: The second argument is always a lookup table
 * value already preshifted from 31 to 8 bits.  We therefore take the
 * opportunity to save on text space and use unsigned char for those
 * tables in this case.
 */

static inline ogg_int32_t MULT32(ogg_int32_t x, ogg_int32_t y) {
  return (x >> 9) * y;  /* y preshifted >>23 */
}

static inline ogg_int32_t MULT31(ogg_int32_t x, ogg_int32_t y) {
  return (x >> 8) * y;  /* y preshifted >>23 */
}

static inline ogg_int32_t MULT31_SHIFT15(ogg_int32_t x, ogg_int32_t y) {
  return (x >> 6) * y;  /* y preshifted >>9 */
}

#endif

/*
 * This should be used as a memory barrier, forcing all cached values in
 * registers to wr writen back to memory.  Might or might not be beneficial
 * depending on the architecture and compiler.
 */
#define MB()

/*
 * The XPROD functions are meant to optimize the cross products found all
 * over the place in mdct.c by forcing memory operation ordering to avoid
 * unnecessary register reloads as soon as memory is being written to.
 * However this is only beneficial on CPUs with a sane number of general
 * purpose registers which exclude the Intel x86.  On Intel, better let the
 * compiler actually reload registers directly from original memory by using
 * macros.
 */

#ifdef __i386__

#define XPROD32(_a, _b, _t, _v, _x, _y)		\
  { *(_x)=MULT32(_a,_t)+MULT32(_b,_v);		\
    *(_y)=MULT32(_b,_t)-MULT32(_a,_v); }
#define XPROD31(_a, _b, _t, _v, _x, _y)		\
  { *(_x)=MULT31(_a,_t)+MULT31(_b,_v);		\
    *(_y)=MULT31(_b,_t)-MULT31(_a,_v); }
#define XNPROD31(_a, _b, _t, _v, _x, _y)	\
  { *(_x)=MULT31(_a,_t)-MULT31(_b,_v);		\
    *(_y)=MULT31(_b,_t)+MULT31(_a,_v); }

#else

static inline void XPROD32(ogg_int32_t  a, ogg_int32_t  b,
			   ogg_int32_t  t, ogg_int32_t  v,
			   ogg_int32_t *x, ogg_int32_t *y)
{
  *x = MULT32(a, t) + MULT32(b, v);
  *y = MULT32(b, t) - MULT32(a, v);
}

static inline void XPROD31(ogg_int32_t  a, ogg_int32_t  b,
			   ogg_int32_t  t, ogg_int32_t  v,
			   ogg_int32_t *x, ogg_int32_t *y)
{
  *x = MULT31(a, t) + MULT31(b, v);
  *y = MULT31(b, t) - MULT31(a, v);
}

static inline void XNPROD31(ogg_int32_t  a, ogg_int32_t  b,
			    ogg_int32_t  t, ogg_int32_t  v,
			    ogg_int32_t *x, ogg_int32_t *y)
{
  *x = MULT31(a, t) - MULT31(b, v);
  *y = MULT31(b, t) + MULT31(a, v);
}

#endif

#endif

#ifndef _V_CLIP_MATH
#define _V_CLIP_MATH

static inline ogg_int32_t CLIP_TO_15(ogg_int32_t x) {
  int ret=x;
  ret-= ((x<=32767)-1)&(x-32767);
  ret-= ((x>=-32768)-1)&(x+32768);
  return(ret);
}

#endif

static inline ogg_int32_t VFLOAT_MULT(ogg_int32_t a,ogg_int32_t ap,
				      ogg_int32_t b,ogg_int32_t bp,
				      ogg_int32_t *p){
  if(a && b){
#ifndef _LOW_ACCURACY_
    *p=ap+bp+32;
    return MULT32(a,b);
#else
    *p=ap+bp+31;
    return (a>>15)*(b>>16);
#endif
  }else
    return 0;
}

static inline ogg_int32_t VFLOAT_MULTI(ogg_int32_t a,ogg_int32_t ap,
				      ogg_int32_t i,
				      ogg_int32_t *p){

  int ip=_ilog(abs(i))-31;
  return VFLOAT_MULT(a,ap,i<<-ip,ip,p);
}

static inline ogg_int32_t VFLOAT_ADD(ogg_int32_t a,ogg_int32_t ap,
				      ogg_int32_t b,ogg_int32_t bp,
				      ogg_int32_t *p){

  if(!a){
    *p=bp;
    return b;
  }else if(!b){
    *p=ap;
    return a;
  }

  /* yes, this can leak a bit. */
  if(ap>bp){
    int shift=ap-bp+1;
    *p=ap+1;
    a>>=1;
    if(shift<32){
      b=(b+(1<<(shift-1)))>>shift;
    }else{
      b=0;
    }
  }else{
    int shift=bp-ap+1;
    *p=bp+1;
    b>>=1;
    if(shift<32){
      a=(a+(1<<(shift-1)))>>shift;
    }else{
      a=0;
    }
  }

  a+=b;
  if((a&0xc0000000)==0xc0000000 ||
     (a&0xc0000000)==0){
    a<<=1;
    (*p)--;
  }
  return(a);
}

#endif




