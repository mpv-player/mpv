/*
   RTjpeg (C) Justin Schoeman 1998 (justin@suntiger.ee.up.ac.za)

   With modifications by:
   (c) 1998, 1999 by Joerg Walter <trouble@moes.pmnet.uni-oldenburg.de>
   and
   (c) 1999 by Wim Taymans <wim.taymans@tvd.be>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"

#include "mpbswap.h"
#include "rtjpegn.h"

#if HAVE_MMX
#include "mmx.h"
#endif

//#define SHOWBLOCK 1
#define BETTERCOMPRESSION 1

static const unsigned char RTjpeg_ZZ[64]={
0,
8, 1,
2, 9, 16,
24, 17, 10, 3,
4, 11, 18, 25, 32,
40, 33, 26, 19, 12, 5,
6, 13, 20, 27, 34, 41, 48,
56, 49, 42, 35, 28, 21, 14, 7,
15, 22, 29, 36, 43, 50, 57,
58, 51, 44, 37, 30, 23,
31, 38, 45, 52, 59,
60, 53, 46, 39,
47, 54, 61,
62, 55,
63 };

static const __u64 RTjpeg_aan_tab[64]={
4294967296ULL, 5957222912ULL, 5611718144ULL, 5050464768ULL, 4294967296ULL, 3374581504ULL, 2324432128ULL, 1184891264ULL,
5957222912ULL, 8263040512ULL, 7783580160ULL, 7005009920ULL, 5957222912ULL, 4680582144ULL, 3224107520ULL, 1643641088ULL,
5611718144ULL, 7783580160ULL, 7331904512ULL, 6598688768ULL, 5611718144ULL, 4408998912ULL, 3036936960ULL, 1548224000ULL,
5050464768ULL, 7005009920ULL, 6598688768ULL, 5938608128ULL, 5050464768ULL, 3968072960ULL, 2733115392ULL, 1393296000ULL,
4294967296ULL, 5957222912ULL, 5611718144ULL, 5050464768ULL, 4294967296ULL, 3374581504ULL, 2324432128ULL, 1184891264ULL,
3374581504ULL, 4680582144ULL, 4408998912ULL, 3968072960ULL, 3374581504ULL, 2651326208ULL, 1826357504ULL, 931136000ULL,
2324432128ULL, 3224107520ULL, 3036936960ULL, 2733115392ULL, 2324432128ULL, 1826357504ULL, 1258030336ULL, 641204288ULL,
1184891264ULL, 1643641088ULL, 1548224000ULL, 1393296000ULL, 1184891264ULL, 931136000ULL, 641204288ULL, 326894240ULL,
};

#if !HAVE_MMX
static __s32 RTjpeg_ws[64+31];
#endif
static __u8 RTjpeg_alldata[2*64+4*64+4*64+4*64+4*64+32];

static __s16 *block; // rh
static __s16 *RTjpeg_block;
static __s32 *RTjpeg_lqt;
static __s32 *RTjpeg_cqt;
static __u32 *RTjpeg_liqt;
static __u32 *RTjpeg_ciqt;

static unsigned char RTjpeg_lb8;
static unsigned char RTjpeg_cb8;
static int RTjpeg_width, RTjpeg_height;
static int RTjpeg_Ywidth, RTjpeg_Cwidth;
static int RTjpeg_Ysize, RTjpeg_Csize;

static __s16 *RTjpeg_old=NULL;

#if HAVE_MMX
static mmx_t RTjpeg_lmask;
static mmx_t RTjpeg_cmask;
#else
static __u16 RTjpeg_lmask;
static __u16 RTjpeg_cmask;
#endif

static const unsigned char RTjpeg_lum_quant_tbl[64] = {
    16,  11,  10,  16,  24,  40,  51,  61,
    12,  12,  14,  19,  26,  58,  60,  55,
    14,  13,  16,  24,  40,  57,  69,  56,
    14,  17,  22,  29,  51,  87,  80,  62,
    18,  22,  37,  56,  68, 109, 103,  77,
    24,  35,  55,  64,  81, 104, 113,  92,
    49,  64,  78,  87, 103, 121, 120, 101,
    72,  92,  95,  98, 112, 100, 103,  99
 };

static const unsigned char RTjpeg_chrom_quant_tbl[64] = {
    17,  18,  24,  47,  99,  99,  99,  99,
    18,  21,  26,  66,  99,  99,  99,  99,
    24,  26,  56,  99,  99,  99,  99,  99,
    47,  66,  99,  99,  99,  99,  99,  99,
    99,  99,  99,  99,  99,  99,  99,  99,
    99,  99,  99,  99,  99,  99,  99,  99,
    99,  99,  99,  99,  99,  99,  99,  99,
    99,  99,  99,  99,  99,  99,  99,  99
 };

#ifdef BETTERCOMPRESSION

/*--------------------------------------------------*/
/*  better encoding, but needs a lot more cpu time  */
/*  seems to be more effective than old method +lzo */
/*  with this encoding lzo isn't efficient anymore  */
/*  there is still more potential for better        */
/*  encoding but that would need even more cputime  */
/*  anyway your mileage may vary                    */
/*                                                  */
/*  written by Martin BIELY and Roman HOCHLEITNER   */
/*--------------------------------------------------*/

/* +++++++++++++++++++++++++++++++++++++++++++++++++++*/
/* Block to Stream (encoding)                         */
/*                                                    */

static int RTjpeg_b2s(__s16 *data, __s8 *strm, __u8 bt8)
{
 register int ci, co=1;
 register __s16 ZZvalue;
 register unsigned char bitten;
 register unsigned char bitoff;

#ifdef SHOWBLOCK

  int ii;
  for (ii=0; ii < 64; ii++) {
    fprintf(stdout, "%d ", data[RTjpeg_ZZ[ii]]);
  }
  fprintf(stdout, "\n\n");

#endif

 // first byte allways written
 ((__u8*)strm)[0]=
      (__u8)(data[RTjpeg_ZZ[0]]>254) ? 254:((data[RTjpeg_ZZ[0]]<0)?0:data[RTjpeg_ZZ[0]]);


 ci=63;
 while (data[RTjpeg_ZZ[ci]]==0 && ci>0) ci--;

 bitten = ((unsigned char)ci) << 2;

 if (ci==0) {
   ((__u8*)strm)[1]= bitten;
   co = 2;
   return (int)co;
 }

 /* bitoff=0 because the high 6bit contain first non zero position */
 bitoff = 0;
 co = 1;

 for(; ci>0; ci--) {

   ZZvalue = data[RTjpeg_ZZ[ci]];

   switch(ZZvalue) {
   case 0:
	break;
   case 1:
        bitten |= (0x01<<bitoff);
	break;
   case -1:
        bitten |= (0x03<<bitoff);
	break;
   default:
        bitten |= (0x02<<bitoff);
	goto HERZWEH;
	break;
   }

   if( bitoff == 0 ) {
      ((__u8*)strm)[co]= bitten;
      bitten = 0;
      bitoff = 8;
      co++;
   } /* "fall through" */
   bitoff-=2;

 }

 /* ci must be 0 */
 if(bitoff != 6) {

      ((__u8*)strm)[co]= bitten;
      co++;

 }
 goto BAUCHWEH;

HERZWEH:
/* ci cannot be 0 */
/* correct bitoff to nibble boundaries */

 switch(bitoff){
 case 4:
 case 6:
   bitoff = 0;
   break;
 case 2:
 case 0:
   ((__u8*)strm)[co]= bitten;
   bitoff = 4;
   co++;
   bitten = 0; // clear half nibble values in bitten
   break;
 default:
   break;
 }

 for(; ci>0; ci--) {

   ZZvalue = data[RTjpeg_ZZ[ci]];

   if( (ZZvalue > 7) || (ZZvalue < -7) ) {
        bitten |= (0x08<<bitoff);
	goto HIRNWEH;
   }

   bitten |= (ZZvalue&0xf)<<bitoff;

   if( bitoff == 0 ) {
      ((__u8*)strm)[co]= bitten;
      bitten = 0;
      bitoff = 8;
      co++;
   } /* "fall thru" */
   bitoff-=4;
 }

 /* ci must be 0 */
 if( bitoff == 0 ) {
    ((__u8*)strm)[co]= bitten;
    co++;
 }
 goto BAUCHWEH;

HIRNWEH:

 ((__u8*)strm)[co]= bitten;
 co++;


 /* bitting is over now we bite */
 for(; ci>0; ci--) {

   ZZvalue = data[RTjpeg_ZZ[ci]];

   if(ZZvalue>0)
   {
     strm[co++]=(__s8)(ZZvalue>127)?127:ZZvalue;
   }
   else
   {
     strm[co++]=(__s8)(ZZvalue<-128)?-128:ZZvalue;
   }

 }


BAUCHWEH:
  /* we gotoo much now we are ill */
#ifdef SHOWBLOCK
{
int i;
fprintf(stdout, "\nco = '%d'\n", co);
 for (i=0; i < co+2; i++) {
   fprintf(stdout, "%d ", strm[i]);
 }
fprintf(stdout, "\n\n");
}
#endif

 return (int)co;
}

#else

static int RTjpeg_b2s(__s16 *data, __s8 *strm, __u8 bt8)
{
 register int ci, co=1, tmp;
 register __s16 ZZvalue;

#ifdef SHOWBLOCK

  int ii;
  for (ii=0; ii < 64; ii++) {
    fprintf(stdout, "%d ", data[RTjpeg_ZZ[ii]]);
  }
  fprintf(stdout, "\n\n");

#endif

 (__u8)strm[0]=(__u8)(data[RTjpeg_ZZ[0]]>254) ? 254:((data[RTjpeg_ZZ[0]]<0)?0:data[RTjpeg_ZZ[0]]);

 for(ci=1; ci<=bt8; ci++)
 {
	ZZvalue = data[RTjpeg_ZZ[ci]];

   if(ZZvalue>0)
	{
     strm[co++]=(__s8)(ZZvalue>127)?127:ZZvalue;
   }
	else
	{
     strm[co++]=(__s8)(ZZvalue<-128)?-128:ZZvalue;
   }
 }

 for(; ci<64; ci++)
 {
  ZZvalue = data[RTjpeg_ZZ[ci]];

  if(ZZvalue>0)
  {
   strm[co++]=(__s8)(ZZvalue>63)?63:ZZvalue;
  }
  else if(ZZvalue<0)
  {
   strm[co++]=(__s8)(ZZvalue<-64)?-64:ZZvalue;
  }
  else /* compress zeros */
  {
   tmp=ci;
   do
   {
    ci++;
   }
	while((ci<64)&&(data[RTjpeg_ZZ[ci]]==0));

   strm[co++]=(__s8)(63+(ci-tmp));
   ci--;
  }
 }
 return (int)co;
}

static int RTjpeg_s2b(__s16 *data, __s8 *strm, __u8 bt8, __u32 *qtbl)
{
 int ci=1, co=1, tmp;
 register int i;

 i=RTjpeg_ZZ[0];
 data[i]=((__u8)strm[0])*qtbl[i];

 for(co=1; co<=bt8; co++)
 {
  i=RTjpeg_ZZ[co];
  data[i]=strm[ci++]*qtbl[i];
 }

 for(; co<64; co++)
 {
  if(strm[ci]>63)
  {
   tmp=co+strm[ci]-63;
   for(; co<tmp; co++)data[RTjpeg_ZZ[co]]=0;
   co--;
  } else
  {
   i=RTjpeg_ZZ[co];
   data[i]=strm[ci]*qtbl[i];
  }
  ci++;
 }
 return (int)ci;
}
#endif

#if HAVE_MMX
static void RTjpeg_quant_init(void)
{
 int i;
 __s16 *qtbl;

 qtbl=(__s16 *)RTjpeg_lqt;
 for(i=0; i<64; i++)qtbl[i]=(__s16)RTjpeg_lqt[i];

 qtbl=(__s16 *)RTjpeg_cqt;
 for(i=0; i<64; i++)qtbl[i]=(__s16)RTjpeg_cqt[i];
}

static mmx_t RTjpeg_ones={0x0001000100010001LL};
static mmx_t RTjpeg_half={0x7fff7fff7fff7fffLL};

static void RTjpeg_quant(__s16 *block, __s32 *qtbl)
{
 int i;
 mmx_t *bl, *ql;

 ql=(mmx_t *)qtbl;
 bl=(mmx_t *)block;

 movq_m2r(RTjpeg_ones, mm6);
 movq_m2r(RTjpeg_half, mm7);

 for(i=16; i; i--)
 {
  movq_m2r(*(ql++), mm0); /* quant vals (4) */
  movq_m2r(*bl, mm2); /* block vals (4) */
  movq_r2r(mm0, mm1);
  movq_r2r(mm2, mm3);

  punpcklwd_r2r(mm6, mm0); /*           1 qb 1 qa */
  punpckhwd_r2r(mm6, mm1); /* 1 qd 1 qc */

  punpcklwd_r2r(mm7, mm2); /*                   32767 bb 32767 ba */
  punpckhwd_r2r(mm7, mm3); /* 32767 bd 32767 bc */

  pmaddwd_r2r(mm2, mm0); /*                         32767+bb*qb 32767+ba*qa */
  pmaddwd_r2r(mm3, mm1); /* 32767+bd*qd 32767+bc*qc */

  psrad_i2r(16, mm0);
  psrad_i2r(16, mm1);

  packssdw_r2r(mm1, mm0);

  movq_r2m(mm0, *(bl++));

 }
}
#else
static void RTjpeg_quant_init(void)
{
}

static void RTjpeg_quant(__s16 *block, __s32 *qtbl)
{
 int i;

 for(i=0; i<64; i++)
   block[i]=(__s16)((block[i]*qtbl[i]+32767)>>16);
}
#endif

/*
 * Perform the forward DCT on one block of samples.
 */
#if HAVE_MMX
static mmx_t RTjpeg_C4   ={0x2D412D412D412D41LL};
static mmx_t RTjpeg_C6   ={0x187E187E187E187ELL};
static mmx_t RTjpeg_C2mC6={0x22A322A322A322A3LL};
static mmx_t RTjpeg_C2pC6={0x539F539F539F539FLL};
static mmx_t RTjpeg_zero ={0x0000000000000000LL};

#else

#define FIX_0_382683433  ((__s32)   98)		/* FIX(0.382683433) */
#define FIX_0_541196100  ((__s32)  139)		/* FIX(0.541196100) */
#define FIX_0_707106781  ((__s32)  181)		/* FIX(0.707106781) */
#define FIX_1_306562965  ((__s32)  334)		/* FIX(1.306562965) */

#define DESCALE10(x) (__s16)( ((x)+128) >> 8)
#define DESCALE20(x)  (__s16)(((x)+32768) >> 16)
#define D_MULTIPLY(var,const)  ((__s32) ((var) * (const)))
#endif

static void RTjpeg_dct_init(void)
{
 int i;

 for(i=0; i<64; i++)
 {
  RTjpeg_lqt[i]=(((__u64)RTjpeg_lqt[i]<<32)/RTjpeg_aan_tab[i]);
  RTjpeg_cqt[i]=(((__u64)RTjpeg_cqt[i]<<32)/RTjpeg_aan_tab[i]);
 }
}

static void RTjpeg_dctY(__u8 *idata, __s16 *odata, int rskip)
{
#if !HAVE_MMX
  __s32 tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;
  __s32 tmp10, tmp11, tmp12, tmp13;
  __s32 z1, z2, z3, z4, z5, z11, z13;
  __u8 *idataptr;
  __s16 *odataptr;
  __s32 *wsptr;
  int ctr;

  idataptr = idata;
  wsptr = RTjpeg_ws;
  for (ctr = 7; ctr >= 0; ctr--) {
    tmp0 = idataptr[0] + idataptr[7];
    tmp7 = idataptr[0] - idataptr[7];
    tmp1 = idataptr[1] + idataptr[6];
    tmp6 = idataptr[1] - idataptr[6];
    tmp2 = idataptr[2] + idataptr[5];
    tmp5 = idataptr[2] - idataptr[5];
    tmp3 = idataptr[3] + idataptr[4];
    tmp4 = idataptr[3] - idataptr[4];

    tmp10 = (tmp0 + tmp3);	/* phase 2 */
    tmp13 = tmp0 - tmp3;
    tmp11 = (tmp1 + tmp2);
    tmp12 = tmp1 - tmp2;

    wsptr[0] = (tmp10 + tmp11)<<8; /* phase 3 */
    wsptr[4] = (tmp10 - tmp11)<<8;

    z1 = D_MULTIPLY(tmp12 + tmp13, FIX_0_707106781); /* c4 */
    wsptr[2] = (tmp13<<8) + z1;	/* phase 5 */
    wsptr[6] = (tmp13<<8) - z1;

    tmp10 = tmp4 + tmp5;	/* phase 2 */
    tmp11 = tmp5 + tmp6;
    tmp12 = tmp6 + tmp7;

    z5 = D_MULTIPLY(tmp10 - tmp12, FIX_0_382683433); /* c6 */
    z2 = D_MULTIPLY(tmp10, FIX_0_541196100) + z5; /* c2-c6 */
    z4 = D_MULTIPLY(tmp12, FIX_1_306562965) + z5; /* c2+c6 */
    z3 = D_MULTIPLY(tmp11, FIX_0_707106781); /* c4 */

    z11 = (tmp7<<8) + z3;		/* phase 5 */
    z13 = (tmp7<<8) - z3;

    wsptr[5] = z13 + z2;	/* phase 6 */
    wsptr[3] = z13 - z2;
    wsptr[1] = z11 + z4;
    wsptr[7] = z11 - z4;

    idataptr += rskip<<3;		/* advance pointer to next row */
    wsptr += 8;
  }

  wsptr = RTjpeg_ws;
  odataptr=odata;
  for (ctr = 7; ctr >= 0; ctr--) {
    tmp0 = wsptr[0] + wsptr[56];
    tmp7 = wsptr[0] - wsptr[56];
    tmp1 = wsptr[8] + wsptr[48];
    tmp6 = wsptr[8] - wsptr[48];
    tmp2 = wsptr[16] + wsptr[40];
    tmp5 = wsptr[16] - wsptr[40];
    tmp3 = wsptr[24] + wsptr[32];
    tmp4 = wsptr[24] - wsptr[32];

    tmp10 = tmp0 + tmp3;	/* phase 2 */
    tmp13 = tmp0 - tmp3;
    tmp11 = tmp1 + tmp2;
    tmp12 = tmp1 - tmp2;

    odataptr[0] = DESCALE10(tmp10 + tmp11); /* phase 3 */
    odataptr[32] = DESCALE10(tmp10 - tmp11);

    z1 = D_MULTIPLY(tmp12 + tmp13, FIX_0_707106781); /* c4 */
    odataptr[16] = DESCALE20((tmp13<<8) + z1); /* phase 5 */
    odataptr[48] = DESCALE20((tmp13<<8) - z1);

    tmp10 = tmp4 + tmp5;	/* phase 2 */
    tmp11 = tmp5 + tmp6;
    tmp12 = tmp6 + tmp7;

    z5 = D_MULTIPLY(tmp10 - tmp12, FIX_0_382683433); /* c6 */
    z2 = D_MULTIPLY(tmp10, FIX_0_541196100) + z5; /* c2-c6 */
    z4 = D_MULTIPLY(tmp12, FIX_1_306562965) + z5; /* c2+c6 */
    z3 = D_MULTIPLY(tmp11, FIX_0_707106781); /* c4 */

    z11 = (tmp7<<8) + z3;		/* phase 5 */
    z13 = (tmp7<<8) - z3;

    odataptr[40] = DESCALE20(z13 + z2); /* phase 6 */
    odataptr[24] = DESCALE20(z13 - z2);
    odataptr[8] = DESCALE20(z11 + z4);
    odataptr[56] = DESCALE20(z11 - z4);

    odataptr++;			/* advance pointer to next column */
    wsptr++;
  }
#else
  volatile mmx_t tmp6, tmp7;
  register mmx_t *dataptr = (mmx_t *)odata;
  mmx_t *idata2 = (mmx_t *)idata;

   // first copy the input 8 bit to the destination 16 bits

   movq_m2r(RTjpeg_zero, mm2);


	movq_m2r(*idata2, mm0);
	movq_r2r(mm0, mm1);

	punpcklbw_r2r(mm2, mm0);
	movq_r2m(mm0, *(dataptr));

	punpckhbw_r2r(mm2, mm1);
	movq_r2m(mm1, *(dataptr+1));

	idata2 += rskip;

	movq_m2r(*idata2, mm0);
	movq_r2r(mm0, mm1);

	punpcklbw_r2r(mm2, mm0);
	movq_r2m(mm0, *(dataptr+2));

	punpckhbw_r2r(mm2, mm1);
	movq_r2m(mm1, *(dataptr+3));

	idata2 += rskip;

	movq_m2r(*idata2, mm0);
	movq_r2r(mm0, mm1);

	punpcklbw_r2r(mm2, mm0);
	movq_r2m(mm0, *(dataptr+4));

	punpckhbw_r2r(mm2, mm1);
	movq_r2m(mm1, *(dataptr+5));

	idata2 += rskip;

	movq_m2r(*idata2, mm0);
	movq_r2r(mm0, mm1);

	punpcklbw_r2r(mm2, mm0);
	movq_r2m(mm0, *(dataptr+6));

	punpckhbw_r2r(mm2, mm1);
	movq_r2m(mm1, *(dataptr+7));

	idata2 += rskip;

	movq_m2r(*idata2, mm0);
	movq_r2r(mm0, mm1);

	punpcklbw_r2r(mm2, mm0);
	movq_r2m(mm0, *(dataptr+8));

	punpckhbw_r2r(mm2, mm1);
	movq_r2m(mm1, *(dataptr+9));

	idata2 += rskip;

	movq_m2r(*idata2, mm0);
	movq_r2r(mm0, mm1);

	punpcklbw_r2r(mm2, mm0);
	movq_r2m(mm0, *(dataptr+10));

	punpckhbw_r2r(mm2, mm1);
	movq_r2m(mm1, *(dataptr+11));

	idata2 += rskip;

	movq_m2r(*idata2, mm0);
	movq_r2r(mm0, mm1);

	punpcklbw_r2r(mm2, mm0);
	movq_r2m(mm0, *(dataptr+12));

	punpckhbw_r2r(mm2, mm1);
	movq_r2m(mm1, *(dataptr+13));

	idata2 += rskip;

	movq_m2r(*idata2, mm0);
	movq_r2r(mm0, mm1);

	punpcklbw_r2r(mm2, mm0);
	movq_r2m(mm0, *(dataptr+14));

	punpckhbw_r2r(mm2, mm1);
	movq_r2m(mm1, *(dataptr+15));

/*  Start Transpose to do calculations on rows */

	movq_m2r(*(dataptr+9), mm7);		 	// m03:m02|m01:m00 - first line (line 4)and copy into m5

	movq_m2r(*(dataptr+13), mm6);	    	// m23:m22|m21:m20 - third line (line 6)and copy into m2
	movq_r2r(mm7, mm5);

	punpcklwd_m2r(*(dataptr+11), mm7); 	// m11:m01|m10:m00 - interleave first and second lines
	movq_r2r(mm6, mm2);

	punpcklwd_m2r(*(dataptr+15), mm6);  // m31:m21|m30:m20 - interleave third and fourth lines
	movq_r2r(mm7, mm1);

	movq_m2r(*(dataptr+11), mm3);	      // m13:m13|m11:m10 - second line
	punpckldq_r2r(mm6, mm7);				// m30:m20|m10:m00 - interleave to produce result 1

	movq_m2r(*(dataptr+15), mm0);	      // m13:m13|m11:m10 - fourth line
	punpckhdq_r2r(mm6, mm1);				// m31:m21|m11:m01 - interleave to produce result 2

	movq_r2m(mm7,*(dataptr+9));			// write result 1
	punpckhwd_r2r(mm3, mm5);				// m13:m03|m12:m02 - interleave first and second lines

	movq_r2m(mm1,*(dataptr+11));			// write result 2
	punpckhwd_r2r(mm0, mm2);				// m33:m23|m32:m22 - interleave third and fourth lines

	movq_r2r(mm5, mm1);
	punpckldq_r2r(mm2, mm5);				// m32:m22|m12:m02 - interleave to produce result 3

	movq_m2r(*(dataptr+1), mm0);			// m03:m02|m01:m00 - first line, 4x4
	punpckhdq_r2r(mm2, mm1);				// m33:m23|m13:m03 - interleave to produce result 4

	movq_r2m(mm5,*(dataptr+13));			// write result 3

	// last 4x4 done

	movq_r2m(mm1, *(dataptr+15));			// write result 4, last 4x4

	movq_m2r(*(dataptr+5), mm2);			// m23:m22|m21:m20 - third line
	movq_r2r(mm0, mm6);

	punpcklwd_m2r(*(dataptr+3), mm0);  	// m11:m01|m10:m00 - interleave first and second lines
	movq_r2r(mm2, mm7);

	punpcklwd_m2r(*(dataptr+7), mm2);  	// m31:m21|m30:m20 - interleave third and fourth lines
	movq_r2r(mm0, mm4);

	//
	movq_m2r(*(dataptr+8), mm1);			// n03:n02|n01:n00 - first line
	punpckldq_r2r(mm2, mm0);				// m30:m20|m10:m00 - interleave to produce first result

	movq_m2r(*(dataptr+12), mm3);			// n23:n22|n21:n20 - third line
	punpckhdq_r2r(mm2, mm4);				// m31:m21|m11:m01 - interleave to produce second result

	punpckhwd_m2r(*(dataptr+3), mm6);  	// m13:m03|m12:m02 - interleave first and second lines
	movq_r2r(mm1, mm2);               	// copy first line

	punpckhwd_m2r(*(dataptr+7), mm7);  	// m33:m23|m32:m22 - interleave third and fourth lines
	movq_r2r(mm6, mm5);						// copy first intermediate result

	movq_r2m(mm0, *(dataptr+8));			// write result 1
	punpckhdq_r2r(mm7, mm5);				// m33:m23|m13:m03 - produce third result

	punpcklwd_m2r(*(dataptr+10), mm1);  // n11:n01|n10:n00 - interleave first and second lines
	movq_r2r(mm3, mm0);						// copy third line

	punpckhwd_m2r(*(dataptr+10), mm2);  // n13:n03|n12:n02 - interleave first and second lines

	movq_r2m(mm4, *(dataptr+10));			// write result 2 out
	punpckldq_r2r(mm7, mm6);				// m32:m22|m12:m02 - produce fourth result

	punpcklwd_m2r(*(dataptr+14), mm3);  // n31:n21|n30:n20 - interleave third and fourth lines
	movq_r2r(mm1, mm4);

	movq_r2m(mm6, *(dataptr+12));			// write result 3 out
	punpckldq_r2r(mm3, mm1);				// n30:n20|n10:n00 - produce first result

	punpckhwd_m2r(*(dataptr+14), mm0);  // n33:n23|n32:n22 - interleave third and fourth lines
	movq_r2r(mm2, mm6);

	movq_r2m(mm5, *(dataptr+14));			// write result 4 out
	punpckhdq_r2r(mm3, mm4);				// n31:n21|n11:n01- produce second result

	movq_r2m(mm1, *(dataptr+1));			// write result 5 out - (first result for other 4 x 4 block)
	punpckldq_r2r(mm0, mm2);				// n32:n22|n12:n02- produce third result

	movq_r2m(mm4, *(dataptr+3));			// write result 6 out
	punpckhdq_r2r(mm0, mm6);				// n33:n23|n13:n03 - produce fourth result

	movq_r2m(mm2, *(dataptr+5));			// write result 7 out

	movq_m2r(*dataptr, mm0);				// m03:m02|m01:m00 - first line, first 4x4

	movq_r2m(mm6, *(dataptr+7));			// write result 8 out


// Do first 4x4 quadrant, which is used in the beginning of the DCT:

	movq_m2r(*(dataptr+4), mm7);			// m23:m22|m21:m20 - third line
	movq_r2r(mm0, mm2);

	punpcklwd_m2r(*(dataptr+2), mm0);  	// m11:m01|m10:m00 - interleave first and second lines
	movq_r2r(mm7, mm4);

	punpcklwd_m2r(*(dataptr+6), mm7);  	// m31:m21|m30:m20 - interleave third and fourth lines
	movq_r2r(mm0, mm1);

	movq_m2r(*(dataptr+2), mm6);			// m13:m12|m11:m10 - second line
	punpckldq_r2r(mm7, mm0);				// m30:m20|m10:m00 - interleave to produce result 1

	movq_m2r(*(dataptr+6), mm5);			// m33:m32|m31:m30 - fourth line
	punpckhdq_r2r(mm7, mm1);				// m31:m21|m11:m01 - interleave to produce result 2

	movq_r2r(mm0, mm7);						// write result 1
	punpckhwd_r2r(mm6, mm2);				// m13:m03|m12:m02 - interleave first and second lines

	psubw_m2r(*(dataptr+14), mm7);		// tmp07=x0-x7	/* Stage 1 */
	movq_r2r(mm1, mm6);						// write result 2

	paddw_m2r(*(dataptr+14), mm0);		// tmp00=x0+x7	/* Stage 1 */
	punpckhwd_r2r(mm5, mm4);   			// m33:m23|m32:m22 - interleave third and fourth lines

	paddw_m2r(*(dataptr+12), mm1);		// tmp01=x1+x6	/* Stage 1 */
	movq_r2r(mm2, mm3);						// copy first intermediate result

	psubw_m2r(*(dataptr+12), mm6);		// tmp06=x1-x6	/* Stage 1 */
	punpckldq_r2r(mm4, mm2);				// m32:m22|m12:m02 - interleave to produce result 3

   movq_r2m(mm7, tmp7);
	movq_r2r(mm2, mm5);						// write result 3

   movq_r2m(mm6, tmp6);
	punpckhdq_r2r(mm4, mm3);				// m33:m23|m13:m03 - interleave to produce result 4

	paddw_m2r(*(dataptr+10), mm2);     	// tmp02=x2+5 /* Stage 1 */
	movq_r2r(mm3, mm4);						// write result 4

/************************************************************************************************
					End of Transpose
************************************************************************************************/


   paddw_m2r(*(dataptr+8), mm3);    	// tmp03=x3+x4 /* stage 1*/
   movq_r2r(mm0, mm7);

   psubw_m2r(*(dataptr+8), mm4);    	// tmp04=x3-x4 /* stage 1*/
   movq_r2r(mm1, mm6);

	paddw_r2r(mm3, mm0);  					// tmp10 = tmp00 + tmp03 /* even 2 */
	psubw_r2r(mm3, mm7);  					// tmp13 = tmp00 - tmp03 /* even 2 */

	psubw_r2r(mm2, mm6);  					// tmp12 = tmp01 - tmp02 /* even 2 */
	paddw_r2r(mm2, mm1);  					// tmp11 = tmp01 + tmp02 /* even 2 */

   psubw_m2r(*(dataptr+10), mm5);    	// tmp05=x2-x5 /* stage 1*/
	paddw_r2r(mm7, mm6);						// tmp12 + tmp13

	/* stage 3 */

   movq_m2r(tmp6, mm2);
   movq_r2r(mm0, mm3);

	psllw_i2r(2, mm6);			// m8 * 2^2
	paddw_r2r(mm1, mm0);

	pmulhw_m2r(RTjpeg_C4, mm6);			// z1
	psubw_r2r(mm1, mm3);

   movq_r2m(mm0, *dataptr);
   movq_r2r(mm7, mm0);

    /* Odd part */
   movq_r2m(mm3, *(dataptr+8));
	paddw_r2r(mm5, mm4);						// tmp10

   movq_m2r(tmp7, mm3);
	paddw_r2r(mm6, mm0);						// tmp32

	paddw_r2r(mm2, mm5);						// tmp11
	psubw_r2r(mm6, mm7);						// tmp33

   movq_r2m(mm0, *(dataptr+4));
	paddw_r2r(mm3, mm2);						// tmp12

	/* stage 4 */

   movq_r2m(mm7, *(dataptr+12));
	movq_r2r(mm4, mm1);						// copy of tmp10

	psubw_r2r(mm2, mm1);						// tmp10 - tmp12
	psllw_i2r(2, mm4);			// m8 * 2^2

	movq_m2r(RTjpeg_C2mC6, mm0);
	psllw_i2r(2, mm1);

	pmulhw_m2r(RTjpeg_C6, mm1);			// z5
	psllw_i2r(2, mm2);

	pmulhw_r2r(mm0, mm4);					// z5

	/* stage 5 */

	pmulhw_m2r(RTjpeg_C2pC6, mm2);
	psllw_i2r(2, mm5);

	pmulhw_m2r(RTjpeg_C4, mm5);			// z3
	movq_r2r(mm3, mm0);						// copy tmp7

   movq_m2r(*(dataptr+1), mm7);
	paddw_r2r(mm1, mm4);						// z2

	paddw_r2r(mm1, mm2);						// z4

	paddw_r2r(mm5, mm0);						// z11
	psubw_r2r(mm5, mm3);						// z13

	/* stage 6 */

	movq_r2r(mm3, mm5);						// copy z13
	psubw_r2r(mm4, mm3);						// y3=z13 - z2

	paddw_r2r(mm4, mm5);						// y5=z13 + z2
	movq_r2r(mm0, mm6);						// copy z11

   movq_r2m(mm3, *(dataptr+6)); 			//save y3
	psubw_r2r(mm2, mm0);						// y7=z11 - z4

   movq_r2m(mm5, *(dataptr+10)); 		//save y5
	paddw_r2r(mm2, mm6);						// y1=z11 + z4

   movq_r2m(mm0, *(dataptr+14)); 		//save y7

	/************************************************
	 *  End of 1st 4 rows
	 ************************************************/

   movq_m2r(*(dataptr+3), mm1); 			// load x1   /* stage 1 */
	movq_r2r(mm7, mm0);						// copy x0

   movq_r2m(mm6, *(dataptr+2)); 			//save y1

   movq_m2r(*(dataptr+5), mm2); 			// load x2   /* stage 1 */
	movq_r2r(mm1, mm6);						// copy x1

   paddw_m2r(*(dataptr+15), mm0);  		// tmp00 = x0 + x7

   movq_m2r(*(dataptr+7), mm3); 			// load x3   /* stage 1 */
	movq_r2r(mm2, mm5);						// copy x2

   psubw_m2r(*(dataptr+15), mm7);  		// tmp07 = x0 - x7
	movq_r2r(mm3, mm4);						// copy x3

   paddw_m2r(*(dataptr+13), mm1);  		// tmp01 = x1 + x6

	movq_r2m(mm7, tmp7);						// save tmp07
	movq_r2r(mm0, mm7);						// copy tmp00

   psubw_m2r(*(dataptr+13), mm6);  		// tmp06 = x1 - x6

   /* stage 2, Even Part */

   paddw_m2r(*(dataptr+9), mm3);  		// tmp03 = x3 + x4

	movq_r2m(mm6, tmp6);						// save tmp07
	movq_r2r(mm1, mm6);						// copy tmp01

   paddw_m2r(*(dataptr+11), mm2);  		// tmp02 = x2 + x5
	paddw_r2r(mm3, mm0);              	// tmp10 = tmp00 + tmp03

	psubw_r2r(mm3, mm7);              	// tmp13 = tmp00 - tmp03

   psubw_m2r(*(dataptr+9), mm4);  		// tmp04 = x3 - x4
	psubw_r2r(mm2, mm6);              	// tmp12 = tmp01 - tmp02

	paddw_r2r(mm2, mm1);              	// tmp11 = tmp01 + tmp02

   psubw_m2r(*(dataptr+11), mm5);  		// tmp05 = x2 - x5
	paddw_r2r(mm7, mm6);              	//  tmp12 + tmp13

   /* stage 3, Even and stage 4 & 5 even */

	movq_m2r(tmp6, mm2);            		// load tmp6
	movq_r2r(mm0, mm3);						// copy tmp10

	psllw_i2r(2, mm6);			// shift z1
	paddw_r2r(mm1, mm0);    				// y0=tmp10 + tmp11

	pmulhw_m2r(RTjpeg_C4, mm6);    		// z1
	psubw_r2r(mm1, mm3);    				// y4=tmp10 - tmp11

   movq_r2m(mm0, *(dataptr+1)); 			//save y0
	movq_r2r(mm7, mm0);						// copy tmp13

	/* odd part */

   movq_r2m(mm3, *(dataptr+9)); 			//save y4
	paddw_r2r(mm5, mm4);              	// tmp10 = tmp4 + tmp5

	movq_m2r(tmp7, mm3);            		// load tmp7
	paddw_r2r(mm6, mm0);              	// tmp32 = tmp13 + z1

	paddw_r2r(mm2, mm5);              	// tmp11 = tmp5 + tmp6
	psubw_r2r(mm6, mm7);              	// tmp33 = tmp13 - z1

   movq_r2m(mm0, *(dataptr+5)); 			//save y2
	paddw_r2r(mm3, mm2);              	// tmp12 = tmp6 + tmp7

	/* stage 4 */

   movq_r2m(mm7, *(dataptr+13)); 		//save y6
	movq_r2r(mm4, mm1);						// copy tmp10

	psubw_r2r(mm2, mm1);    				// tmp10 - tmp12
	psllw_i2r(2, mm4);			// shift tmp10

	movq_m2r(RTjpeg_C2mC6, mm0);			// load C2mC6
	psllw_i2r(2, mm1);			// shift (tmp10-tmp12)

	pmulhw_m2r(RTjpeg_C6, mm1);    		// z5
	psllw_i2r(2, mm5);			// prepare for multiply

	pmulhw_r2r(mm0, mm4);					// multiply by converted real

	/* stage 5 */

	pmulhw_m2r(RTjpeg_C4, mm5);			// z3
	psllw_i2r(2, mm2);			// prepare for multiply

	pmulhw_m2r(RTjpeg_C2pC6, mm2);		// multiply
	movq_r2r(mm3, mm0);						// copy tmp7

	movq_m2r(*(dataptr+9), mm7);		 	// m03:m02|m01:m00 - first line (line 4)and copy into mm7
	paddw_r2r(mm1, mm4);						// z2

	paddw_r2r(mm5, mm0);						// z11
	psubw_r2r(mm5, mm3);						// z13

	/* stage 6 */

	movq_r2r(mm3, mm5);						// copy z13
	paddw_r2r(mm1, mm2);						// z4

	movq_r2r(mm0, mm6);						// copy z11
	psubw_r2r(mm4, mm5);						// y3

	paddw_r2r(mm2, mm6);						// y1
	paddw_r2r(mm4, mm3);						// y5

   movq_r2m(mm5, *(dataptr+7)); 			//save y3

   movq_r2m(mm6, *(dataptr+3)); 			//save y1
	psubw_r2r(mm2, mm0);						// y7

/************************************************************************************************
					Start of Transpose
************************************************************************************************/

 	movq_m2r(*(dataptr+13), mm6);		 	// m23:m22|m21:m20 - third line (line 6)and copy into m2
	movq_r2r(mm7, mm5);						// copy first line

	punpcklwd_r2r(mm3, mm7); 				// m11:m01|m10:m00 - interleave first and second lines
	movq_r2r(mm6, mm2);						// copy third line

	punpcklwd_r2r(mm0, mm6);  				// m31:m21|m30:m20 - interleave third and fourth lines
	movq_r2r(mm7, mm1);						// copy first intermediate result

	punpckldq_r2r(mm6, mm7);				// m30:m20|m10:m00 - interleave to produce result 1

	punpckhdq_r2r(mm6, mm1);				// m31:m21|m11:m01 - interleave to produce result 2

	movq_r2m(mm7, *(dataptr+9));			// write result 1
	punpckhwd_r2r(mm3, mm5);				// m13:m03|m12:m02 - interleave first and second lines

	movq_r2m(mm1, *(dataptr+11));			// write result 2
	punpckhwd_r2r(mm0, mm2);				// m33:m23|m32:m22 - interleave third and fourth lines

	movq_r2r(mm5, mm1);						// copy first intermediate result
	punpckldq_r2r(mm2, mm5);				// m32:m22|m12:m02 - interleave to produce result 3

	movq_m2r(*(dataptr+1), mm0);			// m03:m02|m01:m00 - first line, 4x4
	punpckhdq_r2r(mm2, mm1);				// m33:m23|m13:m03 - interleave to produce result 4

	movq_r2m(mm5, *(dataptr+13));			// write result 3

	/****** last 4x4 done */

	movq_r2m(mm1, *(dataptr+15));			// write result 4, last 4x4

	movq_m2r(*(dataptr+5), mm2);			// m23:m22|m21:m20 - third line
	movq_r2r(mm0, mm6);						// copy first line

	punpcklwd_m2r(*(dataptr+3), mm0);  	// m11:m01|m10:m00 - interleave first and second lines
	movq_r2r(mm2, mm7);						// copy third line

	punpcklwd_m2r(*(dataptr+7), mm2);  	// m31:m21|m30:m20 - interleave third and fourth lines
	movq_r2r(mm0, mm4);						// copy first intermediate result



	movq_m2r(*(dataptr+8), mm1);			// n03:n02|n01:n00 - first line
	punpckldq_r2r(mm2, mm0);				// m30:m20|m10:m00 - interleave to produce first result

	movq_m2r(*(dataptr+12), mm3);			// n23:n22|n21:n20 - third line
	punpckhdq_r2r(mm2, mm4);				// m31:m21|m11:m01 - interleave to produce second result

	punpckhwd_m2r(*(dataptr+3), mm6);  	// m13:m03|m12:m02 - interleave first and second lines
	movq_r2r(mm1, mm2);						// copy first line

	punpckhwd_m2r(*(dataptr+7), mm7);  	// m33:m23|m32:m22 - interleave third and fourth lines
	movq_r2r(mm6, mm5);						// copy first intermediate result

	movq_r2m(mm0, *(dataptr+8));			// write result 1
	punpckhdq_r2r(mm7, mm5);				// m33:m23|m13:m03 - produce third result

	punpcklwd_m2r(*(dataptr+10), mm1);  // n11:n01|n10:n00 - interleave first and second lines
	movq_r2r(mm3, mm0);						// copy third line

	punpckhwd_m2r(*(dataptr+10), mm2);  // n13:n03|n12:n02 - interleave first and second lines

	movq_r2m(mm4, *(dataptr+10));			// write result 2 out
	punpckldq_r2r(mm7, mm6);				// m32:m22|m12:m02 - produce fourth result

	punpcklwd_m2r(*(dataptr+14), mm3);  // n33:n23|n32:n22 - interleave third and fourth lines
	movq_r2r(mm1, mm4);						// copy second intermediate result

	movq_r2m(mm6, *(dataptr+12));			// write result 3 out
	punpckldq_r2r(mm3, mm1);				//

	punpckhwd_m2r(*(dataptr+14), mm0);  // n33:n23|n32:n22 - interleave third and fourth lines
	movq_r2r(mm2, mm6);						// copy second intermediate result

	movq_r2m(mm5, *(dataptr+14));			// write result 4 out
	punpckhdq_r2r(mm3, mm4);				// n31:n21|n11:n01- produce second result

	movq_r2m(mm1, *(dataptr+1));			// write result 5 out - (first result for other 4 x 4 block)
	punpckldq_r2r(mm0, mm2);				// n32:n22|n12:n02- produce third result

	movq_r2m(mm4, *(dataptr+3));			// write result 6 out
	punpckhdq_r2r(mm0, mm6);				// n33:n23|n13:n03 - produce fourth result

	movq_r2m(mm2, *(dataptr+5));			// write result 7 out

	movq_m2r(*dataptr, mm0);				// m03:m02|m01:m00 - first line, first 4x4

	movq_r2m(mm6, *(dataptr+7));			// write result 8 out

// Do first 4x4 quadrant, which is used in the beginning of the DCT:

	movq_m2r(*(dataptr+4), mm7);			// m23:m22|m21:m20 - third line
	movq_r2r(mm0, mm2);						// copy first line

	punpcklwd_m2r(*(dataptr+2), mm0);  	// m11:m01|m10:m00 - interleave first and second lines
	movq_r2r(mm7, mm4);						// copy third line

	punpcklwd_m2r(*(dataptr+6), mm7);  	// m31:m21|m30:m20 - interleave third and fourth lines
	movq_r2r(mm0, mm1);						// copy first intermediate result

	movq_m2r(*(dataptr+2), mm6);			// m13:m12|m11:m10 - second line
	punpckldq_r2r(mm7, mm0);				// m30:m20|m10:m00 - interleave to produce result 1

	movq_m2r(*(dataptr+6), mm5);			// m33:m32|m31:m30 - fourth line
	punpckhdq_r2r(mm7, mm1);				// m31:m21|m11:m01 - interleave to produce result 2

	movq_r2r(mm0, mm7);						// write result 1
	punpckhwd_r2r(mm6, mm2);				// m13:m03|m12:m02 - interleave first and second lines

	psubw_m2r(*(dataptr+14), mm7);		// tmp07=x0-x7	/* Stage 1 */
	movq_r2r(mm1, mm6);						// write result 2

	paddw_m2r(*(dataptr+14), mm0);		// tmp00=x0+x7	/* Stage 1 */
	punpckhwd_r2r(mm5, mm4);   			// m33:m23|m32:m22 - interleave third and fourth lines

	paddw_m2r(*(dataptr+12), mm1);		// tmp01=x1+x6	/* Stage 1 */
	movq_r2r(mm2, mm3);						// copy first intermediate result

	psubw_m2r(*(dataptr+12), mm6);		// tmp06=x1-x6	/* Stage 1 */
	punpckldq_r2r(mm4, mm2);				// m32:m22|m12:m02 - interleave to produce result 3

	movq_r2m(mm7, tmp7);						// save tmp07
	movq_r2r(mm2, mm5);						// write result 3

	movq_r2m(mm6, tmp6);						// save tmp06

	punpckhdq_r2r(mm4, mm3);				// m33:m23|m13:m03 - interleave to produce result 4

	paddw_m2r(*(dataptr+10), mm2);   	// tmp02=x2+x5 /* stage 1 */
	movq_r2r(mm3, mm4);						// write result 4

/************************************************************************************************
					End of Transpose 2
************************************************************************************************/

   paddw_m2r(*(dataptr+8), mm3);    	// tmp03=x3+x4 /* stage 1*/
   movq_r2r(mm0, mm7);

   psubw_m2r(*(dataptr+8), mm4);    	// tmp04=x3-x4 /* stage 1*/
   movq_r2r(mm1, mm6);

	paddw_r2r(mm3, mm0);  					// tmp10 = tmp00 + tmp03 /* even 2 */
	psubw_r2r(mm3, mm7);  					// tmp13 = tmp00 - tmp03 /* even 2 */

	psubw_r2r(mm2, mm6);  					// tmp12 = tmp01 - tmp02 /* even 2 */
	paddw_r2r(mm2, mm1);  					// tmp11 = tmp01 + tmp02 /* even 2 */

   psubw_m2r(*(dataptr+10), mm5);    	// tmp05=x2-x5 /* stage 1*/
	paddw_r2r(mm7, mm6);						// tmp12 + tmp13

	/* stage 3 */

   movq_m2r(tmp6, mm2);
   movq_r2r(mm0, mm3);

	psllw_i2r(2, mm6);			// m8 * 2^2
	paddw_r2r(mm1, mm0);

	pmulhw_m2r(RTjpeg_C4, mm6);			// z1
	psubw_r2r(mm1, mm3);

   movq_r2m(mm0, *dataptr);
   movq_r2r(mm7, mm0);

    /* Odd part */
   movq_r2m(mm3, *(dataptr+8));
	paddw_r2r(mm5, mm4);						// tmp10

   movq_m2r(tmp7, mm3);
	paddw_r2r(mm6, mm0);						// tmp32

	paddw_r2r(mm2, mm5);						// tmp11
	psubw_r2r(mm6, mm7);						// tmp33

   movq_r2m(mm0, *(dataptr+4));
	paddw_r2r(mm3, mm2);						// tmp12

	/* stage 4 */
   movq_r2m(mm7, *(dataptr+12));
	movq_r2r(mm4, mm1);						// copy of tmp10

	psubw_r2r(mm2, mm1);						// tmp10 - tmp12
	psllw_i2r(2, mm4);			// m8 * 2^2

	movq_m2r(RTjpeg_C2mC6, mm0);
	psllw_i2r(2, mm1);

	pmulhw_m2r(RTjpeg_C6, mm1);			// z5
	psllw_i2r(2, mm2);

	pmulhw_r2r(mm0, mm4);					// z5

	/* stage 5 */

	pmulhw_m2r(RTjpeg_C2pC6, mm2);
	psllw_i2r(2, mm5);

	pmulhw_m2r(RTjpeg_C4, mm5);			// z3
	movq_r2r(mm3, mm0);						// copy tmp7

   movq_m2r(*(dataptr+1), mm7);
	paddw_r2r(mm1, mm4);						// z2

	paddw_r2r(mm1, mm2);						// z4

	paddw_r2r(mm5, mm0);						// z11
	psubw_r2r(mm5, mm3);						// z13

	/* stage 6 */

	movq_r2r(mm3, mm5);						// copy z13
	psubw_r2r(mm4, mm3);						// y3=z13 - z2

	paddw_r2r(mm4, mm5);						// y5=z13 + z2
	movq_r2r(mm0, mm6);						// copy z11

   movq_r2m(mm3, *(dataptr+6)); 			//save y3
	psubw_r2r(mm2, mm0);						// y7=z11 - z4

   movq_r2m(mm5, *(dataptr+10)); 		//save y5
	paddw_r2r(mm2, mm6);						// y1=z11 + z4

   movq_r2m(mm0, *(dataptr+14)); 		//save y7

	/************************************************
	 *  End of 1st 4 rows
	 ************************************************/

   movq_m2r(*(dataptr+3), mm1); 			// load x1   /* stage 1 */
	movq_r2r(mm7, mm0);						// copy x0

   movq_r2m(mm6, *(dataptr+2)); 			//save y1

   movq_m2r(*(dataptr+5), mm2); 			// load x2   /* stage 1 */
	movq_r2r(mm1, mm6);						// copy x1

   paddw_m2r(*(dataptr+15), mm0);  		// tmp00 = x0 + x7

   movq_m2r(*(dataptr+7), mm3); 			// load x3   /* stage 1 */
	movq_r2r(mm2, mm5);						// copy x2

   psubw_m2r(*(dataptr+15), mm7);  		// tmp07 = x0 - x7
	movq_r2r(mm3, mm4);						// copy x3

   paddw_m2r(*(dataptr+13), mm1);  		// tmp01 = x1 + x6

	movq_r2m(mm7, tmp7);						// save tmp07
	movq_r2r(mm0, mm7);						// copy tmp00

   psubw_m2r(*(dataptr+13), mm6);  		// tmp06 = x1 - x6

   /* stage 2, Even Part */

   paddw_m2r(*(dataptr+9), mm3);  		// tmp03 = x3 + x4

	movq_r2m(mm6, tmp6);						// save tmp07
	movq_r2r(mm1, mm6);						// copy tmp01

   paddw_m2r(*(dataptr+11), mm2);  		// tmp02 = x2 + x5
	paddw_r2r(mm3, mm0);              	// tmp10 = tmp00 + tmp03

	psubw_r2r(mm3, mm7);              	// tmp13 = tmp00 - tmp03

   psubw_m2r(*(dataptr+9), mm4);  		// tmp04 = x3 - x4
	psubw_r2r(mm2, mm6);              	// tmp12 = tmp01 - tmp02

	paddw_r2r(mm2, mm1);              	// tmp11 = tmp01 + tmp02

   psubw_m2r(*(dataptr+11), mm5);  		// tmp05 = x2 - x5
	paddw_r2r(mm7, mm6);              	//  tmp12 + tmp13

   /* stage 3, Even and stage 4 & 5 even */

	movq_m2r(tmp6, mm2);            		// load tmp6
	movq_r2r(mm0, mm3);						// copy tmp10

	psllw_i2r(2, mm6);			// shift z1
	paddw_r2r(mm1, mm0);    				// y0=tmp10 + tmp11

	pmulhw_m2r(RTjpeg_C4, mm6);    		// z1
	psubw_r2r(mm1, mm3);    				// y4=tmp10 - tmp11

   movq_r2m(mm0, *(dataptr+1)); 			//save y0
	movq_r2r(mm7, mm0);						// copy tmp13

	/* odd part */

   movq_r2m(mm3, *(dataptr+9)); 			//save y4
	paddw_r2r(mm5, mm4);              	// tmp10 = tmp4 + tmp5

	movq_m2r(tmp7, mm3);            		// load tmp7
	paddw_r2r(mm6, mm0);              	// tmp32 = tmp13 + z1

	paddw_r2r(mm2, mm5);              	// tmp11 = tmp5 + tmp6
	psubw_r2r(mm6, mm7);              	// tmp33 = tmp13 - z1

   movq_r2m(mm0, *(dataptr+5)); 			//save y2
	paddw_r2r(mm3, mm2);              	// tmp12 = tmp6 + tmp7

	/* stage 4 */

   movq_r2m(mm7, *(dataptr+13)); 		//save y6
	movq_r2r(mm4, mm1);						// copy tmp10

	psubw_r2r(mm2, mm1);    				// tmp10 - tmp12
	psllw_i2r(2, mm4);			// shift tmp10

	movq_m2r(RTjpeg_C2mC6, mm0);			// load C2mC6
	psllw_i2r(2, mm1);			// shift (tmp10-tmp12)

	pmulhw_m2r(RTjpeg_C6, mm1);    		// z5
	psllw_i2r(2, mm5);			// prepare for multiply

	pmulhw_r2r(mm0, mm4);					// multiply by converted real

	/* stage 5 */

	pmulhw_m2r(RTjpeg_C4, mm5);			// z3
	psllw_i2r(2, mm2);			// prepare for multiply

	pmulhw_m2r(RTjpeg_C2pC6, mm2);		// multiply
	movq_r2r(mm3, mm0);						// copy tmp7

	movq_m2r(*(dataptr+9), mm7);		 	// m03:m02|m01:m00 - first line (line 4)and copy into mm7
	paddw_r2r(mm1, mm4);						// z2

	paddw_r2r(mm5, mm0);						// z11
	psubw_r2r(mm5, mm3);						// z13

	/* stage 6 */

	movq_r2r(mm3, mm5);						// copy z13
	paddw_r2r(mm1, mm2);						// z4

	movq_r2r(mm0, mm6);						// copy z11
	psubw_r2r(mm4, mm5);						// y3

	paddw_r2r(mm2, mm6);						// y1
	paddw_r2r(mm4, mm3);						// y5

   movq_r2m(mm5, *(dataptr+7)); 			//save y3
	psubw_r2r(mm2, mm0);						// yè=z11 - z4

   movq_r2m(mm3, *(dataptr+11)); 		//save y5

   movq_r2m(mm6, *(dataptr+3)); 			//save y1

   movq_r2m(mm0, *(dataptr+15)); 		//save y7


#endif
}

/*

Main Routines

This file contains most of the initialisation and control functions

(C) Justin Schoeman 1998

*/

/*

Private function

Initialise all the cache-aliged data blocks

*/

static void RTjpeg_init_data(void)
{
 unsigned long dptr;

 dptr=(unsigned long)&(RTjpeg_alldata[0]);
 dptr+=32;
 dptr=dptr>>5;
 dptr=dptr<<5; /* cache align data */

 RTjpeg_block=(__s16 *)dptr;
 dptr+=sizeof(__s16)*64;
 RTjpeg_lqt=(__s32 *)dptr;
 dptr+=sizeof(__s32)*64;
 RTjpeg_cqt=(__s32 *)dptr;
 dptr+=sizeof(__s32)*64;
 RTjpeg_liqt=(__u32 *)dptr;
 dptr+=sizeof(__u32)*64;
 RTjpeg_ciqt=(__u32 *)dptr;
}

/*

External Function

Re-set quality factor

Input: buf -> pointer to 128 ints for quant values store to pass back to
              init_decompress.
       Q -> quality factor (192=best, 32=worst)
*/

static void RTjpeg_init_Q(__u8 Q)
{
 int i;
 __u64 qual;

 qual=(__u64)Q<<(32-7); /* 32 bit FP, 255=2, 0=0 */

 for(i=0; i<64; i++)
 {
  RTjpeg_lqt[i]=(__s32)((qual/((__u64)RTjpeg_lum_quant_tbl[i]<<16))>>3);
  if(RTjpeg_lqt[i]==0)RTjpeg_lqt[i]=1;
  RTjpeg_cqt[i]=(__s32)((qual/((__u64)RTjpeg_chrom_quant_tbl[i]<<16))>>3);
  if(RTjpeg_cqt[i]==0)RTjpeg_cqt[i]=1;
  RTjpeg_liqt[i]=(1<<16)/(RTjpeg_lqt[i]<<3);
  RTjpeg_ciqt[i]=(1<<16)/(RTjpeg_cqt[i]<<3);
  RTjpeg_lqt[i]=((1<<16)/RTjpeg_liqt[i])>>3;
  RTjpeg_cqt[i]=((1<<16)/RTjpeg_ciqt[i])>>3;
 }

 RTjpeg_lb8=0;
 while(RTjpeg_liqt[RTjpeg_ZZ[++RTjpeg_lb8]]<=8);
 RTjpeg_lb8--;
 RTjpeg_cb8=0;
 while(RTjpeg_ciqt[RTjpeg_ZZ[++RTjpeg_cb8]]<=8);
 RTjpeg_cb8--;

 RTjpeg_dct_init();
 RTjpeg_quant_init();
}

/*

External Function

Initialise compression.

Input: buf -> pointer to 128 ints for quant values store to pass back to
                init_decompress.
       width -> width of image
       height -> height of image
       Q -> quality factor (192=best, 32=worst)

*/

void RTjpeg_init_compress(__u32 *buf, int width, int height, __u8 Q)
{
 int i;
 __u64 qual;

 RTjpeg_init_data();

 RTjpeg_width=width;
 RTjpeg_height=height;
 RTjpeg_Ywidth = RTjpeg_width>>3;
 RTjpeg_Ysize=width * height;
 RTjpeg_Cwidth = RTjpeg_width>>4;
 RTjpeg_Csize= (width>>1) * height;

 qual=(__u64)Q<<(32-7); /* 32 bit FP, 255=2, 0=0 */

 for(i=0; i<64; i++)
 {
  RTjpeg_lqt[i]=(__s32)((qual/((__u64)RTjpeg_lum_quant_tbl[i]<<16))>>3);
  if(RTjpeg_lqt[i]==0)RTjpeg_lqt[i]=1;
  RTjpeg_cqt[i]=(__s32)((qual/((__u64)RTjpeg_chrom_quant_tbl[i]<<16))>>3);
  if(RTjpeg_cqt[i]==0)RTjpeg_cqt[i]=1;
  RTjpeg_liqt[i]=(1<<16)/(RTjpeg_lqt[i]<<3);
  RTjpeg_ciqt[i]=(1<<16)/(RTjpeg_cqt[i]<<3);
  RTjpeg_lqt[i]=((1<<16)/RTjpeg_liqt[i])>>3;
  RTjpeg_cqt[i]=((1<<16)/RTjpeg_ciqt[i])>>3;
 }

 RTjpeg_lb8=0;
 while(RTjpeg_liqt[RTjpeg_ZZ[++RTjpeg_lb8]]<=8);
 RTjpeg_lb8--;
 RTjpeg_cb8=0;
 while(RTjpeg_ciqt[RTjpeg_ZZ[++RTjpeg_cb8]]<=8);
 RTjpeg_cb8--;

 RTjpeg_dct_init();
 RTjpeg_quant_init();

 for(i=0; i<64; i++)
  buf[i]=le2me_32(RTjpeg_liqt[i]);
 for(i=0; i<64; i++)
  buf[64+i]=le2me_32(RTjpeg_ciqt[i]);
}

int RTjpeg_compressYUV420(__s8 *sp, unsigned char *bp)
{
 __s8 * sb;
 register __s8 * bp1 = bp + (RTjpeg_width<<3);
 register __s8 * bp2 = bp + RTjpeg_Ysize;
 register __s8 * bp3 = bp2 + (RTjpeg_Csize>>1);
 register int i, j, k;

#if HAVE_MMX
 emms();
#endif
 sb=sp;
/* Y */
 for(i=RTjpeg_height>>1; i; i-=8)
 {
  for(j=0, k=0; j<RTjpeg_width; j+=16, k+=8)
  {
   RTjpeg_dctY(bp+j, RTjpeg_block, RTjpeg_Ywidth);
   RTjpeg_quant(RTjpeg_block, RTjpeg_lqt);
   sp+=RTjpeg_b2s(RTjpeg_block, sp, RTjpeg_lb8);

   RTjpeg_dctY(bp+j+8, RTjpeg_block, RTjpeg_Ywidth);
   RTjpeg_quant(RTjpeg_block, RTjpeg_lqt);
   sp+=RTjpeg_b2s(RTjpeg_block, sp, RTjpeg_lb8);

   RTjpeg_dctY(bp1+j, RTjpeg_block, RTjpeg_Ywidth);
   RTjpeg_quant(RTjpeg_block, RTjpeg_lqt);
   sp+=RTjpeg_b2s(RTjpeg_block, sp, RTjpeg_lb8);

   RTjpeg_dctY(bp1+j+8, RTjpeg_block, RTjpeg_Ywidth);
   RTjpeg_quant(RTjpeg_block, RTjpeg_lqt);
   sp+=RTjpeg_b2s(RTjpeg_block, sp, RTjpeg_lb8);

   RTjpeg_dctY(bp2+k, RTjpeg_block, RTjpeg_Cwidth);
   RTjpeg_quant(RTjpeg_block, RTjpeg_cqt);
   sp+=RTjpeg_b2s(RTjpeg_block, sp, RTjpeg_cb8);

   RTjpeg_dctY(bp3+k, RTjpeg_block, RTjpeg_Cwidth);
   RTjpeg_quant(RTjpeg_block, RTjpeg_cqt);
   sp+=RTjpeg_b2s(RTjpeg_block, sp, RTjpeg_cb8);

  }
  bp+=RTjpeg_width<<4;
  bp1+=RTjpeg_width<<4;
  bp2+=RTjpeg_width<<2;
  bp3+=RTjpeg_width<<2;

 }
#if HAVE_MMX
 emms();
#endif
 return (sp-sb);
}

/*
External Function

Initialise additional data structures for motion compensation

*/

void RTjpeg_init_mcompress(void)
{
 unsigned long tmp;

 if(!RTjpeg_old)
 {
  RTjpeg_old=malloc((4*RTjpeg_width*RTjpeg_height)+32);
  tmp=(unsigned long)RTjpeg_old;
  tmp+=32;
  tmp=tmp>>5;
  RTjpeg_old=(__s16 *)(tmp<<5);
 }
 if (!RTjpeg_old)
 {
  fprintf(stderr, "RTjpeg: Could not allocate memory\n");
  exit(-1);
 }
 memset(RTjpeg_old, 0, ((4*RTjpeg_width*RTjpeg_height)));
}

#if HAVE_MMX

static int RTjpeg_bcomp(__s16 *old, mmx_t *mask)
{
 int i;
 mmx_t *mold=(mmx_t *)old;
 mmx_t *mblock=(mmx_t *)RTjpeg_block;
 volatile mmx_t result;
 static mmx_t neg={0xffffffffffffffffULL};

 movq_m2r(*mask, mm7);
 movq_m2r(neg, mm6);
 pxor_r2r(mm5, mm5);

 for(i=0; i<8; i++)
 {
  movq_m2r(*(mblock++), mm0);
  			movq_m2r(*(mblock++), mm2);
  movq_m2r(*(mold++), mm1);
  			movq_m2r(*(mold++), mm3);
  psubsw_r2r(mm1, mm0);
  			psubsw_r2r(mm3, mm2);
  movq_r2r(mm0, mm1);
  			movq_r2r(mm2, mm3);
  pcmpgtw_r2r(mm7, mm0);
  			pcmpgtw_r2r(mm7, mm2);
  pxor_r2r(mm6, mm1);
  			pxor_r2r(mm6, mm3);
  pcmpgtw_r2r(mm7, mm1);
  			pcmpgtw_r2r(mm7, mm3);
  por_r2r(mm0, mm5);
  			por_r2r(mm2, mm5);
  por_r2r(mm1, mm5);
  			por_r2r(mm3, mm5);
 }
 movq_r2m(mm5, result);

 if(result.q)
 {
  return 0;
 }
 return 1;
}

#else
static int RTjpeg_bcomp(__s16 *old, __u16 *mask)
{
 int i;

 for(i=0; i<64; i++)
  if(abs(old[i]-RTjpeg_block[i])>*mask)
  {
    for(i=0; i<16; i++)((__u64 *)old)[i]=((__u64 *)RTjpeg_block)[i];
   return 0;
  }
 return 1;
}
#endif

int RTjpeg_mcompressYUV420(__s8 *sp, unsigned char *bp, __u16 lmask, __u16 cmask)
{
 __s8 * sb;
 register __s8 * bp1 = bp + (RTjpeg_width<<3);
 register __s8 * bp2 = bp + RTjpeg_Ysize;
 register __s8 * bp3 = bp2 + (RTjpeg_Csize>>1);
 register int i, j, k;

#if HAVE_MMX
 emms();
 RTjpeg_lmask.uq=((__u64)lmask<<48)|((__u64)lmask<<32)|((__u64)lmask<<16)|lmask;
 RTjpeg_cmask.uq=((__u64)cmask<<48)|((__u64)cmask<<32)|((__u64)cmask<<16)|cmask;
#else
 RTjpeg_lmask=lmask;
 RTjpeg_cmask=cmask;
#endif

 sb=sp;
 block=RTjpeg_old;
/* Y */
 for(i=RTjpeg_height>>1; i; i-=8)
 {
  for(j=0, k=0; j<RTjpeg_width; j+=16, k+=8)
  {
   RTjpeg_dctY(bp+j, RTjpeg_block, RTjpeg_Ywidth);
   RTjpeg_quant(RTjpeg_block, RTjpeg_lqt);
   if(RTjpeg_bcomp(block, &RTjpeg_lmask))
   {
    *((__u8 *)sp++)=255;
   }
	else sp+=RTjpeg_b2s(RTjpeg_block, sp, RTjpeg_lb8);
   block+=64;

   RTjpeg_dctY(bp+j+8, RTjpeg_block, RTjpeg_Ywidth);
   RTjpeg_quant(RTjpeg_block, RTjpeg_lqt);
   if(RTjpeg_bcomp(block, &RTjpeg_lmask))
   {
    *((__u8 *)sp++)=255;
   }
	else sp+=RTjpeg_b2s(RTjpeg_block, sp, RTjpeg_lb8);
   block+=64;

   RTjpeg_dctY(bp1+j, RTjpeg_block, RTjpeg_Ywidth);
   RTjpeg_quant(RTjpeg_block, RTjpeg_lqt);
   if(RTjpeg_bcomp(block, &RTjpeg_lmask))
   {
    *((__u8 *)sp++)=255;
   }
	else sp+=RTjpeg_b2s(RTjpeg_block, sp, RTjpeg_lb8);
   block+=64;

   RTjpeg_dctY(bp1+j+8, RTjpeg_block, RTjpeg_Ywidth);
   RTjpeg_quant(RTjpeg_block, RTjpeg_lqt);
   if(RTjpeg_bcomp(block, &RTjpeg_lmask))
   {
    *((__u8 *)sp++)=255;
   }
	else sp+=RTjpeg_b2s(RTjpeg_block, sp, RTjpeg_lb8);
   block+=64;

   RTjpeg_dctY(bp2+k, RTjpeg_block, RTjpeg_Cwidth);
   RTjpeg_quant(RTjpeg_block, RTjpeg_cqt);
   if(RTjpeg_bcomp(block, &RTjpeg_cmask))
   {
    *((__u8 *)sp++)=255;
   }
	else sp+=RTjpeg_b2s(RTjpeg_block, sp, RTjpeg_cb8);
   block+=64;

   RTjpeg_dctY(bp3+k, RTjpeg_block, RTjpeg_Cwidth);
   RTjpeg_quant(RTjpeg_block, RTjpeg_cqt);
   if(RTjpeg_bcomp(block, &RTjpeg_cmask))
   {
    *((__u8 *)sp++)=255;
   }
	else sp+=RTjpeg_b2s(RTjpeg_block, sp, RTjpeg_cb8);
   block+=64;
  }
  bp+=RTjpeg_width<<4;
  bp1+=RTjpeg_width<<4;
  bp2+=RTjpeg_width<<2;
  bp3+=RTjpeg_width<<2;

 }
#if HAVE_MMX
 emms();
#endif
 return (sp-sb);
}
