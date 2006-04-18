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
** $Id: ssr_ipqf.c,v 1.14 2004/09/04 14:56:29 menno Exp $
**/

#include "common.h"
#include "structs.h"

#ifdef SSR_DEC

#include "ssr.h"
#include "ssr_ipqf.h"

static real_t **app_pqfbuf;
static real_t **pp_q0, **pp_t0, **pp_t1;

void gc_set_protopqf(real_t *p_proto)
{
    int	j;
    static real_t a_half[48] =
    {
        1.2206911375946939E-05,  1.7261986723798209E-05,  1.2300093657077942E-05,
        -1.0833943097791965E-05, -5.7772498639901686E-05, -1.2764767618947719E-04,
        -2.0965186675013334E-04, -2.8166673689263850E-04, -3.1234860429017460E-04,
        -2.6738519958452353E-04, -1.1949424681824722E-04,  1.3965139412648678E-04,
        4.8864136409185725E-04,  8.7044629275148344E-04,  1.1949430269934793E-03,
        1.3519708175026700E-03,  1.2346314373964412E-03,  7.6953209114159191E-04,
        -5.2242432579537141E-05, -1.1516092887213454E-03, -2.3538469841711277E-03,
        -3.4033123072127277E-03, -4.0028551071986133E-03, -3.8745415659693259E-03,
        -2.8321073426874310E-03, -8.5038892323704195E-04,  1.8856751185350931E-03,
        4.9688741735340923E-03,  7.8056704536795926E-03,  9.7027909685901654E-03,
        9.9960423120166159E-03,  8.2019366335594487E-03,  4.1642072876103365E-03,
        -1.8364453822737758E-03, -9.0384863094167686E-03, -1.6241528177129844E-02,
        -2.1939551286300665E-02, -2.4533179947088161E-02, -2.2591663337768787E-02,
        -1.5122066420044672E-02, -1.7971713448186293E-03,  1.6903413428575379E-02,
        3.9672315874127042E-02,  6.4487527248102796E-02,  8.8850025474701726E-02,
        0.1101132906105560    ,  0.1258540205143761    ,  0.1342239368467012    
    };

    for (j = 0; j < 48; ++j)
    {
        p_proto[j] = p_proto[95-j] = a_half[j];
    }
}

void gc_setcoef_eff_pqfsyn(int mm,
                           int kk,
                           real_t *p_proto,
                           real_t ***ppp_q0,
                           real_t ***ppp_t0,
                           real_t ***ppp_t1)
{
    int	i, k, n;
    real_t	w;

    /* Set 1st Mul&Acc Coef's */
    *ppp_q0 = (real_t **) calloc(mm, sizeof(real_t *));
    for (n = 0; n < mm; ++n)
    {
        (*ppp_q0)[n] = (real_t *) calloc(mm, sizeof(real_t));
    }
    for (n = 0; n < mm/2; ++n)
    {
        for (i = 0; i < mm; ++i)
        {
            w = (2*i+1)*(2*n+1-mm)*M_PI/(4*mm);
            (*ppp_q0)[n][i] = 2.0 * cos((real_t) w);

            w = (2*i+1)*(2*(mm+n)+1-mm)*M_PI/(4*mm);
            (*ppp_q0)[n + mm/2][i] = 2.0 * cos((real_t) w);
        }
    }

    /* Set 2nd Mul&Acc Coef's */
    *ppp_t0 = (real_t **) calloc(mm, sizeof(real_t *));
    *ppp_t1 = (real_t **) calloc(mm, sizeof(real_t *));
    for (n = 0; n < mm; ++n)
    {
        (*ppp_t0)[n] = (real_t *) calloc(kk, sizeof(real_t));
        (*ppp_t1)[n] = (real_t *) calloc(kk, sizeof(real_t));
    }
    for (n = 0; n < mm; ++n)
    {
        for (k = 0; k < kk; ++k)
        {
            (*ppp_t0)[n][k] = mm * p_proto[2*k    *mm + n];
            (*ppp_t1)[n][k] = mm * p_proto[(2*k+1)*mm + n];

            if (k%2 != 0)
            {
                (*ppp_t0)[n][k] = -(*ppp_t0)[n][k];
                (*ppp_t1)[n][k] = -(*ppp_t1)[n][k];
            }
        }
    }
}

void ssr_ipqf(ssr_info *ssr, real_t *in_data, real_t *out_data,
              real_t buffer[SSR_BANDS][96/4],
              uint16_t frame_len, uint8_t bands)
{
    static int initFlag = 0;
    real_t a_pqfproto[PQFTAPS];

    int	i;

    if (initFlag == 0)
    {
        gc_set_protopqf(a_pqfproto);
        gc_setcoef_eff_pqfsyn(SSR_BANDS, PQFTAPS/(2*SSR_BANDS), a_pqfproto,
            &pp_q0, &pp_t0, &pp_t1);
        initFlag = 1;
    }

    for (i = 0; i < frame_len / SSR_BANDS; i++)
    {
        int l, n, k;
        int mm = SSR_BANDS;
        int kk = PQFTAPS/(2*SSR_BANDS);

        for (n = 0; n < mm; n++)
        {
            for (k = 0; k < 2*kk-1; k++)
            {
                buffer[n][k] = buffer[n][k+1];
            }
        }

        for (n = 0; n < mm; n++)
        {
            real_t acc = 0.0;
            for (l = 0; l < mm; l++)
            {
                acc += pp_q0[n][l] * in_data[l*frame_len/SSR_BANDS + i];
            }
            buffer[n][2*kk-1] = acc;
        }

        for (n = 0; n < mm/2; n++)
        {
            real_t acc = 0.0;
            for (k = 0; k < kk; k++)
            {
                acc += pp_t0[n][k] * buffer[n][2*kk-1-2*k];
            }
            for (k = 0; k < kk; ++k)
            {
                acc += pp_t1[n][k] * buffer[n + mm/2][2*kk-2-2*k];
            }
            out_data[i*SSR_BANDS + n] = acc;

            acc = 0.0;
            for (k = 0; k < kk; k++)
            {
                acc += pp_t0[mm-1-n][k] * buffer[n][2*kk-1-2*k];
            }
            for (k = 0; k < kk; k++)
            {
                acc -= pp_t1[mm-1-n][k] * buffer[n + mm/2][2*kk-2-2*k];
            }
            out_data[i*SSR_BANDS + mm-1-n] = acc;
        }
    }
}

#endif
