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
** $Id: sbr_fbt.c,v 1.17 2004/09/08 09:43:11 gcp Exp $
**/

/* Calculate frequency band tables */

#include "common.h"
#include "structs.h"

#ifdef SBR_DEC

#include <stdlib.h>

#include "sbr_syntax.h"
#include "sbr_fbt.h"

/* static function declarations */
static int32_t find_bands(uint8_t warp, uint8_t bands, uint8_t a0, uint8_t a1);


/* calculate the start QMF channel for the master frequency band table */
/* parameter is also called k0 */
uint8_t qmf_start_channel(uint8_t bs_start_freq, uint8_t bs_samplerate_mode,
                           uint32_t sample_rate)
{
    static const uint8_t startMinTable[12] = { 7, 7, 10, 11, 12, 16, 16,
        17, 24, 32, 35, 48 };
    static const uint8_t offsetIndexTable[12] = { 5, 5, 4, 4, 4, 3, 2, 1, 0,
        6, 6, 6 };
    static const int8_t offset[7][16] = {
        { -8, -7, -6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7 },
        { -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, 9, 11, 13 },
        { -5, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, 9, 11, 13, 16 },
        { -6, -4, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, 9, 11, 13, 16 },
        { -4, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, 9, 11, 13, 16, 20 },
        { -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, 9, 11, 13, 16, 20, 24 },
        { 0, 1, 2, 3, 4, 5, 6, 7, 9, 11, 13, 16, 20, 24, 28, 33 }
    };
    uint8_t startMin = startMinTable[get_sr_index(sample_rate)];
    uint8_t offsetIndex = offsetIndexTable[get_sr_index(sample_rate)];

#if 0 /* replaced with table (startMinTable) */
    if (sample_rate >= 64000)
    {
        startMin = (uint8_t)((5000.*128.)/(float)sample_rate + 0.5);
    } else if (sample_rate < 32000) {
        startMin = (uint8_t)((3000.*128.)/(float)sample_rate + 0.5);
    } else {
        startMin = (uint8_t)((4000.*128.)/(float)sample_rate + 0.5);
    }
#endif

    if (bs_samplerate_mode)
    {
        return startMin + offset[offsetIndex][bs_start_freq];

#if 0 /* replaced by offsetIndexTable */ 
        switch (sample_rate)
        {
        case 16000:
            return startMin + offset[0][bs_start_freq];
        case 22050:
            return startMin + offset[1][bs_start_freq];
        case 24000:
            return startMin + offset[2][bs_start_freq];
        case 32000:
            return startMin + offset[3][bs_start_freq];
        default:
            if (sample_rate > 64000)
            {
                return startMin + offset[5][bs_start_freq];
            } else { /* 44100 <= sample_rate <= 64000 */
                return startMin + offset[4][bs_start_freq];
            }
        }
#endif
    } else {
        return startMin + offset[6][bs_start_freq];
    }
}

static int longcmp(const void *a, const void *b)
{
    return ((int)(*(int32_t*)a - *(int32_t*)b));
}

/* calculate the stop QMF channel for the master frequency band table */
/* parameter is also called k2 */
uint8_t qmf_stop_channel(uint8_t bs_stop_freq, uint32_t sample_rate,
                          uint8_t k0)
{
    if (bs_stop_freq == 15)
    {
        return min(64, k0 * 3);
    } else if (bs_stop_freq == 14) {
        return min(64, k0 * 2);
    } else {
        static const uint8_t stopMinTable[12] = { 13, 15, 20, 21, 23,
            32, 32, 35, 48, 64, 70, 96 };
        static const int8_t offset[12][14] = {
            { 0, 2, 4, 6, 8, 11, 14, 18, 22, 26, 31, 37, 44, 51 },
            { 0, 2, 4, 6, 8, 11, 14, 18, 22, 26, 31, 36, 42, 49 },
            { 0, 2, 4, 6, 8, 11, 14, 17, 21, 25, 29, 34, 39, 44 },
            { 0, 2, 4, 6, 8, 11, 14, 17, 20, 24, 28, 33, 38, 43 },
            { 0, 2, 4, 6, 8, 11, 14, 17, 20, 24, 28, 32, 36, 41 },
            { 0, 2, 4, 6, 8, 10, 12, 14, 17, 20, 23, 26, 29, 32 },
            { 0, 2, 4, 6, 8, 10, 12, 14, 17, 20, 23, 26, 29, 32 },
            { 0, 1, 3, 5, 7, 9, 11, 13, 15, 17, 20, 23, 26, 29 },
            { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 14, 16 },
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
            { 0, -1, -2, -3, -4, -5, -6, -6, -6, -6, -6, -6, -6, -6 },
            { 0, -3, -6, -9, -12, -15, -18, -20, -22, -24, -26, -28, -30, -32 }
        };
#if 0
        uint8_t i;
        int32_t stopDk[13], stopDk_t[14], k2;
#endif
        uint8_t stopMin = stopMinTable[get_sr_index(sample_rate)];

#if 0 /* replaced by table lookup */
        if (sample_rate >= 64000)
        {
            stopMin = (uint8_t)((10000.*128.)/(float)sample_rate + 0.5);
        } else if (sample_rate < 32000) {
            stopMin = (uint8_t)((6000.*128.)/(float)sample_rate + 0.5);
        } else {
            stopMin = (uint8_t)((8000.*128.)/(float)sample_rate + 0.5);
        }
#endif

#if 0 /* replaced by table lookup */
        /* diverging power series */
        for (i = 0; i <= 13; i++)
        {
            stopDk_t[i] = (int32_t)(stopMin*pow(64.0/stopMin, i/13.0) + 0.5);
        }
        for (i = 0; i < 13; i++)
        {
            stopDk[i] = stopDk_t[i+1] - stopDk_t[i];
        }

        /* needed? */
        qsort(stopDk, 13, sizeof(stopDk[0]), longcmp);

        k2 = stopMin;
        for (i = 0; i < bs_stop_freq; i++)
        {
            k2 += stopDk[i];
        }
        return min(64, k2);
#endif
        /* bs_stop_freq <= 13 */
        return min(64, stopMin + offset[get_sr_index(sample_rate)][min(bs_stop_freq, 13)]);
    }

    return 0;
}

/* calculate the master frequency table from k0, k2, bs_freq_scale
   and bs_alter_scale

   version for bs_freq_scale = 0
*/
uint8_t master_frequency_table_fs0(sbr_info *sbr, uint8_t k0, uint8_t k2,
                                   uint8_t bs_alter_scale)
{
    int8_t incr;
    uint8_t k;
    uint8_t dk;
    uint32_t nrBands, k2Achieved;
    int32_t k2Diff, vDk[64] = {0};

    /* mft only defined for k2 > k0 */
    if (k2 <= k0)
    {
        sbr->N_master = 0;
        return 1;
    }

    dk = bs_alter_scale ? 2 : 1;

#if 0 /* replaced by float-less design */
    nrBands = 2 * (int32_t)((float)(k2-k0)/(dk*2) + (-1+dk)/2.0f);
#else
    if (bs_alter_scale)
    {
        nrBands = (((k2-k0+2)>>2)<<1);
    } else {
        nrBands = (((k2-k0)>>1)<<1);
    }
#endif
    nrBands = min(nrBands, 63);
    if (nrBands <= 0)
        return 1;

    k2Achieved = k0 + nrBands * dk;
    k2Diff = k2 - k2Achieved;
    for (k = 0; k < nrBands; k++)
        vDk[k] = dk;

    if (k2Diff)
    {
        incr = (k2Diff > 0) ? -1 : 1;
        k = (uint8_t) ((k2Diff > 0) ? (nrBands-1) : 0);

        while (k2Diff != 0)
        {
            vDk[k] -= incr;
            k += incr;
            k2Diff += incr;
        }
    }

    sbr->f_master[0] = k0;
    for (k = 1; k <= nrBands; k++)
        sbr->f_master[k] = (uint8_t)(sbr->f_master[k-1] + vDk[k-1]);

    sbr->N_master = (uint8_t)nrBands;
    sbr->N_master = (min(sbr->N_master, 64));

#if 0
    printf("f_master[%d]: ", nrBands);
    for (k = 0; k <= nrBands; k++)
    {
        printf("%d ", sbr->f_master[k]);
    }
    printf("\n");
#endif

    return 0;
}

/*
   This function finds the number of bands using this formula:
    bands * log(a1/a0)/log(2.0) + 0.5
*/
static int32_t find_bands(uint8_t warp, uint8_t bands, uint8_t a0, uint8_t a1)
{
#ifdef FIXED_POINT
    /* table with log2() values */
    static const real_t log2Table[65] = {
        COEF_CONST(0.0), COEF_CONST(0.0), COEF_CONST(1.0000000000), COEF_CONST(1.5849625007),
        COEF_CONST(2.0000000000), COEF_CONST(2.3219280949), COEF_CONST(2.5849625007), COEF_CONST(2.8073549221),
        COEF_CONST(3.0000000000), COEF_CONST(3.1699250014), COEF_CONST(3.3219280949), COEF_CONST(3.4594316186),
        COEF_CONST(3.5849625007), COEF_CONST(3.7004397181), COEF_CONST(3.8073549221), COEF_CONST(3.9068905956),
        COEF_CONST(4.0000000000), COEF_CONST(4.0874628413), COEF_CONST(4.1699250014), COEF_CONST(4.2479275134),
        COEF_CONST(4.3219280949), COEF_CONST(4.3923174228), COEF_CONST(4.4594316186), COEF_CONST(4.5235619561),
        COEF_CONST(4.5849625007), COEF_CONST(4.6438561898), COEF_CONST(4.7004397181), COEF_CONST(4.7548875022),
        COEF_CONST(4.8073549221), COEF_CONST(4.8579809951), COEF_CONST(4.9068905956), COEF_CONST(4.9541963104),
        COEF_CONST(5.0000000000), COEF_CONST(5.0443941194), COEF_CONST(5.0874628413), COEF_CONST(5.1292830169),
        COEF_CONST(5.1699250014), COEF_CONST(5.2094533656), COEF_CONST(5.2479275134), COEF_CONST(5.2854022189),
        COEF_CONST(5.3219280949), COEF_CONST(5.3575520046), COEF_CONST(5.3923174228), COEF_CONST(5.4262647547),
        COEF_CONST(5.4594316186), COEF_CONST(5.4918530963), COEF_CONST(5.5235619561), COEF_CONST(5.5545888517),
        COEF_CONST(5.5849625007), COEF_CONST(5.6147098441), COEF_CONST(5.6438561898), COEF_CONST(5.6724253420),
        COEF_CONST(5.7004397181), COEF_CONST(5.7279204546), COEF_CONST(5.7548875022), COEF_CONST(5.7813597135),
        COEF_CONST(5.8073549221), COEF_CONST(5.8328900142), COEF_CONST(5.8579809951), COEF_CONST(5.8826430494),
        COEF_CONST(5.9068905956), COEF_CONST(5.9307373376), COEF_CONST(5.9541963104), COEF_CONST(5.9772799235),
        COEF_CONST(6.0)
    };
    real_t r0 = log2Table[a0]; /* coef */
    real_t r1 = log2Table[a1]; /* coef */
    real_t r2 = (r1 - r0); /* coef */

    if (warp)
        r2 = MUL_C(r2, COEF_CONST(1.0/1.3));

    /* convert r2 to real and then multiply and round */
    r2 = (r2 >> (COEF_BITS-REAL_BITS)) * bands + (1<<(REAL_BITS-1));

    return (r2 >> REAL_BITS);
#else
    real_t div = (real_t)log(2.0);
    if (warp) div *= (real_t)1.3;

    return (int32_t)(bands * log((float)a1/(float)a0)/div + 0.5);
#endif
}

static real_t find_initial_power(uint8_t bands, uint8_t a0, uint8_t a1)
{
#ifdef FIXED_POINT
    /* table with log() values */
    static const real_t logTable[65] = {
        COEF_CONST(0.0), COEF_CONST(0.0), COEF_CONST(0.6931471806), COEF_CONST(1.0986122887),
        COEF_CONST(1.3862943611), COEF_CONST(1.6094379124), COEF_CONST(1.7917594692), COEF_CONST(1.9459101491),
        COEF_CONST(2.0794415417), COEF_CONST(2.1972245773), COEF_CONST(2.3025850930), COEF_CONST(2.3978952728),
        COEF_CONST(2.4849066498), COEF_CONST(2.5649493575), COEF_CONST(2.6390573296), COEF_CONST(2.7080502011),
        COEF_CONST(2.7725887222), COEF_CONST(2.8332133441), COEF_CONST(2.8903717579), COEF_CONST(2.9444389792),
        COEF_CONST(2.9957322736), COEF_CONST(3.0445224377), COEF_CONST(3.0910424534), COEF_CONST(3.1354942159),
        COEF_CONST(3.1780538303), COEF_CONST(3.2188758249), COEF_CONST(3.2580965380), COEF_CONST(3.2958368660),
        COEF_CONST(3.3322045102), COEF_CONST(3.3672958300), COEF_CONST(3.4011973817), COEF_CONST(3.4339872045),
        COEF_CONST(3.4657359028), COEF_CONST(3.4965075615), COEF_CONST(3.5263605246), COEF_CONST(3.5553480615),
        COEF_CONST(3.5835189385), COEF_CONST(3.6109179126), COEF_CONST(3.6375861597), COEF_CONST(3.6635616461),
        COEF_CONST(3.6888794541), COEF_CONST(3.7135720667), COEF_CONST(3.7376696183), COEF_CONST(3.7612001157),
        COEF_CONST(3.7841896339), COEF_CONST(3.8066624898), COEF_CONST(3.8286413965), COEF_CONST(3.8501476017),
        COEF_CONST(3.8712010109), COEF_CONST(3.8918202981), COEF_CONST(3.9120230054), COEF_CONST(3.9318256327),
        COEF_CONST(3.9512437186), COEF_CONST(3.9702919136), COEF_CONST(3.9889840466), COEF_CONST(4.0073331852),
        COEF_CONST(4.0253516907), COEF_CONST(4.0430512678), COEF_CONST(4.0604430105), COEF_CONST(4.0775374439),
        COEF_CONST(4.0943445622), COEF_CONST(4.1108738642), COEF_CONST(4.1271343850), COEF_CONST(4.1431347264),
        COEF_CONST(4.158883083)
    };
    /* standard Taylor polynomial coefficients for exp(x) around 0 */
    /* a polynomial around x=1 is more precise, as most values are around 1.07,
       but this is just fine already */
    static const real_t c1 = COEF_CONST(1.0);
    static const real_t c2 = COEF_CONST(1.0/2.0);
    static const real_t c3 = COEF_CONST(1.0/6.0);
    static const real_t c4 = COEF_CONST(1.0/24.0);

    real_t r0 = logTable[a0]; /* coef */
    real_t r1 = logTable[a1]; /* coef */
    real_t r2 = (r1 - r0) / bands; /* coef */
    real_t rexp = c1 + MUL_C((c1 + MUL_C((c2 + MUL_C((c3 + MUL_C(c4,r2)), r2)), r2)), r2);

    return (rexp >> (COEF_BITS-REAL_BITS)); /* real */
#else
    return (real_t)pow((real_t)a1/(real_t)a0, 1.0/(real_t)bands);
#endif
}

/*
   version for bs_freq_scale > 0
*/
uint8_t master_frequency_table(sbr_info *sbr, uint8_t k0, uint8_t k2,
                               uint8_t bs_freq_scale, uint8_t bs_alter_scale)
{
    uint8_t k, bands, twoRegions;
    uint8_t k1;
    uint8_t nrBand0, nrBand1;
    int32_t vDk0[64] = {0}, vDk1[64] = {0};
    int32_t vk0[64] = {0}, vk1[64] = {0};
    uint8_t temp1[] = { 6, 5, 4 };
    real_t q, qk;
    int32_t A_1;
#ifdef FIXED_POINT
    real_t rk2, rk0;
#endif

    /* mft only defined for k2 > k0 */
    if (k2 <= k0)
    {
        sbr->N_master = 0;
        return 1;
    }

    bands = temp1[bs_freq_scale-1];

#ifdef FIXED_POINT
    rk0 = (real_t)k0 << REAL_BITS;
    rk2 = (real_t)k2 << REAL_BITS;
    if (rk2 > MUL_C(rk0, COEF_CONST(2.2449)))
#else
    if ((float)k2/(float)k0 > 2.2449)
#endif
    {
        twoRegions = 1;
        k1 = k0 << 1;
    } else {
        twoRegions = 0;
        k1 = k2;
    }

    nrBand0 = (uint8_t)(2 * find_bands(0, bands, k0, k1));
    nrBand0 = min(nrBand0, 63);
    if (nrBand0 <= 0)
        return 1;

    q = find_initial_power(nrBand0, k0, k1);
#ifdef FIXED_POINT
    qk = (real_t)k0 << REAL_BITS;
    //A_1 = (int32_t)((qk + REAL_CONST(0.5)) >> REAL_BITS);
    A_1 = k0;
#else
    qk = REAL_CONST(k0);
    A_1 = (int32_t)(qk + .5);
#endif
    for (k = 0; k <= nrBand0; k++)
    {
        int32_t A_0 = A_1;
#ifdef FIXED_POINT
        qk = MUL_R(qk,q);
        A_1 = (int32_t)((qk + REAL_CONST(0.5)) >> REAL_BITS);
#else
        qk *= q;
        A_1 = (int32_t)(qk + 0.5);
#endif
        vDk0[k] = A_1 - A_0;
    }

    /* needed? */
    qsort(vDk0, nrBand0, sizeof(vDk0[0]), longcmp);

    vk0[0] = k0;
    for (k = 1; k <= nrBand0; k++)
    {
        vk0[k] = vk0[k-1] + vDk0[k-1];
        if (vDk0[k-1] == 0)
            return 1;
    }

    if (!twoRegions)
    {
        for (k = 0; k <= nrBand0; k++)
            sbr->f_master[k] = (uint8_t) vk0[k];

        sbr->N_master = nrBand0;
        sbr->N_master = min(sbr->N_master, 64);
        return 0;
    }

    nrBand1 = (uint8_t)(2 * find_bands(1 /* warped */, bands, k1, k2));
    nrBand1 = min(nrBand1, 63);

    q = find_initial_power(nrBand1, k1, k2);
#ifdef FIXED_POINT
    qk = (real_t)k1 << REAL_BITS;
    //A_1 = (int32_t)((qk + REAL_CONST(0.5)) >> REAL_BITS);
    A_1 = k1;
#else
    qk = REAL_CONST(k1);
    A_1 = (int32_t)(qk + .5);
#endif
    for (k = 0; k <= nrBand1 - 1; k++)
    {
        int32_t A_0 = A_1;
#ifdef FIXED_POINT
        qk = MUL_R(qk,q);
        A_1 = (int32_t)((qk + REAL_CONST(0.5)) >> REAL_BITS);
#else
        qk *= q;
        A_1 = (int32_t)(qk + 0.5);
#endif
        vDk1[k] = A_1 - A_0;
    }

    if (vDk1[0] < vDk0[nrBand0 - 1])
    {
        int32_t change;

        /* needed? */
        qsort(vDk1, nrBand1 + 1, sizeof(vDk1[0]), longcmp);
        change = vDk0[nrBand0 - 1] - vDk1[0];
        vDk1[0] = vDk0[nrBand0 - 1];
        vDk1[nrBand1 - 1] = vDk1[nrBand1 - 1] - change;
    }

    /* needed? */
    qsort(vDk1, nrBand1, sizeof(vDk1[0]), longcmp);
    vk1[0] = k1;
    for (k = 1; k <= nrBand1; k++)
    {
        vk1[k] = vk1[k-1] + vDk1[k-1];
        if (vDk1[k-1] == 0)
            return 1;
    }

    sbr->N_master = nrBand0 + nrBand1;
    sbr->N_master = min(sbr->N_master, 64);
    for (k = 0; k <= nrBand0; k++)
    {
        sbr->f_master[k] =  (uint8_t) vk0[k];
    }
    for (k = nrBand0 + 1; k <= sbr->N_master; k++)
    {
        sbr->f_master[k] = (uint8_t) vk1[k - nrBand0];
    }

#if 0
    printf("f_master[%d]: ", sbr->N_master);
    for (k = 0; k <= sbr->N_master; k++)
    {
        printf("%d ", sbr->f_master[k]);
    }
    printf("\n");
#endif

    return 0;
}

/* calculate the derived frequency border tables from f_master */
uint8_t derived_frequency_table(sbr_info *sbr, uint8_t bs_xover_band,
                                uint8_t k2)
{
    uint8_t k, i;
    uint32_t minus;

    /* The following relation shall be satisfied: bs_xover_band < N_Master */
    if (sbr->N_master <= bs_xover_band)
        return 1;

    sbr->N_high = sbr->N_master - bs_xover_band;
    sbr->N_low = (sbr->N_high>>1) + (sbr->N_high - ((sbr->N_high>>1)<<1));

    sbr->n[0] = sbr->N_low;
    sbr->n[1] = sbr->N_high;

    for (k = 0; k <= sbr->N_high; k++)
    {
        sbr->f_table_res[HI_RES][k] = sbr->f_master[k + bs_xover_band];
    }

    sbr->M = sbr->f_table_res[HI_RES][sbr->N_high] - sbr->f_table_res[HI_RES][0];
    sbr->kx = sbr->f_table_res[HI_RES][0];
    if (sbr->kx > 32)
        return 1;
    if (sbr->kx + sbr->M > 64)
        return 1;

    minus = (sbr->N_high & 1) ? 1 : 0;

    for (k = 0; k <= sbr->N_low; k++)
    {
        if (k == 0)
            i = 0;
        else
            i = (uint8_t)(2*k - minus);
        sbr->f_table_res[LO_RES][k] = sbr->f_table_res[HI_RES][i];
    }

#if 0
    printf("bs_freq_scale: %d\n", sbr->bs_freq_scale);
    printf("bs_limiter_bands: %d\n", sbr->bs_limiter_bands);
    printf("f_table_res[HI_RES][%d]: ", sbr->N_high);
    for (k = 0; k <= sbr->N_high; k++)
    {
        printf("%d ", sbr->f_table_res[HI_RES][k]);
    }
    printf("\n");
#endif
#if 0
    printf("f_table_res[LO_RES][%d]: ", sbr->N_low);
    for (k = 0; k <= sbr->N_low; k++)
    {
        printf("%d ", sbr->f_table_res[LO_RES][k]);
    }
    printf("\n");
#endif

    sbr->N_Q = 0;
    if (sbr->bs_noise_bands == 0)
    {
        sbr->N_Q = 1;
    } else {
#if 0
        sbr->N_Q = max(1, (int32_t)(sbr->bs_noise_bands*(log(k2/(float)sbr->kx)/log(2.0)) + 0.5));
#else
        sbr->N_Q = (uint8_t)(max(1, find_bands(0, sbr->bs_noise_bands, sbr->kx, k2)));
#endif
        sbr->N_Q = min(5, sbr->N_Q);
    }

    for (k = 0; k <= sbr->N_Q; k++)
    {
        if (k == 0)
        {
            i = 0;
        } else {
            /* i = i + (int32_t)((sbr->N_low - i)/(sbr->N_Q + 1 - k)); */
            i = i + (sbr->N_low - i)/(sbr->N_Q + 1 - k);
        }
        sbr->f_table_noise[k] = sbr->f_table_res[LO_RES][i];
    }

    /* build table for mapping k to g in hf patching */
    for (k = 0; k < 64; k++)
    {
        uint8_t g;
        for (g = 0; g < sbr->N_Q; g++)
        {
            if ((sbr->f_table_noise[g] <= k) &&
                (k < sbr->f_table_noise[g+1]))
            {
                sbr->table_map_k_to_g[k] = g;
                break;
            }
        }
    }

#if 0
    printf("f_table_noise[%d]: ", sbr->N_Q);
    for (k = 0; k <= sbr->N_Q; k++)
    {
        printf("%d ", sbr->f_table_noise[k] - sbr->kx);
    }
    printf("\n");
#endif

    return 0;
}

/* TODO: blegh, ugly */
/* Modified to calculate for all possible bs_limiter_bands always
 * This reduces the number calls to this functions needed (now only on
 * header reset)
 */
void limiter_frequency_table(sbr_info *sbr)
{
#if 0
    static const real_t limiterBandsPerOctave[] = { REAL_CONST(1.2),
        REAL_CONST(2), REAL_CONST(3) };
#else
    static const real_t limiterBandsCompare[] = { REAL_CONST(1.327152),
        REAL_CONST(1.185093), REAL_CONST(1.119872) };
#endif
    uint8_t k, s;
    int8_t nrLim;
#if 0
    real_t limBands;
#endif

    sbr->f_table_lim[0][0] = sbr->f_table_res[LO_RES][0] - sbr->kx;
    sbr->f_table_lim[0][1] = sbr->f_table_res[LO_RES][sbr->N_low] - sbr->kx;
    sbr->N_L[0] = 1;

#if 0
    printf("f_table_lim[%d][%d]: ", 0, sbr->N_L[0]);
    for (k = 0; k <= sbr->N_L[0]; k++)
    {
        printf("%d ", sbr->f_table_lim[0][k]);
    }
    printf("\n");
#endif

    for (s = 1; s < 4; s++)
    {
        int32_t limTable[100 /*TODO*/] = {0};
        uint8_t patchBorders[64/*??*/] = {0};

#if 0
        limBands = limiterBandsPerOctave[s - 1];
#endif

        patchBorders[0] = sbr->kx;
        for (k = 1; k <= sbr->noPatches; k++)
        {
            patchBorders[k] = patchBorders[k-1] + sbr->patchNoSubbands[k-1];
        }

        for (k = 0; k <= sbr->N_low; k++)
        {
            limTable[k] = sbr->f_table_res[LO_RES][k];
        }
        for (k = 1; k < sbr->noPatches; k++)
        {
            limTable[k+sbr->N_low] = patchBorders[k];
        }

        /* needed */
        qsort(limTable, sbr->noPatches + sbr->N_low, sizeof(limTable[0]), longcmp);
        k = 1;
        nrLim = sbr->noPatches + sbr->N_low - 1;

        if (nrLim < 0) // TODO: BIG FAT PROBLEM
            return;

restart:
        if (k <= nrLim)
        {
            real_t nOctaves;

            if (limTable[k-1] != 0)
#if 0
                nOctaves = REAL_CONST(log((float)limTable[k]/(float)limTable[k-1])/log(2.0));
#else
#ifdef FIXED_POINT
                nOctaves = DIV_R((limTable[k]<<REAL_BITS),REAL_CONST(limTable[k-1]));
#else
                nOctaves = (real_t)limTable[k]/(real_t)limTable[k-1];
#endif
#endif
            else
                nOctaves = 0;

#if 0
            if ((MUL_R(nOctaves,limBands)) < REAL_CONST(0.49))
#else
            if (nOctaves < limiterBandsCompare[s - 1])
#endif
            {
                uint8_t i;
                if (limTable[k] != limTable[k-1])
                {
                    uint8_t found = 0, found2 = 0;
                    for (i = 0; i <= sbr->noPatches; i++)
                    {
                        if (limTable[k] == patchBorders[i])
                            found = 1;
                    }
                    if (found)
                    {
                        found2 = 0;
                        for (i = 0; i <= sbr->noPatches; i++)
                        {
                            if (limTable[k-1] == patchBorders[i])
                                found2 = 1;
                        }
                        if (found2)
                        {
                            k++;
                            goto restart;
                        } else {
                            /* remove (k-1)th element */
                            limTable[k-1] = sbr->f_table_res[LO_RES][sbr->N_low];
                            qsort(limTable, sbr->noPatches + sbr->N_low, sizeof(limTable[0]), longcmp);
                            nrLim--;
                            goto restart;
                        }
                    }
                }
                /* remove kth element */
                limTable[k] = sbr->f_table_res[LO_RES][sbr->N_low];
                qsort(limTable, nrLim, sizeof(limTable[0]), longcmp);
                nrLim--;
                goto restart;
            } else {
                k++;
                goto restart;
            }
        }

        sbr->N_L[s] = nrLim;
        for (k = 0; k <= nrLim; k++)
        {
            sbr->f_table_lim[s][k] = limTable[k] - sbr->kx;
        }

#if 0
        printf("f_table_lim[%d][%d]: ", s, sbr->N_L[s]);
        for (k = 0; k <= sbr->N_L[s]; k++)
        {
            printf("%d ", sbr->f_table_lim[s][k]);
        }
        printf("\n");
#endif
    }
}

#endif
