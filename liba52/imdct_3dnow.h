/*
   3DNOW and 3DNOWEX optimized IMDCT
   Licence: GPL v2
   Copyrights: Nick Kurshev
*/

#undef FFT_4_3DNOW
#undef FFT_8_3DNOW
#undef FFT_ASMB_3DNOW
#undef FFT_ASMB16_3DNOW
#undef FFT_128P_3DNOW

#ifdef HAVE_3DNOWEX
#define FFT_4_3DNOW fft_4_3dnowex
#define FFT_8_3DNOW fft_8_3dnowex
#define FFT_ASMB_3DNOW fft_asmb_3dnowex
#define FFT_ASMB16_3DNOW fft_asmb16_3dnowex
#define FFT_128P_3DNOW fft_128p_3dnowex
#else
#define FFT_4_3DNOW fft_4_3dnow
#define FFT_8_3DNOW fft_8_3dnow
#define FFT_ASMB_3DNOW fft_asmb_3dnow
#define FFT_ASMB16_3DNOW fft_asmb16_3dnow
#define FFT_128P_3DNOW fft_128p_3dnow
#endif

static void FFT_4_3DNOW(complex_t *x)
{
  /* delta_p = 1 here */
  /* x[k] = sum_{i=0..3} x[i] * w^{i*k}, w=e^{-2*pi/4} 
   */
  __asm__ __volatile__(
	"movq	24(%1), %%mm3\n\t"
	"movq	8(%1), %%mm1\n\t"
	"pxor	%2, %%mm3\n\t" /* mm3.re | -mm3.im */
	"pxor   %3, %%mm1\n\t" /* -mm1.re | mm1.im */
	"pfadd	%%mm1, %%mm3\n\t" /* vi.im = x[3].re - x[1].re; */
	"movq	%%mm3, %%mm4\n\t" /* vi.re =-x[3].im + x[1].im; mm4 = vi */
#ifdef HAVE_3DNOWEX
	"pswapd %%mm4, %%mm4\n\t"
#else
	"punpckldq %%mm4, %%mm5\n\t"
	"punpckhdq %%mm5, %%mm4\n\t"
#endif
	"movq	(%1), %%mm5\n\t" /* yb.re = x[0].re - x[2].re; */
	"movq	(%1), %%mm6\n\t" /* yt.re = x[0].re + x[2].re; */
	"movq	24(%1), %%mm7\n\t" /* u.re  = x[3].re + x[1].re; */
	"pfsub	16(%1), %%mm5\n\t" /* yb.im = x[0].im - x[2].im; mm5 = yb */
	"pfadd	16(%1), %%mm6\n\t" /* yt.im = x[0].im + x[2].im; mm6 = yt */
	"pfadd	8(%1), %%mm7\n\t" /* u.im  = x[3].im + x[1].im; mm7 = u */

	"movq	%%mm6, %%mm0\n\t" /* x[0].re = yt.re + u.re; */
	"movq	%%mm5, %%mm1\n\t" /* x[1].re = yb.re + vi.re; */
	"pfadd	%%mm7, %%mm0\n\t" /*x[0].im = yt.im + u.im; */
	"pfadd	%%mm4, %%mm1\n\t" /* x[1].im = yb.im + vi.im; */
	"movq	%%mm0, (%0)\n\t"
	"movq	%%mm1, 8(%0)\n\t"

	"pfsub	%%mm7, %%mm6\n\t" /* x[2].re = yt.re - u.re; */
	"pfsub	%%mm4, %%mm5\n\t" /* x[3].re = yb.re - vi.re; */
	"movq	%%mm6, 16(%0)\n\t" /* x[2].im = yt.im - u.im; */
	"movq	%%mm5, 24(%0)" /* x[3].im = yb.im - vi.im; */
	:"=r"(x)
	:"0"(x),
	 "m"(x_plus_minus_3dnow),
	 "m"(x_minus_plus_3dnow)
	:"memory");
}

static void FFT_8_3DNOW(complex_t *x)
{
  /* delta_p = diag{1, sqrt(i)} here */
  /* x[k] = sum_{i=0..7} x[i] * w^{i*k}, w=e^{-2*pi/8} 
   */
  complex_t wT1, wB1, wB2;
  
  __asm__ __volatile__(
	"movq	8(%2), %%mm0\n\t"
	"movq	24(%2), %%mm1\n\t"
	"movq	%%mm0, %0\n\t"  /* wT1 = x[1]; */
	"movq	%%mm1, %1\n\t" /* wB1 = x[3]; */
	:"=m"(wT1), "=m"(wB1)
	:"r"(x)
	:"memory");

  __asm__ __volatile__(
	"movq	16(%0), %%mm2\n\t"
	"movq	32(%0), %%mm3\n\t"
	"movq	%%mm2, 8(%0)\n\t"  /* x[1] = x[2]; */
	"movq	48(%0), %%mm4\n\t"
	"movq	%%mm3, 16(%0)\n\t" /* x[2] = x[4]; */
	"movq	%%mm4, 24(%0)\n\t" /* x[3] = x[6]; */
	:"=r"(x)
	:"0"(x)
	:"memory");

  fft_4_3dnow(&x[0]);
  
  /* x[0] x[4] x[2] x[6] */
  
  __asm__ __volatile__(
      "movq	40(%1), %%mm0\n\t"
      "movq	%%mm0,	%%mm3\n\t"
      "movq	56(%1),	%%mm1\n\t"
      "pfadd	%%mm1,	%%mm0\n\t"
      "pfsub	%%mm1,	%%mm3\n\t"
      "movq	(%2),	%%mm2\n\t"
      "pfadd	%%mm2,	%%mm0\n\t"
      "pfadd	%%mm2,	%%mm3\n\t"
      "movq	(%3),	%%mm1\n\t"
      "pfadd	%%mm1,	%%mm0\n\t"
      "pfsub	%%mm1,	%%mm3\n\t"
      "movq	(%1),	%%mm1\n\t"
      "movq	16(%1),	%%mm4\n\t"
      "movq	%%mm1,	%%mm2\n\t"
#ifdef HAVE_3DNOWEX
      "pswapd	%%mm3,	%%mm3\n\t"
#else
      "punpckldq %%mm3,	%%mm6\n\t"
      "punpckhdq %%mm6,	%%mm3\n\t"
#endif
      "pfadd	%%mm0,	%%mm1\n\t"
      "movq	%%mm4,	%%mm5\n\t"
      "pfsub	%%mm0,	%%mm2\n\t"
      "pfadd	%%mm3,	%%mm4\n\t"
      "movq	%%mm1,	(%0)\n\t"
      "pfsub	%%mm3,	%%mm5\n\t"
      "movq	%%mm2,	32(%0)\n\t"
      "movd	%%mm4,	16(%0)\n\t"
      "movd	%%mm5,	48(%0)\n\t"
      "psrlq	$32, %%mm4\n\t"
      "psrlq	$32, %%mm5\n\t"
      "movd	%%mm4,	52(%0)\n\t"
      "movd	%%mm5,	20(%0)"
      :"=r"(x)
      :"0"(x), "r"(&wT1), "r"(&wB1)
      :"memory");
  
  /* x[1] x[5] */
  __asm__ __volatile__ (
	"movq	%6,	%%mm6\n\t"
	"movq	%5,	%%mm7\n\t"
	"movq	%1,	%%mm0\n\t"
	"movq	%2,	%%mm1\n\t"
	"movq	56(%3),	%%mm3\n\t"
	"pfsub	40(%3),	%%mm0\n\t"
#ifdef HAVE_3DNOWEX
	"pswapd	%%mm1,	%%mm1\n\t"
#else
	"punpckldq %%mm1, %%mm2\n\t"
	"punpckhdq %%mm2, %%mm1\n\t"
#endif
	"pxor	%%mm7,	%%mm1\n\t"
	"pfadd	%%mm1,	%%mm0\n\t"
#ifdef HAVE_3DNOWEX
	"pswapd	%%mm3,	%%mm3\n\t"
#else
	"punpckldq %%mm3, %%mm2\n\t"
	"punpckhdq %%mm2, %%mm3\n\t"
#endif
	"pxor	%%mm6,	%%mm3\n\t"
	"pfadd	%%mm3,	%%mm0\n\t"
	"movq	%%mm0,	%%mm1\n\t"
	"pxor	%%mm6,	%%mm1\n\t"
	"pfacc	%%mm1,	%%mm0\n\t"
	"pfmul	%4,	%%mm0\n\t"
	
	"movq	40(%3),	%%mm5\n\t"
#ifdef HAVE_3DNOWEX
	"pswapd	%%mm5,	%%mm5\n\t"
#else
	"punpckldq %%mm5, %%mm1\n\t"
	"punpckhdq %%mm1, %%mm5\n\t"
#endif
	"movq	%%mm5,	%0\n\t"
	
	"movq	8(%3),	%%mm1\n\t"
	"movq	%%mm1,	%%mm2\n\t"
	"pfsub	%%mm0,	%%mm1\n\t"
	"pfadd	%%mm0,	%%mm2\n\t"
	"movq	%%mm1,	40(%3)\n\t"
	"movq	%%mm2,	8(%3)\n\t"
	:"=m"(wB2)
	:"m"(wT1), "m"(wB1), "r"(x), "m"(HSQRT2_3DNOW), 
	 "m"(x_plus_minus_3dnow), "m"(x_minus_plus_3dnow)
	:"memory");


  /* x[3] x[7] */
  __asm__ __volatile__(
	"movq	%1,	%%mm0\n\t"
#ifdef HAVE_3DNOWEX
	"pswapd	%3,	%%mm1\n\t"
#else
	"movq	%3,	%%mm1\n\t"
	"punpckldq %%mm1, %%mm2\n\t"
	"punpckhdq %%mm2, %%mm1\n\t"
#endif
	"pxor	%%mm6,	%%mm1\n\t"	
	"pfadd	%%mm1,	%%mm0\n\t"
	"movq	%2,	%%mm2\n\t"
	"movq	56(%4),	%%mm3\n\t"
	"pxor	%%mm7,	%%mm3\n\t"
	"pfadd	%%mm3,	%%mm2\n\t"
#ifdef HAVE_3DNOWEX
	"pswapd	%%mm2,	%%mm2\n\t"
#else
	"punpckldq %%mm2, %%mm5\n\t"
	"punpckhdq %%mm5, %%mm2\n\t"
#endif
	"movq	24(%4),	%%mm3\n\t"
	"pfsub	%%mm2,	%%mm0\n\t"
	"movq	%%mm3,	%%mm4\n\t"
	"movq	%%mm0,	%%mm1\n\t"
	"pxor	%%mm6,	%%mm0\n\t"
	"pfacc	%%mm1,	%%mm0\n\t"
	"pfmul	%5,	%%mm0\n\t"
	"movq	%%mm0,	%%mm1\n\t"
	"pxor	%%mm6,	%%mm1\n\t"
	"pxor	%%mm7,	%%mm0\n\t"
	"pfadd	%%mm1,	%%mm3\n\t"
	"pfadd	%%mm0,	%%mm4\n\t"
	"movq	%%mm4,	24(%0)\n\t"
	"movq	%%mm3,	56(%0)\n\t"
	:"=r"(x)
	:"m"(wT1), "m"(wB2), "m"(wB1), "0"(x), "m"(HSQRT2_3DNOW)
	:"memory");
}

static void FFT_ASMB_3DNOW(int k, complex_t *x, complex_t *wTB,
		     const complex_t *d, const complex_t *d_3)
{
  register complex_t  *x2k, *x3k, *x4k, *wB;

  TRANS_FILL_MM6_MM7_3DNOW();
  x2k = x + 2 * k;
  x3k = x2k + 2 * k;
  x4k = x3k + 2 * k;
  wB = wTB + 2 * k;
  
  TRANSZERO_3DNOW(x[0],x2k[0],x3k[0],x4k[0]);
  TRANS_3DNOW(x[1],x2k[1],x3k[1],x4k[1],wTB[1],wB[1],d[1],d_3[1]);
  
  --k;
  for(;;) {
     TRANS_3DNOW(x[2],x2k[2],x3k[2],x4k[2],wTB[2],wB[2],d[2],d_3[2]);
     TRANS_3DNOW(x[3],x2k[3],x3k[3],x4k[3],wTB[3],wB[3],d[3],d_3[3]);
     if (!--k) break;
     x += 2;
     x2k += 2;
     x3k += 2;
     x4k += 2;
     d += 2;
     d_3 += 2;
     wTB += 2;
     wB += 2;
  }
 
}

void FFT_ASMB16_3DNOW(complex_t *x, complex_t *wTB)
{
  int k = 2;

  TRANS_FILL_MM6_MM7_3DNOW();
  /* transform x[0], x[8], x[4], x[12] */
  TRANSZERO_3DNOW(x[0],x[4],x[8],x[12]);

  /* transform x[1], x[9], x[5], x[13] */
  TRANS_3DNOW(x[1],x[5],x[9],x[13],wTB[1],wTB[5],delta16[1],delta16_3[1]);

  /* transform x[2], x[10], x[6], x[14] */
  TRANSHALF_16_3DNOW(x[2],x[6],x[10],x[14]);

  /* transform x[3], x[11], x[7], x[15] */
  TRANS_3DNOW(x[3],x[7],x[11],x[15],wTB[3],wTB[7],delta16[3],delta16_3[3]);

} 

static void FFT_128P_3DNOW(complex_t *a)
{
  FFT_8_3DNOW(&a[0]); FFT_4_3DNOW(&a[8]); FFT_4_3DNOW(&a[12]);
  FFT_ASMB16_3DNOW(&a[0], &a[8]);
  
  FFT_8_3DNOW(&a[16]), FFT_8_3DNOW(&a[24]);
  FFT_ASMB_3DNOW(4, &a[0], &a[16],&delta32[0], &delta32_3[0]);

  FFT_8_3DNOW(&a[32]); FFT_4_3DNOW(&a[40]); FFT_4_3DNOW(&a[44]);
  FFT_ASMB16_3DNOW(&a[32], &a[40]);

  FFT_8_3DNOW(&a[48]); FFT_4_3DNOW(&a[56]); FFT_4_3DNOW(&a[60]);
  FFT_ASMB16_3DNOW(&a[48], &a[56]);

  FFT_ASMB_3DNOW(8, &a[0], &a[32],&delta64[0], &delta64_3[0]);

  FFT_8_3DNOW(&a[64]); FFT_4_3DNOW(&a[72]); FFT_4_3DNOW(&a[76]);
  /* FFT_16(&a[64]); */
  FFT_ASMB16_3DNOW(&a[64], &a[72]);

  FFT_8_3DNOW(&a[80]); FFT_8_3DNOW(&a[88]);
  
  /* FFT_32(&a[64]); */
  FFT_ASMB_3DNOW(4, &a[64], &a[80],&delta32[0], &delta32_3[0]);

  FFT_8_3DNOW(&a[96]); FFT_4_3DNOW(&a[104]), FFT_4_3DNOW(&a[108]);
  /* FFT_16(&a[96]); */
  FFT_ASMB16_3DNOW(&a[96], &a[104]);

  FFT_8_3DNOW(&a[112]), FFT_8_3DNOW(&a[120]);
  /* FFT_32(&a[96]); */
  FFT_ASMB_3DNOW(4, &a[96], &a[112], &delta32[0], &delta32_3[0]);
  
  /* FFT_128(&a[0]); */
  FFT_ASMB_3DNOW(16, &a[0], &a[64], &delta128[0], &delta128_3[0]);
}

static void
#ifdef HAVE_3DNOWEX
imdct_do_512_3dnowex
#else
imdct_do_512_3dnow
#endif
(sample_t data[],sample_t delay[], sample_t bias)
{
    int i;
/*	int k;
    int p,q;
    int m;
    int two_m;
    int two_m_plus_one;

    sample_t tmp_a_i;
    sample_t tmp_a_r;
    sample_t tmp_b_i;
    sample_t tmp_b_r;*/

    sample_t *data_ptr;
    sample_t *delay_ptr;
    sample_t *window_ptr;
	
    /* 512 IMDCT with source and dest data in 'data' */
	
    /* Pre IFFT complex multiply plus IFFT cmplx conjugate & reordering*/
#if 1
      __asm__ __volatile__ (
	"movq %0, %%mm7\n\t"
	::"m"(x_plus_minus_3dnow)
	:"memory");
	for( i=0; i < 128; i++) {
		int j = pm128[i];
	__asm__ __volatile__ (
		"movd	%1, %%mm0\n\t"
		"movd	%3, %%mm1\n\t"
		"punpckldq %2, %%mm0\n\t" /* mm0 = data[256-2*j-1] | data[2*j]*/
		"punpckldq %4, %%mm1\n\t" /* mm1 = xcos[j] | xsin[j] */
		"movq	%%mm0, %%mm2\n\t"
		"pfmul	%%mm1, %%mm0\n\t"
#ifdef HAVE_3DNOWEX
		"pswapd	%%mm1, %%mm1\n\t"
#else
		"punpckldq %%mm1, %%mm5\n\t"
		"punpckhdq %%mm5, %%mm1\n\t"
#endif
		"pfmul	%%mm1, %%mm2\n\t"
#ifdef HAVE_3DNOWEX
		"pfpnacc %%mm2, %%mm0\n\t"
#else
		"pxor	%%mm7, %%mm0\n\t"
		"pfacc	%%mm2, %%mm0\n\t"
#endif
		"pxor	%%mm7, %%mm0\n\t"
		"movq	%%mm0, %0"
		:"=m"(buf[i])
		:"m"(data[256-2*j-1]), "m"(data[2*j]), "m"(xcos1[j]), "m"(xsin1[j])
		:"memory"
	);
/*		buf[i].re = (data[256-2*j-1] * xcos1[j] - data[2*j] * xsin1[j]);
		buf[i].im = (data[256-2*j-1] * xsin1[j] + data[2*j] * xcos1[j])*(-1.0);*/
	}
#else
  __asm__ __volatile__ ("femms":::"memory");
    for( i=0; i < 128; i++) {
	/* z[i] = (X[256-2*i-1] + j * X[2*i]) * (xcos1[i] + j * xsin1[i]) ; */ 
	int j= pm128[i];
	buf[i].real =         (data[256-2*j-1] * xcos1[j])  -  (data[2*j]       * xsin1[j]);
	buf[i].imag = -1.0 * ((data[2*j]       * xcos1[j])  +  (data[256-2*j-1] * xsin1[j]));
    }
#endif

    /* FFT Merge */
/* unoptimized variant
    for (m=1; m < 7; m++) {
	if(m)
	    two_m = (1 << m);
	else
	    two_m = 1;

	two_m_plus_one = (1 << (m+1));

	for(i = 0; i < 128; i += two_m_plus_one) {
	    for(k = 0; k < two_m; k++) {
		p = k + i;
		q = p + two_m;
		tmp_a_r = buf[p].real;
		tmp_a_i = buf[p].imag;
		tmp_b_r = buf[q].real * w[m][k].real - buf[q].imag * w[m][k].imag;
		tmp_b_i = buf[q].imag * w[m][k].real + buf[q].real * w[m][k].imag;
		buf[p].real = tmp_a_r + tmp_b_r;
		buf[p].imag =  tmp_a_i + tmp_b_i;
		buf[q].real = tmp_a_r - tmp_b_r;
		buf[q].imag =  tmp_a_i - tmp_b_i;
	    }
	}
    }
*/

    FFT_128P_3DNOW (&buf[0]);
//    asm volatile ("femms \n\t":::"memory");
    
    /* Post IFFT complex multiply  plus IFFT complex conjugate*/
#if 1  
  __asm__ __volatile__ (
	"movq %0, %%mm7\n\t"
	"movq %1, %%mm6\n\t"
	::"m"(x_plus_minus_3dnow),
	"m"(x_minus_plus_3dnow)
	:"eax","memory");
	for (i=0; i < 128; i++) {
	    __asm__ __volatile__ (
		"movq %1, %%mm0\n\t" /* ac3_buf[i].re | ac3_buf[i].im */
		"movq %%mm0, %%mm1\n\t" /* ac3_buf[i].re | ac3_buf[i].im */
#ifndef HAVE_3DNOWEX
		"punpckldq %%mm1, %%mm2\n\t"
		"punpckhdq %%mm2, %%mm1\n\t"
#else			 
		"pswapd %%mm1, %%mm1\n\t" /* ac3_buf[i].re | ac3_buf[i].im */
#endif			 
		"movd %3, %%mm3\n\t" /* ac3_xsin[i] */
		"punpckldq %2, %%mm3\n\t" /* ac3_xsin[i] | ac3_xcos[i] */
		"pfmul %%mm3, %%mm0\n\t"
		"pfmul %%mm3, %%mm1\n\t"
#ifndef HAVE_3DNOWEX
		"pxor  %%mm7, %%mm0\n\t"
		"pfacc %%mm1, %%mm0\n\t"
		"punpckldq %%mm0, %%mm1\n\t"
		"punpckhdq %%mm1, %%mm0\n\t"
		"movq %%mm0, %0\n\t"
#else
		"pfpnacc %%mm1, %%mm0\n\t" /* mm0 = mm0[0] - mm0[1] | mm1[0] + mm1[1] */
		"pswapd %%mm0, %%mm0\n\t"
		"movq %%mm0, %0"
#endif
		:"=m"(buf[i])
		:"m"(buf[i]),"m"(xcos1[i]),"m"(xsin1[i])
		:"memory");
/*		ac3_buf[i].re =(tmp_a_r * ac3_xcos1[i])  +  (tmp_a_i  * ac3_xsin1[i]);
		ac3_buf[i].im =(tmp_a_r * ac3_xsin1[i])  -  (tmp_a_i  * ac3_xcos1[i]);*/
	}
#else    
  __asm__ __volatile__ ("femms":::"memory");
    for( i=0; i < 128; i++) {
	/* y[n] = z[n] * (xcos1[n] + j * xsin1[n]) ; */
	tmp_a_r =        buf[i].real;
	tmp_a_i = -1.0 * buf[i].imag;
	buf[i].real =(tmp_a_r * xcos1[i])  -  (tmp_a_i  * xsin1[i]);
	buf[i].imag =(tmp_a_r * xsin1[i])  +  (tmp_a_i  * xcos1[i]);
    }
#endif
	
    data_ptr = data;
    delay_ptr = delay;
    window_ptr = a52_imdct_window;

    /* Window and convert to real valued signal */
#if 1
	asm volatile (
		"movd (%0), %%mm3	\n\t"
		"punpckldq %%mm3, %%mm3	\n\t"
	:: "r" (&bias)
	);
	for (i=0; i< 64; i++) {
/* merge two loops in one to enable working of 2 decoders */
	__asm__ __volatile__ (
		"movd	516(%1), %%mm0\n\t"
		"movd	(%1), %%mm1\n\t" /**data_ptr++=-buf[64+i].im**window_ptr+++*delay_ptr++;*/
		"punpckldq (%2), %%mm0\n\t"/*data_ptr[128]=-buf[i].re*window_ptr[128]+delay_ptr[128];*/
		"punpckldq 516(%2), %%mm1\n\t"
		"pfmul	(%3), %%mm0\n\t"/**data_ptr++=buf[64-i-1].re**window_ptr+++*delay_ptr++;*/
		"pfmul	512(%3), %%mm1\n\t"
		"pxor	%%mm6, %%mm0\n\t"/*data_ptr[128]=buf[128-i-1].im*window_ptr[128]+delay_ptr[128];*/
		"pxor	%%mm6, %%mm1\n\t"
		"pfadd	(%4), %%mm0\n\t"
		"pfadd	512(%4), %%mm1\n\t"
		"pfadd %%mm3, %%mm0\n\t"
		"pfadd %%mm3, %%mm1\n\t"
		"movq	%%mm0, (%0)\n\t"
		"movq	%%mm1, 512(%0)"
		:"=r"(data_ptr)
		:"r"(&buf[i].real), "r"(&buf[64-i-1].real), "r"(window_ptr), "r"(delay_ptr), "0"(data_ptr)
		:"memory");
		data_ptr += 2;
		window_ptr += 2;
		delay_ptr += 2;
	}
	window_ptr += 128;
#else    
  __asm__ __volatile__ ("femms":::"memory");
    for(i=0; i< 64; i++) { 
	*data_ptr++   = -buf[64+i].imag   * *window_ptr++ + *delay_ptr++ + bias; 
	*data_ptr++   =  buf[64-i-1].real * *window_ptr++ + *delay_ptr++ + bias; 
    }
    
    for(i=0; i< 64; i++) { 
	*data_ptr++  = -buf[i].real       * *window_ptr++ + *delay_ptr++ + bias; 
	*data_ptr++  =  buf[128-i-1].imag * *window_ptr++ + *delay_ptr++ + bias; 
    }
#endif

    /* The trailing edge of the window goes into the delay line */
    delay_ptr = delay;
#if 1
	for(i=0; i< 64; i++) {
/* merge two loops in one to enable working of 2 decoders */
	    window_ptr -=2;
	    __asm__ __volatile__(
		"movd	508(%1), %%mm0\n\t"
		"movd	(%1), %%mm1\n\t"
		"punpckldq (%2), %%mm0\n\t"
		"punpckldq 508(%2), %%mm1\n\t"
#ifdef HAVE_3DNOWEX
		"pswapd	(%3), %%mm3\n\t"
		"pswapd	-512(%3), %%mm4\n\t"
#else
		"movq	(%3), %%mm3\n\t"
		"punpckldq %%mm3, %%mm2\n\t"
		"punpckhdq %%mm2, %%mm3\n\t"
		"movq	-512(%3), %%mm4\n\t"
		"punpckldq %%mm4, %%mm2\n\t"
		"punpckhdq %%mm2, %%mm4\n\t"
#endif
		"pfmul	%%mm3, %%mm0\n\t"
		"pfmul	%%mm4, %%mm1\n\t"
		"pxor	%%mm6, %%mm0\n\t"
		"pxor	%%mm7, %%mm1\n\t"
		"movq	%%mm0, (%0)\n\t"
		"movq	%%mm1, 512(%0)"
		:"=r"(delay_ptr)
		:"r"(&buf[i].imag), "r"(&buf[64-i-1].imag), "r"(window_ptr), "0"(delay_ptr)
		:"memory");
		delay_ptr += 2;
	}
  __asm__ __volatile__ ("femms":::"memory");
#else    
  __asm__ __volatile__ ("femms":::"memory");
    for(i=0; i< 64; i++) { 
	*delay_ptr++  = -buf[64+i].real   * *--window_ptr; 
	*delay_ptr++  =  buf[64-i-1].imag * *--window_ptr; 
    }
    
    for(i=0; i<64; i++) {
	*delay_ptr++  =  buf[i].imag       * *--window_ptr; 
	*delay_ptr++  = -buf[128-i-1].real * *--window_ptr; 
    }
#endif    
}
