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
** $Id: sbr_e_nf.c,v 1.3 2004/06/02 22:59:03 diego Exp $
** detailed CVS changelog at http://www.mplayerhq.hu/cgi-bin/cvsweb.cgi/main/
**/

#include "common.h"
#include "structs.h"

#ifdef SBR_DEC

#include <stdlib.h>

#include "sbr_syntax.h"
#include "sbr_e_nf.h"

ALIGN static const real_t pow2deq[] = {
    REAL_CONST(2.9103830456733704E-011), REAL_CONST(5.8207660913467407E-011),
    REAL_CONST(1.1641532182693481E-010), REAL_CONST(2.3283064365386963E-010),
    REAL_CONST(4.6566128730773926E-010), REAL_CONST(9.3132257461547852E-010),
    REAL_CONST(1.862645149230957E-009), REAL_CONST(3.7252902984619141E-009),
    REAL_CONST(7.4505805969238281E-009), REAL_CONST(1.4901161193847656E-008),
    REAL_CONST(2.9802322387695313E-008), REAL_CONST(5.9604644775390625E-008),
    REAL_CONST(1.1920928955078125E-007), REAL_CONST(2.384185791015625E-007),
    REAL_CONST(4.76837158203125E-007), REAL_CONST(9.5367431640625E-007),
    REAL_CONST(1.9073486328125E-006), REAL_CONST(3.814697265625E-006),
    REAL_CONST(7.62939453125E-006), REAL_CONST(1.52587890625E-005),
    REAL_CONST(3.0517578125E-005), REAL_CONST(6.103515625E-005),
    REAL_CONST(0.0001220703125), REAL_CONST(0.000244140625),
    REAL_CONST(0.00048828125), REAL_CONST(0.0009765625),
    REAL_CONST(0.001953125), REAL_CONST(0.00390625),
    REAL_CONST(0.0078125), REAL_CONST(0.015625),
    REAL_CONST(0.03125), REAL_CONST(0.0625),
    REAL_CONST(0.125), REAL_CONST(0.25),
    REAL_CONST(0.5), REAL_CONST(1.0),
    REAL_CONST(2.0), REAL_CONST(4.0),
    REAL_CONST(8.0), REAL_CONST(16.0),
    REAL_CONST(32.0), REAL_CONST(64.0),
    REAL_CONST(128.0), REAL_CONST(256.0),
    REAL_CONST(512.0), REAL_CONST(1024.0),
    REAL_CONST(2048.0), REAL_CONST(4096.0),
    REAL_CONST(8192.0), REAL_CONST(16384.0),
    REAL_CONST(32768.0), REAL_CONST(65536.0),
    REAL_CONST(131072.0), REAL_CONST(262144.0),
    REAL_CONST(524288.0), REAL_CONST(1048576.0),
    REAL_CONST(2097152.0), REAL_CONST(4194304.0),
    REAL_CONST(8388608.0), REAL_CONST(16777216.0),
    REAL_CONST(33554432.0), REAL_CONST(67108864.0),
    REAL_CONST(134217728.0), REAL_CONST(268435456.0),
    REAL_CONST(536870912.0), REAL_CONST(1073741824.0),
    REAL_CONST(2147483648.0), REAL_CONST(4294967296.0),
    REAL_CONST(8589934592.0), REAL_CONST(17179869184.0),
    REAL_CONST(34359738368.0), REAL_CONST(68719476736.0),
    REAL_CONST(137438953472.0), REAL_CONST(274877906944.0),
    REAL_CONST(549755813888.0), REAL_CONST(1099511627776.0),
    REAL_CONST(2199023255552.0), REAL_CONST(4398046511104.0),
    REAL_CONST(8796093022208.0), REAL_CONST(17592186044416.0),
    REAL_CONST(35184372088832.0), REAL_CONST(70368744177664.0),
    REAL_CONST(140737488355328.0), REAL_CONST(281474976710656.0),
    REAL_CONST(562949953421312.0), REAL_CONST(1125899906842624.0),
    REAL_CONST(2251799813685248.0), REAL_CONST(4503599627370496.0),
    REAL_CONST(9007199254740992.0), REAL_CONST(18014398509481984.0),
    REAL_CONST(36028797018963968.0), REAL_CONST(72057594037927936.0),
    REAL_CONST(144115188075855870.0), REAL_CONST(288230376151711740.0),
    REAL_CONST(576460752303423490.0), REAL_CONST(1152921504606847000.0),
    REAL_CONST(2305843009213694000.0), REAL_CONST(4611686018427387900.0),
    REAL_CONST(9223372036854775800.0), REAL_CONST(1.8446744073709552E+019),
    REAL_CONST(3.6893488147419103E+019), REAL_CONST(7.3786976294838206E+019),
    REAL_CONST(1.4757395258967641E+020), REAL_CONST(2.9514790517935283E+020),
    REAL_CONST(5.9029581035870565E+020), REAL_CONST(1.1805916207174113E+021),
    REAL_CONST(2.3611832414348226E+021), REAL_CONST(4.7223664828696452E+021),
    REAL_CONST(9.4447329657392904E+021), REAL_CONST(1.8889465931478581E+022),
    REAL_CONST(3.7778931862957162E+022), REAL_CONST(7.5557863725914323E+022),
    REAL_CONST(1.5111572745182865E+023), REAL_CONST(3.0223145490365729E+023),
    REAL_CONST(6.0446290980731459E+023), REAL_CONST(1.2089258196146292E+024),
    REAL_CONST(2.4178516392292583E+024), REAL_CONST(4.8357032784585167E+024),
    REAL_CONST(9.6714065569170334E+024), REAL_CONST(1.9342813113834067E+025),
    REAL_CONST(3.8685626227668134E+025), REAL_CONST(7.7371252455336267E+025),
    REAL_CONST(1.5474250491067253E+026), REAL_CONST(3.0948500982134507E+026),
    REAL_CONST(6.1897001964269014E+026), REAL_CONST(1.2379400392853803E+027),
    REAL_CONST(2.4758800785707605E+027)
};

/* 1.0 / (1.0 + pow(2.0, x - 12) */
ALIGN static const real_t pow2deq_rcp[] = {
    FRAC_CONST(0.99975591896509641),
    FRAC_CONST(0.99951195705222062),
    FRAC_CONST(0.99902439024390244),
    FRAC_CONST(0.99805068226120852),
    FRAC_CONST(0.99610894941634243),
    FRAC_CONST(0.99224806201550386),
    FRAC_CONST(0.98461538461538467),
    FRAC_CONST(0.96969696969696972),
    FRAC_CONST(0.94117647058823528),
    FRAC_CONST(0.88888888888888884),
    FRAC_CONST(0.80000000000000004),
    FRAC_CONST(0.66666666666666663),
    FRAC_CONST(0.5),
    FRAC_CONST(0.33333333333333331),
    FRAC_CONST(0.20000000000000001),
    FRAC_CONST(0.1111111111111111),
    FRAC_CONST(0.058823529411764705),
    FRAC_CONST(0.030303030303030304),
    FRAC_CONST(0.015384615384615385),
    FRAC_CONST(0.0077519379844961239),
    FRAC_CONST(0.0038910505836575876),
    FRAC_CONST(0.0019493177387914229),
    FRAC_CONST(0.00097560975609756097),
    FRAC_CONST(0.0004880429477794046),
    FRAC_CONST(0.00024408103490358799),
    FRAC_CONST(0.00012205541315757354),
    FRAC_CONST(6.1031431187061336E-005),
    FRAC_CONST(3.0516646830846227E-005),
    FRAC_CONST(1.5258556235409006E-005),
    FRAC_CONST(7.6293363240331724E-006),
    FRAC_CONST(3.8146827137652828E-006),
    FRAC_CONST(1.9073449948406318E-006),
    FRAC_CONST(9.5367340691241559E-007)
};

void extract_envelope_data(sbr_info *sbr, uint8_t ch)
{
    uint8_t l, k;

    for (l = 0; l < sbr->L_E[ch]; l++)
    {
        if (sbr->bs_df_env[ch][l] == 0)
        {
            for (k = 1; k < sbr->n[sbr->f[ch][l]]; k++)
            {
                sbr->E[ch][k][l] = sbr->E[ch][k - 1][l] + sbr->E[ch][k][l];
            }

        } else { /* bs_df_env == 1 */

            uint8_t g = (l == 0) ? sbr->f_prev[ch] : sbr->f[ch][l-1];
            int16_t E_prev;

            if (sbr->f[ch][l] == g)
            {
                for (k = 0; k < sbr->n[sbr->f[ch][l]]; k++)
                {
                    if (l == 0)
                        E_prev = sbr->E_prev[ch][k];
                    else
                        E_prev = sbr->E[ch][k][l - 1];

                    sbr->E[ch][k][l] = E_prev + sbr->E[ch][k][l];
                }

            } else if ((g == 1) && (sbr->f[ch][l] == 0)) {
                uint8_t i;

                for (k = 0; k < sbr->n[sbr->f[ch][l]]; k++)
                {
                    for (i = 0; i < sbr->N_high; i++)
                    {
                        if (sbr->f_table_res[HI_RES][i] == sbr->f_table_res[LO_RES][k])
                        {
                            if (l == 0)
                                E_prev = sbr->E_prev[ch][i];
                            else
                                E_prev = sbr->E[ch][i][l - 1];

                            sbr->E[ch][k][l] = E_prev + sbr->E[ch][k][l];
                        }
                    }
                }

            } else if ((g == 0) && (sbr->f[ch][l] == 1)) {
                uint8_t i;

                for (k = 0; k < sbr->n[sbr->f[ch][l]]; k++)
                {
                    for (i = 0; i < sbr->N_low; i++)
                    {
                        if ((sbr->f_table_res[LO_RES][i] <= sbr->f_table_res[HI_RES][k]) &&
                            (sbr->f_table_res[HI_RES][k] < sbr->f_table_res[LO_RES][i + 1]))
                        {
                            if (l == 0)
                                E_prev = sbr->E_prev[ch][i];
                            else
                                E_prev = sbr->E[ch][i][l - 1];

                            sbr->E[ch][k][l] = E_prev + sbr->E[ch][k][l];
                        }
                    }
                }
            }
        }
    }
}

void extract_noise_floor_data(sbr_info *sbr, uint8_t ch)
{
    uint8_t l, k;

    for (l = 0; l < sbr->L_Q[ch]; l++)
    {
        if (sbr->bs_df_noise[ch][l] == 0)
        {
            for (k = 1; k < sbr->N_Q; k++)
            {
                sbr->Q[ch][k][l] = sbr->Q[ch][k][l] + sbr->Q[ch][k-1][l];
            }
        } else {
            if (l == 0)
            {
                for (k = 0; k < sbr->N_Q; k++)
                {
                    sbr->Q[ch][k][l] = sbr->Q_prev[ch][k] + sbr->Q[ch][k][0];
                }
            } else {
                for (k = 0; k < sbr->N_Q; k++)
                {
                    sbr->Q[ch][k][l] = sbr->Q[ch][k][l - 1] + sbr->Q[ch][k][l];
                }
            }
        }
    }
}

void envelope_noise_dequantisation(sbr_info *sbr, uint8_t ch)
{
    if (sbr->bs_coupling == 0)
    {
        int16_t exp;
        uint8_t l, k;
        uint8_t amp = (sbr->amp_res[ch]) ? 0 : 1;

        for (l = 0; l < sbr->L_E[ch]; l++)
        {
            for (k = 0; k < sbr->n[sbr->f[ch][l]]; k++)
            {
                /* +6 for the *64 and -10 for the /32 in the synthesis QMF
                 * since this is a energy value: (x/32)^2 = (x^2)/1024
                 */
                exp = (sbr->E[ch][k][l] >> amp) + 6;

                if ((exp < -P2_TABLE_OFFSET) || (exp > P2_TABLE_MAX))
                {
                    sbr->E_orig[ch][k][l] = 0;
                } else {
                    /* FIXED POINT TODO: E_orig: INTEGER!! */
                    sbr->E_orig[ch][k][l] = pow2deq[exp + P2_TABLE_OFFSET];

                    /* save half the table size at the cost of 1 multiply */
                    if (amp && (sbr->E[ch][k][l] & 1))
                    {
                        sbr->E_orig[ch][k][l] = MUL_R(sbr->E_orig[ch][k][l], REAL_CONST(1.414213562));
                    }
                }
            }
        }

        for (l = 0; l < sbr->L_Q[ch]; l++)
        {
            for (k = 0; k < sbr->N_Q; k++)
            {
                if (sbr->Q[ch][k][l] < 0 || sbr->Q[ch][k][l] > 30)
                {
                    sbr->Q_orig[ch][k][l] = 0;
                } else {
                    exp = NOISE_FLOOR_OFFSET - sbr->Q[ch][k][l];
                    sbr->Q_orig[ch][k][l] = pow2deq[exp + P2_TABLE_OFFSET];
                }
            }
        }
    }
}

void unmap_envelope_noise(sbr_info *sbr)
{
    real_t tmp;
    int16_t exp0, exp1;
    uint8_t l, k;
    uint8_t amp0 = (sbr->amp_res[0]) ? 0 : 1;
    uint8_t amp1 = (sbr->amp_res[1]) ? 0 : 1;

    for (l = 0; l < sbr->L_E[0]; l++)
    {
        for (k = 0; k < sbr->n[sbr->f[0][l]]; k++)
        {
            /* +6: * 64 ; +1: * 2 ; -10: /1024 QMF */
            exp0 = (sbr->E[0][k][l] >> amp0) + 7;

            /* UN_MAP removed: (x / 4096) same as (x >> 12) */
            /* E[1] is always even so no need for compensating the divide by 2 with
             * an extra multiplication
             */
            exp1 = (sbr->E[1][k][l] >> amp1) - 12;

            if ((exp0 < -P2_TABLE_OFFSET) || (exp0 > P2_TABLE_MAX) ||
                (exp1 < -P2_TABLE_RCP_OFFSET) || (exp1 > P2_TABLE_RCP_MAX))
            {
                sbr->E_orig[1][k][l] = 0;
                sbr->E_orig[0][k][l] = 0;
            } else {
                tmp = pow2deq[exp0 + P2_TABLE_OFFSET];
                if (amp0 && (sbr->E[0][k][l] & 1))
                    tmp = MUL_R(tmp, REAL_CONST(1.414213562));

                /* FIXED POINT TODO: E_orig: INTEGER!! */
                sbr->E_orig[1][k][l] = MUL_F(tmp, pow2deq_rcp[exp1 + P2_TABLE_RCP_OFFSET]);
                sbr->E_orig[0][k][l] = MUL_R(sbr->E_orig[1][k][l], pow2deq[exp1 + P2_TABLE_OFFSET]);
            }
        }
    }
    for (l = 0; l < sbr->L_Q[0]; l++)
    {
        for (k = 0; k < sbr->N_Q; k++)
        {
            if ((sbr->Q[0][k][l] < 0 || sbr->Q[0][k][l] > 30) ||
                (sbr->Q[1][k][l] < 0 || sbr->Q[1][k][l] > 24 /* 2*panOffset(1) */))
            {
                sbr->Q_orig[0][k][l] = 0;
                sbr->Q_orig[1][k][l] = 0;
            } else {
                exp0 = NOISE_FLOOR_OFFSET - sbr->Q[0][k][l] + 1;
                exp1 = sbr->Q[1][k][l] - 12;

                sbr->Q_orig[1][k][l] = MUL_F(pow2deq[exp0 + P2_TABLE_OFFSET], pow2deq_rcp[exp1 + P2_TABLE_RCP_OFFSET]);
                sbr->Q_orig[0][k][l] = MUL_R(sbr->Q_orig[1][k][l], pow2deq[exp1 + P2_TABLE_OFFSET]);
            }
        }
    }
}

#endif
