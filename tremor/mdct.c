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

 function: normalized modified discrete cosine transform
           power of two length transform only [64 <= n ]
 last mod: $Id$

 Original algorithm adapted long ago from _The use of multirate filter
 banks for coding of high quality digital audio_, by T. Sporer,
 K. Brandenburg and B. Edler, collection of the European Signal
 Processing Conference (EUSIPCO), Amsterdam, June 1992, Vol.1, pp
 211-214

 The below code implements an algorithm that no longer looks much like
 that presented in the paper, but the basic structure remains if you
 dig deep enough to see it.

 This module DOES NOT INCLUDE code to generate/apply the window
 function.  Everybody has their own weird favorite including me... I
 happen to like the properties of y=sin(.5PI*sin^2(x)), but others may
 vehemently disagree.

 ********************************************************************/

#include "ivorbiscodec.h"
#include "os.h"
#include "misc.h"
#include "mdct.h"
#include "mdct_lookup.h"


/* 8 point butterfly (in place) */
STIN void mdct_butterfly_8(DATA_TYPE *x){

  REG_TYPE r0   = x[4] + x[0];
  REG_TYPE r1   = x[4] - x[0];
  REG_TYPE r2   = x[5] + x[1];
  REG_TYPE r3   = x[5] - x[1];
  REG_TYPE r4   = x[6] + x[2];
  REG_TYPE r5   = x[6] - x[2];
  REG_TYPE r6   = x[7] + x[3];
  REG_TYPE r7   = x[7] - x[3];

	   x[0] = r5   + r3;
	   x[1] = r7   - r1;
	   x[2] = r5   - r3;
	   x[3] = r7   + r1;
           x[4] = r4   - r0;
	   x[5] = r6   - r2;
           x[6] = r4   + r0;
	   x[7] = r6   + r2;
	   MB();
}

/* 16 point butterfly (in place, 4 register) */
STIN void mdct_butterfly_16(DATA_TYPE *x){

  REG_TYPE r0, r1;

	   r0 = x[ 0] - x[ 8]; x[ 8] += x[ 0];
	   r1 = x[ 1] - x[ 9]; x[ 9] += x[ 1];
	   x[ 0] = MULT31((r0 + r1) , cPI2_8);
	   x[ 1] = MULT31((r1 - r0) , cPI2_8);
	   MB();

	   r0 = x[10] - x[ 2]; x[10] += x[ 2];
	   r1 = x[ 3] - x[11]; x[11] += x[ 3];
	   x[ 2] = r1; x[ 3] = r0;
	   MB();

	   r0 = x[12] - x[ 4]; x[12] += x[ 4];
	   r1 = x[13] - x[ 5]; x[13] += x[ 5];
	   x[ 4] = MULT31((r0 - r1) , cPI2_8);
	   x[ 5] = MULT31((r0 + r1) , cPI2_8);
	   MB();

	   r0 = x[14] - x[ 6]; x[14] += x[ 6];
	   r1 = x[15] - x[ 7]; x[15] += x[ 7];
	   x[ 6] = r0; x[ 7] = r1;
	   MB();

	   mdct_butterfly_8(x);
	   mdct_butterfly_8(x+8);
}

/* 32 point butterfly (in place, 4 register) */
STIN void mdct_butterfly_32(DATA_TYPE *x){

  REG_TYPE r0, r1;

	   r0 = x[30] - x[14]; x[30] += x[14];           
	   r1 = x[31] - x[15]; x[31] += x[15];
	   x[14] = r0; x[15] = r1;
	   MB();

	   r0 = x[28] - x[12]; x[28] += x[12];           
	   r1 = x[29] - x[13]; x[29] += x[13];
	   XNPROD31( r0, r1, cPI1_8, cPI3_8, &x[12], &x[13] );
	   MB();

	   r0 = x[26] - x[10]; x[26] += x[10];
	   r1 = x[27] - x[11]; x[27] += x[11];
	   x[10] = MULT31((r0 - r1) , cPI2_8);
	   x[11] = MULT31((r0 + r1) , cPI2_8);
	   MB();

	   r0 = x[24] - x[ 8]; x[24] += x[ 8];
	   r1 = x[25] - x[ 9]; x[25] += x[ 9];
	   XNPROD31( r0, r1, cPI3_8, cPI1_8, &x[ 8], &x[ 9] );
	   MB();

	   r0 = x[22] - x[ 6]; x[22] += x[ 6];
	   r1 = x[ 7] - x[23]; x[23] += x[ 7];
	   x[ 6] = r1; x[ 7] = r0;
	   MB();

	   r0 = x[ 4] - x[20]; x[20] += x[ 4];
	   r1 = x[ 5] - x[21]; x[21] += x[ 5];
	   XPROD31 ( r0, r1, cPI3_8, cPI1_8, &x[ 4], &x[ 5] );
	   MB();

	   r0 = x[ 2] - x[18]; x[18] += x[ 2];
	   r1 = x[ 3] - x[19]; x[19] += x[ 3];
	   x[ 2] = MULT31((r1 + r0) , cPI2_8);
	   x[ 3] = MULT31((r1 - r0) , cPI2_8);
	   MB();

	   r0 = x[ 0] - x[16]; x[16] += x[ 0];
	   r1 = x[ 1] - x[17]; x[17] += x[ 1];
	   XPROD31 ( r0, r1, cPI1_8, cPI3_8, &x[ 0], &x[ 1] );
	   MB();

	   mdct_butterfly_16(x);
	   mdct_butterfly_16(x+16);
}

/* N/stage point generic N stage butterfly (in place, 2 register) */
STIN void mdct_butterfly_generic(DATA_TYPE *x,int points,int step){

  LOOKUP_T *T   = sincos_lookup0;
  DATA_TYPE *x1        = x + points      - 8;
  DATA_TYPE *x2        = x + (points>>1) - 8;
  REG_TYPE   r0;
  REG_TYPE   r1;

  do{
    r0 = x1[6] - x2[6]; x1[6] += x2[6];
    r1 = x2[7] - x1[7]; x1[7] += x2[7];
    XPROD31( r1, r0, T[0], T[1], &x2[6], &x2[7] ); T+=step;

    r0 = x1[4] - x2[4]; x1[4] += x2[4];
    r1 = x2[5] - x1[5]; x1[5] += x2[5];
    XPROD31( r1, r0, T[0], T[1], &x2[4], &x2[5] ); T+=step;

    r0 = x1[2] - x2[2]; x1[2] += x2[2];
    r1 = x2[3] - x1[3]; x1[3] += x2[3];
    XPROD31( r1, r0, T[0], T[1], &x2[2], &x2[3] ); T+=step;

    r0 = x1[0] - x2[0]; x1[0] += x2[0];
    r1 = x2[1] - x1[1]; x1[1] += x2[1];
    XPROD31( r1, r0, T[0], T[1], &x2[0], &x2[1] ); T+=step;

    x1-=8; x2-=8;
  }while(T<sincos_lookup0+1024);
  do{
    r0 = x1[6] - x2[6]; x1[6] += x2[6];
    r1 = x1[7] - x2[7]; x1[7] += x2[7];
    XNPROD31( r0, r1, T[0], T[1], &x2[6], &x2[7] ); T-=step;

    r0 = x1[4] - x2[4]; x1[4] += x2[4];
    r1 = x1[5] - x2[5]; x1[5] += x2[5];
    XNPROD31( r0, r1, T[0], T[1], &x2[4], &x2[5] ); T-=step;

    r0 = x1[2] - x2[2]; x1[2] += x2[2];
    r1 = x1[3] - x2[3]; x1[3] += x2[3];
    XNPROD31( r0, r1, T[0], T[1], &x2[2], &x2[3] ); T-=step;

    r0 = x1[0] - x2[0]; x1[0] += x2[0];
    r1 = x1[1] - x2[1]; x1[1] += x2[1];
    XNPROD31( r0, r1, T[0], T[1], &x2[0], &x2[1] ); T-=step;

    x1-=8; x2-=8;
  }while(T>sincos_lookup0);
  do{
    r0 = x2[6] - x1[6]; x1[6] += x2[6];
    r1 = x2[7] - x1[7]; x1[7] += x2[7];
    XPROD31( r0, r1, T[0], T[1], &x2[6], &x2[7] ); T+=step;

    r0 = x2[4] - x1[4]; x1[4] += x2[4];
    r1 = x2[5] - x1[5]; x1[5] += x2[5];
    XPROD31( r0, r1, T[0], T[1], &x2[4], &x2[5] ); T+=step;

    r0 = x2[2] - x1[2]; x1[2] += x2[2];
    r1 = x2[3] - x1[3]; x1[3] += x2[3];
    XPROD31( r0, r1, T[0], T[1], &x2[2], &x2[3] ); T+=step;

    r0 = x2[0] - x1[0]; x1[0] += x2[0];
    r1 = x2[1] - x1[1]; x1[1] += x2[1];
    XPROD31( r0, r1, T[0], T[1], &x2[0], &x2[1] ); T+=step;

    x1-=8; x2-=8;
  }while(T<sincos_lookup0+1024);
  do{
    r0 = x1[6] - x2[6]; x1[6] += x2[6];
    r1 = x2[7] - x1[7]; x1[7] += x2[7];
    XNPROD31( r1, r0, T[0], T[1], &x2[6], &x2[7] ); T-=step;

    r0 = x1[4] - x2[4]; x1[4] += x2[4];
    r1 = x2[5] - x1[5]; x1[5] += x2[5];
    XNPROD31( r1, r0, T[0], T[1], &x2[4], &x2[5] ); T-=step;

    r0 = x1[2] - x2[2]; x1[2] += x2[2];
    r1 = x2[3] - x1[3]; x1[3] += x2[3];
    XNPROD31( r1, r0, T[0], T[1], &x2[2], &x2[3] ); T-=step;

    r0 = x1[0] - x2[0]; x1[0] += x2[0];
    r1 = x2[1] - x1[1]; x1[1] += x2[1];
    XNPROD31( r1, r0, T[0], T[1], &x2[0], &x2[1] ); T-=step;

    x1-=8; x2-=8;
  }while(T>sincos_lookup0);
}

STIN void mdct_butterflies(DATA_TYPE *x,int points,int shift){

  int stages=8-shift;
  int i,j;
  
  for(i=0;--stages>0;i++){
    for(j=0;j<(1<<i);j++)
      mdct_butterfly_generic(x+(points>>i)*j,points>>i,4<<(i+shift));
  }

  for(j=0;j<points;j+=32)
    mdct_butterfly_32(x+j);

}

static unsigned char bitrev[16]={0,8,4,12,2,10,6,14,1,9,5,13,3,11,7,15};

STIN int bitrev12(int x){
  return bitrev[x>>8]|(bitrev[(x&0x0f0)>>4]<<4)|(((int)bitrev[x&0x00f])<<8);
}

STIN void mdct_bitreverse(DATA_TYPE *x,int n,int step,int shift){

  int          bit   = 0;
  DATA_TYPE   *w0    = x;
  DATA_TYPE   *w1    = x = w0+(n>>1);
  LOOKUP_T    *T = (step>=4)?(sincos_lookup0+(step>>1)):sincos_lookup1;
  LOOKUP_T    *Ttop  = T+1024;
  DATA_TYPE    r2;

  do{
    DATA_TYPE r3     = bitrev12(bit++);
    DATA_TYPE *x0    = x + ((r3 ^ 0xfff)>>shift) -1;
    DATA_TYPE *x1    = x + (r3>>shift);

    REG_TYPE  r0     = x0[0]  + x1[0];
    REG_TYPE  r1     = x1[1]  - x0[1];

	      XPROD32( r0, r1, T[1], T[0], &r2, &r3 ); T+=step;

	      w1    -= 4;

	      r0     = (x0[1] + x1[1])>>1;
              r1     = (x0[0] - x1[0])>>1;
	      w0[0]  = r0     + r2;
	      w0[1]  = r1     + r3;
	      w1[2]  = r0     - r2;
	      w1[3]  = r3     - r1;

	      r3     = bitrev12(bit++);
              x0     = x + ((r3 ^ 0xfff)>>shift) -1;
              x1     = x + (r3>>shift);

              r0     = x0[0]  + x1[0];
              r1     = x1[1]  - x0[1];

	      XPROD32( r0, r1, T[1], T[0], &r2, &r3 ); T+=step;

              r0     = (x0[1] + x1[1])>>1;
              r1     = (x0[0] - x1[0])>>1;
	      w0[2]  = r0     + r2;
	      w0[3]  = r1     + r3;
	      w1[0]  = r0     - r2;
	      w1[1]  = r3     - r1;

	      w0    += 4;
  }while(T<Ttop);
  do{
    DATA_TYPE r3     = bitrev12(bit++);
    DATA_TYPE *x0    = x + ((r3 ^ 0xfff)>>shift) -1;
    DATA_TYPE *x1    = x + (r3>>shift);

    REG_TYPE  r0     = x0[0]  + x1[0];
    REG_TYPE  r1     = x1[1]  - x0[1];

	      T-=step; XPROD32( r0, r1, T[0], T[1], &r2, &r3 );

	      w1    -= 4;

	      r0     = (x0[1] + x1[1])>>1;
              r1     = (x0[0] - x1[0])>>1;
	      w0[0]  = r0     + r2;
	      w0[1]  = r1     + r3;
	      w1[2]  = r0     - r2;
	      w1[3]  = r3     - r1;

	      r3     = bitrev12(bit++);
              x0     = x + ((r3 ^ 0xfff)>>shift) -1;
              x1     = x + (r3>>shift);

              r0     = x0[0]  + x1[0];
              r1     = x1[1]  - x0[1];

	      T-=step; XPROD32( r0, r1, T[0], T[1], &r2, &r3 );

              r0     = (x0[1] + x1[1])>>1;
              r1     = (x0[0] - x1[0])>>1;
	      w0[2]  = r0     + r2;
	      w0[3]  = r1     + r3;
	      w1[0]  = r0     - r2;
	      w1[1]  = r3     - r1;

	      w0    += 4;
  }while(w0<w1);
}

void mdct_backward(int n, DATA_TYPE *in, DATA_TYPE *out){
  int n2=n>>1;
  int n4=n>>2;
  DATA_TYPE *iX;
  DATA_TYPE *oX;
  LOOKUP_T *T;
  LOOKUP_T *V;
  int shift;
  int step;

  for (shift=6;!(n&(1<<shift));shift++);
  shift=13-shift;
  step=2<<shift;
   
  /* rotate */

  iX            = in+n2-7;
  oX            = out+n2+n4;
  T             = sincos_lookup0;

  do{
    oX-=4;
    XPROD31( iX[4], iX[6], T[0], T[1], &oX[2], &oX[3] ); T+=step;
    XPROD31( iX[0], iX[2], T[0], T[1], &oX[0], &oX[1] ); T+=step;
    iX-=8;
  }while(iX>=in+n4);
  do{
    oX-=4;
    XPROD31( iX[4], iX[6], T[1], T[0], &oX[2], &oX[3] ); T-=step;
    XPROD31( iX[0], iX[2], T[1], T[0], &oX[0], &oX[1] ); T-=step;
    iX-=8;
  }while(iX>=in);

  iX            = in+n2-8;
  oX            = out+n2+n4;
  T             = sincos_lookup0;

  do{
    T+=step; XNPROD31( iX[6], iX[4], T[0], T[1], &oX[0], &oX[1] );
    T+=step; XNPROD31( iX[2], iX[0], T[0], T[1], &oX[2], &oX[3] );
    iX-=8;
    oX+=4;
  }while(iX>=in+n4);
  do{
    T-=step; XNPROD31( iX[6], iX[4], T[1], T[0], &oX[0], &oX[1] );
    T-=step; XNPROD31( iX[2], iX[0], T[1], T[0], &oX[2], &oX[3] );
    iX-=8;
    oX+=4;
  }while(iX>=in);

  mdct_butterflies(out+n2,n2,shift);
  mdct_bitreverse(out,n,step,shift);

  /* rotate + window */

  step>>=2;
  {
    DATA_TYPE *oX1=out+n2+n4;
    DATA_TYPE *oX2=out+n2+n4;
    DATA_TYPE *iX =out;

    switch(step) {
      default: {
        T=(step>=4)?(sincos_lookup0+(step>>1)):sincos_lookup1;
        do{
          oX1-=4;
	  XPROD31( iX[0], -iX[1], T[0], T[1], &oX1[3], &oX2[0] ); T+=step;
	  XPROD31( iX[2], -iX[3], T[0], T[1], &oX1[2], &oX2[1] ); T+=step;
	  XPROD31( iX[4], -iX[5], T[0], T[1], &oX1[1], &oX2[2] ); T+=step;
	  XPROD31( iX[6], -iX[7], T[0], T[1], &oX1[0], &oX2[3] ); T+=step;
	  oX2+=4;
	  iX+=8;
	}while(iX<oX1);
	break;
      }

      case 1: {
        /* linear interpolation between table values: offset=0.5, step=1 */
	REG_TYPE  t0,t1,v0,v1;
        T         = sincos_lookup0;
        V         = sincos_lookup1;
	t0        = (*T++)>>1;
	t1        = (*T++)>>1;
        do{
          oX1-=4;

	  t0 += (v0 = (*V++)>>1);
	  t1 += (v1 = (*V++)>>1);
	  XPROD31( iX[0], -iX[1], t0, t1, &oX1[3], &oX2[0] );
	  v0 += (t0 = (*T++)>>1);
	  v1 += (t1 = (*T++)>>1);
	  XPROD31( iX[2], -iX[3], v0, v1, &oX1[2], &oX2[1] );
	  t0 += (v0 = (*V++)>>1);
	  t1 += (v1 = (*V++)>>1);
	  XPROD31( iX[4], -iX[5], t0, t1, &oX1[1], &oX2[2] );
	  v0 += (t0 = (*T++)>>1);
	  v1 += (t1 = (*T++)>>1);
	  XPROD31( iX[6], -iX[7], v0, v1, &oX1[0], &oX2[3] );

	  oX2+=4;
	  iX+=8;
	}while(iX<oX1);
	break;
      }

      case 0: {
        /* linear interpolation between table values: offset=0.25, step=0.5 */
	REG_TYPE  t0,t1,v0,v1,q0,q1;
        T         = sincos_lookup0;
        V         = sincos_lookup1;
	t0        = *T++;
	t1        = *T++;
        do{
          oX1-=4;

	  v0  = *V++;
	  v1  = *V++;
	  t0 +=  (q0 = (v0-t0)>>2);
	  t1 +=  (q1 = (v1-t1)>>2);
	  XPROD31( iX[0], -iX[1], t0, t1, &oX1[3], &oX2[0] );
	  t0  = v0-q0;
	  t1  = v1-q1;
	  XPROD31( iX[2], -iX[3], t0, t1, &oX1[2], &oX2[1] );

	  t0  = *T++;
	  t1  = *T++;
	  v0 += (q0 = (t0-v0)>>2);
	  v1 += (q1 = (t1-v1)>>2);
	  XPROD31( iX[4], -iX[5], v0, v1, &oX1[1], &oX2[2] );
	  v0  = t0-q0;
	  v1  = t1-q1;
	  XPROD31( iX[6], -iX[7], v0, v1, &oX1[0], &oX2[3] );

	  oX2+=4;
	  iX+=8;
	}while(iX<oX1);
	break;
      }
    }

    iX=out+n2+n4;
    oX1=out+n4;
    oX2=oX1;

    do{
      oX1-=4;
      iX-=4;

      oX2[0] = -(oX1[3] = iX[3]);
      oX2[1] = -(oX1[2] = iX[2]);
      oX2[2] = -(oX1[1] = iX[1]);
      oX2[3] = -(oX1[0] = iX[0]);

      oX2+=4;
    }while(oX2<iX);

    iX=out+n2+n4;
    oX1=out+n2+n4;
    oX2=out+n2;

    do{
      oX1-=4;
      oX1[0]= iX[3];
      oX1[1]= iX[2];
      oX1[2]= iX[1];
      oX1[3]= iX[0];
      iX+=4;
    }while(oX1>oX2);
  }
}

