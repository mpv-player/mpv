
/*
 * Discrete Cosine Tansform (DCT) for Altivec
 * Copyright (c) 2004 Romain Dolbeau <romain@dolbeau.org>
 * based upon code from "mp3lib/dct64.c"
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 */

#include <stdio.h>
#include "mpg123.h"

#ifdef HAVE_ALTIVEC_H
#include <altivec.h>
#endif

// used to build registers permutation vectors (vcprm)
// the 's' are for words in the _s_econd vector
#define WORD_0 0x00,0x01,0x02,0x03
#define WORD_1 0x04,0x05,0x06,0x07
#define WORD_2 0x08,0x09,0x0a,0x0b
#define WORD_3 0x0c,0x0d,0x0e,0x0f
#define WORD_s0 0x10,0x11,0x12,0x13
#define WORD_s1 0x14,0x15,0x16,0x17
#define WORD_s2 0x18,0x19,0x1a,0x1b
#define WORD_s3 0x1c,0x1d,0x1e,0x1f

#define vcprm(a,b,c,d) (const vector unsigned char){WORD_ ## a, WORD_ ## b, WORD_ ## c, WORD_ ## d}
#define vcii(a,b,c,d) (const vector float){FLOAT_ ## a, FLOAT_ ## b, FLOAT_ ## c, FLOAT_ ## d}

#define FOUROF(a) {a,a,a,a}

// vcprmle is used to keep the same index as in the SSE version.
// it's the same as vcprm, with the index inversed
// ('le' is Little Endian)
#define vcprmle(a,b,c,d) vcprm(d,c,b,a)

// used to build inverse/identity vectors (vcii)
// n is _n_egative, p is _p_ositive
#define FLOAT_n -1.
#define FLOAT_p 1.

void dct64_altivec(real *a,real *b,real *c)
{
  real __attribute__ ((aligned(16))) b1[0x20];
  real __attribute__ ((aligned(16))) b2[0x20];

  real *out0 = a;
  real *out1 = b;
  real *samples = c;

  const vector float vczero = (const vector float)FOUROF(0.);
  const vector unsigned char reverse = (const vector unsigned char)vcprm(3,2,1,0);


  if (((unsigned long)b1 & 0x0000000F) ||
      ((unsigned long)b2 & 0x0000000F))

  {
    printf("MISALIGNED:\t%p\t%p\t%p\t%p\t%p\n",
           b1, b2, a, b, samples);
  }


#ifdef ALTIVEC_USE_REFERENCE_C_CODE

  {
    register real *costab = mp3lib_pnts[0];

    b1[0x00] = samples[0x00] + samples[0x1F];
    b1[0x01] = samples[0x01] + samples[0x1E];
    b1[0x02] = samples[0x02] + samples[0x1D];
    b1[0x03] = samples[0x03] + samples[0x1C];
    b1[0x04] = samples[0x04] + samples[0x1B];
    b1[0x05] = samples[0x05] + samples[0x1A];
    b1[0x06] = samples[0x06] + samples[0x19];
    b1[0x07] = samples[0x07] + samples[0x18];
    b1[0x08] = samples[0x08] + samples[0x17];
    b1[0x09] = samples[0x09] + samples[0x16];
    b1[0x0A] = samples[0x0A] + samples[0x15];
    b1[0x0B] = samples[0x0B] + samples[0x14];
    b1[0x0C] = samples[0x0C] + samples[0x13];
    b1[0x0D] = samples[0x0D] + samples[0x12];
    b1[0x0E] = samples[0x0E] + samples[0x11];
    b1[0x0F] = samples[0x0F] + samples[0x10];
    b1[0x10] = (samples[0x0F] - samples[0x10]) * costab[0xF];
    b1[0x11] = (samples[0x0E] - samples[0x11]) * costab[0xE];
    b1[0x12] = (samples[0x0D] - samples[0x12]) * costab[0xD];
    b1[0x13] = (samples[0x0C] - samples[0x13]) * costab[0xC];
    b1[0x14] = (samples[0x0B] - samples[0x14]) * costab[0xB];
    b1[0x15] = (samples[0x0A] - samples[0x15]) * costab[0xA];
    b1[0x16] = (samples[0x09] - samples[0x16]) * costab[0x9];
    b1[0x17] = (samples[0x08] - samples[0x17]) * costab[0x8];
    b1[0x18] = (samples[0x07] - samples[0x18]) * costab[0x7];
    b1[0x19] = (samples[0x06] - samples[0x19]) * costab[0x6];
    b1[0x1A] = (samples[0x05] - samples[0x1A]) * costab[0x5];
    b1[0x1B] = (samples[0x04] - samples[0x1B]) * costab[0x4];
    b1[0x1C] = (samples[0x03] - samples[0x1C]) * costab[0x3];
    b1[0x1D] = (samples[0x02] - samples[0x1D]) * costab[0x2];
    b1[0x1E] = (samples[0x01] - samples[0x1E]) * costab[0x1];
    b1[0x1F] = (samples[0x00] - samples[0x1F]) * costab[0x0];

  }
  {
    register real *costab = mp3lib_pnts[1];

    b2[0x00] = b1[0x00] + b1[0x0F];
    b2[0x01] = b1[0x01] + b1[0x0E];
    b2[0x02] = b1[0x02] + b1[0x0D];
    b2[0x03] = b1[0x03] + b1[0x0C];
    b2[0x04] = b1[0x04] + b1[0x0B];
    b2[0x05] = b1[0x05] + b1[0x0A];
    b2[0x06] = b1[0x06] + b1[0x09];
    b2[0x07] = b1[0x07] + b1[0x08];
    b2[0x08] = (b1[0x07] - b1[0x08]) * costab[7];
    b2[0x09] = (b1[0x06] - b1[0x09]) * costab[6];
    b2[0x0A] = (b1[0x05] - b1[0x0A]) * costab[5];
    b2[0x0B] = (b1[0x04] - b1[0x0B]) * costab[4];
    b2[0x0C] = (b1[0x03] - b1[0x0C]) * costab[3];
    b2[0x0D] = (b1[0x02] - b1[0x0D]) * costab[2];
    b2[0x0E] = (b1[0x01] - b1[0x0E]) * costab[1];
    b2[0x0F] = (b1[0x00] - b1[0x0F]) * costab[0];
    b2[0x10] = b1[0x10] + b1[0x1F];
    b2[0x11] = b1[0x11] + b1[0x1E];
    b2[0x12] = b1[0x12] + b1[0x1D];
    b2[0x13] = b1[0x13] + b1[0x1C];
    b2[0x14] = b1[0x14] + b1[0x1B];
    b2[0x15] = b1[0x15] + b1[0x1A];
    b2[0x16] = b1[0x16] + b1[0x19];
    b2[0x17] = b1[0x17] + b1[0x18];
    b2[0x18] = (b1[0x18] - b1[0x17]) * costab[7];
    b2[0x19] = (b1[0x19] - b1[0x16]) * costab[6];
    b2[0x1A] = (b1[0x1A] - b1[0x15]) * costab[5];
    b2[0x1B] = (b1[0x1B] - b1[0x14]) * costab[4];
    b2[0x1C] = (b1[0x1C] - b1[0x13]) * costab[3];
    b2[0x1D] = (b1[0x1D] - b1[0x12]) * costab[2];
    b2[0x1E] = (b1[0x1E] - b1[0x11]) * costab[1];
    b2[0x1F] = (b1[0x1F] - b1[0x10]) * costab[0];

  }

  {
    register real *costab = mp3lib_pnts[2];

    b1[0x00] = b2[0x00] + b2[0x07];
    b1[0x01] = b2[0x01] + b2[0x06];
    b1[0x02] = b2[0x02] + b2[0x05];
    b1[0x03] = b2[0x03] + b2[0x04];
    b1[0x04] = (b2[0x03] - b2[0x04]) * costab[3];
    b1[0x05] = (b2[0x02] - b2[0x05]) * costab[2];
    b1[0x06] = (b2[0x01] - b2[0x06]) * costab[1];
    b1[0x07] = (b2[0x00] - b2[0x07]) * costab[0];
    b1[0x08] = b2[0x08] + b2[0x0F];
    b1[0x09] = b2[0x09] + b2[0x0E];
    b1[0x0A] = b2[0x0A] + b2[0x0D];
    b1[0x0B] = b2[0x0B] + b2[0x0C];
    b1[0x0C] = (b2[0x0C] - b2[0x0B]) * costab[3];
    b1[0x0D] = (b2[0x0D] - b2[0x0A]) * costab[2];
    b1[0x0E] = (b2[0x0E] - b2[0x09]) * costab[1];
    b1[0x0F] = (b2[0x0F] - b2[0x08]) * costab[0];
    b1[0x10] = b2[0x10] + b2[0x17];
    b1[0x11] = b2[0x11] + b2[0x16];
    b1[0x12] = b2[0x12] + b2[0x15];
    b1[0x13] = b2[0x13] + b2[0x14];
    b1[0x14] = (b2[0x13] - b2[0x14]) * costab[3];
    b1[0x15] = (b2[0x12] - b2[0x15]) * costab[2];
    b1[0x16] = (b2[0x11] - b2[0x16]) * costab[1];
    b1[0x17] = (b2[0x10] - b2[0x17]) * costab[0];
    b1[0x18] = b2[0x18] + b2[0x1F];
    b1[0x19] = b2[0x19] + b2[0x1E];
    b1[0x1A] = b2[0x1A] + b2[0x1D];
    b1[0x1B] = b2[0x1B] + b2[0x1C];
    b1[0x1C] = (b2[0x1C] - b2[0x1B]) * costab[3];
    b1[0x1D] = (b2[0x1D] - b2[0x1A]) * costab[2];
    b1[0x1E] = (b2[0x1E] - b2[0x19]) * costab[1];
    b1[0x1F] = (b2[0x1F] - b2[0x18]) * costab[0];
  }

#else /* ALTIVEC_USE_REFERENCE_C_CODE */

  // How does it work ?
  // the first three passes are reproducted in the three block below
  // all computations are done on a 4 elements vector
  // 'reverse' is a special perumtation vector used to reverse
  // the order of the elements inside a vector.
  // note that all loads/stores to b1 (b2) between passes 1 and 2 (2 and 3)
  // have been removed, all elements are stored inside b1vX (b2vX)
  {
    register vector float
      b1v0, b1v1, b1v2, b1v3,
      b1v4, b1v5, b1v6, b1v7;
    register vector float
      temp1, temp2;

    {
      register real *costab = mp3lib_pnts[0];

      register vector float
        samplesv1, samplesv2, samplesv3, samplesv4,
        samplesv5, samplesv6, samplesv7, samplesv8,
        samplesv9;
      register vector unsigned char samples_perm = vec_lvsl(0, samples);
      register vector float costabv1, costabv2, costabv3, costabv4, costabv5;
      register vector unsigned char costab_perm = vec_lvsl(0, costab);

      samplesv1 = vec_ld(0, samples);
      samplesv2 = vec_ld(16, samples);
      samplesv1 = vec_perm(samplesv1, samplesv2, samples_perm);
      samplesv3 = vec_ld(32, samples);
      samplesv2 = vec_perm(samplesv2, samplesv3, samples_perm);
      samplesv4 = vec_ld(48, samples);
      samplesv3 = vec_perm(samplesv3, samplesv4, samples_perm);
      samplesv5 = vec_ld(64, samples);
      samplesv4 = vec_perm(samplesv4, samplesv5, samples_perm);
      samplesv6 = vec_ld(80, samples);
      samplesv5 = vec_perm(samplesv5, samplesv6, samples_perm);
      samplesv7 = vec_ld(96, samples);
      samplesv6 = vec_perm(samplesv6, samplesv7, samples_perm);
      samplesv8 = vec_ld(112, samples);
      samplesv7 = vec_perm(samplesv7, samplesv8, samples_perm);
      samplesv9 = vec_ld(128, samples);
      samplesv8 = vec_perm(samplesv8, samplesv9, samples_perm);

      temp1 = vec_add(samplesv1,
                      vec_perm(samplesv8, samplesv8, reverse));
      //vec_st(temp1, 0, b1);
      b1v0 = temp1;
      temp1 = vec_add(samplesv2,
                      vec_perm(samplesv7, samplesv7, reverse));
      //vec_st(temp1, 16, b1);
      b1v1 = temp1;
      temp1 = vec_add(samplesv3,
                      vec_perm(samplesv6, samplesv6, reverse));
      //vec_st(temp1, 32, b1);
      b1v2 = temp1;
      temp1 = vec_add(samplesv4,
                      vec_perm(samplesv5, samplesv5, reverse));
      //vec_st(temp1, 48, b1);
      b1v3 = temp1;

      costabv1 = vec_ld(0, costab);
      costabv2 = vec_ld(16, costab);
      costabv1 = vec_perm(costabv1, costabv2, costab_perm);
      costabv3 = vec_ld(32, costab);
      costabv2 = vec_perm(costabv2, costabv3, costab_perm);
      costabv4 = vec_ld(48, costab);
      costabv3 = vec_perm(costabv3, costabv4, costab_perm);
      costabv5 = vec_ld(64, costab);
      costabv4 = vec_perm(costabv4, costabv5, costab_perm);

      temp1 = vec_sub(vec_perm(samplesv4, samplesv4, reverse),
                      samplesv5);
      temp2 = vec_madd(temp1,
                       vec_perm(costabv4, costabv4, reverse),
                       vczero);
      //vec_st(temp2, 64, b1);
      b1v4 = temp2;

      temp1 = vec_sub(vec_perm(samplesv3, samplesv3, reverse),
                      samplesv6);
      temp2 = vec_madd(temp1,
                       vec_perm(costabv3, costabv3, reverse),
                       vczero);
      //vec_st(temp2, 80, b1);
      b1v5 = temp2;
      temp1 = vec_sub(vec_perm(samplesv2, samplesv2, reverse),
                      samplesv7);
      temp2 = vec_madd(temp1,
                       vec_perm(costabv2, costabv2, reverse),
                       vczero);
      //vec_st(temp2, 96, b1);
      b1v6 = temp2;

      temp1 = vec_sub(vec_perm(samplesv1, samplesv1, reverse),
                      samplesv8);
      temp2 = vec_madd(temp1,
                       vec_perm(costabv1, costabv1, reverse),
                       vczero);
      //vec_st(temp2, 112, b1);
      b1v7 = temp2;

    }

    {
      register vector float
        b2v0, b2v1, b2v2, b2v3,
        b2v4, b2v5, b2v6, b2v7;
      {
        register real *costab = mp3lib_pnts[1];
        register vector float costabv1r, costabv2r, costabv1, costabv2, costabv3;
        register vector unsigned char costab_perm = vec_lvsl(0, costab);

        costabv1 = vec_ld(0, costab);
        costabv2 = vec_ld(16, costab);
        costabv1 = vec_perm(costabv1, costabv2, costab_perm);
        costabv3  = vec_ld(32, costab);
        costabv2 = vec_perm(costabv2, costabv3 , costab_perm);
        costabv1r = vec_perm(costabv1, costabv1, reverse);
        costabv2r = vec_perm(costabv2, costabv2, reverse);

        temp1 = vec_add(b1v0, vec_perm(b1v3, b1v3, reverse));
        //vec_st(temp1, 0, b2);
        b2v0 = temp1;
        temp1 = vec_add(b1v1, vec_perm(b1v2, b1v2, reverse));
        //vec_st(temp1, 16, b2);
        b2v1 = temp1;
        temp2 = vec_sub(vec_perm(b1v1, b1v1, reverse), b1v2);
        temp1 = vec_madd(temp2, costabv2r, vczero);
        //vec_st(temp1, 32, b2);
        b2v2 = temp1;
        temp2 = vec_sub(vec_perm(b1v0, b1v0, reverse), b1v3);
        temp1 = vec_madd(temp2, costabv1r, vczero);
        //vec_st(temp1, 48, b2);
        b2v3 = temp1;
        temp1 = vec_add(b1v4, vec_perm(b1v7, b1v7, reverse));
        //vec_st(temp1, 64, b2);
        b2v4 = temp1;
        temp1 = vec_add(b1v5, vec_perm(b1v6, b1v6, reverse));
        //vec_st(temp1, 80, b2);
        b2v5 = temp1;
        temp2 = vec_sub(b1v6, vec_perm(b1v5, b1v5, reverse));
        temp1 = vec_madd(temp2, costabv2r, vczero);
        //vec_st(temp1, 96, b2);
        b2v6 = temp1;
        temp2 = vec_sub(b1v7, vec_perm(b1v4, b1v4, reverse));
        temp1 = vec_madd(temp2, costabv1r, vczero);
        //vec_st(temp1, 112, b2);
        b2v7 = temp1;
      }

      {
        register real *costab = mp3lib_pnts[2];


        vector float costabv1r, costabv1, costabv2;
        vector unsigned char costab_perm = vec_lvsl(0, costab);

        costabv1 = vec_ld(0, costab);
        costabv2 = vec_ld(16, costab);
        costabv1 = vec_perm(costabv1, costabv2, costab_perm);
        costabv1r = vec_perm(costabv1, costabv1, reverse);

        temp1 = vec_add(b2v0, vec_perm(b2v1, b2v1, reverse));
        vec_st(temp1, 0, b1);
        temp2 = vec_sub(vec_perm(b2v0, b2v0, reverse), b2v1);
        temp1 = vec_madd(temp2, costabv1r, vczero);
        vec_st(temp1, 16, b1);

        temp1 = vec_add(b2v2, vec_perm(b2v3, b2v3, reverse));
        vec_st(temp1, 32, b1);
        temp2 = vec_sub(b2v3, vec_perm(b2v2, b2v2, reverse));
        temp1 = vec_madd(temp2, costabv1r, vczero);
        vec_st(temp1, 48, b1);

        temp1 = vec_add(b2v4, vec_perm(b2v5, b2v5, reverse));
        vec_st(temp1, 64, b1);
        temp2 = vec_sub(vec_perm(b2v4, b2v4, reverse), b2v5);
        temp1 = vec_madd(temp2, costabv1r, vczero);
        vec_st(temp1, 80, b1);

        temp1 = vec_add(b2v6, vec_perm(b2v7, b2v7, reverse));
        vec_st(temp1, 96, b1);
        temp2 = vec_sub(b2v7, vec_perm(b2v6, b2v6, reverse));
        temp1 = vec_madd(temp2, costabv1r, vczero);
        vec_st(temp1, 112, b1);

      }
    }
  }

#endif /* ALTIVEC_USE_REFERENCE_C_CODE */

  {
    register real const cos0 = mp3lib_pnts[3][0];
    register real const cos1 = mp3lib_pnts[3][1];

    b2[0x00] = b1[0x00] + b1[0x03];
    b2[0x01] = b1[0x01] + b1[0x02];
    b2[0x02] = (b1[0x01] - b1[0x02]) * cos1;
    b2[0x03] = (b1[0x00] - b1[0x03]) * cos0;
    b2[0x04] = b1[0x04] + b1[0x07];
    b2[0x05] = b1[0x05] + b1[0x06];
    b2[0x06] = (b1[0x06] - b1[0x05]) * cos1;
    b2[0x07] = (b1[0x07] - b1[0x04]) * cos0;
    b2[0x08] = b1[0x08] + b1[0x0B];
    b2[0x09] = b1[0x09] + b1[0x0A];
    b2[0x0A] = (b1[0x09] - b1[0x0A]) * cos1;
    b2[0x0B] = (b1[0x08] - b1[0x0B]) * cos0;
    b2[0x0C] = b1[0x0C] + b1[0x0F];
    b2[0x0D] = b1[0x0D] + b1[0x0E];
    b2[0x0E] = (b1[0x0E] - b1[0x0D]) * cos1;
    b2[0x0F] = (b1[0x0F] - b1[0x0C]) * cos0;
    b2[0x10] = b1[0x10] + b1[0x13];
    b2[0x11] = b1[0x11] + b1[0x12];
    b2[0x12] = (b1[0x11] - b1[0x12]) * cos1;
    b2[0x13] = (b1[0x10] - b1[0x13]) * cos0;
    b2[0x14] = b1[0x14] + b1[0x17];
    b2[0x15] = b1[0x15] + b1[0x16];
    b2[0x16] = (b1[0x16] - b1[0x15]) * cos1;
    b2[0x17] = (b1[0x17] - b1[0x14]) * cos0;
    b2[0x18] = b1[0x18] + b1[0x1B];
    b2[0x19] = b1[0x19] + b1[0x1A];
    b2[0x1A] = (b1[0x19] - b1[0x1A]) * cos1;
    b2[0x1B] = (b1[0x18] - b1[0x1B]) * cos0;
    b2[0x1C] = b1[0x1C] + b1[0x1F];
    b2[0x1D] = b1[0x1D] + b1[0x1E];
    b2[0x1E] = (b1[0x1E] - b1[0x1D]) * cos1;
    b2[0x1F] = (b1[0x1F] - b1[0x1C]) * cos0;
  }

  {
    register real const cos0 = mp3lib_pnts[4][0];

    b1[0x00] = b2[0x00] + b2[0x01];
    b1[0x01] = (b2[0x00] - b2[0x01]) * cos0;
    b1[0x02] = b2[0x02] + b2[0x03];
    b1[0x03] = (b2[0x03] - b2[0x02]) * cos0;
    b1[0x02] += b1[0x03];

    b1[0x04] = b2[0x04] + b2[0x05];
    b1[0x05] = (b2[0x04] - b2[0x05]) * cos0;
    b1[0x06] = b2[0x06] + b2[0x07];
    b1[0x07] = (b2[0x07] - b2[0x06]) * cos0;
    b1[0x06] += b1[0x07];
    b1[0x04] += b1[0x06];
    b1[0x06] += b1[0x05];
    b1[0x05] += b1[0x07];

    b1[0x08] = b2[0x08] + b2[0x09];
    b1[0x09] = (b2[0x08] - b2[0x09]) * cos0;
    b1[0x0A] = b2[0x0A] + b2[0x0B];
    b1[0x0B] = (b2[0x0B] - b2[0x0A]) * cos0;
    b1[0x0A] += b1[0x0B];

    b1[0x0C] = b2[0x0C] + b2[0x0D];
    b1[0x0D] = (b2[0x0C] - b2[0x0D]) * cos0;
    b1[0x0E] = b2[0x0E] + b2[0x0F];
    b1[0x0F] = (b2[0x0F] - b2[0x0E]) * cos0;
    b1[0x0E] += b1[0x0F];
    b1[0x0C] += b1[0x0E];
    b1[0x0E] += b1[0x0D];
    b1[0x0D] += b1[0x0F];

    b1[0x10] = b2[0x10] + b2[0x11];
    b1[0x11] = (b2[0x10] - b2[0x11]) * cos0;
    b1[0x12] = b2[0x12] + b2[0x13];
    b1[0x13] = (b2[0x13] - b2[0x12]) * cos0;
    b1[0x12] += b1[0x13];

    b1[0x14] = b2[0x14] + b2[0x15];
    b1[0x15] = (b2[0x14] - b2[0x15]) * cos0;
    b1[0x16] = b2[0x16] + b2[0x17];
    b1[0x17] = (b2[0x17] - b2[0x16]) * cos0;
    b1[0x16] += b1[0x17];
    b1[0x14] += b1[0x16];
    b1[0x16] += b1[0x15];
    b1[0x15] += b1[0x17];

    b1[0x18] = b2[0x18] + b2[0x19];
    b1[0x19] = (b2[0x18] - b2[0x19]) * cos0;
    b1[0x1A] = b2[0x1A] + b2[0x1B];
    b1[0x1B] = (b2[0x1B] - b2[0x1A]) * cos0;
    b1[0x1A] += b1[0x1B];

    b1[0x1C] = b2[0x1C] + b2[0x1D];
    b1[0x1D] = (b2[0x1C] - b2[0x1D]) * cos0;
    b1[0x1E] = b2[0x1E] + b2[0x1F];
    b1[0x1F] = (b2[0x1F] - b2[0x1E]) * cos0;
    b1[0x1E] += b1[0x1F];
    b1[0x1C] += b1[0x1E];
    b1[0x1E] += b1[0x1D];
    b1[0x1D] += b1[0x1F];
  }

  out0[0x10*16] = b1[0x00];
  out0[0x10*12] = b1[0x04];
  out0[0x10* 8] = b1[0x02];
  out0[0x10* 4] = b1[0x06];
  out0[0x10* 0] = b1[0x01];
  out1[0x10* 0] = b1[0x01];
  out1[0x10* 4] = b1[0x05];
  out1[0x10* 8] = b1[0x03];
  out1[0x10*12] = b1[0x07];

  b1[0x08] += b1[0x0C];
  out0[0x10*14] = b1[0x08];
  b1[0x0C] += b1[0x0a];
  out0[0x10*10] = b1[0x0C];
  b1[0x0A] += b1[0x0E];
  out0[0x10* 6] = b1[0x0A];
  b1[0x0E] += b1[0x09];
  out0[0x10* 2] = b1[0x0E];
  b1[0x09] += b1[0x0D];
  out1[0x10* 2] = b1[0x09];
  b1[0x0D] += b1[0x0B];
  out1[0x10* 6] = b1[0x0D];
  b1[0x0B] += b1[0x0F];
  out1[0x10*10] = b1[0x0B];
  out1[0x10*14] = b1[0x0F];

  b1[0x18] += b1[0x1C];
  out0[0x10*15] = b1[0x10] + b1[0x18];
  out0[0x10*13] = b1[0x18] + b1[0x14];
  b1[0x1C] += b1[0x1a];
  out0[0x10*11] = b1[0x14] + b1[0x1C];
  out0[0x10* 9] = b1[0x1C] + b1[0x12];
  b1[0x1A] += b1[0x1E];
  out0[0x10* 7] = b1[0x12] + b1[0x1A];
  out0[0x10* 5] = b1[0x1A] + b1[0x16];
  b1[0x1E] += b1[0x19];
  out0[0x10* 3] = b1[0x16] + b1[0x1E];
  out0[0x10* 1] = b1[0x1E] + b1[0x11];
  b1[0x19] += b1[0x1D];
  out1[0x10* 1] = b1[0x11] + b1[0x19];
  out1[0x10* 3] = b1[0x19] + b1[0x15];
  b1[0x1D] += b1[0x1B];
  out1[0x10* 5] = b1[0x15] + b1[0x1D];
  out1[0x10* 7] = b1[0x1D] + b1[0x13];
  b1[0x1B] += b1[0x1F];
  out1[0x10* 9] = b1[0x13] + b1[0x1B];
  out1[0x10*11] = b1[0x1B] + b1[0x17];
  out1[0x10*13] = b1[0x17] + b1[0x1F];
  out1[0x10*15] = b1[0x1F];
}
