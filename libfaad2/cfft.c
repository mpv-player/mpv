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
** $Id: cfft.c,v 1.30 2004/09/08 09:43:11 gcp Exp $
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
    case 128: cfft->tab = (complex_t*)cfft_tab_128; break;
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

