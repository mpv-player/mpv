/*
** FAAD2 - Freeware Advanced Audio (AAC) Decoder including SBR decoding
** Copyright (C) 2003-2004 M. Bakker, Ahead Software AG, http://www.nero.com
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
**
** Any non-GPL usage of this software or parts of this software is strictly
** forbidden.
**
** Commercial non-GPL licensing of this software is possible.
** For more info contact Ahead Software through Mpeg4AAClicense@nero.com.
**
** Initially modified for use with MPlayer by Arpad Gereöffy on 2003/08/30
** $Id: cfft.c,v 1.4 2004/06/23 13:50:49 diego Exp $
** detailed CVS changelog at http://www.mplayerhq.hu/cgi-bin/cvsweb.cgi/main/
**/

/*
 * Algorithmically based on Fortran-77 FFTPACK
 * by Paul N. Swarztrauber(Version 4, 1985).
 *
 * Does even sized fft only
 */

/* isign is +1 for backward and -1 for forward transforms */

#include "common.h"
#include "structs.h"

#include <stdlib.h>

#include "cfft.h"
#include "cfft_tab.h"


/* static function declarations */
#ifdef USE_SSE
static void passf2pos_sse(const uint16_t l1, const complex_t *cc,
                          complex_t *ch, const complex_t *wa);
static void passf2pos_sse_ido(const uint16_t ido, const uint16_t l1, const complex_t *cc,
                              complex_t *ch, const complex_t *wa);
static void passf4pos_sse_ido(const uint16_t ido, const uint16_t l1, const complex_t *cc, complex_t *ch,
                              const complex_t *wa1, const complex_t *wa2, const complex_t *wa3);
#endif
static void passf2pos(const uint16_t ido, const uint16_t l1, const complex_t *cc,
                      complex_t *ch, const complex_t *wa);
static void passf2neg(const uint16_t ido, const uint16_t l1, const complex_t *cc,
                      complex_t *ch, const complex_t *wa);
static void passf3(const uint16_t ido, const uint16_t l1, const complex_t *cc,
                   complex_t *ch, const complex_t *wa1, const complex_t *wa2, const int8_t isign);
static void passf4pos(const uint16_t ido, const uint16_t l1, const complex_t *cc, complex_t *ch,
                      const complex_t *wa1, const complex_t *wa2, const complex_t *wa3);
static void passf4neg(const uint16_t ido, const uint16_t l1, const complex_t *cc, complex_t *ch,
                      const complex_t *wa1, const complex_t *wa2, const complex_t *wa3);
static void passf5(const uint16_t ido, const uint16_t l1, const complex_t *cc, complex_t *ch,
                   const complex_t *wa1, const complex_t *wa2, const complex_t *wa3,
                   const complex_t *wa4, const int8_t isign);
INLINE void cfftf1(uint16_t n, complex_t *c, complex_t *ch,
                   const uint16_t *ifac, const complex_t *wa, const int8_t isign);
static void cffti1(uint16_t n, complex_t *wa, uint16_t *ifac);


/*----------------------------------------------------------------------
   passf2, passf3, passf4, passf5. Complex FFT passes fwd and bwd.
  ----------------------------------------------------------------------*/

#if 0 //def USE_SSE
static void passf2pos_sse(const uint16_t l1, const complex_t *cc,
                          complex_t *ch, const complex_t *wa)
{
    uint16_t k, ah, ac;

    for (k = 0; k < l1; k++)
    {
        ah = 2*k;
        ac = 4*k;

        RE(ch[ah])    = RE(cc[ac]) + RE(cc[ac+1]);
        IM(ch[ah])    = IM(cc[ac]) + IM(cc[ac+1]);

        RE(ch[ah+l1]) = RE(cc[ac]) - RE(cc[ac+1]);
        IM(ch[ah+l1]) = IM(cc[ac]) - IM(cc[ac+1]);
    }
}

static void passf2pos_sse_ido(const uint16_t ido, const uint16_t l1, const complex_t *cc,
                              complex_t *ch, const complex_t *wa)
{
    uint16_t i, k, ah, ac;

    for (k = 0; k < l1; k++)
    {
        ah = k*ido;
        ac = 2*k*ido;

        for (i = 0; i < ido; i+=4)
        {
            __m128 m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14;
            __m128 m15, m16, m17, m18, m19, m20, m21, m22, m23, m24;
            __m128 w1, w2, w3, w4;

            m1 = _mm_load_ps(&RE(cc[ac+i]));
            m2 = _mm_load_ps(&RE(cc[ac+ido+i]));
            m5 = _mm_load_ps(&RE(cc[ac+i+2]));
            m6 = _mm_load_ps(&RE(cc[ac+ido+i+2]));
            w1 = _mm_load_ps(&RE(wa[i]));
            w3 = _mm_load_ps(&RE(wa[i+2]));

            m3 = _mm_add_ps(m1, m2);
            m15 = _mm_add_ps(m5, m6);

            m4 = _mm_sub_ps(m1, m2);
            m16 = _mm_sub_ps(m5, m6);

            _mm_store_ps(&RE(ch[ah+i]), m3);
            _mm_store_ps(&RE(ch[ah+i+2]), m15);


            w2 = _mm_shuffle_ps(w1, w1, _MM_SHUFFLE(2, 3, 0, 1));
            w4 = _mm_shuffle_ps(w3, w3, _MM_SHUFFLE(2, 3, 0, 1));

            m7 = _mm_mul_ps(m4, w1);
            m17 = _mm_mul_ps(m16, w3);
            m8 = _mm_mul_ps(m4, w2);
            m18 = _mm_mul_ps(m16, w4);

            m9  = _mm_shuffle_ps(m7, m8, _MM_SHUFFLE(2, 0, 2, 0));
            m19 = _mm_shuffle_ps(m17, m18, _MM_SHUFFLE(2, 0, 2, 0));
            m10 = _mm_shuffle_ps(m7, m8, _MM_SHUFFLE(3, 1, 3, 1));
            m20 = _mm_shuffle_ps(m17, m18, _MM_SHUFFLE(3, 1, 3, 1));

            m11 = _mm_add_ps(m9, m10);
            m21 = _mm_add_ps(m19, m20);
            m12 = _mm_sub_ps(m9, m10);
            m22 = _mm_sub_ps(m19, m20);

            m13 = _mm_shuffle_ps(m11, m11, _MM_SHUFFLE(0, 0, 3, 2));
            m23 = _mm_shuffle_ps(m21, m21, _MM_SHUFFLE(0, 0, 3, 2));

            m14 = _mm_unpacklo_ps(m12, m13);
            m24 = _mm_unpacklo_ps(m22, m23);

            _mm_store_ps(&RE(ch[ah+i+l1*ido]), m14);
            _mm_store_ps(&RE(ch[ah+i+2+l1*ido]), m24);
        }
    }
}
#endif

static void passf2pos(const uint16_t ido, const uint16_t l1, const complex_t *cc,
                      complex_t *ch, const complex_t *wa)
{
    uint16_t i, k, ah, ac;

    if (ido == 1)
    {
        for (k = 0; k < l1; k++)
        {
            ah = 2*k;
            ac = 4*k;

            RE(ch[ah])    = RE(cc[ac]) + RE(cc[ac+1]);
            RE(ch[ah+l1]) = RE(cc[ac]) - RE(cc[ac+1]);
            IM(ch[ah])    = IM(cc[ac]) + IM(cc[ac+1]);
            IM(ch[ah+l1]) = IM(cc[ac]) - IM(cc[ac+1]);
        }
    } else {
        for (k = 0; k < l1; k++)
        {
            ah = k*ido;
            ac = 2*k*ido;

            for (i = 0; i < ido; i++)
            {
                complex_t t2;

                RE(ch[ah+i]) = RE(cc[ac+i]) + RE(cc[ac+i+ido]);
                RE(t2)       = RE(cc[ac+i]) - RE(cc[ac+i+ido]);

                IM(ch[ah+i]) = IM(cc[ac+i]) + IM(cc[ac+i+ido]);
                IM(t2)       = IM(cc[ac+i]) - IM(cc[ac+i+ido]);

#if 1
                ComplexMult(&IM(ch[ah+i+l1*ido]), &RE(ch[ah+i+l1*ido]),
                    IM(t2), RE(t2), RE(wa[i]), IM(wa[i]));
#else
                ComplexMult(&RE(ch[ah+i+l1*ido]), &IM(ch[ah+i+l1*ido]),
                    RE(t2), IM(t2), RE(wa[i]), IM(wa[i]));
#endif
            }
        }
    }
}

static void passf2neg(const uint16_t ido, const uint16_t l1, const complex_t *cc,
                      complex_t *ch, const complex_t *wa)
{
    uint16_t i, k, ah, ac;

    if (ido == 1)
    {
        for (k = 0; k < l1; k++)
        {
            ah = 2*k;
            ac = 4*k;

            RE(ch[ah])    = RE(cc[ac]) + RE(cc[ac+1]);
            RE(ch[ah+l1]) = RE(cc[ac]) - RE(cc[ac+1]);
            IM(ch[ah])    = IM(cc[ac]) + IM(cc[ac+1]);
            IM(ch[ah+l1]) = IM(cc[ac]) - IM(cc[ac+1]);
        }
    } else {
        for (k = 0; k < l1; k++)
        {
            ah = k*ido;
            ac = 2*k*ido;

            for (i = 0; i < ido; i++)
            {
                complex_t t2;

                RE(ch[ah+i]) = RE(cc[ac+i]) + RE(cc[ac+i+ido]);
                RE(t2)       = RE(cc[ac+i]) - RE(cc[ac+i+ido]);

                IM(ch[ah+i]) = IM(cc[ac+i]) + IM(cc[ac+i+ido]);
                IM(t2)       = IM(cc[ac+i]) - IM(cc[ac+i+ido]);

#if 1
                ComplexMult(&RE(ch[ah+i+l1*ido]), &IM(ch[ah+i+l1*ido]),
                    RE(t2), IM(t2), RE(wa[i]), IM(wa[i]));
#else
                ComplexMult(&IM(ch[ah+i+l1*ido]), &RE(ch[ah+i+l1*ido]),
                    IM(t2), RE(t2), RE(wa[i]), IM(wa[i]));
#endif
            }
        }
    }
}


static void passf3(const uint16_t ido, const uint16_t l1, const complex_t *cc,
                   complex_t *ch, const complex_t *wa1, const complex_t *wa2,
                   const int8_t isign)
{
    static real_t taur = FRAC_CONST(-0.5);
    static real_t taui = FRAC_CONST(0.866025403784439);
    uint16_t i, k, ac, ah;
    complex_t c2, c3, d2, d3, t2;

    if (ido == 1)
    {
        if (isign == 1)
        {
            for (k = 0; k < l1; k++)
            {
                ac = 3*k+1;
                ah = k;

                RE(t2) = RE(cc[ac]) + RE(cc[ac+1]);
                IM(t2) = IM(cc[ac]) + IM(cc[ac+1]);
                RE(c2) = RE(cc[ac-1]) + MUL_F(RE(t2),taur);
                IM(c2) = IM(cc[ac-1]) + MUL_F(IM(t2),taur);

                RE(ch[ah]) = RE(cc[ac-1]) + RE(t2);
                IM(ch[ah]) = IM(cc[ac-1]) + IM(t2);

                RE(c3) = MUL_F((RE(cc[ac]) - RE(cc[ac+1])), taui);
                IM(c3) = MUL_F((IM(cc[ac]) - IM(cc[ac+1])), taui);

                RE(ch[ah+l1]) = RE(c2) - IM(c3);
                IM(ch[ah+l1]) = IM(c2) + RE(c3);
                RE(ch[ah+2*l1]) = RE(c2) + IM(c3);
                IM(ch[ah+2*l1]) = IM(c2) - RE(c3);
            }
        } else {
            for (k = 0; k < l1; k++)
            {
                ac = 3*k+1;
                ah = k;

                RE(t2) = RE(cc[ac]) + RE(cc[ac+1]);
                IM(t2) = IM(cc[ac]) + IM(cc[ac+1]);
                RE(c2) = RE(cc[ac-1]) + MUL_F(RE(t2),taur);
                IM(c2) = IM(cc[ac-1]) + MUL_F(IM(t2),taur);

                RE(ch[ah]) = RE(cc[ac-1]) + RE(t2);
                IM(ch[ah]) = IM(cc[ac-1]) + IM(t2);

                RE(c3) = MUL_F((RE(cc[ac]) - RE(cc[ac+1])), taui);
                IM(c3) = MUL_F((IM(cc[ac]) - IM(cc[ac+1])), taui);

                RE(ch[ah+l1]) = RE(c2) + IM(c3);
                IM(ch[ah+l1]) = IM(c2) - RE(c3);
                RE(ch[ah+2*l1]) = RE(c2) - IM(c3);
                IM(ch[ah+2*l1]) = IM(c2) + RE(c3);
            }
        }
    } else {
        if (isign == 1)
        {
            for (k = 0; k < l1; k++)
            {
                for (i = 0; i < ido; i++)
                {
                    ac = i + (3*k+1)*ido;
                    ah = i + k * ido;

                    RE(t2) = RE(cc[ac]) + RE(cc[ac+ido]);
                    RE(c2) = RE(cc[ac-ido]) + MUL_F(RE(t2),taur);
                    IM(t2) = IM(cc[ac]) + IM(cc[ac+ido]);
                    IM(c2) = IM(cc[ac-ido]) + MUL_F(IM(t2),taur);

                    RE(ch[ah]) = RE(cc[ac-ido]) + RE(t2);
                    IM(ch[ah]) = IM(cc[ac-ido]) + IM(t2);

                    RE(c3) = MUL_F((RE(cc[ac]) - RE(cc[ac+ido])), taui);
                    IM(c3) = MUL_F((IM(cc[ac]) - IM(cc[ac+ido])), taui);

                    RE(d2) = RE(c2) - IM(c3);
                    IM(d3) = IM(c2) - RE(c3);
                    RE(d3) = RE(c2) + IM(c3);
                    IM(d2) = IM(c2) + RE(c3);

#if 1
                    ComplexMult(&IM(ch[ah+l1*ido]), &RE(ch[ah+l1*ido]),
                        IM(d2), RE(d2), RE(wa1[i]), IM(wa1[i]));
                    ComplexMult(&IM(ch[ah+2*l1*ido]), &RE(ch[ah+2*l1*ido]),
                        IM(d3), RE(d3), RE(wa2[i]), IM(wa2[i]));
#else
                    ComplexMult(&RE(ch[ah+l1*ido]), &IM(ch[ah+l1*ido]),
                        RE(d2), IM(d2), RE(wa1[i]), IM(wa1[i]));
                    ComplexMult(&RE(ch[ah+2*l1*ido]), &IM(ch[ah+2*l1*ido]),
                        RE(d3), IM(d3), RE(wa2[i]), IM(wa2[i]));
#endif
                }
            }
        } else {
            for (k = 0; k < l1; k++)
            {
                for (i = 0; i < ido; i++)
                {
                    ac = i + (3*k+1)*ido;
                    ah = i + k * ido;

                    RE(t2) = RE(cc[ac]) + RE(cc[ac+ido]);
                    RE(c2) = RE(cc[ac-ido]) + MUL_F(RE(t2),taur);
                    IM(t2) = IM(cc[ac]) + IM(cc[ac+ido]);
                    IM(c2) = IM(cc[ac-ido]) + MUL_F(IM(t2),taur);

                    RE(ch[ah]) = RE(cc[ac-ido]) + RE(t2);
                    IM(ch[ah]) = IM(cc[ac-ido]) + IM(t2);

                    RE(c3) = MUL_F((RE(cc[ac]) - RE(cc[ac+ido])), taui);
                    IM(c3) = MUL_F((IM(cc[ac]) - IM(cc[ac+ido])), taui);

                    RE(d2) = RE(c2) + IM(c3);
                    IM(d3) = IM(c2) + RE(c3);
                    RE(d3) = RE(c2) - IM(c3);
                    IM(d2) = IM(c2) - RE(c3);

#if 1
                    ComplexMult(&RE(ch[ah+l1*ido]), &IM(ch[ah+l1*ido]),
                        RE(d2), IM(d2), RE(wa1[i]), IM(wa1[i]));
                    ComplexMult(&RE(ch[ah+2*l1*ido]), &IM(ch[ah+2*l1*ido]),
                        RE(d3), IM(d3), RE(wa2[i]), IM(wa2[i]));
#else
                    ComplexMult(&IM(ch[ah+l1*ido]), &RE(ch[ah+l1*ido]),
                        IM(d2), RE(d2), RE(wa1[i]), IM(wa1[i]));
                    ComplexMult(&IM(ch[ah+2*l1*ido]), &RE(ch[ah+2*l1*ido]),
                        IM(d3), RE(d3), RE(wa2[i]), IM(wa2[i]));
#endif
                }
            }
        }
    }
}

#ifdef USE_SSE
ALIGN static const int32_t negate[4] = { 0x0, 0x0, 0x0, 0x80000000 };

__declspec(naked) static void passf4pos_sse(const uint16_t l1, const complex_t *cc,
                                     complex_t *ch, const complex_t *wa1, const complex_t *wa2,
                                     const complex_t *wa3)
{
    __asm {
        push      ebx
        mov       ebx, esp
        and       esp, -16
        push      edi
        push      esi
        sub       esp, 8
        movzx     edi, WORD PTR [ebx+8]

        movaps    xmm1, XMMWORD PTR negate

        test      edi, edi
        jle       l1_is_zero

        lea       esi, DWORD PTR [edi+edi]
        add       esi, esi
        sub       esi, edi
        add       esi, esi
        add       esi, esi
        add       esi, esi
        mov       eax, DWORD PTR [ebx+16]
        add       esi, eax
        lea       ecx, DWORD PTR [edi+edi]
        add       ecx, ecx
        add       ecx, ecx
        add       ecx, ecx
        add       ecx, eax
        lea       edx, DWORD PTR [edi+edi]
        add       edx, edx
        add       edx, edx
        add       edx, eax
        xor       eax, eax
        mov       DWORD PTR [esp], ebp
        mov       ebp, DWORD PTR [ebx+12]

fftloop:
        lea       edi, DWORD PTR [eax+eax]
        add       edi, edi
        movaps    xmm2, XMMWORD PTR [ebp+edi*8]
        movaps    xmm0, XMMWORD PTR [ebp+edi*8+16]
        movaps    xmm7, XMMWORD PTR [ebp+edi*8+32]
        movaps    xmm5, XMMWORD PTR [ebp+edi*8+48]
        movaps    xmm6, xmm2
        addps     xmm6, xmm0
        movaps    xmm4, xmm1
        xorps     xmm4, xmm7
        movaps    xmm3, xmm1
        xorps     xmm3, xmm5
        xorps     xmm2, xmm1
        xorps     xmm0, xmm1
        addps     xmm7, xmm5
        subps     xmm2, xmm0
        movaps    xmm0, xmm6
        shufps    xmm0, xmm7, 68
        subps     xmm4, xmm3
        shufps    xmm6, xmm7, 238
        movaps    xmm5, xmm2
        shufps    xmm5, xmm4, 68
        movaps    xmm3, xmm0
        addps     xmm3, xmm6
        shufps    xmm2, xmm4, 187
        subps     xmm0, xmm6
        movaps    xmm4, xmm5
        addps     xmm4, xmm2
        mov       edi, DWORD PTR [ebx+16]
        movaps    XMMWORD PTR [edi+eax*8], xmm3
        subps     xmm5, xmm2
        movaps    XMMWORD PTR [edx+eax*8], xmm4
        movaps    XMMWORD PTR [ecx+eax*8], xmm0
        movaps    XMMWORD PTR [esi+eax*8], xmm5
        add       eax, 2
        movzx     eax, ax
        movzx     edi, WORD PTR [ebx+8]
        cmp       eax, edi
        jl        fftloop

        mov       ebp, DWORD PTR [esp]

l1_is_zero:

        add       esp, 8
        pop       esi
        pop       edi
        mov       esp, ebx
        pop       ebx
        ret
    }
}
#endif

#if 0
static void passf4pos_sse_ido(const uint16_t ido, const uint16_t l1, const complex_t *cc,
                              complex_t *ch, const complex_t *wa1, const complex_t *wa2,
                              const complex_t *wa3)
{
    uint16_t i, k, ac, ah;

    for (k = 0; k < l1; k++)
    {
        ac = 4*k*ido;
        ah = k*ido;

        for (i = 0; i < ido; i+=2)
        {
            __m128 m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16;
            __m128 n1, n2, n3, n4, n5, n6, n7, n8, n9, m17, m18, m19, m20, m21, m22, m23;
            __m128 w1, w2, w3, w4, w5, w6, m24, m25, m26, m27, m28, m29, m30;
            __m128 neg1 = _mm_set_ps(-1.0, 1.0, -1.0, 1.0);

            m1 = _mm_load_ps(&RE(cc[ac+i]));
            m2 = _mm_load_ps(&RE(cc[ac+i+2*ido]));
            m3 = _mm_add_ps(m1, m2);
            m4 = _mm_sub_ps(m1, m2);

            n1 = _mm_load_ps(&RE(cc[ac+i+ido]));
            n2 = _mm_load_ps(&RE(cc[ac+i+3*ido]));
            n3 = _mm_add_ps(n1, n2);

            n4 = _mm_mul_ps(neg1, n1);
            n5 = _mm_mul_ps(neg1, n2);
            n6 = _mm_sub_ps(n4, n5);

            m5 = _mm_add_ps(m3, n3);

            n7 = _mm_shuffle_ps(n6, n6, _MM_SHUFFLE(2, 3, 0, 1));
            n8 = _mm_add_ps(m4, n7);

            m6 = _mm_sub_ps(m3, n3);
            n9 = _mm_sub_ps(m4, n7);

            _mm_store_ps(&RE(ch[ah+i]), m5);

#if 0
            static INLINE void ComplexMult(real_t *y1, real_t *y2,
                real_t x1, real_t x2, real_t c1, real_t c2)
            {
                *y1 = MUL_F(x1, c1) + MUL_F(x2, c2);
                *y2 = MUL_F(x2, c1) - MUL_F(x1, c2);
            }

            m7.0 = RE(c2)*RE(wa1[i])
            m7.1 = IM(c2)*IM(wa1[i])
            m7.2 = RE(c6)*RE(wa1[i+1])
            m7.3 = IM(c6)*IM(wa1[i+1])

            m8.0 = RE(c2)*IM(wa1[i])
            m8.1 = IM(c2)*RE(wa1[i])
            m8.2 = RE(c6)*IM(wa1[i+1])
            m8.3 = IM(c6)*RE(wa1[i+1])

            RE(0) = m7.0 - m7.1
            IM(0) = m8.0 + m8.1
            RE(1) = m7.2 - m7.3
            IM(1) = m8.2 + m8.3

            ////
            RE(0) = RE(c2)*RE(wa1[i])   - IM(c2)*IM(wa1[i])
            IM(0) = RE(c2)*IM(wa1[i])   + IM(c2)*RE(wa1[i])
            RE(1) = RE(c6)*RE(wa1[i+1]) - IM(c6)*IM(wa1[i+1])
            IM(1) = RE(c6)*IM(wa1[i+1]) + IM(c6)*RE(wa1[i+1])
#endif

            w1 = _mm_load_ps(&RE(wa1[i]));
            w3 = _mm_load_ps(&RE(wa2[i]));
            w5 = _mm_load_ps(&RE(wa3[i]));

            w2 = _mm_shuffle_ps(w1, w1, _MM_SHUFFLE(2, 3, 0, 1));
            w4 = _mm_shuffle_ps(w3, w3, _MM_SHUFFLE(2, 3, 0, 1));
            w6 = _mm_shuffle_ps(w5, w5, _MM_SHUFFLE(2, 3, 0, 1));

            m7 = _mm_mul_ps(n8, w1);
            m15 = _mm_mul_ps(m6, w3);
            m23 = _mm_mul_ps(n9, w5);
            m8 = _mm_mul_ps(n8, w2);
            m16 = _mm_mul_ps(m6, w4);
            m24 = _mm_mul_ps(n9, w6);

            m9  = _mm_shuffle_ps(m7, m8, _MM_SHUFFLE(2, 0, 2, 0));
            m17 = _mm_shuffle_ps(m15, m16, _MM_SHUFFLE(2, 0, 2, 0));
            m25 = _mm_shuffle_ps(m23, m24, _MM_SHUFFLE(2, 0, 2, 0));
            m10 = _mm_shuffle_ps(m7, m8, _MM_SHUFFLE(3, 1, 3, 1));
            m18 = _mm_shuffle_ps(m15, m16, _MM_SHUFFLE(3, 1, 3, 1));
            m26 = _mm_shuffle_ps(m23, m24, _MM_SHUFFLE(3, 1, 3, 1));

            m11 = _mm_add_ps(m9, m10);
            m19 = _mm_add_ps(m17, m18);
            m27 = _mm_add_ps(m25, m26);
            m12 = _mm_sub_ps(m9, m10);
            m20 = _mm_sub_ps(m17, m18);
            m28 = _mm_sub_ps(m25, m26);

            m13 = _mm_shuffle_ps(m11, m11, _MM_SHUFFLE(0, 0, 3, 2));
            m21 = _mm_shuffle_ps(m19, m19, _MM_SHUFFLE(0, 0, 3, 2));
            m29 = _mm_shuffle_ps(m27, m27, _MM_SHUFFLE(0, 0, 3, 2));
            m14 = _mm_unpacklo_ps(m12, m13);
            m22 = _mm_unpacklo_ps(m20, m21);
            m30 = _mm_unpacklo_ps(m28, m29);

            _mm_store_ps(&RE(ch[ah+i+l1*ido]), m14);
            _mm_store_ps(&RE(ch[ah+i+2*l1*ido]), m22);
            _mm_store_ps(&RE(ch[ah+i+3*l1*ido]), m30);
        }
    }
}
#endif

static void passf4pos(const uint16_t ido, const uint16_t l1, const complex_t *cc,
                      complex_t *ch, const complex_t *wa1, const complex_t *wa2,
                      const complex_t *wa3)
{
    uint16_t i, k, ac, ah;

    if (ido == 1)
    {
        for (k = 0; k < l1; k++)
        {
            complex_t t1, t2, t3, t4;

            ac = 4*k;
            ah = k;

            RE(t2) = RE(cc[ac])   + RE(cc[ac+2]);
            RE(t1) = RE(cc[ac])   - RE(cc[ac+2]);
            IM(t2) = IM(cc[ac])   + IM(cc[ac+2]);
            IM(t1) = IM(cc[ac])   - IM(cc[ac+2]);
            RE(t3) = RE(cc[ac+1]) + RE(cc[ac+3]);
            IM(t4) = RE(cc[ac+1]) - RE(cc[ac+3]);
            IM(t3) = IM(cc[ac+3]) + IM(cc[ac+1]);
            RE(t4) = IM(cc[ac+3]) - IM(cc[ac+1]);

            RE(ch[ah])      = RE(t2) + RE(t3);
            RE(ch[ah+2*l1]) = RE(t2) - RE(t3);

            IM(ch[ah])      = IM(t2) + IM(t3);
            IM(ch[ah+2*l1]) = IM(t2) - IM(t3);

            RE(ch[ah+l1])   = RE(t1) + RE(t4);
            RE(ch[ah+3*l1]) = RE(t1) - RE(t4);

            IM(ch[ah+l1])   = IM(t1) + IM(t4);
            IM(ch[ah+3*l1]) = IM(t1) - IM(t4);
        }
    } else {
        for (k = 0; k < l1; k++)
        {
            ac = 4*k*ido;
            ah = k*ido;

            for (i = 0; i < ido; i++)
            {
                complex_t c2, c3, c4, t1, t2, t3, t4;

                RE(t2) = RE(cc[ac+i]) + RE(cc[ac+i+2*ido]);
                RE(t1) = RE(cc[ac+i]) - RE(cc[ac+i+2*ido]);
                IM(t2) = IM(cc[ac+i]) + IM(cc[ac+i+2*ido]);
                IM(t1) = IM(cc[ac+i]) - IM(cc[ac+i+2*ido]);
                RE(t3) = RE(cc[ac+i+ido]) + RE(cc[ac+i+3*ido]);
                IM(t4) = RE(cc[ac+i+ido]) - RE(cc[ac+i+3*ido]);
                IM(t3) = IM(cc[ac+i+3*ido]) + IM(cc[ac+i+ido]);
                RE(t4) = IM(cc[ac+i+3*ido]) - IM(cc[ac+i+ido]);

                RE(c2) = RE(t1) + RE(t4);
                RE(c4) = RE(t1) - RE(t4);

                IM(c2) = IM(t1) + IM(t4);
                IM(c4) = IM(t1) - IM(t4);

                RE(ch[ah+i]) = RE(t2) + RE(t3);
                RE(c3)       = RE(t2) - RE(t3);

                IM(ch[ah+i]) = IM(t2) + IM(t3);
                IM(c3)       = IM(t2) - IM(t3);

#if 1
                ComplexMult(&IM(ch[ah+i+l1*ido]), &RE(ch[ah+i+l1*ido]),
                    IM(c2), RE(c2), RE(wa1[i]), IM(wa1[i]));
                ComplexMult(&IM(ch[ah+i+2*l1*ido]), &RE(ch[ah+i+2*l1*ido]),
                    IM(c3), RE(c3), RE(wa2[i]), IM(wa2[i]));
                ComplexMult(&IM(ch[ah+i+3*l1*ido]), &RE(ch[ah+i+3*l1*ido]),
                    IM(c4), RE(c4), RE(wa3[i]), IM(wa3[i]));
#else
                ComplexMult(&RE(ch[ah+i+l1*ido]), &IM(ch[ah+i+l1*ido]),
                    RE(c2), IM(c2), RE(wa1[i]), IM(wa1[i]));
                ComplexMult(&RE(ch[ah+i+2*l1*ido]), &IM(ch[ah+i+2*l1*ido]),
                    RE(c3), IM(c3), RE(wa2[i]), IM(wa2[i]));
                ComplexMult(&RE(ch[ah+i+3*l1*ido]), &IM(ch[ah+i+3*l1*ido]),
                    RE(c4), IM(c4), RE(wa3[i]), IM(wa3[i]));
#endif
            }
        }
    }
}

static void passf4neg(const uint16_t ido, const uint16_t l1, const complex_t *cc,
                      complex_t *ch, const complex_t *wa1, const complex_t *wa2,
                      const complex_t *wa3)
{
    uint16_t i, k, ac, ah;

    if (ido == 1)
    {
        for (k = 0; k < l1; k++)
        {
            complex_t t1, t2, t3, t4;

            ac = 4*k;
            ah = k;

            RE(t2) = RE(cc[ac])   + RE(cc[ac+2]);
            RE(t1) = RE(cc[ac])   - RE(cc[ac+2]);
            IM(t2) = IM(cc[ac])   + IM(cc[ac+2]);
            IM(t1) = IM(cc[ac])   - IM(cc[ac+2]);
            RE(t3) = RE(cc[ac+1]) + RE(cc[ac+3]);
            IM(t4) = RE(cc[ac+1]) - RE(cc[ac+3]);
            IM(t3) = IM(cc[ac+3]) + IM(cc[ac+1]);
            RE(t4) = IM(cc[ac+3]) - IM(cc[ac+1]);

            RE(ch[ah])      = RE(t2) + RE(t3);
            RE(ch[ah+2*l1]) = RE(t2) - RE(t3);

            IM(ch[ah])      = IM(t2) + IM(t3);
            IM(ch[ah+2*l1]) = IM(t2) - IM(t3);

            RE(ch[ah+l1])   = RE(t1) - RE(t4);
            RE(ch[ah+3*l1]) = RE(t1) + RE(t4);

            IM(ch[ah+l1])   = IM(t1) - IM(t4);
            IM(ch[ah+3*l1]) = IM(t1) + IM(t4);
        }
    } else {
        for (k = 0; k < l1; k++)
        {
            ac = 4*k*ido;
            ah = k*ido;

            for (i = 0; i < ido; i++)
            {
                complex_t c2, c3, c4, t1, t2, t3, t4;

                RE(t2) = RE(cc[ac+i]) + RE(cc[ac+i+2*ido]);
                RE(t1) = RE(cc[ac+i]) - RE(cc[ac+i+2*ido]);
                IM(t2) = IM(cc[ac+i]) + IM(cc[ac+i+2*ido]);
                IM(t1) = IM(cc[ac+i]) - IM(cc[ac+i+2*ido]);
                RE(t3) = RE(cc[ac+i+ido]) + RE(cc[ac+i+3*ido]);
                IM(t4) = RE(cc[ac+i+ido]) - RE(cc[ac+i+3*ido]);
                IM(t3) = IM(cc[ac+i+3*ido]) + IM(cc[ac+i+ido]);
                RE(t4) = IM(cc[ac+i+3*ido]) - IM(cc[ac+i+ido]);

                RE(c2) = RE(t1) - RE(t4);
                RE(c4) = RE(t1) + RE(t4);

                IM(c2) = IM(t1) - IM(t4);
                IM(c4) = IM(t1) + IM(t4);

                RE(ch[ah+i]) = RE(t2) + RE(t3);
                RE(c3)       = RE(t2) - RE(t3);

                IM(ch[ah+i]) = IM(t2) + IM(t3);
                IM(c3)       = IM(t2) - IM(t3);

#if 1
                ComplexMult(&RE(ch[ah+i+l1*ido]), &IM(ch[ah+i+l1*ido]),
                    RE(c2), IM(c2), RE(wa1[i]), IM(wa1[i]));
                ComplexMult(&RE(ch[ah+i+2*l1*ido]), &IM(ch[ah+i+2*l1*ido]),
                    RE(c3), IM(c3), RE(wa2[i]), IM(wa2[i]));
                ComplexMult(&RE(ch[ah+i+3*l1*ido]), &IM(ch[ah+i+3*l1*ido]),
                    RE(c4), IM(c4), RE(wa3[i]), IM(wa3[i]));
#else
                ComplexMult(&IM(ch[ah+i+l1*ido]), &RE(ch[ah+i+l1*ido]),
                    IM(c2), RE(c2), RE(wa1[i]), IM(wa1[i]));
                ComplexMult(&IM(ch[ah+i+2*l1*ido]), &RE(ch[ah+i+2*l1*ido]),
                    IM(c3), RE(c3), RE(wa2[i]), IM(wa2[i]));
                ComplexMult(&IM(ch[ah+i+3*l1*ido]), &RE(ch[ah+i+3*l1*ido]),
                    IM(c4), RE(c4), RE(wa3[i]), IM(wa3[i]));
#endif
            }
        }
    }
}

static void passf5(const uint16_t ido, const uint16_t l1, const complex_t *cc,
                   complex_t *ch, const complex_t *wa1, const complex_t *wa2, const complex_t *wa3,
                   const complex_t *wa4, const int8_t isign)
{
    static real_t tr11 = FRAC_CONST(0.309016994374947);
    static real_t ti11 = FRAC_CONST(0.951056516295154);
    static real_t tr12 = FRAC_CONST(-0.809016994374947);
    static real_t ti12 = FRAC_CONST(0.587785252292473);
    uint16_t i, k, ac, ah;
    complex_t c2, c3, c4, c5, d3, d4, d5, d2, t2, t3, t4, t5;

    if (ido == 1)
    {
        if (isign == 1)
        {
            for (k = 0; k < l1; k++)
            {
                ac = 5*k + 1;
                ah = k;

                RE(t2) = RE(cc[ac]) + RE(cc[ac+3]);
                IM(t2) = IM(cc[ac]) + IM(cc[ac+3]);
                RE(t3) = RE(cc[ac+1]) + RE(cc[ac+2]);
                IM(t3) = IM(cc[ac+1]) + IM(cc[ac+2]);
                RE(t4) = RE(cc[ac+1]) - RE(cc[ac+2]);
                IM(t4) = IM(cc[ac+1]) - IM(cc[ac+2]);
                RE(t5) = RE(cc[ac]) - RE(cc[ac+3]);
                IM(t5) = IM(cc[ac]) - IM(cc[ac+3]);

                RE(ch[ah]) = RE(cc[ac-1]) + RE(t2) + RE(t3);
                IM(ch[ah]) = IM(cc[ac-1]) + IM(t2) + IM(t3);

                RE(c2) = RE(cc[ac-1]) + MUL_F(RE(t2),tr11) + MUL_F(RE(t3),tr12);
                IM(c2) = IM(cc[ac-1]) + MUL_F(IM(t2),tr11) + MUL_F(IM(t3),tr12);
                RE(c3) = RE(cc[ac-1]) + MUL_F(RE(t2),tr12) + MUL_F(RE(t3),tr11);
                IM(c3) = IM(cc[ac-1]) + MUL_F(IM(t2),tr12) + MUL_F(IM(t3),tr11);

                ComplexMult(&RE(c5), &RE(c4),
                    ti11, ti12, RE(t5), RE(t4));
                ComplexMult(&IM(c5), &IM(c4),
                    ti11, ti12, IM(t5), IM(t4));

                RE(ch[ah+l1]) = RE(c2) - IM(c5);
                IM(ch[ah+l1]) = IM(c2) + RE(c5);
                RE(ch[ah+2*l1]) = RE(c3) - IM(c4);
                IM(ch[ah+2*l1]) = IM(c3) + RE(c4);
                RE(ch[ah+3*l1]) = RE(c3) + IM(c4);
                IM(ch[ah+3*l1]) = IM(c3) - RE(c4);
                RE(ch[ah+4*l1]) = RE(c2) + IM(c5);
                IM(ch[ah+4*l1]) = IM(c2) - RE(c5);
            }
        } else {
            for (k = 0; k < l1; k++)
            {
                ac = 5*k + 1;
                ah = k;

                RE(t2) = RE(cc[ac]) + RE(cc[ac+3]);
                IM(t2) = IM(cc[ac]) + IM(cc[ac+3]);
                RE(t3) = RE(cc[ac+1]) + RE(cc[ac+2]);
                IM(t3) = IM(cc[ac+1]) + IM(cc[ac+2]);
                RE(t4) = RE(cc[ac+1]) - RE(cc[ac+2]);
                IM(t4) = IM(cc[ac+1]) - IM(cc[ac+2]);
                RE(t5) = RE(cc[ac]) - RE(cc[ac+3]);
                IM(t5) = IM(cc[ac]) - IM(cc[ac+3]);

                RE(ch[ah]) = RE(cc[ac-1]) + RE(t2) + RE(t3);
                IM(ch[ah]) = IM(cc[ac-1]) + IM(t2) + IM(t3);

                RE(c2) = RE(cc[ac-1]) + MUL_F(RE(t2),tr11) + MUL_F(RE(t3),tr12);
                IM(c2) = IM(cc[ac-1]) + MUL_F(IM(t2),tr11) + MUL_F(IM(t3),tr12);
                RE(c3) = RE(cc[ac-1]) + MUL_F(RE(t2),tr12) + MUL_F(RE(t3),tr11);
                IM(c3) = IM(cc[ac-1]) + MUL_F(IM(t2),tr12) + MUL_F(IM(t3),tr11);

                ComplexMult(&RE(c4), &RE(c5),
                    ti12, ti11, RE(t5), RE(t4));
                ComplexMult(&IM(c4), &IM(c5),
                    ti12, ti12, IM(t5), IM(t4));

                RE(ch[ah+l1]) = RE(c2) + IM(c5);
                IM(ch[ah+l1]) = IM(c2) - RE(c5);
                RE(ch[ah+2*l1]) = RE(c3) + IM(c4);
                IM(ch[ah+2*l1]) = IM(c3) - RE(c4);
                RE(ch[ah+3*l1]) = RE(c3) - IM(c4);
                IM(ch[ah+3*l1]) = IM(c3) + RE(c4);
                RE(ch[ah+4*l1]) = RE(c2) - IM(c5);
                IM(ch[ah+4*l1]) = IM(c2) + RE(c5);
            }
        }
    } else {
        if (isign == 1)
        {
            for (k = 0; k < l1; k++)
            {
                for (i = 0; i < ido; i++)
                {
                    ac = i + (k*5 + 1) * ido;
                    ah = i + k * ido;

                    RE(t2) = RE(cc[ac]) + RE(cc[ac+3*ido]);
                    IM(t2) = IM(cc[ac]) + IM(cc[ac+3*ido]);
                    RE(t3) = RE(cc[ac+ido]) + RE(cc[ac+2*ido]);
                    IM(t3) = IM(cc[ac+ido]) + IM(cc[ac+2*ido]);
                    RE(t4) = RE(cc[ac+ido]) - RE(cc[ac+2*ido]);
                    IM(t4) = IM(cc[ac+ido]) - IM(cc[ac+2*ido]);
                    RE(t5) = RE(cc[ac]) - RE(cc[ac+3*ido]);
                    IM(t5) = IM(cc[ac]) - IM(cc[ac+3*ido]);

                    RE(ch[ah]) = RE(cc[ac-ido]) + RE(t2) + RE(t3);
                    IM(ch[ah]) = IM(cc[ac-ido]) + IM(t2) + IM(t3);

                    RE(c2) = RE(cc[ac-ido]) + MUL_F(RE(t2),tr11) + MUL_F(RE(t3),tr12);
                    IM(c2) = IM(cc[ac-ido]) + MUL_F(IM(t2),tr11) + MUL_F(IM(t3),tr12);
                    RE(c3) = RE(cc[ac-ido]) + MUL_F(RE(t2),tr12) + MUL_F(RE(t3),tr11);
                    IM(c3) = IM(cc[ac-ido]) + MUL_F(IM(t2),tr12) + MUL_F(IM(t3),tr11);

                    ComplexMult(&RE(c5), &RE(c4),
                        ti11, ti12, RE(t5), RE(t4));
                    ComplexMult(&IM(c5), &IM(c4),
                        ti11, ti12, IM(t5), IM(t4));

                    IM(d2) = IM(c2) + RE(c5);
                    IM(d3) = IM(c3) + RE(c4);
                    RE(d4) = RE(c3) + IM(c4);
                    RE(d5) = RE(c2) + IM(c5);
                    RE(d2) = RE(c2) - IM(c5);
                    IM(d5) = IM(c2) - RE(c5);
                    RE(d3) = RE(c3) - IM(c4);
                    IM(d4) = IM(c3) - RE(c4);

#if 1
                    ComplexMult(&IM(ch[ah+l1*ido]), &RE(ch[ah+l1*ido]),
                        IM(d2), RE(d2), RE(wa1[i]), IM(wa1[i]));
                    ComplexMult(&IM(ch[ah+2*l1*ido]), &RE(ch[ah+2*l1*ido]),
                        IM(d3), RE(d3), RE(wa2[i]), IM(wa2[i]));
                    ComplexMult(&IM(ch[ah+3*l1*ido]), &RE(ch[ah+3*l1*ido]),
                        IM(d4), RE(d4), RE(wa3[i]), IM(wa3[i]));
                    ComplexMult(&IM(ch[ah+4*l1*ido]), &RE(ch[ah+4*l1*ido]),
                        IM(d5), RE(d5), RE(wa4[i]), IM(wa4[i]));
#else
                    ComplexMult(&RE(ch[ah+l1*ido]), &IM(ch[ah+l1*ido]),
                        RE(d2), IM(d2), RE(wa1[i]), IM(wa1[i]));
                    ComplexMult(&RE(ch[ah+2*l1*ido]), &IM(ch[ah+2*l1*ido]),
                        RE(d3), IM(d3), RE(wa2[i]), IM(wa2[i]));
                    ComplexMult(&RE(ch[ah+3*l1*ido]), &IM(ch[ah+3*l1*ido]),
                        RE(d4), IM(d4), RE(wa3[i]), IM(wa3[i]));
                    ComplexMult(&RE(ch[ah+4*l1*ido]), &IM(ch[ah+4*l1*ido]),
                        RE(d5), IM(d5), RE(wa4[i]), IM(wa4[i]));
#endif
                }
            }
        } else {
            for (k = 0; k < l1; k++)
            {
                for (i = 0; i < ido; i++)
                {
                    ac = i + (k*5 + 1) * ido;
                    ah = i + k * ido;

                    RE(t2) = RE(cc[ac]) + RE(cc[ac+3*ido]);
                    IM(t2) = IM(cc[ac]) + IM(cc[ac+3*ido]);
                    RE(t3) = RE(cc[ac+ido]) + RE(cc[ac+2*ido]);
                    IM(t3) = IM(cc[ac+ido]) + IM(cc[ac+2*ido]);
                    RE(t4) = RE(cc[ac+ido]) - RE(cc[ac+2*ido]);
                    IM(t4) = IM(cc[ac+ido]) - IM(cc[ac+2*ido]);
                    RE(t5) = RE(cc[ac]) - RE(cc[ac+3*ido]);
                    IM(t5) = IM(cc[ac]) - IM(cc[ac+3*ido]);

                    RE(ch[ah]) = RE(cc[ac-ido]) + RE(t2) + RE(t3);
                    IM(ch[ah]) = IM(cc[ac-ido]) + IM(t2) + IM(t3);

                    RE(c2) = RE(cc[ac-ido]) + MUL_F(RE(t2),tr11) + MUL_F(RE(t3),tr12);
                    IM(c2) = IM(cc[ac-ido]) + MUL_F(IM(t2),tr11) + MUL_F(IM(t3),tr12);
                    RE(c3) = RE(cc[ac-ido]) + MUL_F(RE(t2),tr12) + MUL_F(RE(t3),tr11);
                    IM(c3) = IM(cc[ac-ido]) + MUL_F(IM(t2),tr12) + MUL_F(IM(t3),tr11);

                    ComplexMult(&RE(c4), &RE(c5),
                        ti12, ti11, RE(t5), RE(t4));
                    ComplexMult(&IM(c4), &IM(c5),
                        ti12, ti12, IM(t5), IM(t4));

                    IM(d2) = IM(c2) - RE(c5);
                    IM(d3) = IM(c3) - RE(c4);
                    RE(d4) = RE(c3) - IM(c4);
                    RE(d5) = RE(c2) - IM(c5);
                    RE(d2) = RE(c2) + IM(c5);
                    IM(d5) = IM(c2) + RE(c5);
                    RE(d3) = RE(c3) + IM(c4);
                    IM(d4) = IM(c3) + RE(c4);

#if 1
                    ComplexMult(&RE(ch[ah+l1*ido]), &IM(ch[ah+l1*ido]),
                        RE(d2), IM(d2), RE(wa1[i]), IM(wa1[i]));
                    ComplexMult(&RE(ch[ah+2*l1*ido]), &IM(ch[ah+2*l1*ido]),
                        RE(d3), IM(d3), RE(wa2[i]), IM(wa2[i]));
                    ComplexMult(&RE(ch[ah+3*l1*ido]), &IM(ch[ah+3*l1*ido]),
                        RE(d4), IM(d4), RE(wa3[i]), IM(wa3[i]));
                    ComplexMult(&RE(ch[ah+4*l1*ido]), &IM(ch[ah+4*l1*ido]),
                        RE(d5), IM(d5), RE(wa4[i]), IM(wa4[i]));
#else
                    ComplexMult(&IM(ch[ah+l1*ido]), &RE(ch[ah+l1*ido]),
                        IM(d2), RE(d2), RE(wa1[i]), IM(wa1[i]));
                    ComplexMult(&IM(ch[ah+2*l1*ido]), &RE(ch[ah+2*l1*ido]),
                        IM(d3), RE(d3), RE(wa2[i]), IM(wa2[i]));
                    ComplexMult(&IM(ch[ah+3*l1*ido]), &RE(ch[ah+3*l1*ido]),
                        IM(d4), RE(d4), RE(wa3[i]), IM(wa3[i]));
                    ComplexMult(&IM(ch[ah+4*l1*ido]), &RE(ch[ah+4*l1*ido]),
                        IM(d5), RE(d5), RE(wa4[i]), IM(wa4[i]));
#endif
                }
            }
        }
    }
}


/*----------------------------------------------------------------------
   cfftf1, cfftf, cfftb, cffti1, cffti. Complex FFTs.
  ----------------------------------------------------------------------*/

#ifdef USE_SSE

#define CONV(A,B,C) ( (A<<2) | ((B & 0x1)<<1) | ((C==1)&0x1) )

static INLINE void cfftf1pos_sse(uint16_t n, complex_t *c, complex_t *ch,
                                 const uint16_t *ifac, const complex_t *wa,
                                 const int8_t isign)
{
    uint16_t i;
    uint16_t k1, l1, l2;
    uint16_t na, nf, ip, iw, ix2, ix3, ix4, ido, idl1;

    nf = ifac[1];
    na = 0;
    l1 = 1;
    iw = 0;

    for (k1 = 2; k1 <= nf+1; k1++)
    {
        ip = ifac[k1];
        l2 = ip*l1;
        ido = n / l2;
        idl1 = ido*l1;

        ix2 = iw + ido;
        ix3 = ix2 + ido;
        ix4 = ix3 + ido;

        switch (CONV(ip,na,ido))
        {
        case CONV(4,0,0):
            //passf4pos_sse_ido((const uint16_t)ido, (const uint16_t)l1, (const complex_t*)c, ch, &wa[iw], &wa[ix2], &wa[ix3]);
            passf4pos((const uint16_t)ido, (const uint16_t)l1, (const complex_t*)c, ch, &wa[iw], &wa[ix2], &wa[ix3]);
            break;
        case CONV(4,0,1):
            passf4pos_sse((const uint16_t)l1, (const complex_t*)c, ch, &wa[iw], &wa[ix2], &wa[ix3]);
            break;
        case CONV(4,1,0):
            passf4pos((const uint16_t)ido, (const uint16_t)l1, (const complex_t*)ch, c, &wa[iw], &wa[ix2], &wa[ix3]);
            //passf4pos_sse_ido((const uint16_t)ido, (const uint16_t)l1, (const complex_t*)ch, c, &wa[iw], &wa[ix2], &wa[ix3]);
            break;
        case CONV(4,1,1):
            passf4pos_sse((const uint16_t)l1, (const complex_t*)ch, c, &wa[iw], &wa[ix2], &wa[ix3]);
            break;
        case CONV(2,0,0):
            passf2pos((const uint16_t)ido, (const uint16_t)l1, (const complex_t*)c, ch, &wa[iw]);
            //passf2pos_sse_ido((const uint16_t)ido, (const uint16_t)l1, (const complex_t*)c, ch, &wa[iw]);
            break;
        case CONV(2,0,1):
            passf2pos((const uint16_t)ido, (const uint16_t)l1, (const complex_t*)c, ch, &wa[iw]);
            //passf2pos_sse((const uint16_t)l1, (const complex_t*)c, ch, &wa[iw]);
            break;
        case CONV(2,1,0):
            passf2pos((const uint16_t)ido, (const uint16_t)l1, (const complex_t*)ch, c, &wa[iw]);
            //passf2pos_sse_ido((const uint16_t)ido, (const uint16_t)l1, (const complex_t*)ch, c, &wa[iw]);
            break;
        case CONV(2,1,1):
            passf2pos((const uint16_t)ido, (const uint16_t)l1, (const complex_t*)ch, c, &wa[iw]);
            //passf2pos_sse((const uint16_t)l1, (const complex_t*)ch, c, &wa[iw]);
            break;
        case CONV(3,0,0):
        case CONV(3,0,1):
            passf3((const uint16_t)ido, (const uint16_t)l1, (const complex_t*)c, ch, &wa[iw], &wa[ix2], isign);
            break;
        case CONV(3,1,0):
        case CONV(3,1,1):
            passf3((const uint16_t)ido, (const uint16_t)l1, (const complex_t*)ch, c, &wa[iw], &wa[ix2], isign);
            break;
        case CONV(5,0,0):
        case CONV(5,0,1):
            passf5((const uint16_t)ido, (const uint16_t)l1, (const complex_t*)c, ch, &wa[iw], &wa[ix2], &wa[ix3], &wa[ix4], isign);
            break;
        case CONV(5,1,0):
        case CONV(5,1,1):
            passf5((const uint16_t)ido, (const uint16_t)l1, (const complex_t*)ch, c, &wa[iw], &wa[ix2], &wa[ix3], &wa[ix4], isign);
            break;
        }

        na = 1 - na;

        l1 = l2;
        iw += (ip-1) * ido;
    }

    if (na == 0)
        return;

    for (i = 0; i < n; i++)
    {
        RE(c[i]) = RE(ch[i]);
        IM(c[i]) = IM(ch[i]);
    }
}
#endif

static INLINE void cfftf1pos(uint16_t n, complex_t *c, complex_t *ch,
                             const uint16_t *ifac, const complex_t *wa,
                             const int8_t isign)
{
    uint16_t i;
    uint16_t k1, l1, l2;
    uint16_t na, nf, ip, iw, ix2, ix3, ix4, ido, idl1;

    nf = ifac[1];
    na = 0;
    l1 = 1;
    iw = 0;

    for (k1 = 2; k1 <= nf+1; k1++)
    {
        ip = ifac[k1];
        l2 = ip*l1;
        ido = n / l2;
        idl1 = ido*l1;

        switch (ip)
        {
        case 4:
            ix2 = iw + ido;
            ix3 = ix2 + ido;

            if (na == 0)
                passf4pos((const uint16_t)ido, (const uint16_t)l1, (const complex_t*)c, ch, &wa[iw], &wa[ix2], &wa[ix3]);
            else
                passf4pos((const uint16_t)ido, (const uint16_t)l1, (const complex_t*)ch, c, &wa[iw], &wa[ix2], &wa[ix3]);

            na = 1 - na;
            break;
        case 2:
            if (na == 0)
                passf2pos((const uint16_t)ido, (const uint16_t)l1, (const complex_t*)c, ch, &wa[iw]);
            else
                passf2pos((const uint16_t)ido, (const uint16_t)l1, (const complex_t*)ch, c, &wa[iw]);

            na = 1 - na;
            break;
        case 3:
            ix2 = iw + ido;

            if (na == 0)
                passf3((const uint16_t)ido, (const uint16_t)l1, (const complex_t*)c, ch, &wa[iw], &wa[ix2], isign);
            else
                passf3((const uint16_t)ido, (const uint16_t)l1, (const complex_t*)ch, c, &wa[iw], &wa[ix2], isign);

            na = 1 - na;
            break;
        case 5:
            ix2 = iw + ido;
            ix3 = ix2 + ido;
            ix4 = ix3 + ido;

            if (na == 0)
                passf5((const uint16_t)ido, (const uint16_t)l1, (const complex_t*)c, ch, &wa[iw], &wa[ix2], &wa[ix3], &wa[ix4], isign);
            else
                passf5((const uint16_t)ido, (const uint16_t)l1, (const complex_t*)ch, c, &wa[iw], &wa[ix2], &wa[ix3], &wa[ix4], isign);

            na = 1 - na;
            break;
        }

        l1 = l2;
        iw += (ip-1) * ido;
    }

    if (na == 0)
        return;

    for (i = 0; i < n; i++)
    {
        RE(c[i]) = RE(ch[i]);
        IM(c[i]) = IM(ch[i]);
    }
}

static INLINE void cfftf1neg(uint16_t n, complex_t *c, complex_t *ch,
                             const uint16_t *ifac, const complex_t *wa,
                             const int8_t isign)
{
    uint16_t i;
    uint16_t k1, l1, l2;
    uint16_t na, nf, ip, iw, ix2, ix3, ix4, ido, idl1;

    nf = ifac[1];
    na = 0;
    l1 = 1;
    iw = 0;

    for (k1 = 2; k1 <= nf+1; k1++)
    {
        ip = ifac[k1];
        l2 = ip*l1;
        ido = n / l2;
        idl1 = ido*l1;

        switch (ip)
        {
        case 4:
            ix2 = iw + ido;
            ix3 = ix2 + ido;

            if (na == 0)
                passf4neg((const uint16_t)ido, (const uint16_t)l1, (const complex_t*)c, ch, &wa[iw], &wa[ix2], &wa[ix3]);
            else
                passf4neg((const uint16_t)ido, (const uint16_t)l1, (const complex_t*)ch, c, &wa[iw], &wa[ix2], &wa[ix3]);

            na = 1 - na;
            break;
        case 2:
            if (na == 0)
                passf2neg((const uint16_t)ido, (const uint16_t)l1, (const complex_t*)c, ch, &wa[iw]);
            else
                passf2neg((const uint16_t)ido, (const uint16_t)l1, (const complex_t*)ch, c, &wa[iw]);

            na = 1 - na;
            break;
        case 3:
            ix2 = iw + ido;

            if (na == 0)
                passf3((const uint16_t)ido, (const uint16_t)l1, (const complex_t*)c, ch, &wa[iw], &wa[ix2], isign);
            else
                passf3((const uint16_t)ido, (const uint16_t)l1, (const complex_t*)ch, c, &wa[iw], &wa[ix2], isign);

            na = 1 - na;
            break;
        case 5:
            ix2 = iw + ido;
            ix3 = ix2 + ido;
            ix4 = ix3 + ido;

            if (na == 0)
                passf5((const uint16_t)ido, (const uint16_t)l1, (const complex_t*)c, ch, &wa[iw], &wa[ix2], &wa[ix3], &wa[ix4], isign);
            else
                passf5((const uint16_t)ido, (const uint16_t)l1, (const complex_t*)ch, c, &wa[iw], &wa[ix2], &wa[ix3], &wa[ix4], isign);

            na = 1 - na;
            break;
        }

        l1 = l2;
        iw += (ip-1) * ido;
    }

    if (na == 0)
        return;

    for (i = 0; i < n; i++)
    {
        RE(c[i]) = RE(ch[i]);
        IM(c[i]) = IM(ch[i]);
    }
}

void cfftf(cfft_info *cfft, complex_t *c)
{
    cfftf1neg(cfft->n, c, cfft->work, (const uint16_t*)cfft->ifac, (const complex_t*)cfft->tab, -1);
}

void cfftb(cfft_info *cfft, complex_t *c)
{
    cfftf1pos(cfft->n, c, cfft->work, (const uint16_t*)cfft->ifac, (const complex_t*)cfft->tab, +1);
}

#ifdef USE_SSE
void cfftb_sse(cfft_info *cfft, complex_t *c)
{
    cfftf1pos_sse(cfft->n, c, cfft->work, (const uint16_t*)cfft->ifac, (const complex_t*)cfft->tab, +1);
}
#endif

static void cffti1(uint16_t n, complex_t *wa, uint16_t *ifac)
{
    static uint16_t ntryh[4] = {3, 4, 2, 5};
#ifndef FIXED_POINT
    real_t arg, argh, argld, fi;
    uint16_t ido, ipm;
    uint16_t i1, k1, l1, l2;
    uint16_t ld, ii, ip;
#endif
    uint16_t ntry = 0, i, j;
    uint16_t ib;
    uint16_t nf, nl, nq, nr;

    nl = n;
    nf = 0;
    j = 0;

startloop:
    j++;

    if (j <= 4)
        ntry = ntryh[j-1];
    else
        ntry += 2;

    do
    {
        nq = nl / ntry;
        nr = nl - ntry*nq;

        if (nr != 0)
            goto startloop;

        nf++;
        ifac[nf+1] = ntry;
        nl = nq;

        if (ntry == 2 && nf != 1)
        {
            for (i = 2; i <= nf; i++)
            {
                ib = nf - i + 2;
                ifac[ib+1] = ifac[ib];
            }
            ifac[2] = 2;
        }
    } while (nl != 1);

    ifac[0] = n;
    ifac[1] = nf;

#ifndef FIXED_POINT
    argh = (real_t)2.0*(real_t)M_PI / (real_t)n;
    i = 0;
    l1 = 1;

    for (k1 = 1; k1 <= nf; k1++)
    {
        ip = ifac[k1+1];
        ld = 0;
        l2 = l1*ip;
        ido = n / l2;
        ipm = ip - 1;

        for (j = 0; j < ipm; j++)
        {
            i1 = i;
            RE(wa[i]) = 1.0;
            IM(wa[i]) = 0.0;
            ld += l1;
            fi = 0;
            argld = ld*argh;

            for (ii = 0; ii < ido; ii++)
            {
                i++;
                fi++;
                arg = fi * argld;
                RE(wa[i]) = (real_t)cos(arg);
#if 1
                IM(wa[i]) = (real_t)sin(arg);
#else
                IM(wa[i]) = (real_t)-sin(arg);
#endif
            }

            if (ip > 5)
            {
                RE(wa[i1]) = RE(wa[i]);
                IM(wa[i1]) = IM(wa[i]);
            }
        }
        l1 = l2;
    }
#endif
}

cfft_info *cffti(uint16_t n)
{
    cfft_info *cfft = (cfft_info*)faad_malloc(sizeof(cfft_info));

    cfft->n = n;
    cfft->work = (complex_t*)faad_malloc(n*sizeof(complex_t));

#ifndef FIXED_POINT
    cfft->tab = (complex_t*)faad_malloc(n*sizeof(complex_t));

    cffti1(n, cfft->tab, cfft->ifac);
#else
    cffti1(n, NULL, cfft->ifac);

    switch (n)
    {
    case 64: cfft->tab = (complex_t*)cfft_tab_64; break;
    case 512: cfft->tab = (complex_t*)cfft_tab_512; break;
#ifdef LD_DEC
    case 256: cfft->tab = (complex_t*)cfft_tab_256; break;
#endif

#ifdef ALLOW_SMALL_FRAMELENGTH
    case 60: cfft->tab = (complex_t*)cfft_tab_60; break;
    case 480: cfft->tab = (complex_t*)cfft_tab_480; break;
#ifdef LD_DEC
    case 240: cfft->tab = (complex_t*)cfft_tab_240; break;
#endif
#endif
    }
#endif

    return cfft;
}

void cfftu(cfft_info *cfft)
{
    if (cfft->work) faad_free(cfft->work);
#ifndef FIXED_POINT
    if (cfft->tab) faad_free(cfft->tab);
#endif

    if (cfft) faad_free(cfft);
}

