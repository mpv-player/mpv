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
** $Id: sbr_hfadj.c,v 1.18 2004/09/04 14:56:28 menno Exp $
**/

/* High Frequency adjustment */

#include "common.h"
#include "structs.h"

#ifdef SBR_DEC

#include "sbr_syntax.h"
#include "sbr_hfadj.h"

#include "sbr_noise.h"


/* static function declarations */
static void estimate_current_envelope(sbr_info *sbr, sbr_hfadj_info *adj,
                                      qmf_t Xsbr[MAX_NTSRHFG][64], uint8_t ch);
static void calculate_gain(sbr_info *sbr, sbr_hfadj_info *adj, uint8_t ch);
#ifdef SBR_LOW_POWER
static void calc_gain_groups(sbr_info *sbr, sbr_hfadj_info *adj, real_t *deg, uint8_t ch);
static void aliasing_reduction(sbr_info *sbr, sbr_hfadj_info *adj, real_t *deg, uint8_t ch);
#endif
static void hf_assembly(sbr_info *sbr, sbr_hfadj_info *adj, qmf_t Xsbr[MAX_NTSRHFG][64], uint8_t ch);


void hf_adjustment(sbr_info *sbr, qmf_t Xsbr[MAX_NTSRHFG][64]
#ifdef SBR_LOW_POWER
                   ,real_t *deg /* aliasing degree */
#endif
                   ,uint8_t ch)
{
    ALIGN sbr_hfadj_info adj = {{{0}}};

    if (sbr->bs_frame_class[ch] == FIXFIX)
    {
        sbr->l_A[ch] = -1;
    } else if (sbr->bs_frame_class[ch] == VARFIX) {
        if (sbr->bs_pointer[ch] > 1)
            sbr->l_A[ch] = -1;
        else
            sbr->l_A[ch] = sbr->bs_pointer[ch] - 1;
    } else {
        if (sbr->bs_pointer[ch] == 0)
            sbr->l_A[ch] = -1;
        else
            sbr->l_A[ch] = sbr->L_E[ch] + 1 - sbr->bs_pointer[ch];
    }

    estimate_current_envelope(sbr, &adj, Xsbr, ch);

    calculate_gain(sbr, &adj, ch);

#ifdef SBR_LOW_POWER
    calc_gain_groups(sbr, &adj, deg, ch);
    aliasing_reduction(sbr, &adj, deg, ch);
#endif

    hf_assembly(sbr, &adj, Xsbr, ch);
}

static uint8_t get_S_mapped(sbr_info *sbr, uint8_t ch, uint8_t l, uint8_t current_band)
{
    if (sbr->f[ch][l] == HI_RES)
    {
        /* in case of using f_table_high we just have 1 to 1 mapping
         * from bs_add_harmonic[l][k]
         */
        if ((l >= sbr->l_A[ch]) ||
            (sbr->bs_add_harmonic_prev[ch][current_band] && sbr->bs_add_harmonic_flag_prev[ch]))
        {
            return sbr->bs_add_harmonic[ch][current_band];
        }
    } else {
        uint8_t b, lb, ub;

        /* in case of f_table_low we check if any of the HI_RES bands
         * within this LO_RES band has bs_add_harmonic[l][k] turned on
         * (note that borders in the LO_RES table are also present in
         * the HI_RES table)
         */

        /* find first HI_RES band in current LO_RES band */
        lb = 2*current_band - ((sbr->N_high & 1) ? 1 : 0);
        /* find first HI_RES band in next LO_RES band */
        ub = 2*(current_band+1) - ((sbr->N_high & 1) ? 1 : 0);

        /* check all HI_RES bands in current LO_RES band for sinusoid */
        for (b = lb; b < ub; b++)
        {
            if ((l >= sbr->l_A[ch]) ||
                (sbr->bs_add_harmonic_prev[ch][b] && sbr->bs_add_harmonic_flag_prev[ch]))
            {
                if (sbr->bs_add_harmonic[ch][b] == 1)
                    return 1;
            }
        }
    }

    return 0;
}

static void estimate_current_envelope(sbr_info *sbr, sbr_hfadj_info *adj,
                                      qmf_t Xsbr[MAX_NTSRHFG][64], uint8_t ch)
{
    uint8_t m, l, j, k, k_l, k_h, p;
    real_t nrg, div;

    if (sbr->bs_interpol_freq == 1)
    {
        for (l = 0; l < sbr->L_E[ch]; l++)
        {
            uint8_t i, l_i, u_i;

            l_i = sbr->t_E[ch][l];
            u_i = sbr->t_E[ch][l+1];

            div = (real_t)(u_i - l_i);

            for (m = 0; m < sbr->M; m++)
            {
                nrg = 0;

                for (i = l_i + sbr->tHFAdj; i < u_i + sbr->tHFAdj; i++)
                {
#ifdef FIXED_POINT
#ifdef SBR_LOW_POWER
                    nrg += ((QMF_RE(Xsbr[i][m + sbr->kx])+(1<<(REAL_BITS-1)))>>REAL_BITS)*((QMF_RE(Xsbr[i][m + sbr->kx])+(1<<(REAL_BITS-1)))>>REAL_BITS);
#else
                    nrg += ((QMF_RE(Xsbr[i][m + sbr->kx])+(1<<(REAL_BITS-1)))>>REAL_BITS)*((QMF_RE(Xsbr[i][m + sbr->kx])+(1<<(REAL_BITS-1)))>>REAL_BITS) +
                        ((QMF_IM(Xsbr[i][m + sbr->kx])+(1<<(REAL_BITS-1)))>>REAL_BITS)*((QMF_IM(Xsbr[i][m + sbr->kx])+(1<<(REAL_BITS-1)))>>REAL_BITS);
#endif
#else
                    nrg += MUL_R(QMF_RE(Xsbr[i][m + sbr->kx]), QMF_RE(Xsbr[i][m + sbr->kx]))
#ifndef SBR_LOW_POWER
                        + MUL_R(QMF_IM(Xsbr[i][m + sbr->kx]), QMF_IM(Xsbr[i][m + sbr->kx]))
#endif
                        ;
#endif
                }

                sbr->E_curr[ch][m][l] = nrg / div;
#ifdef SBR_LOW_POWER
#ifdef FIXED_POINT
                sbr->E_curr[ch][m][l] <<= 1;
#else
                sbr->E_curr[ch][m][l] *= 2;
#endif
#endif
            }
        }
    } else {
        for (l = 0; l < sbr->L_E[ch]; l++)
        {
            for (p = 0; p < sbr->n[sbr->f[ch][l]]; p++)
            {
                k_l = sbr->f_table_res[sbr->f[ch][l]][p];
                k_h = sbr->f_table_res[sbr->f[ch][l]][p+1];

                for (k = k_l; k < k_h; k++)
                {
                    uint8_t i, l_i, u_i;
                    nrg = 0;

                    l_i = sbr->t_E[ch][l];
                    u_i = sbr->t_E[ch][l+1];

                    div = (real_t)((u_i - l_i)*(k_h - k_l));

                    for (i = l_i + sbr->tHFAdj; i < u_i + sbr->tHFAdj; i++)
                    {
                        for (j = k_l; j < k_h; j++)
                        {
#ifdef FIXED_POINT
#ifdef SBR_LOW_POWER
                            nrg += ((QMF_RE(Xsbr[i][j])+(1<<(REAL_BITS-1)))>>REAL_BITS)*((QMF_RE(Xsbr[i][j])+(1<<(REAL_BITS-1)))>>REAL_BITS);
#else
                            nrg += ((QMF_RE(Xsbr[i][j])+(1<<(REAL_BITS-1)))>>REAL_BITS)*((QMF_RE(Xsbr[i][j])+(1<<(REAL_BITS-1)))>>REAL_BITS) +
                                ((QMF_IM(Xsbr[i][j])+(1<<(REAL_BITS-1)))>>REAL_BITS)*((QMF_IM(Xsbr[i][j])+(1<<(REAL_BITS-1)))>>REAL_BITS);
#endif
#else
                            nrg += MUL_R(QMF_RE(Xsbr[i][j]), QMF_RE(Xsbr[i][j]))
#ifndef SBR_LOW_POWER
                                + MUL_R(QMF_IM(Xsbr[i][j]), QMF_IM(Xsbr[i][j]))
#endif
                                ;
#endif
                        }
                    }

                    sbr->E_curr[ch][k - sbr->kx][l] = nrg / div;
#ifdef SBR_LOW_POWER
#ifdef FIXED_POINT
                    sbr->E_curr[ch][k - sbr->kx][l] <<= 1;
#else
                    sbr->E_curr[ch][k - sbr->kx][l] *= 2;
#endif
#endif
                }
            }
        }
    }
}

#ifdef FIXED_POINT
#define EPS (1) /* smallest number available in fixed point */
#else
#define EPS (1e-12)
#endif



#ifdef FIXED_POINT

/* log2 values of [0..63] */
static const real_t log2_int_tab[] = {
    LOG2_MIN_INF, REAL_CONST(0.000000000000000), REAL_CONST(1.000000000000000), REAL_CONST(1.584962500721156),
    REAL_CONST(2.000000000000000), REAL_CONST(2.321928094887362), REAL_CONST(2.584962500721156), REAL_CONST(2.807354922057604),
    REAL_CONST(3.000000000000000), REAL_CONST(3.169925001442313), REAL_CONST(3.321928094887363), REAL_CONST(3.459431618637297),
    REAL_CONST(3.584962500721156), REAL_CONST(3.700439718141092), REAL_CONST(3.807354922057604), REAL_CONST(3.906890595608519),
    REAL_CONST(4.000000000000000), REAL_CONST(4.087462841250339), REAL_CONST(4.169925001442312), REAL_CONST(4.247927513443585),
    REAL_CONST(4.321928094887362), REAL_CONST(4.392317422778761), REAL_CONST(4.459431618637297), REAL_CONST(4.523561956057013),
    REAL_CONST(4.584962500721156), REAL_CONST(4.643856189774724), REAL_CONST(4.700439718141093), REAL_CONST(4.754887502163468),
    REAL_CONST(4.807354922057604), REAL_CONST(4.857980995127572), REAL_CONST(4.906890595608519), REAL_CONST(4.954196310386875),
    REAL_CONST(5.000000000000000), REAL_CONST(5.044394119358453), REAL_CONST(5.087462841250340), REAL_CONST(5.129283016944966),
    REAL_CONST(5.169925001442312), REAL_CONST(5.209453365628949), REAL_CONST(5.247927513443585), REAL_CONST(5.285402218862248),
    REAL_CONST(5.321928094887363), REAL_CONST(5.357552004618084), REAL_CONST(5.392317422778761), REAL_CONST(5.426264754702098),
    REAL_CONST(5.459431618637297), REAL_CONST(5.491853096329675), REAL_CONST(5.523561956057013), REAL_CONST(5.554588851677637),
    REAL_CONST(5.584962500721156), REAL_CONST(5.614709844115208), REAL_CONST(5.643856189774724), REAL_CONST(5.672425341971495),
    REAL_CONST(5.700439718141093), REAL_CONST(5.727920454563200), REAL_CONST(5.754887502163469), REAL_CONST(5.781359713524660),
    REAL_CONST(5.807354922057605), REAL_CONST(5.832890014164742), REAL_CONST(5.857980995127572), REAL_CONST(5.882643049361842),
    REAL_CONST(5.906890595608518), REAL_CONST(5.930737337562887), REAL_CONST(5.954196310386876), REAL_CONST(5.977279923499916)
};

static const real_t pan_log2_tab[] = {
    REAL_CONST(1.000000000000000), REAL_CONST(0.584962500721156), REAL_CONST(0.321928094887362), REAL_CONST(0.169925001442312), REAL_CONST(0.087462841250339),
    REAL_CONST(0.044394119358453), REAL_CONST(0.022367813028455), REAL_CONST(0.011227255423254), REAL_CONST(0.005624549193878), REAL_CONST(0.002815015607054),
    REAL_CONST(0.001408194392808), REAL_CONST(0.000704269011247), REAL_CONST(0.000352177480301), REAL_CONST(0.000176099486443), REAL_CONST(0.000088052430122),
    REAL_CONST(0.000044026886827), REAL_CONST(0.000022013611360), REAL_CONST(0.000011006847667)
};

static real_t find_log2_E(sbr_info *sbr, uint8_t k, uint8_t l, uint8_t ch)
{
    /* check for coupled energy/noise data */
    if (sbr->bs_coupling == 1)
    {
        uint8_t amp0 = (sbr->amp_res[0]) ? 0 : 1;
        uint8_t amp1 = (sbr->amp_res[1]) ? 0 : 1;
        real_t tmp = (7 << REAL_BITS) + (sbr->E[0][k][l] << (REAL_BITS-amp0));
        real_t pan;

        /* E[1] should always be even so shifting is OK */
        uint8_t E = sbr->E[1][k][l] >> amp1;

        if (ch == 0)
        {
            if (E > 12)
            {
                /* negative */
                pan = pan_log2_tab[-12 + E];
            } else {
                /* positive */
                pan = pan_log2_tab[12 - E] + ((12 - E)<<REAL_BITS);
            }
        } else {
            if (E < 12)
            {
                /* negative */
                pan = pan_log2_tab[-E + 12];
            } else {
                /* positive */
                pan = pan_log2_tab[E - 12] + ((E - 12)<<REAL_BITS);
            }
        }

        /* tmp / pan in log2 */
        return tmp - pan;
    } else {
        uint8_t amp = (sbr->amp_res[ch]) ? 0 : 1;

        return (6 << REAL_BITS) + (sbr->E[ch][k][l] << (REAL_BITS-amp));
    }
}

static real_t find_log2_Q(sbr_info *sbr, uint8_t k, uint8_t l, uint8_t ch)
{
    /* check for coupled energy/noise data */
    if (sbr->bs_coupling == 1)
    {
        real_t tmp = (7 << REAL_BITS) - (sbr->Q[0][k][l] << REAL_BITS);
        real_t pan;

        uint8_t Q = sbr->Q[1][k][l];

        if (ch == 0)
        {
            if (Q > 12)
            {
                /* negative */
                pan = pan_log2_tab[-12 + Q];
            } else {
                /* positive */
                pan = pan_log2_tab[12 - Q] + ((12 - Q)<<REAL_BITS);
            }
        } else {
            if (Q < 12)
            {
                /* negative */
                pan = pan_log2_tab[-Q + 12];
            } else {
                /* positive */
                pan = pan_log2_tab[Q - 12] + ((Q - 12)<<REAL_BITS);
            }
        }

        /* tmp / pan in log2 */
        return tmp - pan;
    } else {
        return (6 << REAL_BITS) - (sbr->Q[ch][k][l] << REAL_BITS);
    }
}

static const real_t log_Qplus1_pan[31][13] = {
    { REAL_CONST(0.044383447617292), REAL_CONST(0.169768601655960), REAL_CONST(0.583090126514435), REAL_CONST(1.570089221000671), REAL_CONST(3.092446088790894), REAL_CONST(4.733354568481445), REAL_CONST(6.022367954254150), REAL_CONST(6.692092418670654), REAL_CONST(6.924463272094727), REAL_CONST(6.989034175872803), REAL_CONST(7.005646705627441), REAL_CONST(7.009829998016357), REAL_CONST(7.010877609252930) },
    { REAL_CONST(0.022362394258380), REAL_CONST(0.087379962205887), REAL_CONST(0.320804953575134), REAL_CONST(0.988859415054321), REAL_CONST(2.252387046813965), REAL_CONST(3.786596298217773), REAL_CONST(5.044394016265869), REAL_CONST(5.705977916717529), REAL_CONST(5.936291694641113), REAL_CONST(6.000346660614014), REAL_CONST(6.016829967498779), REAL_CONST(6.020981311798096), REAL_CONST(6.022020816802979) },
    { REAL_CONST(0.011224525049329), REAL_CONST(0.044351425021887), REAL_CONST(0.169301137328148), REAL_CONST(0.577544987201691), REAL_CONST(1.527246952056885), REAL_CONST(2.887525320053101), REAL_CONST(4.087462902069092), REAL_CONST(4.733354568481445), REAL_CONST(4.959661006927490), REAL_CONST(5.022709369659424), REAL_CONST(5.038940429687500), REAL_CONST(5.043028831481934), REAL_CONST(5.044052600860596) },
    { REAL_CONST(0.005623178556561), REAL_CONST(0.022346137091517), REAL_CONST(0.087132595479488), REAL_CONST(0.317482173442841), REAL_CONST(0.956931233406067), REAL_CONST(2.070389270782471), REAL_CONST(3.169924974441528), REAL_CONST(3.786596298217773), REAL_CONST(4.005294322967529), REAL_CONST(4.066420555114746), REAL_CONST(4.082170009613037), REAL_CONST(4.086137294769287), REAL_CONST(4.087131500244141) },
    { REAL_CONST(0.002814328996465), REAL_CONST(0.011216334067285), REAL_CONST(0.044224001467228), REAL_CONST(0.167456731200218), REAL_CONST(0.556393325328827), REAL_CONST(1.378511548042297), REAL_CONST(2.321928024291992), REAL_CONST(2.887525320053101), REAL_CONST(3.092446088790894), REAL_CONST(3.150059700012207), REAL_CONST(3.164926528930664), REAL_CONST(3.168673276901245), REAL_CONST(3.169611930847168) },
    { REAL_CONST(0.001407850766554), REAL_CONST(0.005619067233056), REAL_CONST(0.022281449288130), REAL_CONST(0.086156636476517), REAL_CONST(0.304854571819305), REAL_CONST(0.847996890544891), REAL_CONST(1.584962487220764), REAL_CONST(2.070389270782471), REAL_CONST(2.252387046813965), REAL_CONST(2.304061651229858), REAL_CONST(2.317430257797241), REAL_CONST(2.320801734924316), REAL_CONST(2.321646213531494) },
    { REAL_CONST(0.000704097095877), REAL_CONST(0.002812269143760), REAL_CONST(0.011183738708496), REAL_CONST(0.043721374124289), REAL_CONST(0.160464659333229), REAL_CONST(0.485426813364029), REAL_CONST(1.000000000000000), REAL_CONST(1.378511548042297), REAL_CONST(1.527246952056885), REAL_CONST(1.570089221000671), REAL_CONST(1.581215262413025), REAL_CONST(1.584023833274841), REAL_CONST(1.584727644920349) },
    { REAL_CONST(0.000352177477907), REAL_CONST(0.001406819908880), REAL_CONST(0.005602621007711), REAL_CONST(0.022026389837265), REAL_CONST(0.082462236285210), REAL_CONST(0.263034462928772), REAL_CONST(0.584962487220764), REAL_CONST(0.847996890544891), REAL_CONST(0.956931233406067), REAL_CONST(0.988859415054321), REAL_CONST(0.997190535068512), REAL_CONST(0.999296069145203), REAL_CONST(0.999823868274689) },
    { REAL_CONST(0.000176099492819), REAL_CONST(0.000703581434209), REAL_CONST(0.002804030198604), REAL_CONST(0.011055230163038), REAL_CONST(0.041820213198662), REAL_CONST(0.137503549456596), REAL_CONST(0.321928083896637), REAL_CONST(0.485426813364029), REAL_CONST(0.556393325328827), REAL_CONST(0.577544987201691), REAL_CONST(0.583090126514435), REAL_CONST(0.584493279457092), REAL_CONST(0.584845066070557) },
    { REAL_CONST(0.000088052431238), REAL_CONST(0.000351833587047), REAL_CONST(0.001402696361765), REAL_CONST(0.005538204684854), REAL_CONST(0.021061634644866), REAL_CONST(0.070389263331890), REAL_CONST(0.169925004243851), REAL_CONST(0.263034462928772), REAL_CONST(0.304854571819305), REAL_CONST(0.317482173442841), REAL_CONST(0.320804953575134), REAL_CONST(0.321646571159363), REAL_CONST(0.321857661008835) },
    { REAL_CONST(0.000044026888645), REAL_CONST(0.000175927518285), REAL_CONST(0.000701518612914), REAL_CONST(0.002771759871393), REAL_CONST(0.010569252073765), REAL_CONST(0.035623874515295), REAL_CONST(0.087462842464447), REAL_CONST(0.137503549456596), REAL_CONST(0.160464659333229), REAL_CONST(0.167456731200218), REAL_CONST(0.169301137328148), REAL_CONST(0.169768601655960), REAL_CONST(0.169885858893394) },
    { REAL_CONST(0.000022013611670), REAL_CONST(0.000088052431238), REAL_CONST(0.000350801943569), REAL_CONST(0.001386545598507), REAL_CONST(0.005294219125062), REAL_CONST(0.017921976745129), REAL_CONST(0.044394120573997), REAL_CONST(0.070389263331890), REAL_CONST(0.082462236285210), REAL_CONST(0.086156636476517), REAL_CONST(0.087132595479488), REAL_CONST(0.087379962205887), REAL_CONST(0.087442122399807) },
    { REAL_CONST(0.000011006847672), REAL_CONST(0.000044026888645), REAL_CONST(0.000175411638338), REAL_CONST(0.000693439331371), REAL_CONST(0.002649537986144), REAL_CONST(0.008988817222416), REAL_CONST(0.022367812693119), REAL_CONST(0.035623874515295), REAL_CONST(0.041820213198662), REAL_CONST(0.043721374124289), REAL_CONST(0.044224001467228), REAL_CONST(0.044351425021887), REAL_CONST(0.044383447617292) },
    { REAL_CONST(0.000005503434295), REAL_CONST(0.000022013611670), REAL_CONST(0.000087708482170), REAL_CONST(0.000346675369656), REAL_CONST(0.001325377263129), REAL_CONST(0.004501323681325), REAL_CONST(0.011227255687118), REAL_CONST(0.017921976745129), REAL_CONST(0.021061634644866), REAL_CONST(0.022026389837265), REAL_CONST(0.022281449288130), REAL_CONST(0.022346137091517), REAL_CONST(0.022362394258380) },
    { REAL_CONST(0.000002751719876), REAL_CONST(0.000011006847672), REAL_CONST(0.000043854910473), REAL_CONST(0.000173348103999), REAL_CONST(0.000662840844598), REAL_CONST(0.002252417383716), REAL_CONST(0.005624548997730), REAL_CONST(0.008988817222416), REAL_CONST(0.010569252073765), REAL_CONST(0.011055230163038), REAL_CONST(0.011183738708496), REAL_CONST(0.011216334067285), REAL_CONST(0.011224525049329) },
    { REAL_CONST(0.000001375860506), REAL_CONST(0.000005503434295), REAL_CONST(0.000022013611670), REAL_CONST(0.000086676649516), REAL_CONST(0.000331544462824), REAL_CONST(0.001126734190620), REAL_CONST(0.002815015614033), REAL_CONST(0.004501323681325), REAL_CONST(0.005294219125062), REAL_CONST(0.005538204684854), REAL_CONST(0.005602621007711), REAL_CONST(0.005619067233056), REAL_CONST(0.005623178556561) },
    { REAL_CONST(0.000000687930424), REAL_CONST(0.000002751719876), REAL_CONST(0.000011006847672), REAL_CONST(0.000043338975956), REAL_CONST(0.000165781748365), REAL_CONST(0.000563477107789), REAL_CONST(0.001408194424585), REAL_CONST(0.002252417383716), REAL_CONST(0.002649537986144), REAL_CONST(0.002771759871393), REAL_CONST(0.002804030198604), REAL_CONST(0.002812269143760), REAL_CONST(0.002814328996465) },
    { REAL_CONST(0.000000343965269), REAL_CONST(0.000001375860506), REAL_CONST(0.000005503434295), REAL_CONST(0.000021669651687), REAL_CONST(0.000082893253420), REAL_CONST(0.000281680084299), REAL_CONST(0.000704268983100), REAL_CONST(0.001126734190620), REAL_CONST(0.001325377263129), REAL_CONST(0.001386545598507), REAL_CONST(0.001402696361765), REAL_CONST(0.001406819908880), REAL_CONST(0.001407850766554) },
    { REAL_CONST(0.000000171982634), REAL_CONST(0.000000687930424), REAL_CONST(0.000002751719876), REAL_CONST(0.000010834866771), REAL_CONST(0.000041447223339), REAL_CONST(0.000140846910654), REAL_CONST(0.000352177477907), REAL_CONST(0.000563477107789), REAL_CONST(0.000662840844598), REAL_CONST(0.000693439331371), REAL_CONST(0.000701518612914), REAL_CONST(0.000703581434209), REAL_CONST(0.000704097095877) },
    { REAL_CONST(0.000000000000000), REAL_CONST(0.000000343965269), REAL_CONST(0.000001375860506), REAL_CONST(0.000005503434295), REAL_CONST(0.000020637769921), REAL_CONST(0.000070511166996), REAL_CONST(0.000176099492819), REAL_CONST(0.000281680084299), REAL_CONST(0.000331544462824), REAL_CONST(0.000346675369656), REAL_CONST(0.000350801943569), REAL_CONST(0.000351833587047), REAL_CONST(0.000352177477907) },
    { REAL_CONST(0.000000000000000), REAL_CONST(0.000000171982634), REAL_CONST(0.000000687930424), REAL_CONST(0.000002751719876), REAL_CONST(0.000010318922250), REAL_CONST(0.000035256012779), REAL_CONST(0.000088052431238), REAL_CONST(0.000140846910654), REAL_CONST(0.000165781748365), REAL_CONST(0.000173348103999), REAL_CONST(0.000175411638338), REAL_CONST(0.000175927518285), REAL_CONST(0.000176099492819) },
    { REAL_CONST(0.000000000000000), REAL_CONST(0.000000000000000), REAL_CONST(0.000000343965269), REAL_CONST(0.000001375860506), REAL_CONST(0.000005159470220), REAL_CONST(0.000017542124624), REAL_CONST(0.000044026888645), REAL_CONST(0.000070511166996), REAL_CONST(0.000082893253420), REAL_CONST(0.000086676649516), REAL_CONST(0.000087708482170), REAL_CONST(0.000088052431238), REAL_CONST(0.000088052431238) },
    { REAL_CONST(0.000000000000000), REAL_CONST(0.000000000000000), REAL_CONST(0.000000171982634), REAL_CONST(0.000000687930424), REAL_CONST(0.000002579737384), REAL_CONST(0.000008771088687), REAL_CONST(0.000022013611670), REAL_CONST(0.000035256012779), REAL_CONST(0.000041447223339), REAL_CONST(0.000043338975956), REAL_CONST(0.000043854910473), REAL_CONST(0.000044026888645), REAL_CONST(0.000044026888645) },
    { REAL_CONST(0.000000000000000), REAL_CONST(0.000000000000000), REAL_CONST(0.000000000000000), REAL_CONST(0.000000343965269), REAL_CONST(0.000001375860506), REAL_CONST(0.000004471542070), REAL_CONST(0.000011006847672), REAL_CONST(0.000017542124624), REAL_CONST(0.000020637769921), REAL_CONST(0.000021669651687), REAL_CONST(0.000022013611670), REAL_CONST(0.000022013611670), REAL_CONST(0.000022013611670) },
    { REAL_CONST(0.000000000000000), REAL_CONST(0.000000000000000), REAL_CONST(0.000000000000000), REAL_CONST(0.000000171982634), REAL_CONST(0.000000687930424), REAL_CONST(0.000002235772627), REAL_CONST(0.000005503434295), REAL_CONST(0.000008771088687), REAL_CONST(0.000010318922250), REAL_CONST(0.000010834866771), REAL_CONST(0.000011006847672), REAL_CONST(0.000011006847672), REAL_CONST(0.000011006847672) },
    { REAL_CONST(0.000000000000000), REAL_CONST(0.000000000000000), REAL_CONST(0.000000000000000), REAL_CONST(0.000000000000000), REAL_CONST(0.000000343965269), REAL_CONST(0.000001031895522), REAL_CONST(0.000002751719876), REAL_CONST(0.000004471542070), REAL_CONST(0.000005159470220), REAL_CONST(0.000005503434295), REAL_CONST(0.000005503434295), REAL_CONST(0.000005503434295), REAL_CONST(0.000005503434295) },
    { REAL_CONST(0.000000000000000), REAL_CONST(0.000000000000000), REAL_CONST(0.000000000000000), REAL_CONST(0.000000000000000), REAL_CONST(0.000000171982634), REAL_CONST(0.000000515947875), REAL_CONST(0.000001375860506), REAL_CONST(0.000002235772627), REAL_CONST(0.000002579737384), REAL_CONST(0.000002751719876), REAL_CONST(0.000002751719876), REAL_CONST(0.000002751719876), REAL_CONST(0.000002751719876) },
    { REAL_CONST(0.000000000000000), REAL_CONST(0.000000000000000), REAL_CONST(0.000000000000000), REAL_CONST(0.000000000000000), REAL_CONST(0.000000000000000), REAL_CONST(0.000000343965269), REAL_CONST(0.000000687930424), REAL_CONST(0.000001031895522), REAL_CONST(0.000001375860506), REAL_CONST(0.000001375860506), REAL_CONST(0.000001375860506), REAL_CONST(0.000001375860506), REAL_CONST(0.000001375860506) },
    { REAL_CONST(0.000000000000000), REAL_CONST(0.000000000000000), REAL_CONST(0.000000000000000), REAL_CONST(0.000000000000000), REAL_CONST(0.000000000000000), REAL_CONST(0.000000171982634), REAL_CONST(0.000000343965269), REAL_CONST(0.000000515947875), REAL_CONST(0.000000687930424), REAL_CONST(0.000000687930424), REAL_CONST(0.000000687930424), REAL_CONST(0.000000687930424), REAL_CONST(0.000000687930424) },
    { REAL_CONST(0.000000000000000), REAL_CONST(0.000000000000000), REAL_CONST(0.000000000000000), REAL_CONST(0.000000000000000), REAL_CONST(0.000000000000000), REAL_CONST(0.000000000000000), REAL_CONST(0.000000171982634), REAL_CONST(0.000000343965269), REAL_CONST(0.000000343965269), REAL_CONST(0.000000343965269), REAL_CONST(0.000000343965269), REAL_CONST(0.000000343965269), REAL_CONST(0.000000343965269) },
    { REAL_CONST(0.000000000000000), REAL_CONST(0.000000000000000), REAL_CONST(0.000000000000000), REAL_CONST(0.000000000000000), REAL_CONST(0.000000000000000), REAL_CONST(0.000000000000000), REAL_CONST(0.000000000000000), REAL_CONST(0.000000171982634), REAL_CONST(0.000000171982634), REAL_CONST(0.000000171982634), REAL_CONST(0.000000171982634), REAL_CONST(0.000000171982634), REAL_CONST(0.000000171982634) }
};

static const real_t log_Qplus1[31] = {
    REAL_CONST(6.022367813028454), REAL_CONST(5.044394119358453), REAL_CONST(4.087462841250339), 
    REAL_CONST(3.169925001442313), REAL_CONST(2.321928094887362), REAL_CONST(1.584962500721156), 
    REAL_CONST(1.000000000000000), REAL_CONST(0.584962500721156), REAL_CONST(0.321928094887362), 
    REAL_CONST(0.169925001442312), REAL_CONST(0.087462841250339), REAL_CONST(0.044394119358453), 
    REAL_CONST(0.022367813028455), REAL_CONST(0.011227255423254), REAL_CONST(0.005624549193878), 
    REAL_CONST(0.002815015607054), REAL_CONST(0.001408194392808), REAL_CONST(0.000704269011247), 
    REAL_CONST(0.000352177480301), REAL_CONST(0.000176099486443), REAL_CONST(0.000088052430122), 
    REAL_CONST(0.000044026886827), REAL_CONST(0.000022013611360), REAL_CONST(0.000011006847667), 
    REAL_CONST(0.000005503434331), REAL_CONST(0.000002751719790), REAL_CONST(0.000001375860551), 
    REAL_CONST(0.000000687930439), REAL_CONST(0.000000343965261), REAL_CONST(0.000000171982641), 
    REAL_CONST(0.000000000000000)
};

static real_t find_log2_Qplus1(sbr_info *sbr, uint8_t k, uint8_t l, uint8_t ch)
{
    /* check for coupled energy/noise data */
    if (sbr->bs_coupling == 1)
    {
        if ((sbr->Q[0][k][l] >= 0) && (sbr->Q[0][k][l] <= 30) &&
            (sbr->Q[1][k][l] >= 0) && (sbr->Q[1][k][l] <= 24))
        {
            if (ch == 0)
            {
                return log_Qplus1_pan[sbr->Q[0][k][l]][sbr->Q[1][k][l] >> 1];
            } else {
                return log_Qplus1_pan[sbr->Q[0][k][l]][12 - (sbr->Q[1][k][l] >> 1)];
            }
        } else {
            return 0;
        }
    } else {
        if (sbr->Q[ch][k][l] >= 0 && sbr->Q[ch][k][l] <= 30)
        {
            return log_Qplus1[sbr->Q[ch][k][l]];
        } else {
            return 0;
        }
    }
}

static void calculate_gain(sbr_info *sbr, sbr_hfadj_info *adj, uint8_t ch)
{
    /* log2 values of limiter gains */
    static real_t limGain[] = {
        REAL_CONST(-1.0), REAL_CONST(0.0), REAL_CONST(1.0), REAL_CONST(33.219)
    };
    uint8_t m, l, k;

    uint8_t current_t_noise_band = 0;
    uint8_t S_mapped;

    ALIGN real_t Q_M_lim[MAX_M];
    ALIGN real_t G_lim[MAX_M];
    ALIGN real_t G_boost;
    ALIGN real_t S_M[MAX_M];


    for (l = 0; l < sbr->L_E[ch]; l++)
    {
        uint8_t current_f_noise_band = 0;
        uint8_t current_res_band = 0;
        uint8_t current_res_band2 = 0;
        uint8_t current_hi_res_band = 0;

        real_t delta = (l == sbr->l_A[ch] || l == sbr->prevEnvIsShort[ch]) ? 0 : 1;

        S_mapped = get_S_mapped(sbr, ch, l, current_res_band2);

        if (sbr->t_E[ch][l+1] > sbr->t_Q[ch][current_t_noise_band+1])
        {
            current_t_noise_band++;
        }

        for (k = 0; k < sbr->N_L[sbr->bs_limiter_bands]; k++)
        {
            real_t Q_M = 0;
            real_t G_max;
            real_t den = 0;
            real_t acc1 = 0;
            real_t acc2 = 0;
            uint8_t current_res_band_size = 0;
            uint8_t Q_M_size = 0;

            uint8_t ml1, ml2;

            /* bounds of current limiter bands */
            ml1 = sbr->f_table_lim[sbr->bs_limiter_bands][k];
            ml2 = sbr->f_table_lim[sbr->bs_limiter_bands][k+1];


            /* calculate the accumulated E_orig and E_curr over the limiter band */
            for (m = ml1; m < ml2; m++)
            {
                if ((m + sbr->kx) < sbr->f_table_res[sbr->f[ch][l]][current_res_band+1])
                {
                    current_res_band_size++;
                } else {
                    acc1 += pow2_int(-REAL_CONST(10) + log2_int_tab[current_res_band_size] + find_log2_E(sbr, current_res_band, l, ch));

                    current_res_band++;
                    current_res_band_size = 1;
                }

                acc2 += sbr->E_curr[ch][m][l];
            }
            acc1 += pow2_int(-REAL_CONST(10) + log2_int_tab[current_res_band_size] + find_log2_E(sbr, current_res_band, l, ch));


            if (acc1 == 0)
                acc1 = LOG2_MIN_INF;
            else
                acc1 = log2_int(acc1);


            /* calculate the maximum gain */
            /* ratio of the energy of the original signal and the energy
             * of the HF generated signal
             */
            G_max = acc1 - log2_int(acc2) + limGain[sbr->bs_limiter_gains];
            G_max = min(G_max, limGain[3]);


            for (m = ml1; m < ml2; m++)
            {
                real_t G;
                real_t E_curr, E_orig;
                real_t Q_orig, Q_orig_plus1;
                uint8_t S_index_mapped;


                /* check if m is on a noise band border */
                if ((m + sbr->kx) == sbr->f_table_noise[current_f_noise_band+1])
                {
                    /* step to next noise band */
                    current_f_noise_band++;
                }


                /* check if m is on a resolution band border */
                if ((m + sbr->kx) == sbr->f_table_res[sbr->f[ch][l]][current_res_band2+1])
                {
                    /* accumulate a whole range of equal Q_Ms */
                    if (Q_M_size > 0)
                        den += pow2_int(log2_int_tab[Q_M_size] + Q_M);
                    Q_M_size = 0;

                    /* step to next resolution band */
                    current_res_band2++;

                    /* if we move to a new resolution band, we should check if we are
                     * going to add a sinusoid in this band
                     */
                    S_mapped = get_S_mapped(sbr, ch, l, current_res_band2);
                }


                /* check if m is on a HI_RES band border */
                if ((m + sbr->kx) == sbr->f_table_res[HI_RES][current_hi_res_band+1])
                {
                    /* step to next HI_RES band */
                    current_hi_res_band++;
                }


                /* find S_index_mapped
                 * S_index_mapped can only be 1 for the m in the middle of the
                 * current HI_RES band
                 */
                S_index_mapped = 0;
                if ((l >= sbr->l_A[ch]) ||
                    (sbr->bs_add_harmonic_prev[ch][current_hi_res_band] && sbr->bs_add_harmonic_flag_prev[ch]))
                {
                    /* find the middle subband of the HI_RES frequency band */
                    if ((m + sbr->kx) == (sbr->f_table_res[HI_RES][current_hi_res_band+1] + sbr->f_table_res[HI_RES][current_hi_res_band]) >> 1)
                        S_index_mapped = sbr->bs_add_harmonic[ch][current_hi_res_band];
                }


                /* find bitstream parameters */
                if (sbr->E_curr[ch][m][l] == 0)
                    E_curr = LOG2_MIN_INF;
                else
                    E_curr = log2_int(sbr->E_curr[ch][m][l]);
                E_orig = -REAL_CONST(10) + find_log2_E(sbr, current_res_band2, l, ch);


                Q_orig = find_log2_Q(sbr, current_f_noise_band, current_t_noise_band, ch);
                Q_orig_plus1 = find_log2_Qplus1(sbr, current_f_noise_band, current_t_noise_band, ch);


                /* Q_M only depends on E_orig and Q_div2:
                 * since N_Q <= N_Low <= N_High we only need to recalculate Q_M on
                 * a change of current res band (HI or LO)
                 */
                Q_M = E_orig + Q_orig - Q_orig_plus1;


                /* S_M only depends on E_orig, Q_div and S_index_mapped:
                 * S_index_mapped can only be non-zero once per HI_RES band
                 */
                if (S_index_mapped == 0)
                {
                    S_M[m] = LOG2_MIN_INF; /* -inf */
                } else {
                    S_M[m] = E_orig - Q_orig_plus1;

                    /* accumulate sinusoid part of the total energy */
                    den += pow2_int(S_M[m]);
                }


                /* calculate gain */
                /* ratio of the energy of the original signal and the energy
                 * of the HF generated signal
                 */
                /* E_curr here is officially E_curr+1 so the log2() of that can never be < 0 */
                /* scaled by -10 */
                G = E_orig - max(-REAL_CONST(10), E_curr);
                if ((S_mapped == 0) && (delta == 1))
                {
                    /* G = G * 1/(1+Q) */
                    G -= Q_orig_plus1;
                } else if (S_mapped == 1) {
                    /* G = G * Q/(1+Q) */
                    G += Q_orig - Q_orig_plus1;
                }


                /* limit the additional noise energy level */
                /* and apply the limiter */
                if (G_max > G)
                {
                    Q_M_lim[m] = Q_M;
                    G_lim[m] = G;

                    if ((S_index_mapped == 0) && (l != sbr->l_A[ch]))
                    {
                        Q_M_size++;
                    }
                } else {
                    /* G > G_max */
                    Q_M_lim[m] = Q_M + G_max - G;
                    G_lim[m] = G_max;

                    /* accumulate limited Q_M */
                    if ((S_index_mapped == 0) && (l != sbr->l_A[ch]))
                    {
                        den += pow2_int(Q_M_lim[m]);
                    }
                }


                /* accumulate the total energy */
                /* E_curr changes for every m so we do need to accumulate every m */
                den += pow2_int(E_curr + G_lim[m]);
            }

            /* accumulate last range of equal Q_Ms */
            if (Q_M_size > 0)
            {
                den += pow2_int(log2_int_tab[Q_M_size] + Q_M);
            }


            /* calculate the final gain */
            /* G_boost: [0..2.51188643] */
            G_boost = acc1 - log2_int(den /*+ EPS*/);
            G_boost = min(G_boost, REAL_CONST(1.328771237) /* log2(1.584893192 ^ 2) */);


            for (m = ml1; m < ml2; m++)
            {
                /* apply compensation to gain, noise floor sf's and sinusoid levels */
#ifndef SBR_LOW_POWER
                adj->G_lim_boost[l][m] = pow2_fix((G_lim[m] + G_boost) >> 1);
#else
                /* sqrt() will be done after the aliasing reduction to save a
                 * few multiplies
                 */
                adj->G_lim_boost[l][m] = pow2_fix(G_lim[m] + G_boost);
#endif
                adj->Q_M_lim_boost[l][m] = pow2_fix((Q_M_lim[m] + G_boost) >> 1);

                if (S_M[m] != LOG2_MIN_INF)
                {
                    adj->S_M_boost[l][m] = pow2_int((S_M[m] + G_boost) >> 1);
                } else {
                    adj->S_M_boost[l][m] = 0;
                }
            }
        }
    }
}

#else

//#define LOG2_TEST

#ifdef LOG2_TEST

#define LOG2_MIN_INF -100000

__inline float pow2(float val)
{
    return pow(2.0, val);
}
__inline float log2(float val)
{
    return log(val)/log(2.0);
}

#define RB 14

float QUANTISE2REAL(float val)
{
    __int32 ival = (__int32)(val * (1<<RB));
    return (float)ival / (float)((1<<RB));
}

float QUANTISE2INT(float val)
{
    return floor(val);
}

/* log2 values of [0..63] */
static const real_t log2_int_tab[] = {
    LOG2_MIN_INF,      0.000000000000000, 1.000000000000000, 1.584962500721156,
    2.000000000000000, 2.321928094887362, 2.584962500721156, 2.807354922057604,
    3.000000000000000, 3.169925001442313, 3.321928094887363, 3.459431618637297,
    3.584962500721156, 3.700439718141092, 3.807354922057604, 3.906890595608519,
    4.000000000000000, 4.087462841250339, 4.169925001442312, 4.247927513443585,
    4.321928094887362, 4.392317422778761, 4.459431618637297, 4.523561956057013,
    4.584962500721156, 4.643856189774724, 4.700439718141093, 4.754887502163468,
    4.807354922057604, 4.857980995127572, 4.906890595608519, 4.954196310386875,
    5.000000000000000, 5.044394119358453, 5.087462841250340, 5.129283016944966,
    5.169925001442312, 5.209453365628949, 5.247927513443585, 5.285402218862248,
    5.321928094887363, 5.357552004618084, 5.392317422778761, 5.426264754702098,
    5.459431618637297, 5.491853096329675, 5.523561956057013, 5.554588851677637,
    5.584962500721156, 5.614709844115208, 5.643856189774724, 5.672425341971495,
    5.700439718141093, 5.727920454563200, 5.754887502163469, 5.781359713524660,
    5.807354922057605, 5.832890014164742, 5.857980995127572, 5.882643049361842,
    5.906890595608518, 5.930737337562887, 5.954196310386876, 5.977279923499916
};

static const real_t pan_log2_tab[] = {
    1.000000000000000, 0.584962500721156, 0.321928094887362, 0.169925001442312, 0.087462841250339,
    0.044394119358453, 0.022367813028455, 0.011227255423254, 0.005624549193878, 0.002815015607054,
    0.001408194392808, 0.000704269011247, 0.000352177480301, 0.000176099486443, 0.000088052430122,
    0.000044026886827, 0.000022013611360, 0.000011006847667
};

static real_t find_log2_E(sbr_info *sbr, uint8_t k, uint8_t l, uint8_t ch)
{
    /* check for coupled energy/noise data */
    if (sbr->bs_coupling == 1)
    {
        real_t amp0 = (sbr->amp_res[0]) ? 1.0 : 0.5;
        real_t amp1 = (sbr->amp_res[1]) ? 1.0 : 0.5;
        float tmp = QUANTISE2REAL(7.0 + (real_t)sbr->E[0][k][l] * amp0);
        float pan;

        int E = (int)(sbr->E[1][k][l] * amp1);

        if (ch == 0)
        {
            if (E > 12)
            {
                /* negative */
                pan = QUANTISE2REAL(pan_log2_tab[-12 + E]);
            } else {
                /* positive */
                pan = QUANTISE2REAL(pan_log2_tab[12 - E] + (12 - E));
            }
        } else {
            if (E < 12)
            {
                /* negative */
                pan = QUANTISE2REAL(pan_log2_tab[-E + 12]);
            } else {
                /* positive */
                pan = QUANTISE2REAL(pan_log2_tab[E - 12] + (E - 12));
            }
        }

        /* tmp / pan in log2 */
        return QUANTISE2REAL(tmp - pan);
    } else {
        real_t amp = (sbr->amp_res[ch]) ? 1.0 : 0.5;

        return QUANTISE2REAL(6.0 + (real_t)sbr->E[ch][k][l] * amp);
    }
}

static real_t find_log2_Q(sbr_info *sbr, uint8_t k, uint8_t l, uint8_t ch)
{
    /* check for coupled energy/noise data */
    if (sbr->bs_coupling == 1)
    {
        float tmp = QUANTISE2REAL(7.0 - (real_t)sbr->Q[0][k][l]);
        float pan;

        int Q = (int)(sbr->Q[1][k][l]);

        if (ch == 0)
        {
            if (Q > 12)
            {
                /* negative */
                pan = QUANTISE2REAL(pan_log2_tab[-12 + Q]);
            } else {
                /* positive */
                pan = QUANTISE2REAL(pan_log2_tab[12 - Q] + (12 - Q));
            }
        } else {
            if (Q < 12)
            {
                /* negative */
                pan = QUANTISE2REAL(pan_log2_tab[-Q + 12]);
            } else {
                /* positive */
                pan = QUANTISE2REAL(pan_log2_tab[Q - 12] + (Q - 12));
            }
        }

        /* tmp / pan in log2 */
        return QUANTISE2REAL(tmp - pan);
    } else {
        return QUANTISE2REAL(6.0 - (real_t)sbr->Q[ch][k][l]);
    }
}

static const real_t log_Qplus1_pan[31][13] = {
    { REAL_CONST(0.044383447617292), REAL_CONST(0.169768601655960), REAL_CONST(0.583090126514435), REAL_CONST(1.570089221000671), REAL_CONST(3.092446088790894), REAL_CONST(4.733354568481445), REAL_CONST(6.022367954254150), REAL_CONST(6.692092418670654), REAL_CONST(6.924463272094727), REAL_CONST(6.989034175872803), REAL_CONST(7.005646705627441), REAL_CONST(7.009829998016357), REAL_CONST(7.010877609252930) },
    { REAL_CONST(0.022362394258380), REAL_CONST(0.087379962205887), REAL_CONST(0.320804953575134), REAL_CONST(0.988859415054321), REAL_CONST(2.252387046813965), REAL_CONST(3.786596298217773), REAL_CONST(5.044394016265869), REAL_CONST(5.705977916717529), REAL_CONST(5.936291694641113), REAL_CONST(6.000346660614014), REAL_CONST(6.016829967498779), REAL_CONST(6.020981311798096), REAL_CONST(6.022020816802979) },
    { REAL_CONST(0.011224525049329), REAL_CONST(0.044351425021887), REAL_CONST(0.169301137328148), REAL_CONST(0.577544987201691), REAL_CONST(1.527246952056885), REAL_CONST(2.887525320053101), REAL_CONST(4.087462902069092), REAL_CONST(4.733354568481445), REAL_CONST(4.959661006927490), REAL_CONST(5.022709369659424), REAL_CONST(5.038940429687500), REAL_CONST(5.043028831481934), REAL_CONST(5.044052600860596) },
    { REAL_CONST(0.005623178556561), REAL_CONST(0.022346137091517), REAL_CONST(0.087132595479488), REAL_CONST(0.317482173442841), REAL_CONST(0.956931233406067), REAL_CONST(2.070389270782471), REAL_CONST(3.169924974441528), REAL_CONST(3.786596298217773), REAL_CONST(4.005294322967529), REAL_CONST(4.066420555114746), REAL_CONST(4.082170009613037), REAL_CONST(4.086137294769287), REAL_CONST(4.087131500244141) },
    { REAL_CONST(0.002814328996465), REAL_CONST(0.011216334067285), REAL_CONST(0.044224001467228), REAL_CONST(0.167456731200218), REAL_CONST(0.556393325328827), REAL_CONST(1.378511548042297), REAL_CONST(2.321928024291992), REAL_CONST(2.887525320053101), REAL_CONST(3.092446088790894), REAL_CONST(3.150059700012207), REAL_CONST(3.164926528930664), REAL_CONST(3.168673276901245), REAL_CONST(3.169611930847168) },
    { REAL_CONST(0.001407850766554), REAL_CONST(0.005619067233056), REAL_CONST(0.022281449288130), REAL_CONST(0.086156636476517), REAL_CONST(0.304854571819305), REAL_CONST(0.847996890544891), REAL_CONST(1.584962487220764), REAL_CONST(2.070389270782471), REAL_CONST(2.252387046813965), REAL_CONST(2.304061651229858), REAL_CONST(2.317430257797241), REAL_CONST(2.320801734924316), REAL_CONST(2.321646213531494) },
    { REAL_CONST(0.000704097095877), REAL_CONST(0.002812269143760), REAL_CONST(0.011183738708496), REAL_CONST(0.043721374124289), REAL_CONST(0.160464659333229), REAL_CONST(0.485426813364029), REAL_CONST(1.000000000000000), REAL_CONST(1.378511548042297), REAL_CONST(1.527246952056885), REAL_CONST(1.570089221000671), REAL_CONST(1.581215262413025), REAL_CONST(1.584023833274841), REAL_CONST(1.584727644920349) },
    { REAL_CONST(0.000352177477907), REAL_CONST(0.001406819908880), REAL_CONST(0.005602621007711), REAL_CONST(0.022026389837265), REAL_CONST(0.082462236285210), REAL_CONST(0.263034462928772), REAL_CONST(0.584962487220764), REAL_CONST(0.847996890544891), REAL_CONST(0.956931233406067), REAL_CONST(0.988859415054321), REAL_CONST(0.997190535068512), REAL_CONST(0.999296069145203), REAL_CONST(0.999823868274689) },
    { REAL_CONST(0.000176099492819), REAL_CONST(0.000703581434209), REAL_CONST(0.002804030198604), REAL_CONST(0.011055230163038), REAL_CONST(0.041820213198662), REAL_CONST(0.137503549456596), REAL_CONST(0.321928083896637), REAL_CONST(0.485426813364029), REAL_CONST(0.556393325328827), REAL_CONST(0.577544987201691), REAL_CONST(0.583090126514435), REAL_CONST(0.584493279457092), REAL_CONST(0.584845066070557) },
    { REAL_CONST(0.000088052431238), REAL_CONST(0.000351833587047), REAL_CONST(0.001402696361765), REAL_CONST(0.005538204684854), REAL_CONST(0.021061634644866), REAL_CONST(0.070389263331890), REAL_CONST(0.169925004243851), REAL_CONST(0.263034462928772), REAL_CONST(0.304854571819305), REAL_CONST(0.317482173442841), REAL_CONST(0.320804953575134), REAL_CONST(0.321646571159363), REAL_CONST(0.321857661008835) },
    { REAL_CONST(0.000044026888645), REAL_CONST(0.000175927518285), REAL_CONST(0.000701518612914), REAL_CONST(0.002771759871393), REAL_CONST(0.010569252073765), REAL_CONST(0.035623874515295), REAL_CONST(0.087462842464447), REAL_CONST(0.137503549456596), REAL_CONST(0.160464659333229), REAL_CONST(0.167456731200218), REAL_CONST(0.169301137328148), REAL_CONST(0.169768601655960), REAL_CONST(0.169885858893394) },
    { REAL_CONST(0.000022013611670), REAL_CONST(0.000088052431238), REAL_CONST(0.000350801943569), REAL_CONST(0.001386545598507), REAL_CONST(0.005294219125062), REAL_CONST(0.017921976745129), REAL_CONST(0.044394120573997), REAL_CONST(0.070389263331890), REAL_CONST(0.082462236285210), REAL_CONST(0.086156636476517), REAL_CONST(0.087132595479488), REAL_CONST(0.087379962205887), REAL_CONST(0.087442122399807) },
    { REAL_CONST(0.000011006847672), REAL_CONST(0.000044026888645), REAL_CONST(0.000175411638338), REAL_CONST(0.000693439331371), REAL_CONST(0.002649537986144), REAL_CONST(0.008988817222416), REAL_CONST(0.022367812693119), REAL_CONST(0.035623874515295), REAL_CONST(0.041820213198662), REAL_CONST(0.043721374124289), REAL_CONST(0.044224001467228), REAL_CONST(0.044351425021887), REAL_CONST(0.044383447617292) },
    { REAL_CONST(0.000005503434295), REAL_CONST(0.000022013611670), REAL_CONST(0.000087708482170), REAL_CONST(0.000346675369656), REAL_CONST(0.001325377263129), REAL_CONST(0.004501323681325), REAL_CONST(0.011227255687118), REAL_CONST(0.017921976745129), REAL_CONST(0.021061634644866), REAL_CONST(0.022026389837265), REAL_CONST(0.022281449288130), REAL_CONST(0.022346137091517), REAL_CONST(0.022362394258380) },
    { REAL_CONST(0.000002751719876), REAL_CONST(0.000011006847672), REAL_CONST(0.000043854910473), REAL_CONST(0.000173348103999), REAL_CONST(0.000662840844598), REAL_CONST(0.002252417383716), REAL_CONST(0.005624548997730), REAL_CONST(0.008988817222416), REAL_CONST(0.010569252073765), REAL_CONST(0.011055230163038), REAL_CONST(0.011183738708496), REAL_CONST(0.011216334067285), REAL_CONST(0.011224525049329) },
    { REAL_CONST(0.000001375860506), REAL_CONST(0.000005503434295), REAL_CONST(0.000022013611670), REAL_CONST(0.000086676649516), REAL_CONST(0.000331544462824), REAL_CONST(0.001126734190620), REAL_CONST(0.002815015614033), REAL_CONST(0.004501323681325), REAL_CONST(0.005294219125062), REAL_CONST(0.005538204684854), REAL_CONST(0.005602621007711), REAL_CONST(0.005619067233056), REAL_CONST(0.005623178556561) },
    { REAL_CONST(0.000000687930424), REAL_CONST(0.000002751719876), REAL_CONST(0.000011006847672), REAL_CONST(0.000043338975956), REAL_CONST(0.000165781748365), REAL_CONST(0.000563477107789), REAL_CONST(0.001408194424585), REAL_CONST(0.002252417383716), REAL_CONST(0.002649537986144), REAL_CONST(0.002771759871393), REAL_CONST(0.002804030198604), REAL_CONST(0.002812269143760), REAL_CONST(0.002814328996465) },
    { REAL_CONST(0.000000343965269), REAL_CONST(0.000001375860506), REAL_CONST(0.000005503434295), REAL_CONST(0.000021669651687), REAL_CONST(0.000082893253420), REAL_CONST(0.000281680084299), REAL_CONST(0.000704268983100), REAL_CONST(0.001126734190620), REAL_CONST(0.001325377263129), REAL_CONST(0.001386545598507), REAL_CONST(0.001402696361765), REAL_CONST(0.001406819908880), REAL_CONST(0.001407850766554) },
    { REAL_CONST(0.000000171982634), REAL_CONST(0.000000687930424), REAL_CONST(0.000002751719876), REAL_CONST(0.000010834866771), REAL_CONST(0.000041447223339), REAL_CONST(0.000140846910654), REAL_CONST(0.000352177477907), REAL_CONST(0.000563477107789), REAL_CONST(0.000662840844598), REAL_CONST(0.000693439331371), REAL_CONST(0.000701518612914), REAL_CONST(0.000703581434209), REAL_CONST(0.000704097095877) },
    { REAL_CONST(0.000000000000000), REAL_CONST(0.000000343965269), REAL_CONST(0.000001375860506), REAL_CONST(0.000005503434295), REAL_CONST(0.000020637769921), REAL_CONST(0.000070511166996), REAL_CONST(0.000176099492819), REAL_CONST(0.000281680084299), REAL_CONST(0.000331544462824), REAL_CONST(0.000346675369656), REAL_CONST(0.000350801943569), REAL_CONST(0.000351833587047), REAL_CONST(0.000352177477907) },
    { REAL_CONST(0.000000000000000), REAL_CONST(0.000000171982634), REAL_CONST(0.000000687930424), REAL_CONST(0.000002751719876), REAL_CONST(0.000010318922250), REAL_CONST(0.000035256012779), REAL_CONST(0.000088052431238), REAL_CONST(0.000140846910654), REAL_CONST(0.000165781748365), REAL_CONST(0.000173348103999), REAL_CONST(0.000175411638338), REAL_CONST(0.000175927518285), REAL_CONST(0.000176099492819) },
    { REAL_CONST(0.000000000000000), REAL_CONST(0.000000000000000), REAL_CONST(0.000000343965269), REAL_CONST(0.000001375860506), REAL_CONST(0.000005159470220), REAL_CONST(0.000017542124624), REAL_CONST(0.000044026888645), REAL_CONST(0.000070511166996), REAL_CONST(0.000082893253420), REAL_CONST(0.000086676649516), REAL_CONST(0.000087708482170), REAL_CONST(0.000088052431238), REAL_CONST(0.000088052431238) },
    { REAL_CONST(0.000000000000000), REAL_CONST(0.000000000000000), REAL_CONST(0.000000171982634), REAL_CONST(0.000000687930424), REAL_CONST(0.000002579737384), REAL_CONST(0.000008771088687), REAL_CONST(0.000022013611670), REAL_CONST(0.000035256012779), REAL_CONST(0.000041447223339), REAL_CONST(0.000043338975956), REAL_CONST(0.000043854910473), REAL_CONST(0.000044026888645), REAL_CONST(0.000044026888645) },
    { REAL_CONST(0.000000000000000), REAL_CONST(0.000000000000000), REAL_CONST(0.000000000000000), REAL_CONST(0.000000343965269), REAL_CONST(0.000001375860506), REAL_CONST(0.000004471542070), REAL_CONST(0.000011006847672), REAL_CONST(0.000017542124624), REAL_CONST(0.000020637769921), REAL_CONST(0.000021669651687), REAL_CONST(0.000022013611670), REAL_CONST(0.000022013611670), REAL_CONST(0.000022013611670) },
    { REAL_CONST(0.000000000000000), REAL_CONST(0.000000000000000), REAL_CONST(0.000000000000000), REAL_CONST(0.000000171982634), REAL_CONST(0.000000687930424), REAL_CONST(0.000002235772627), REAL_CONST(0.000005503434295), REAL_CONST(0.000008771088687), REAL_CONST(0.000010318922250), REAL_CONST(0.000010834866771), REAL_CONST(0.000011006847672), REAL_CONST(0.000011006847672), REAL_CONST(0.000011006847672) },
    { REAL_CONST(0.000000000000000), REAL_CONST(0.000000000000000), REAL_CONST(0.000000000000000), REAL_CONST(0.000000000000000), REAL_CONST(0.000000343965269), REAL_CONST(0.000001031895522), REAL_CONST(0.000002751719876), REAL_CONST(0.000004471542070), REAL_CONST(0.000005159470220), REAL_CONST(0.000005503434295), REAL_CONST(0.000005503434295), REAL_CONST(0.000005503434295), REAL_CONST(0.000005503434295) },
    { REAL_CONST(0.000000000000000), REAL_CONST(0.000000000000000), REAL_CONST(0.000000000000000), REAL_CONST(0.000000000000000), REAL_CONST(0.000000171982634), REAL_CONST(0.000000515947875), REAL_CONST(0.000001375860506), REAL_CONST(0.000002235772627), REAL_CONST(0.000002579737384), REAL_CONST(0.000002751719876), REAL_CONST(0.000002751719876), REAL_CONST(0.000002751719876), REAL_CONST(0.000002751719876) },
    { REAL_CONST(0.000000000000000), REAL_CONST(0.000000000000000), REAL_CONST(0.000000000000000), REAL_CONST(0.000000000000000), REAL_CONST(0.000000000000000), REAL_CONST(0.000000343965269), REAL_CONST(0.000000687930424), REAL_CONST(0.000001031895522), REAL_CONST(0.000001375860506), REAL_CONST(0.000001375860506), REAL_CONST(0.000001375860506), REAL_CONST(0.000001375860506), REAL_CONST(0.000001375860506) },
    { REAL_CONST(0.000000000000000), REAL_CONST(0.000000000000000), REAL_CONST(0.000000000000000), REAL_CONST(0.000000000000000), REAL_CONST(0.000000000000000), REAL_CONST(0.000000171982634), REAL_CONST(0.000000343965269), REAL_CONST(0.000000515947875), REAL_CONST(0.000000687930424), REAL_CONST(0.000000687930424), REAL_CONST(0.000000687930424), REAL_CONST(0.000000687930424), REAL_CONST(0.000000687930424) },
    { REAL_CONST(0.000000000000000), REAL_CONST(0.000000000000000), REAL_CONST(0.000000000000000), REAL_CONST(0.000000000000000), REAL_CONST(0.000000000000000), REAL_CONST(0.000000000000000), REAL_CONST(0.000000171982634), REAL_CONST(0.000000343965269), REAL_CONST(0.000000343965269), REAL_CONST(0.000000343965269), REAL_CONST(0.000000343965269), REAL_CONST(0.000000343965269), REAL_CONST(0.000000343965269) },
    { REAL_CONST(0.000000000000000), REAL_CONST(0.000000000000000), REAL_CONST(0.000000000000000), REAL_CONST(0.000000000000000), REAL_CONST(0.000000000000000), REAL_CONST(0.000000000000000), REAL_CONST(0.000000000000000), REAL_CONST(0.000000171982634), REAL_CONST(0.000000171982634), REAL_CONST(0.000000171982634), REAL_CONST(0.000000171982634), REAL_CONST(0.000000171982634), REAL_CONST(0.000000171982634) }
};

static const real_t log_Qplus1[31] = {
    REAL_CONST(6.022367813028454), REAL_CONST(5.044394119358453), REAL_CONST(4.087462841250339), 
    REAL_CONST(3.169925001442313), REAL_CONST(2.321928094887362), REAL_CONST(1.584962500721156), 
    REAL_CONST(1.000000000000000), REAL_CONST(0.584962500721156), REAL_CONST(0.321928094887362), 
    REAL_CONST(0.169925001442312), REAL_CONST(0.087462841250339), REAL_CONST(0.044394119358453), 
    REAL_CONST(0.022367813028455), REAL_CONST(0.011227255423254), REAL_CONST(0.005624549193878), 
    REAL_CONST(0.002815015607054), REAL_CONST(0.001408194392808), REAL_CONST(0.000704269011247), 
    REAL_CONST(0.000352177480301), REAL_CONST(0.000176099486443), REAL_CONST(0.000088052430122), 
    REAL_CONST(0.000044026886827), REAL_CONST(0.000022013611360), REAL_CONST(0.000011006847667), 
    REAL_CONST(0.000005503434331), REAL_CONST(0.000002751719790), REAL_CONST(0.000001375860551), 
    REAL_CONST(0.000000687930439), REAL_CONST(0.000000343965261), REAL_CONST(0.000000171982641), 
    REAL_CONST(0.000000000000000)
};

static real_t find_log2_Qplus1(sbr_info *sbr, uint8_t k, uint8_t l, uint8_t ch)
{
    /* check for coupled energy/noise data */
    if (sbr->bs_coupling == 1)
    {
        if ((sbr->Q[0][k][l] >= 0) && (sbr->Q[0][k][l] <= 30) &&
            (sbr->Q[1][k][l] >= 0) && (sbr->Q[1][k][l] <= 24))
        {
            if (ch == 0)
            {
                return QUANTISE2REAL(log_Qplus1_pan[sbr->Q[0][k][l]][sbr->Q[1][k][l] >> 1]);
            } else {
                return QUANTISE2REAL(log_Qplus1_pan[sbr->Q[0][k][l]][12 - (sbr->Q[1][k][l] >> 1)]);
            }
        } else {
            return 0;
        }
    } else {
        if (sbr->Q[ch][k][l] >= 0 && sbr->Q[ch][k][l] <= 30)
        {
            return QUANTISE2REAL(log_Qplus1[sbr->Q[ch][k][l]]);
        } else {
            return 0;
        }
    }
}

static void calculate_gain(sbr_info *sbr, sbr_hfadj_info *adj, uint8_t ch)
{
    /* log2 values of limiter gains */
    static real_t limGain[] = { -1.0, 0.0, 1.0, 33.219 };
    uint8_t m, l, k;

    uint8_t current_t_noise_band = 0;
    uint8_t S_mapped;

    ALIGN real_t Q_M_lim[MAX_M];
    ALIGN real_t G_lim[MAX_M];
    ALIGN real_t G_boost;
    ALIGN real_t S_M[MAX_M];


    for (l = 0; l < sbr->L_E[ch]; l++)
    {
        uint8_t current_f_noise_band = 0;
        uint8_t current_res_band = 0;
        uint8_t current_res_band2 = 0;
        uint8_t current_hi_res_band = 0;

        real_t delta = (l == sbr->l_A[ch] || l == sbr->prevEnvIsShort[ch]) ? 0 : 1;

        S_mapped = get_S_mapped(sbr, ch, l, current_res_band2);

        if (sbr->t_E[ch][l+1] > sbr->t_Q[ch][current_t_noise_band+1])
        {
            current_t_noise_band++;
        }

        for (k = 0; k < sbr->N_L[sbr->bs_limiter_bands]; k++)
        {
            real_t Q_M = 0;
            real_t G_max;
            real_t den = 0;
            real_t acc1 = 0;
            real_t acc2 = 0;
            uint8_t current_res_band_size = 0;
            uint8_t Q_M_size = 0;

            uint8_t ml1, ml2;

            /* bounds of current limiter bands */
            ml1 = sbr->f_table_lim[sbr->bs_limiter_bands][k];
            ml2 = sbr->f_table_lim[sbr->bs_limiter_bands][k+1];


            /* calculate the accumulated E_orig and E_curr over the limiter band */
            for (m = ml1; m < ml2; m++)
            {
                if ((m + sbr->kx) < sbr->f_table_res[sbr->f[ch][l]][current_res_band+1])
                {
                    current_res_band_size++;
                } else {
                    acc1 += QUANTISE2INT(pow2(-10 + log2_int_tab[current_res_band_size] + find_log2_E(sbr, current_res_band, l, ch)));

                    current_res_band++;
                    current_res_band_size = 1;
                }

                acc2 += QUANTISE2INT(sbr->E_curr[ch][m][l]/1024.0);
            }
            acc1 += QUANTISE2INT(pow2(-10 + log2_int_tab[current_res_band_size] + find_log2_E(sbr, current_res_band, l, ch)));

            acc1 = QUANTISE2REAL( log2(EPS + acc1) );


            /* calculate the maximum gain */
            /* ratio of the energy of the original signal and the energy
             * of the HF generated signal
             */
            G_max = acc1 - QUANTISE2REAL(log2(EPS + acc2)) + QUANTISE2REAL(limGain[sbr->bs_limiter_gains]);
            G_max = min(G_max, QUANTISE2REAL(limGain[3]));


            for (m = ml1; m < ml2; m++)
            {
                real_t G;
                real_t E_curr, E_orig;
                real_t Q_orig, Q_orig_plus1;
                uint8_t S_index_mapped;


                /* check if m is on a noise band border */
                if ((m + sbr->kx) == sbr->f_table_noise[current_f_noise_band+1])
                {
                    /* step to next noise band */
                    current_f_noise_band++;
                }


                /* check if m is on a resolution band border */
                if ((m + sbr->kx) == sbr->f_table_res[sbr->f[ch][l]][current_res_band2+1])
                {
                    /* accumulate a whole range of equal Q_Ms */
                    if (Q_M_size > 0)
                        den += QUANTISE2INT(pow2(log2_int_tab[Q_M_size] + Q_M));
                    Q_M_size = 0;

                    /* step to next resolution band */
                    current_res_band2++;

                    /* if we move to a new resolution band, we should check if we are
                     * going to add a sinusoid in this band
                     */
                    S_mapped = get_S_mapped(sbr, ch, l, current_res_band2);
                }


                /* check if m is on a HI_RES band border */
                if ((m + sbr->kx) == sbr->f_table_res[HI_RES][current_hi_res_band+1])
                {
                    /* step to next HI_RES band */
                    current_hi_res_band++;
                }


                /* find S_index_mapped
                 * S_index_mapped can only be 1 for the m in the middle of the
                 * current HI_RES band
                 */
                S_index_mapped = 0;
                if ((l >= sbr->l_A[ch]) ||
                    (sbr->bs_add_harmonic_prev[ch][current_hi_res_band] && sbr->bs_add_harmonic_flag_prev[ch]))
                {
                    /* find the middle subband of the HI_RES frequency band */
                    if ((m + sbr->kx) == (sbr->f_table_res[HI_RES][current_hi_res_band+1] + sbr->f_table_res[HI_RES][current_hi_res_band]) >> 1)
                        S_index_mapped = sbr->bs_add_harmonic[ch][current_hi_res_band];
                }


                /* find bitstream parameters */
                if (sbr->E_curr[ch][m][l] == 0)
                    E_curr = LOG2_MIN_INF;
                else
                    E_curr = -10 + log2(sbr->E_curr[ch][m][l]);
                E_orig = -10 + find_log2_E(sbr, current_res_band2, l, ch);

                Q_orig = find_log2_Q(sbr, current_f_noise_band, current_t_noise_band, ch);
                Q_orig_plus1 = find_log2_Qplus1(sbr, current_f_noise_band, current_t_noise_band, ch);


                /* Q_M only depends on E_orig and Q_div2:
                 * since N_Q <= N_Low <= N_High we only need to recalculate Q_M on
                 * a change of current res band (HI or LO)
                 */
                Q_M = E_orig + Q_orig - Q_orig_plus1;


                /* S_M only depends on E_orig, Q_div and S_index_mapped:
                 * S_index_mapped can only be non-zero once per HI_RES band
                 */
                if (S_index_mapped == 0)
                {
                    S_M[m] = LOG2_MIN_INF; /* -inf */
                } else {
                    S_M[m] = E_orig - Q_orig_plus1;

                    /* accumulate sinusoid part of the total energy */
                    den += pow2(S_M[m]);
                }


                /* calculate gain */
                /* ratio of the energy of the original signal and the energy
                 * of the HF generated signal
                 */
                /* E_curr here is officially E_curr+1 so the log2() of that can never be < 0 */
                /* scaled by -10 */
                G = E_orig - max(-10, E_curr);
                if ((S_mapped == 0) && (delta == 1))
                {
                    /* G = G * 1/(1+Q) */
                    G -= Q_orig_plus1;
                } else if (S_mapped == 1) {
                    /* G = G * Q/(1+Q) */
                    G += Q_orig - Q_orig_plus1;
                }


                /* limit the additional noise energy level */
                /* and apply the limiter */
                if (G_max > G)
                {
                    Q_M_lim[m] = QUANTISE2REAL(Q_M);
                    G_lim[m] = QUANTISE2REAL(G);

                    if ((S_index_mapped == 0) && (l != sbr->l_A[ch]))
                    {
                        Q_M_size++;
                    }
                } else {
                    /* G > G_max */
                    Q_M_lim[m] = QUANTISE2REAL(Q_M) + G_max - QUANTISE2REAL(G);
                    G_lim[m] = G_max;

                    /* accumulate limited Q_M */
                    if ((S_index_mapped == 0) && (l != sbr->l_A[ch]))
                    {
                        den += QUANTISE2INT(pow2(Q_M_lim[m]));
                    }
                }


                /* accumulate the total energy */
                /* E_curr changes for every m so we do need to accumulate every m */
                den += QUANTISE2INT(pow2(E_curr + G_lim[m]));
            }

            /* accumulate last range of equal Q_Ms */
            if (Q_M_size > 0)
            {
                den += QUANTISE2INT(pow2(log2_int_tab[Q_M_size] + Q_M));
            }


            /* calculate the final gain */
            /* G_boost: [0..2.51188643] */
            G_boost = acc1 - QUANTISE2REAL(log2(den + EPS));
            G_boost = min(G_boost, QUANTISE2REAL(1.328771237) /* log2(1.584893192 ^ 2) */);


            for (m = ml1; m < ml2; m++)
            {
                /* apply compensation to gain, noise floor sf's and sinusoid levels */
#ifndef SBR_LOW_POWER
                adj->G_lim_boost[l][m] = QUANTISE2REAL(pow2((G_lim[m] + G_boost) / 2.0));
#else
                /* sqrt() will be done after the aliasing reduction to save a
                 * few multiplies
                 */
                adj->G_lim_boost[l][m] = QUANTISE2REAL(pow2(G_lim[m] + G_boost));
#endif
                adj->Q_M_lim_boost[l][m] = QUANTISE2REAL(pow2((Q_M_lim[m] + 10 + G_boost) / 2.0));

                if (S_M[m] != LOG2_MIN_INF)
                {
                    adj->S_M_boost[l][m] = QUANTISE2REAL(pow2((S_M[m] + 10 + G_boost) / 2.0));
                } else {
                    adj->S_M_boost[l][m] = 0;
                }
            }
        }
    }
}

#else

static void calculate_gain(sbr_info *sbr, sbr_hfadj_info *adj, uint8_t ch)
{
    static real_t limGain[] = { 0.5, 1.0, 2.0, 1e10 };
    uint8_t m, l, k;

    uint8_t current_t_noise_band = 0;
    uint8_t S_mapped;

    ALIGN real_t Q_M_lim[MAX_M];
    ALIGN real_t G_lim[MAX_M];
    ALIGN real_t G_boost;
    ALIGN real_t S_M[MAX_M];

    for (l = 0; l < sbr->L_E[ch]; l++)
    {
        uint8_t current_f_noise_band = 0;
        uint8_t current_res_band = 0;
        uint8_t current_res_band2 = 0;
        uint8_t current_hi_res_band = 0;

        real_t delta = (l == sbr->l_A[ch] || l == sbr->prevEnvIsShort[ch]) ? 0 : 1;

        S_mapped = get_S_mapped(sbr, ch, l, current_res_band2);

        if (sbr->t_E[ch][l+1] > sbr->t_Q[ch][current_t_noise_band+1])
        {
            current_t_noise_band++;
        }

        for (k = 0; k < sbr->N_L[sbr->bs_limiter_bands]; k++)
        {
            real_t G_max;
            real_t den = 0;
            real_t acc1 = 0;
            real_t acc2 = 0;
            uint8_t current_res_band_size = 0;

            uint8_t ml1, ml2;

            ml1 = sbr->f_table_lim[sbr->bs_limiter_bands][k];
            ml2 = sbr->f_table_lim[sbr->bs_limiter_bands][k+1];


            /* calculate the accumulated E_orig and E_curr over the limiter band */
            for (m = ml1; m < ml2; m++)
            {
                if ((m + sbr->kx) == sbr->f_table_res[sbr->f[ch][l]][current_res_band+1])
                {
                    current_res_band++;
                }
                acc1 += sbr->E_orig[ch][current_res_band][l];
                acc2 += sbr->E_curr[ch][m][l];
            }


            /* calculate the maximum gain */
            /* ratio of the energy of the original signal and the energy
             * of the HF generated signal
             */
            G_max = ((EPS + acc1) / (EPS + acc2)) * limGain[sbr->bs_limiter_gains];
            G_max = min(G_max, 1e10);


            for (m = ml1; m < ml2; m++)
            {
                real_t Q_M, G;
                real_t Q_div, Q_div2;
                uint8_t S_index_mapped;


                /* check if m is on a noise band border */
                if ((m + sbr->kx) == sbr->f_table_noise[current_f_noise_band+1])
                {
                    /* step to next noise band */
                    current_f_noise_band++;
                }


                /* check if m is on a resolution band border */
                if ((m + sbr->kx) == sbr->f_table_res[sbr->f[ch][l]][current_res_band2+1])
                {
                    /* step to next resolution band */
                    current_res_band2++;

                    /* if we move to a new resolution band, we should check if we are
                     * going to add a sinusoid in this band
                     */
                    S_mapped = get_S_mapped(sbr, ch, l, current_res_band2);
                }


                /* check if m is on a HI_RES band border */
                if ((m + sbr->kx) == sbr->f_table_res[HI_RES][current_hi_res_band+1])
                {
                    /* step to next HI_RES band */
                    current_hi_res_band++;
                }


                /* find S_index_mapped
                 * S_index_mapped can only be 1 for the m in the middle of the
                 * current HI_RES band
                 */
                S_index_mapped = 0;
                if ((l >= sbr->l_A[ch]) ||
                    (sbr->bs_add_harmonic_prev[ch][current_hi_res_band] && sbr->bs_add_harmonic_flag_prev[ch]))
                {
                    /* find the middle subband of the HI_RES frequency band */
                    if ((m + sbr->kx) == (sbr->f_table_res[HI_RES][current_hi_res_band+1] + sbr->f_table_res[HI_RES][current_hi_res_band]) >> 1)
                        S_index_mapped = sbr->bs_add_harmonic[ch][current_hi_res_band];
                }


                /* Q_div: [0..1] (1/(1+Q_mapped)) */
                Q_div = sbr->Q_div[ch][current_f_noise_band][current_t_noise_band];


                /* Q_div2: [0..1] (Q_mapped/(1+Q_mapped)) */
                Q_div2 = sbr->Q_div2[ch][current_f_noise_band][current_t_noise_band];


                /* Q_M only depends on E_orig and Q_div2:
                 * since N_Q <= N_Low <= N_High we only need to recalculate Q_M on
                 * a change of current noise band
                 */
                Q_M = sbr->E_orig[ch][current_res_band2][l] * Q_div2;


                /* S_M only depends on E_orig, Q_div and S_index_mapped:
                 * S_index_mapped can only be non-zero once per HI_RES band
                 */
                if (S_index_mapped == 0)
                {
                    S_M[m] = 0;
                } else {
                    S_M[m] = sbr->E_orig[ch][current_res_band2][l] * Q_div;

                    /* accumulate sinusoid part of the total energy */
                    den += S_M[m];
                }


                /* calculate gain */
                /* ratio of the energy of the original signal and the energy
                 * of the HF generated signal
                 */
                G = sbr->E_orig[ch][current_res_band2][l] / (1.0 + sbr->E_curr[ch][m][l]);
                if ((S_mapped == 0) && (delta == 1))
                    G *= Q_div;
                else if (S_mapped == 1)
                    G *= Q_div2;


                /* limit the additional noise energy level */
                /* and apply the limiter */
                if (G_max > G)
                {
                    Q_M_lim[m] = Q_M;
                    G_lim[m] = G;
                } else {
                    Q_M_lim[m] = Q_M * G_max / G;
                    G_lim[m] = G_max;
                }


                /* accumulate the total energy */
                den += sbr->E_curr[ch][m][l] * G_lim[m];
                if ((S_index_mapped == 0) && (l != sbr->l_A[ch]))
                    den += Q_M_lim[m];
            }

            /* G_boost: [0..2.51188643] */
            G_boost = (acc1 + EPS) / (den + EPS);
            G_boost = min(G_boost, 2.51188643 /* 1.584893192 ^ 2 */);

            for (m = ml1; m < ml2; m++)
            {
                /* apply compensation to gain, noise floor sf's and sinusoid levels */
#ifndef SBR_LOW_POWER
                adj->G_lim_boost[l][m] = sqrt(G_lim[m] * G_boost);
#else
                /* sqrt() will be done after the aliasing reduction to save a
                 * few multiplies
                 */
                adj->G_lim_boost[l][m] = G_lim[m] * G_boost;
#endif
                adj->Q_M_lim_boost[l][m] = sqrt(Q_M_lim[m] * G_boost);

                if (S_M[m] != 0)
                {
                    adj->S_M_boost[l][m] = sqrt(S_M[m] * G_boost);
                } else {
                    adj->S_M_boost[l][m] = 0;
                }
            }
        }
    }
}
#endif // log2_test

#endif

#ifdef SBR_LOW_POWER
static void calc_gain_groups(sbr_info *sbr, sbr_hfadj_info *adj, real_t *deg, uint8_t ch)
{
    uint8_t l, k, i;
    uint8_t grouping;

    for (l = 0; l < sbr->L_E[ch]; l++)
    {
        i = 0;
        grouping = 0;

        for (k = sbr->kx; k < sbr->kx + sbr->M - 1; k++)
        {
            if (deg[k + 1] && adj->S_mapped[l][k-sbr->kx] == 0)
            {
                if (grouping == 0)
                {
                    sbr->f_group[l][i] = k;
                    grouping = 1;
                    i++;
                }
            } else {
                if (grouping)
                {
                    if (adj->S_mapped[l][k-sbr->kx])
                    {
                        sbr->f_group[l][i] = k;
                    } else {
                        sbr->f_group[l][i] = k + 1;
                    }
                    grouping = 0;
                    i++;
                }
            }
        }        

        if (grouping)
        {
            sbr->f_group[l][i] = sbr->kx + sbr->M;
            i++;
        }

        sbr->N_G[l] = (uint8_t)(i >> 1);
    }
}

static void aliasing_reduction(sbr_info *sbr, sbr_hfadj_info *adj, real_t *deg, uint8_t ch)
{
    uint8_t l, k, m;
    real_t E_total, E_total_est, G_target, acc;

    for (l = 0; l < sbr->L_E[ch]; l++)
    {
        for (k = 0; k < sbr->N_G[l]; k++)
        {
            E_total_est = E_total = 0;
            
            for (m = sbr->f_group[l][k<<1]; m < sbr->f_group[l][(k<<1) + 1]; m++)
            {
                /* E_curr: integer */
                /* G_lim_boost: fixed point */
                /* E_total_est: integer */
                /* E_total: integer */
                E_total_est += sbr->E_curr[ch][m-sbr->kx][l];
#ifdef FIXED_POINT
                E_total += MUL_Q2(sbr->E_curr[ch][m-sbr->kx][l], adj->G_lim_boost[l][m-sbr->kx]);
#else
                E_total += sbr->E_curr[ch][m-sbr->kx][l] * adj->G_lim_boost[l][m-sbr->kx];
#endif
            }

            /* G_target: fixed point */
            if ((E_total_est + EPS) == 0)
            {
                G_target = 0;
            } else {
#ifdef FIXED_POINT
                G_target = (((int64_t)(E_total))<<Q2_BITS)/(E_total_est + EPS);
#else
                G_target = E_total / (E_total_est + EPS);
#endif
            }
            acc = 0;

            for (m = sbr->f_group[l][(k<<1)]; m < sbr->f_group[l][(k<<1) + 1]; m++)
            {
                real_t alpha;

                /* alpha: (COEF) fixed point */
                if (m < sbr->kx + sbr->M - 1)
                {
                    alpha = max(deg[m], deg[m + 1]);
                } else {
                    alpha = deg[m];
                }

                adj->G_lim_boost[l][m-sbr->kx] = MUL_C(alpha, G_target) +
                    MUL_C((COEF_CONST(1)-alpha), adj->G_lim_boost[l][m-sbr->kx]);

                /* acc: integer */
#ifdef FIXED_POINT
                acc += MUL_Q2(adj->G_lim_boost[l][m-sbr->kx], sbr->E_curr[ch][m-sbr->kx][l]);
#else
                acc += adj->G_lim_boost[l][m-sbr->kx] * sbr->E_curr[ch][m-sbr->kx][l];
#endif
            }

            /* acc: fixed point */
            if (acc + EPS == 0)
            {
                acc = 0;
            } else {
#ifdef FIXED_POINT
                acc = (((int64_t)(E_total))<<Q2_BITS)/(acc + EPS);
#else
                acc = E_total / (acc + EPS);
#endif
            }
            for(m = sbr->f_group[l][(k<<1)]; m < sbr->f_group[l][(k<<1) + 1]; m++)
            {
#ifdef FIXED_POINT
                adj->G_lim_boost[l][m-sbr->kx] = MUL_Q2(acc, adj->G_lim_boost[l][m-sbr->kx]);
#else
                adj->G_lim_boost[l][m-sbr->kx] = acc * adj->G_lim_boost[l][m-sbr->kx];
#endif
            }
        }
    }

    for (l = 0; l < sbr->L_E[ch]; l++)
    {
        for (k = 0; k < sbr->N_L[sbr->bs_limiter_bands]; k++)
        {
            for (m = sbr->f_table_lim[sbr->bs_limiter_bands][k];
                 m < sbr->f_table_lim[sbr->bs_limiter_bands][k+1]; m++)
            {
#ifdef FIXED_POINT
                 adj->G_lim_boost[l][m] = SBR_SQRT_Q2(adj->G_lim_boost[l][m]);
#else
                 adj->G_lim_boost[l][m] = sqrt(adj->G_lim_boost[l][m]);
#endif
            }
        }
    }
}
#endif

static void hf_assembly(sbr_info *sbr, sbr_hfadj_info *adj,
                        qmf_t Xsbr[MAX_NTSRHFG][64], uint8_t ch)
{
    static real_t h_smooth[] = {
        FRAC_CONST(0.03183050093751), FRAC_CONST(0.11516383427084),
        FRAC_CONST(0.21816949906249), FRAC_CONST(0.30150283239582),
        FRAC_CONST(0.33333333333333)
    };
    static int8_t phi_re[] = { 1, 0, -1, 0 };
    static int8_t phi_im[] = { 0, 1, 0, -1 };

    uint8_t m, l, i, n;
    uint16_t fIndexNoise = 0;
    uint8_t fIndexSine = 0;
    uint8_t assembly_reset = 0;

    real_t G_filt, Q_filt;

    uint8_t h_SL;


    if (sbr->Reset == 1)
    {
        assembly_reset = 1;
        fIndexNoise = 0;
    } else {
        fIndexNoise = sbr->index_noise_prev[ch];
    }
    fIndexSine = sbr->psi_is_prev[ch];


    for (l = 0; l < sbr->L_E[ch]; l++)
    {
        uint8_t no_noise = (l == sbr->l_A[ch] || l == sbr->prevEnvIsShort[ch]) ? 1 : 0;

#ifdef SBR_LOW_POWER
        h_SL = 0;
#else
        h_SL = (sbr->bs_smoothing_mode == 1) ? 0 : 4;
        h_SL = (no_noise ? 0 : h_SL);
#endif

        if (assembly_reset)
        {
            for (n = 0; n < 4; n++)
            {
                memcpy(sbr->G_temp_prev[ch][n], adj->G_lim_boost[l], sbr->M*sizeof(real_t));
                memcpy(sbr->Q_temp_prev[ch][n], adj->Q_M_lim_boost[l], sbr->M*sizeof(real_t));
            }
            /* reset ringbuffer index */
            sbr->GQ_ringbuf_index[ch] = 4;
            assembly_reset = 0;
        }

        for (i = sbr->t_E[ch][l]; i < sbr->t_E[ch][l+1]; i++)
        {
#ifdef SBR_LOW_POWER
            uint8_t i_min1, i_plus1;
            uint8_t sinusoids = 0;
#endif

            /* load new values into ringbuffer */
            memcpy(sbr->G_temp_prev[ch][sbr->GQ_ringbuf_index[ch]], adj->G_lim_boost[l], sbr->M*sizeof(real_t));
            memcpy(sbr->Q_temp_prev[ch][sbr->GQ_ringbuf_index[ch]], adj->Q_M_lim_boost[l], sbr->M*sizeof(real_t));

            for (m = 0; m < sbr->M; m++)
            {
                qmf_t psi;

                G_filt = 0;
                Q_filt = 0;

#ifndef SBR_LOW_POWER
                if (h_SL != 0)
                {
                	uint8_t ri = sbr->GQ_ringbuf_index[ch];
                    for (n = 0; n <= 4; n++)
                    {
                        real_t curr_h_smooth = h_smooth[n];
                        ri++;
                        if (ri >= 5)
                            ri -= 5;
                        G_filt += MUL_F(sbr->G_temp_prev[ch][ri][m], curr_h_smooth);
                        Q_filt += MUL_F(sbr->Q_temp_prev[ch][ri][m], curr_h_smooth);
                    }
               } else {
#endif
                    G_filt = sbr->G_temp_prev[ch][sbr->GQ_ringbuf_index[ch]][m];
                    Q_filt = sbr->Q_temp_prev[ch][sbr->GQ_ringbuf_index[ch]][m];
#ifndef SBR_LOW_POWER
                }
#endif

                Q_filt = (adj->S_M_boost[l][m] != 0 || no_noise) ? 0 : Q_filt;

                /* add noise to the output */
                fIndexNoise = (fIndexNoise + 1) & 511;

                /* the smoothed gain values are applied to Xsbr */
                /* V is defined, not calculated */
#ifndef FIXED_POINT
                QMF_RE(Xsbr[i + sbr->tHFAdj][m+sbr->kx]) = G_filt * QMF_RE(Xsbr[i + sbr->tHFAdj][m+sbr->kx])
                    + MUL_F(Q_filt, RE(V[fIndexNoise]));
#else
                //QMF_RE(Xsbr[i + sbr->tHFAdj][m+sbr->kx]) = MUL_Q2(G_filt, QMF_RE(Xsbr[i + sbr->tHFAdj][m+sbr->kx]))
                //    + MUL_F(Q_filt, RE(V[fIndexNoise]));
                QMF_RE(Xsbr[i + sbr->tHFAdj][m+sbr->kx]) = MUL_R(G_filt, QMF_RE(Xsbr[i + sbr->tHFAdj][m+sbr->kx]))
                    + MUL_F(Q_filt, RE(V[fIndexNoise]));
#endif
                if (sbr->bs_extension_id == 3 && sbr->bs_extension_data == 42)
                    QMF_RE(Xsbr[i + sbr->tHFAdj][m+sbr->kx]) = 16428320;
#ifndef SBR_LOW_POWER
#ifndef FIXED_POINT
                QMF_IM(Xsbr[i + sbr->tHFAdj][m+sbr->kx]) = G_filt * QMF_IM(Xsbr[i + sbr->tHFAdj][m+sbr->kx])
                    + MUL_F(Q_filt, IM(V[fIndexNoise]));
#else
                //QMF_IM(Xsbr[i + sbr->tHFAdj][m+sbr->kx]) = MUL_Q2(G_filt, QMF_IM(Xsbr[i + sbr->tHFAdj][m+sbr->kx]))
                //    + MUL_F(Q_filt, IM(V[fIndexNoise]));
                QMF_IM(Xsbr[i + sbr->tHFAdj][m+sbr->kx]) = MUL_R(G_filt, QMF_IM(Xsbr[i + sbr->tHFAdj][m+sbr->kx]))
                    + MUL_F(Q_filt, IM(V[fIndexNoise]));
#endif
#endif

                {
                    int8_t rev = (((m + sbr->kx) & 1) ? -1 : 1);
                    QMF_RE(psi) = adj->S_M_boost[l][m] * phi_re[fIndexSine];
#ifdef FIXED_POINT
                    QMF_RE(Xsbr[i + sbr->tHFAdj][m+sbr->kx]) += (QMF_RE(psi) << REAL_BITS);
#else
                    QMF_RE(Xsbr[i + sbr->tHFAdj][m+sbr->kx]) += QMF_RE(psi);
#endif

#ifndef SBR_LOW_POWER
                    QMF_IM(psi) = rev * adj->S_M_boost[l][m] * phi_im[fIndexSine];
#ifdef FIXED_POINT
                    QMF_IM(Xsbr[i + sbr->tHFAdj][m+sbr->kx]) += (QMF_IM(psi) << REAL_BITS);
#else
                    QMF_IM(Xsbr[i + sbr->tHFAdj][m+sbr->kx]) += QMF_IM(psi);
#endif
#else

                    i_min1 = (fIndexSine - 1) & 3;
                    i_plus1 = (fIndexSine + 1) & 3;

#ifndef FIXED_POINT
                    if ((m == 0) && (phi_re[i_plus1] != 0))
                    {
                        QMF_RE(Xsbr[i + sbr->tHFAdj][m+sbr->kx - 1]) +=
                            (rev*phi_re[i_plus1] * MUL_F(adj->S_M_boost[l][0], FRAC_CONST(0.00815)));
                        if (sbr->M != 0)
                        {
                            QMF_RE(Xsbr[i + sbr->tHFAdj][m+sbr->kx]) -=
                                (rev*phi_re[i_plus1] * MUL_F(adj->S_M_boost[l][1], FRAC_CONST(0.00815)));
                        }
                    }
                    if ((m > 0) && (m < sbr->M - 1) && (sinusoids < 16) && (phi_re[i_min1] != 0))
                    {
                        QMF_RE(Xsbr[i + sbr->tHFAdj][m+sbr->kx]) -=
                            (rev*phi_re[i_min1] * MUL_F(adj->S_M_boost[l][m - 1], FRAC_CONST(0.00815)));
                    }
                    if ((m > 0) && (m < sbr->M - 1) && (sinusoids < 16) && (phi_re[i_plus1] != 0))
                    {
                        QMF_RE(Xsbr[i + sbr->tHFAdj][m+sbr->kx]) -=
                            (rev*phi_re[i_plus1] * MUL_F(adj->S_M_boost[l][m + 1], FRAC_CONST(0.00815)));
                    }
                    if ((m == sbr->M - 1) && (sinusoids < 16) && (phi_re[i_min1] != 0))
                    {
                        if (m > 0)
                        {
                            QMF_RE(Xsbr[i + sbr->tHFAdj][m+sbr->kx]) -=
                                (rev*phi_re[i_min1] * MUL_F(adj->S_M_boost[l][m - 1], FRAC_CONST(0.00815)));
                        }
                        if (m + sbr->kx < 64)
                        {
                            QMF_RE(Xsbr[i + sbr->tHFAdj][m+sbr->kx + 1]) +=
                                (rev*phi_re[i_min1] * MUL_F(adj->S_M_boost[l][m], FRAC_CONST(0.00815)));
                        }
                    }
#else
                    if ((m == 0) && (phi_re[i_plus1] != 0))
                    {
                        QMF_RE(Xsbr[i + sbr->tHFAdj][m+sbr->kx - 1]) +=
                            (rev*phi_re[i_plus1] * MUL_F((adj->S_M_boost[l][0]<<REAL_BITS), FRAC_CONST(0.00815)));
                        if (sbr->M != 0)
                        {
                            QMF_RE(Xsbr[i + sbr->tHFAdj][m+sbr->kx]) -=
                                (rev*phi_re[i_plus1] * MUL_F((adj->S_M_boost[l][1]<<REAL_BITS), FRAC_CONST(0.00815)));
                        }
                    }
                    if ((m > 0) && (m < sbr->M - 1) && (sinusoids < 16) && (phi_re[i_min1] != 0))
                    {
                        QMF_RE(Xsbr[i + sbr->tHFAdj][m+sbr->kx]) -=
                            (rev*phi_re[i_min1] * MUL_F((adj->S_M_boost[l][m - 1]<<REAL_BITS), FRAC_CONST(0.00815)));
                    }
                    if ((m > 0) && (m < sbr->M - 1) && (sinusoids < 16) && (phi_re[i_plus1] != 0))
                    {
                        QMF_RE(Xsbr[i + sbr->tHFAdj][m+sbr->kx]) -=
                            (rev*phi_re[i_plus1] * MUL_F((adj->S_M_boost[l][m + 1]<<REAL_BITS), FRAC_CONST(0.00815)));
                    }
                    if ((m == sbr->M - 1) && (sinusoids < 16) && (phi_re[i_min1] != 0))
                    {
                        if (m > 0)
                        {
                            QMF_RE(Xsbr[i + sbr->tHFAdj][m+sbr->kx]) -=
                                (rev*phi_re[i_min1] * MUL_F((adj->S_M_boost[l][m - 1]<<REAL_BITS), FRAC_CONST(0.00815)));
                        }
                        if (m + sbr->kx < 64)
                        {
                            QMF_RE(Xsbr[i + sbr->tHFAdj][m+sbr->kx + 1]) +=
                                (rev*phi_re[i_min1] * MUL_F((adj->S_M_boost[l][m]<<REAL_BITS), FRAC_CONST(0.00815)));
                        }
                    }
#endif

                    if (adj->S_M_boost[l][m] != 0)
                        sinusoids++;
#endif
                }
            }

            fIndexSine = (fIndexSine + 1) & 3;

            /* update the ringbuffer index used for filtering G and Q with h_smooth */
            sbr->GQ_ringbuf_index[ch]++;
            if (sbr->GQ_ringbuf_index[ch] >= 5)
                sbr->GQ_ringbuf_index[ch] = 0;
        }
    }

    sbr->index_noise_prev[ch] = fIndexNoise;
    sbr->psi_is_prev[ch] = fIndexSine;
}

#endif
