/*
 * Modified for use with MPlayer, for details see the changelog at
 * http://svn.mplayerhq.hu/mplayer/trunk/
 * $Id$
 */

/* 
// This is an optimized DCT from Jeff Tsay's maplay 1.2+ package.
// Saved one multiplication by doing the 'twiddle factor' stuff
// together with the window mul. (MH)
//
// This uses Byeong Gi Lee's Fast Cosine Transform algorithm, but the
// 9 point IDCT needs to be reduced further. Unfortunately, I don't
// know how to do that, because 9 is not an even number. - Jeff.
//
//////////////////////////////////////////////////////////////////
//
// 9 Point Inverse Discrete Cosine Transform
//
// This piece of code is Copyright 1997 Mikko Tommila and is freely usable
// by anybody. The algorithm itself is of course in the public domain.
//
// Again derived heuristically from the 9-point WFTA.
//
// The algorithm is optimized (?) for speed, not for small rounding errors or
// good readability.
//
// 36 additions, 11 multiplications
//
// Again this is very likely sub-optimal.
//
// The code is optimized to use a minimum number of temporary variables,
// so it should compile quite well even on 8-register Intel x86 processors.
// This makes the code quite obfuscated and very difficult to understand.
//
// References:
// [1] S. Winograd: "On Computing the Discrete Fourier Transform",
//     Mathematics of Computation, Volume 32, Number 141, January 1978,
//     Pages 175-199
*/

/*------------------------------------------------------------------*/
/*                                                                  */
/*    Function: Calculation of the inverse MDCT                     */
/*                                                                  */
/*------------------------------------------------------------------*/

static void dct36(real *inbuf,real *o1,real *o2,real *wintab,real *tsbuf)
{
#ifdef NEW_DCT9
  real tmp[18];
#endif

  {
    register real *in = inbuf;

    in[17]+=in[16]; in[16]+=in[15]; in[15]+=in[14];
    in[14]+=in[13]; in[13]+=in[12]; in[12]+=in[11];
    in[11]+=in[10]; in[10]+=in[9];  in[9] +=in[8];
    in[8] +=in[7];  in[7] +=in[6];  in[6] +=in[5];
    in[5] +=in[4];  in[4] +=in[3];  in[3] +=in[2];
    in[2] +=in[1];  in[1] +=in[0];

    in[17]+=in[15]; in[15]+=in[13]; in[13]+=in[11]; in[11]+=in[9];
    in[9] +=in[7];  in[7] +=in[5];  in[5] +=in[3];  in[3] +=in[1];


#ifdef NEW_DCT9
    {
      real t0, t1, t2, t3, t4, t5, t6, t7;

      t1 = COS6_2 * in[12];
      t2 = COS6_2 * (in[8] + in[16] - in[4]);

      t3 = in[0] + t1;
      t4 = in[0] - t1 - t1;
      t5 = t4 - t2;

      t0 = cos9[0] * (in[4] + in[8]);
      t1 = cos9[1] * (in[8] - in[16]);

      tmp[4] = t4 + t2 + t2;
      t2 = cos9[2] * (in[4] + in[16]);

      t6 = t3 - t0 - t2;
      t0 += t3 + t1;
      t3 += t2 - t1;

      t2 = cos18[0] * (in[2]  + in[10]);
      t4 = cos18[1] * (in[10] - in[14]);
      t7 = COS6_1 * in[6];

      t1 = t2 + t4 + t7;
      tmp[0] = t0 + t1;
      tmp[8] = t0 - t1;
      t1 = cos18[2] * (in[2] + in[14]);
      t2 += t1 - t7;

      tmp[3] = t3 + t2;
      t0 = COS6_1 * (in[10] + in[14] - in[2]);
      tmp[5] = t3 - t2;

      t4 -= t1 + t7;

      tmp[1] = t5 - t0;
      tmp[7] = t5 + t0;
      tmp[2] = t6 + t4;
      tmp[6] = t6 - t4;
    }

    {
      real t0, t1, t2, t3, t4, t5, t6, t7;

      t1 = COS6_2 * in[13];
      t2 = COS6_2 * (in[9] + in[17] - in[5]);

      t3 = in[1] + t1;
      t4 = in[1] - t1 - t1;
      t5 = t4 - t2;

      t0 = cos9[0] * (in[5] + in[9]);
      t1 = cos9[1] * (in[9] - in[17]);

      tmp[13] = (t4 + t2 + t2) * tfcos36[17-13];
      t2 = cos9[2] * (in[5] + in[17]);

      t6 = t3 - t0 - t2;
      t0 += t3 + t1;
      t3 += t2 - t1;

      t2 = cos18[0] * (in[3]  + in[11]);
      t4 = cos18[1] * (in[11] - in[15]);
      t7 = COS6_1 * in[7];

      t1 = t2 + t4 + t7;
      tmp[17] = (t0 + t1) * tfcos36[17-17];
      tmp[9]  = (t0 - t1) * tfcos36[17-9];
      t1 = cos18[2] * (in[3] + in[15]);
      t2 += t1 - t7;

      tmp[14] = (t3 + t2) * tfcos36[17-14];
      t0 = COS6_1 * (in[11] + in[15] - in[3]);
      tmp[12] = (t3 - t2) * tfcos36[17-12];

      t4 -= t1 + t7;

      tmp[16] = (t5 - t0) * tfcos36[17-16];
      tmp[10] = (t5 + t0) * tfcos36[17-10];
      tmp[15] = (t6 + t4) * tfcos36[17-15];
      tmp[11] = (t6 - t4) * tfcos36[17-11];
   }

#define MACRO(v) { \
    real tmpval; \
    real sum0 = tmp[(v)]; \
    real sum1 = tmp[17-(v)]; \
    out2[9+(v)] = (tmpval = sum0 + sum1) * w[27+(v)]; \
    out2[8-(v)] = tmpval * w[26-(v)]; \
    sum0 -= sum1; \
    ts[SBLIMIT*(8-(v))] = out1[8-(v)] + sum0 * w[8-(v)]; \
    ts[SBLIMIT*(9+(v))] = out1[9+(v)] + sum0 * w[9+(v)]; }

{
   register real *out2 = o2;
   register real *w = wintab;
   register real *out1 = o1;
   register real *ts = tsbuf;

   MACRO(0);
   MACRO(1);
   MACRO(2);
   MACRO(3);
   MACRO(4);
   MACRO(5);
   MACRO(6);
   MACRO(7);
   MACRO(8);
}

#else

  {

#define MACRO0(v) { \
    real tmp; \
    out2[9+(v)] = (tmp = sum0 + sum1) * w[27+(v)]; \
    out2[8-(v)] = tmp * w[26-(v)];  } \
    sum0 -= sum1; \
    ts[SBLIMIT*(8-(v))] = out1[8-(v)] + sum0 * w[8-(v)]; \
    ts[SBLIMIT*(9+(v))] = out1[9+(v)] + sum0 * w[9+(v)]; 
#define MACRO1(v) { \
	real sum0,sum1; \
    sum0 = tmp1a + tmp2a; \
	sum1 = (tmp1b + tmp2b) * tfcos36[(v)]; \
	MACRO0(v); }
#define MACRO2(v) { \
    real sum0,sum1; \
    sum0 = tmp2a - tmp1a; \
    sum1 = (tmp2b - tmp1b) * tfcos36[(v)]; \
	MACRO0(v); }

    register const real *c = COS9;
    register real *out2 = o2;
	register real *w = wintab;
	register real *out1 = o1;
	register real *ts = tsbuf;

    real ta33,ta66,tb33,tb66;

    ta33 = in[2*3+0] * c[3];
    ta66 = in[2*6+0] * c[6];
    tb33 = in[2*3+1] * c[3];
    tb66 = in[2*6+1] * c[6];

    { 
      real tmp1a,tmp2a,tmp1b,tmp2b;
      tmp1a =             in[2*1+0] * c[1] + ta33 + in[2*5+0] * c[5] + in[2*7+0] * c[7];
      tmp1b =             in[2*1+1] * c[1] + tb33 + in[2*5+1] * c[5] + in[2*7+1] * c[7];
      tmp2a = in[2*0+0] + in[2*2+0] * c[2] + in[2*4+0] * c[4] + ta66 + in[2*8+0] * c[8];
      tmp2b = in[2*0+1] + in[2*2+1] * c[2] + in[2*4+1] * c[4] + tb66 + in[2*8+1] * c[8];

      MACRO1(0);
      MACRO2(8);
    }

    {
      real tmp1a,tmp2a,tmp1b,tmp2b;
      tmp1a = ( in[2*1+0] - in[2*5+0] - in[2*7+0] ) * c[3];
      tmp1b = ( in[2*1+1] - in[2*5+1] - in[2*7+1] ) * c[3];
      tmp2a = ( in[2*2+0] - in[2*4+0] - in[2*8+0] ) * c[6] - in[2*6+0] + in[2*0+0];
      tmp2b = ( in[2*2+1] - in[2*4+1] - in[2*8+1] ) * c[6] - in[2*6+1] + in[2*0+1];

      MACRO1(1);
      MACRO2(7);
    }

    {
      real tmp1a,tmp2a,tmp1b,tmp2b;
      tmp1a =             in[2*1+0] * c[5] - ta33 - in[2*5+0] * c[7] + in[2*7+0] * c[1];
      tmp1b =             in[2*1+1] * c[5] - tb33 - in[2*5+1] * c[7] + in[2*7+1] * c[1];
      tmp2a = in[2*0+0] - in[2*2+0] * c[8] - in[2*4+0] * c[2] + ta66 + in[2*8+0] * c[4];
      tmp2b = in[2*0+1] - in[2*2+1] * c[8] - in[2*4+1] * c[2] + tb66 + in[2*8+1] * c[4];

      MACRO1(2);
      MACRO2(6);
    }

    {
      real tmp1a,tmp2a,tmp1b,tmp2b;
      tmp1a =             in[2*1+0] * c[7] - ta33 + in[2*5+0] * c[1] - in[2*7+0] * c[5];
      tmp1b =             in[2*1+1] * c[7] - tb33 + in[2*5+1] * c[1] - in[2*7+1] * c[5];
      tmp2a = in[2*0+0] - in[2*2+0] * c[4] + in[2*4+0] * c[8] + ta66 - in[2*8+0] * c[2];
      tmp2b = in[2*0+1] - in[2*2+1] * c[4] + in[2*4+1] * c[8] + tb66 - in[2*8+1] * c[2];

      MACRO1(3);
      MACRO2(5);
    }

	{
		real sum0,sum1;
    	sum0 =  in[2*0+0] - in[2*2+0] + in[2*4+0] - in[2*6+0] + in[2*8+0];
    	sum1 = (in[2*0+1] - in[2*2+1] + in[2*4+1] - in[2*6+1] + in[2*8+1] ) * tfcos36[4];
		MACRO0(4);
	}
  }
#endif

  }
}

