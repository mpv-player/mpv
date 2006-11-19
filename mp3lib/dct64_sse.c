/*
 * Discrete Cosine Tansform (DCT) for SSE
 * Copyright (c) 2006 Zuxy MENG <zuxy.meng@gmail.com>
 * based upon code from mp3lib/dct64.c, mp3lib/dct64_altivec.c
 * and mp3lib/dct64_MMX.c
 */

/* NOTE: The following code is suboptimal! It can be improved (at least) by

   1. Replace all movups by movaps. (Can Parameter c be always aligned on 
      a 16-byte boundary?)

   2. Rewritten using intrinsics. (GCC generally optimizes intrinsics
      better. However, when __m128 locals are involved, GCC may
      produce bad code that uses movaps to access a stack not aligned
      on a 16-byte boundary, which leads to run-time crashes.)

*/

typedef float real;

extern float __attribute__((aligned(16))) costab_mmx[];

static const int ppnn[4] __attribute__((aligned(16))) =
{ 0, 0, 1 << 31, 1 << 31 };

static const int pnpn[4] __attribute__((aligned(16))) =
{ 0, 1 << 31, 0, 1 << 31 };

static const int nnnn[4] __attribute__((aligned(16))) =
{ 1 << 31, 1 << 31, 1 << 31, 1 << 31 };

void dct64_sse(real *a,real *b,real *c)
{
    static real __attribute__ ((aligned(16))) b1[0x20];
    static real __attribute__ ((aligned(16))) b2[0x20];
    static real const one = 1.f;

    short *out0 = (short*)a;
    short *out1 = (short*)b;

    {
        real *costab = costab_mmx;
        int i;

        for (i = 0; i < 0x20 / 2; i += 4)
        {
            asm(
                "movaps    %2, %%xmm3\n\t"
                "shufps    $27, %%xmm3, %%xmm3\n\t"
                "movups    %3, %%xmm1\n\t"
                "movaps    %%xmm1, %%xmm4\n\t"
                "movups    %4, %%xmm2\n\t"
                "shufps    $27, %%xmm4, %%xmm4\n\t"
                "movaps    %%xmm2, %%xmm0\n\t"
                "shufps    $27, %%xmm0, %%xmm0\n\t"
                "addps     %%xmm0, %%xmm1\n\t"
                "movaps    %%xmm1, %0\n\t"
                "subps     %%xmm2, %%xmm4\n\t"
                "mulps     %%xmm3, %%xmm4\n\t"
                "movaps    %%xmm4, %1\n\t"
                :"=m"(*(b1 + i)), "=m"(*(b1 + 0x1c - i))
                :"m"(*(costab + i)), "m"(*(c + i)), "m"(*(c + 0x1c - i))
               );
        }
    }

    {
        int i;

        for (i = 0; i < 0x20; i += 0x10)
        {
            asm(
                "movaps    %4, %%xmm1\n\t"
                "movaps    %5, %%xmm3\n\t"
                "movaps    %6, %%xmm4\n\t"
                "movaps    %7, %%xmm6\n\t"
                "movaps    %%xmm1, %%xmm7\n\t"
                "shufps    $27, %%xmm7, %%xmm7\n\t"
                "movaps    %%xmm3, %%xmm5\n\t"
                "shufps    $27, %%xmm5, %%xmm5\n\t"
                "movaps    %%xmm4, %%xmm2\n\t"
                "shufps    $27, %%xmm2, %%xmm2\n\t"
                "movaps    %%xmm6, %%xmm0\n\t"
                "shufps    $27, %%xmm0, %%xmm0\n\t"
                "addps     %%xmm0, %%xmm1\n\t"
                "movaps    %%xmm1, %0\n\t"
                "addps     %%xmm2, %%xmm3\n\t"
                "movaps    %%xmm3, %1\n\t"
                "subps     %%xmm4, %%xmm5\n\t"
                "movaps    %%xmm5, %2\n\t"
                "subps     %%xmm6, %%xmm7\n\t"
                "movaps    %%xmm7, %3\n\t"
                :"=m"(*(b2 + i)), "=m"(*(b2 + i + 4)), "=m"(*(b2 + i + 8)), "=m"(*(b2 + i + 12))
                :"m"(*(b1 + i)), "m"(*(b1 + i + 4)), "m"(*(b1 + i + 8)), "m"(*(b1 + i + 12))
                );
        }
    }

    {
        real *costab = costab_mmx + 16;
        asm(
            "movaps    %4, %%xmm0\n\t"
            "movaps    %5, %%xmm1\n\t"
            "movaps    %8, %%xmm4\n\t"
            "xorps     %%xmm6, %%xmm6\n\t"
            "shufps    $27, %%xmm4, %%xmm4\n\t"
            "mulps     %%xmm4, %%xmm1\n\t"
            "movaps    %9, %%xmm2\n\t"
            "xorps     %%xmm7, %%xmm7\n\t"
            "shufps    $27, %%xmm2, %%xmm2\n\t"
            "mulps     %%xmm2, %%xmm0\n\t"
            "movaps    %%xmm0, %0\n\t"
            "movaps    %%xmm1, %1\n\t"
            "movaps    %6, %%xmm3\n\t"
            "mulps     %%xmm2, %%xmm3\n\t"
            "subps     %%xmm3, %%xmm6\n\t"
            "movaps    %%xmm6, %2\n\t"
            "movaps    %7, %%xmm5\n\t"
            "mulps     %%xmm4, %%xmm5\n\t"
            "subps     %%xmm5, %%xmm7\n\t"
            "movaps    %%xmm7, %3\n\t"
            :"=m"(*(b2 + 8)), "=m"(*(b2 + 0xc)), "=m"(*(b2 + 0x18)), "=m"(*(b2 + 0x1c))
            :"m"(*(b2 + 8)), "m"(*(b2 + 0xc)), "m"(*(b2 + 0x18)), "m"(*(b2 + 0x1c)), "m"(*costab), "m"(*(costab + 4))
            );
    }

    {
        real *costab = costab_mmx + 24;
        int i;

        asm(
            "movaps    %0, %%xmm0\n\t"
            "shufps    $27, %%xmm0, %%xmm0\n\t"
            "movaps    %1, %%xmm5\n\t"
            "movaps    %%xmm5, %%xmm6\n\t"
            :
            :"m"(*costab), "m"(*nnnn)
           );

        for (i = 0; i < 0x20; i += 8)
        {
            asm(
                "movaps    %2, %%xmm2\n\t"
                "movaps    %3, %%xmm3\n\t"
                "movaps    %%xmm2, %%xmm4\n\t"
                "xorps     %%xmm5, %%xmm6\n\t"
                "shufps    $27, %%xmm4, %%xmm4\n\t"
                "movaps    %%xmm3, %%xmm1\n\t"
                "shufps    $27, %%xmm1, %%xmm1\n\t"
                "addps     %%xmm1, %%xmm2\n\t"
                "movaps    %%xmm2, %0\n\t"
                "subps     %%xmm3, %%xmm4\n\t"
                "xorps     %%xmm6, %%xmm4\n\t"
                "mulps     %%xmm0, %%xmm4\n\t"
                "movaps    %%xmm4, %1\n\t"
                :"=m"(*(b1 + i)), "=m"(*(b1 + i + 4))
                :"m"(*(b2 + i)), "m"(*(b2 + i + 4))
               );
        }
    }

    {
        int i;

        asm(
            "movss     %0, %%xmm1\n\t"
            "movss     %1, %%xmm0\n\t"
            "movaps    %%xmm1, %%xmm3\n\t"
            "unpcklps  %%xmm0, %%xmm3\n\t"
            "movss     %2, %%xmm2\n\t"
            "movaps    %%xmm1, %%xmm0\n\t"
            "unpcklps  %%xmm2, %%xmm0\n\t"
            "unpcklps  %%xmm3, %%xmm0\n\t"
            "movaps    %3, %%xmm2\n\t"
            :
            :"m"(one), "m"(costab_mmx[28]), "m"(costab_mmx[29]), "m"(*ppnn)
           );

        for (i = 0; i < 0x20; i += 8)
        {
            asm(
                "movaps    %2, %%xmm3\n\t"
                "movaps    %%xmm3, %%xmm4\n\t"
                "shufps    $20, %%xmm4, %%xmm4\n\t"
                "shufps    $235, %%xmm3, %%xmm3\n\t"
                "xorps     %%xmm2, %%xmm3\n\t"
                "addps     %%xmm3, %%xmm4\n\t"
                "mulps     %%xmm0, %%xmm4\n\t"
                "movaps    %%xmm4, %0\n\t"
                "movaps    %3, %%xmm6\n\t"
                "movaps    %%xmm6, %%xmm5\n\t"
                "shufps    $27, %%xmm5, %%xmm5\n\t"
                "xorps     %%xmm2, %%xmm5\n\t"
                "addps     %%xmm5, %%xmm6\n\t"
                "mulps     %%xmm0, %%xmm6\n\t"
                "movaps    %%xmm6, %1\n\t"
                :"=m"(*(b2 + i)), "=m"(*(b2 + i + 4))
                :"m"(*(b1 + i)), "m"(*(b1 + i + 4))
               );
        }
    }

    {
        int i;
        asm(
            "movss     %0, %%xmm0\n\t"
            "movaps    %%xmm1, %%xmm2\n\t"
            "movaps    %%xmm0, %%xmm7\n\t"
            "unpcklps  %%xmm1, %%xmm2\n\t"
            "unpcklps  %%xmm0, %%xmm7\n\t"
            "movaps    %1, %%xmm0\n\t"
            "unpcklps  %%xmm7, %%xmm2\n\t"
            :
            :"m"(costab_mmx[30]), "m"(*pnpn)
           );

        for (i = 0x8; i < 0x20; i += 8)
        {
            asm volatile (
                          "movaps    %2, %%xmm1\n\t"
                          "movaps    %%xmm1, %%xmm3\n\t"
                          "shufps    $224, %%xmm3, %%xmm3\n\t"
                          "shufps    $181, %%xmm1, %%xmm1\n\t"
                          "xorps     %%xmm0, %%xmm1\n\t"
                          "addps     %%xmm1, %%xmm3\n\t"
                          "mulps     %%xmm2, %%xmm3\n\t"
                          "movaps    %%xmm3, %0\n\t"
                          "movaps    %3, %%xmm4\n\t"
                          "movaps    %%xmm4, %%xmm5\n\t"
                          "shufps    $224, %%xmm5, %%xmm5\n\t"
                          "shufps    $181, %%xmm4, %%xmm4\n\t"
                          "xorps     %%xmm0, %%xmm4\n\t"
                          "addps     %%xmm4, %%xmm5\n\t"
                          "mulps     %%xmm2, %%xmm5\n\t"
                          "movaps    %%xmm5, %1\n\t"
                          :"=m"(*(b1 + i)), "=m"(*(b1 + i + 4))
                          :"m"(*(b2 + i)), "m"(*(b2 + i + 4))
                          :"memory"
                         );
        }
        for (i = 0x8; i < 0x20; i += 8)
        {
            b1[i + 2] += b1[i + 3];
            b1[i + 6] += b1[i + 7];
            b1[i + 4] += b1[i + 6];
            b1[i + 6] += b1[i + 5];
            b1[i + 5] += b1[i + 7];
        }
    }

#if 0
    /* Reference C code */

    /*
       Should run faster than x87 asm, given that the compiler is sane.
       However, the C code dosen't round with saturation (0x7fff for too
       large positive float, 0x8000 for too small negative float). You
       can hear the difference if you listen carefully.
    */

    out0[256] = (short)(b2[0] + b2[1]);
    out0[0] = (short)((b2[0] - b2[1]) * costab_mmx[30]);
    out1[128] = (short)((b2[3] - b2[2]) * costab_mmx[30]);
    out0[128] = (short)((b2[3] - b2[2]) * costab_mmx[30] + b2[3] + b2[2]);
    out1[192] = (short)((b2[7] - b2[6]) * costab_mmx[30]);
    out0[192] = (short)((b2[7] - b2[6]) * costab_mmx[30] + b2[6] + b2[7] + b2[4] + b2[5]);
    out0[64] = (short)((b2[7] - b2[6]) * costab_mmx[30] + b2[6] + b2[7] + (b2[4] - b2[5]) * costab_mmx[30]);
    out1[64] = (short)((b2[7] - b2[6]) * costab_mmx[30] + (b2[4] - b2[5]) * costab_mmx[30]);

    out0[224] = (short)(b1[8] + b1[12]);
    out0[160] = (short)(b1[12] + b1[10]);
    out0[96] = (short)(b1[10] + b1[14]);
    out0[32] = (short)(b1[14] + b1[9]);
    out1[32] = (short)(b1[9] + b1[13]);
    out1[96] = (short)(b1[13] + b1[11]);
    out1[222] = (short)b1[15];
    out1[160] = (short)(b1[15] + b1[11]);
    out0[240] = (short)(b1[24] + b1[28] + b1[16]);
    out0[208] = (short)(b1[24] + b1[28] + b1[20]);
    out0[176] = (short)(b1[28] + b1[26] + b1[20]);
    out0[144] = (short)(b1[28] + b1[26] + b1[18]);
    out0[112] = (short)(b1[26] + b1[30] + b1[18]);
    out0[80] = (short)(b1[26] + b1[30] + b1[22]);
    out0[48] = (short)(b1[30] + b1[25] + b1[22]);
    out0[16] = (short)(b1[30] + b1[25] + b1[17]);
    out1[16] = (short)(b1[25] + b1[29] + b1[17]);
    out1[48] = (short)(b1[25] + b1[29] + b1[21]);
    out1[80] = (short)(b1[29] + b1[27] + b1[21]);
    out1[112] = (short)(b1[29] + b1[27] + b1[19]);
    out1[144] = (short)(b1[27] + b1[31] + b1[19]);
    out1[176] = (short)(b1[27] + b1[31] + b1[23]);
    out1[240] = (short)(b1[31]);
    out1[208] = (short)(b1[31] + b1[23]);

#else
    /*
       To do saturation efficiently in x86 we can use fist(t)(p),
       pf2iw, or packssdw. We use fist(p) here.
    */
    asm(
        "flds       %0\n\t"
        "flds     (%2)\n\t"
        "fadds   4(%2)\n\t"
        "fistp 512(%3)\n\t"

        "flds     (%2)\n\t"
        "fsubs   4(%2)\n\t"
        "fmul  %%st(1)\n\t"
        "fistp    (%3)\n\t"

        "flds   12(%2)\n\t"
        "fsubs   8(%2)\n\t"
        "fmul  %%st(1)\n\t"
        "fist  256(%4)\n\t"
        "fadds  12(%2)\n\t"
        "fadds   8(%2)\n\t"
        "fistp 256(%3)\n\t"

        "flds   16(%2)\n\t"
        "fsubs  20(%2)\n\t"
        "fmul  %%st(1)\n\t"

        "flds   28(%2)\n\t"
        "fsubs  24(%2)\n\t"
        "fmul  %%st(2)\n\t"
        "fist  384(%4)\n\t"
        "fld   %%st(0)\n\t"
        "fadds  24(%2)\n\t"
        "fadds  28(%2)\n\t"
        "fld   %%st(0)\n\t"
        "fadds  16(%2)\n\t"
        "fadds  20(%2)\n\t"
        "fistp 384(%3)\n\t"
        "fadd  %%st(2)\n\t"
        "fistp 128(%3)\n\t"
        "faddp %%st(1)\n\t"
        "fistp 128(%4)\n\t"

        "flds   32(%1)\n\t"
        "fadds  48(%1)\n\t"
        "fistp 448(%3)\n\t"

        "flds   48(%1)\n\t"
        "fadds  40(%1)\n\t"
        "fistp 320(%3)\n\t"

        "flds   40(%1)\n\t"
        "fadds  56(%1)\n\t"
        "fistp 192(%3)\n\t"

        "flds   56(%1)\n\t"
        "fadds  36(%1)\n\t"
        "fistp  64(%3)\n\t"

        "flds   36(%1)\n\t"
        "fadds  52(%1)\n\t"
        "fistp  64(%4)\n\t"

        "flds   52(%1)\n\t"
        "fadds  44(%1)\n\t"
        "fistp 192(%4)\n\t"

        "flds   60(%1)\n\t"
        "fist  448(%4)\n\t"
        "fadds  44(%1)\n\t"
        "fistp 320(%4)\n\t"

        "flds   96(%1)\n\t"
        "fadds 112(%1)\n\t"
        "fld   %%st(0)\n\t"
        "fadds  64(%1)\n\t"
        "fistp 480(%3)\n\t"
        "fadds  80(%1)\n\t"
        "fistp 416(%3)\n\t"

        "flds  112(%1)\n\t"
        "fadds 104(%1)\n\t"
        "fld   %%st(0)\n\t"
        "fadds  80(%1)\n\t"
        "fistp 352(%3)\n\t"
        "fadds  72(%1)\n\t"
        "fistp 288(%3)\n\t"

        "flds  104(%1)\n\t"
        "fadds 120(%1)\n\t"
        "fld   %%st(0)\n\t"
        "fadds  72(%1)\n\t"
        "fistp 224(%3)\n\t"
        "fadds  88(%1)\n\t"
        "fistp 160(%3)\n\t"

        "flds  120(%1)\n\t"
        "fadds 100(%1)\n\t"
        "fld   %%st(0)\n\t"
        "fadds  88(%1)\n\t"
        "fistp  96(%3)\n\t"
        "fadds  68(%1)\n\t"
        "fistp  32(%3)\n\t"

        "flds  100(%1)\n\t"
        "fadds 116(%1)\n\t"
        "fld   %%st(0)\n\t"
        "fadds  68(%1)\n\t"
        "fistp  32(%4)\n\t"
        "fadds  84(%1)\n\t"
        "fistp  96(%4)\n\t"

        "flds  116(%1)\n\t"
        "fadds 108(%1)\n\t"
        "fld   %%st(0)\n\t"
        "fadds  84(%1)\n\t"
        "fistp 160(%4)\n\t"
        "fadds  76(%1)\n\t"
        "fistp 224(%4)\n\t"

        "flds  108(%1)\n\t"
        "fadds 124(%1)\n\t"
        "fld   %%st(0)\n\t"
        "fadds  76(%1)\n\t"
        "fistp 288(%4)\n\t"
        "fadds  92(%1)\n\t"
        "fistp 352(%4)\n\t"

        "flds  124(%1)\n\t"
        "fist  480(%4)\n\t"
        "fadds  92(%1)\n\t"
        "fistp 416(%4)\n\t"
        ".byte 0xdf, 0xc0\n\t" // ffreep %%st(0)
        :
        :"m"(costab_mmx[30]), "r"(b1), "r"(b2), "r"(a), "r"(b)
        :"memory"
        );
#endif
    out1[0] = out0[0];
}

