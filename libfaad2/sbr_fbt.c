/*
** FAAD2 - Freeware Advanced Audio (AAC) Decoder including SBR decoding
** Copyright (C) 2003 M. Bakker, Ahead Software AG, http://www.nero.com
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
** $Id: sbr_fbt.c,v 1.1 2003/07/29 08:20:13 menno Exp $
**/

/* Calculate frequency band tables */

#include "common.h"
#include "structs.h"

#ifdef SBR_DEC

#include <stdlib.h>

#include "sbr_syntax.h"
#include "sbr_fbt.h"


/* calculate the start QMF channel for the master frequency band table */
/* parameter is also called k0 */
uint16_t qmf_start_channel(uint8_t bs_start_freq, uint8_t bs_samplerate_mode,
                           uint32_t sample_rate)
{
    static int8_t offset[7][16] = {
        { -8, -7, -6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7 },
        { -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, 9, 11, 13 },
        { -5, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, 9, 11, 13, 16 },
        { -6, -4, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, 9, 11, 13, 16 },
        { -4, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, 9, 11, 13, 16, 20 },
        { -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, 9, 11, 13, 16, 20, 24 },
        { 0, 1, 2, 3, 4, 5, 6, 7, 9, 11, 13, 16, 20, 24, 28, 33 }
    };
    uint8_t startMin;

    if (sample_rate >= 64000)
    {
        startMin = (uint8_t)((5000.*128.)/(float)sample_rate + 0.5);
    } else if (sample_rate < 32000) {
        startMin = (uint8_t)((3000.*128.)/(float)sample_rate + 0.5);
    } else {
        startMin = (uint8_t)((4000.*128.)/(float)sample_rate + 0.5);
    }

    if (bs_samplerate_mode)
    {
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
    } else {
        return startMin + offset[6][bs_start_freq];
    }
}

static int32_t longcmp(const void *a, const void *b)
{
    return ((int32_t)(*(int32_t*)a - *(int32_t*)b));
}

/* calculate the stop QMF channel for the master frequency band table */
/* parameter is also called k2 */
uint16_t qmf_stop_channel(uint8_t bs_stop_freq, uint32_t sample_rate,
                          uint16_t k0)
{
    if (bs_stop_freq == 15)
    {
        return min(64, k0 * 3);
    } else if (bs_stop_freq == 14) {
        return min(64, k0 * 2);
    } else {
        uint8_t i;
        uint8_t stopMin;
        int32_t stopDk[13], stopDk_t[14], k2;

        if (sample_rate >= 64000)
        {
            stopMin = (uint8_t)((10000.*128.)/(float)sample_rate + 0.5);
        } else if (sample_rate < 32000) {
            stopMin = (uint8_t)((6000.*128.)/(float)sample_rate + 0.5);
        } else {
            stopMin = (uint8_t)((8000.*128.)/(float)sample_rate + 0.5);
        }

        for (i = 0; i <= 13; i++)
        {
            stopDk_t[i] = (int32_t)(stopMin*pow(64.0/stopMin, i/13.0) + 0.5);
        }
        for (i = 0; i < 13; i++)
        {
            stopDk[i] = stopDk_t[i+1] - stopDk_t[i];
        }

        /* needed? or does this always reverse the array? */
        qsort(stopDk, 13, sizeof(stopDk[0]), longcmp);

        k2 = stopMin;
        for (i = 0; i < bs_stop_freq; i++)
        {
            k2 += stopDk[i];
        }
        return min(64, k2);
    }

    return 0;
}

/* calculate the master frequency table from k0, k2, bs_freq_scale
   and bs_alter_scale

   version for bs_freq_scale = 0
*/
void master_frequency_table_fs0(sbr_info *sbr, uint16_t k0, uint16_t k2,
                                uint8_t bs_alter_scale)
{
    int8_t incr;
    uint8_t k;
    uint8_t dk;
    uint32_t nrBands, k2Achieved;
    int32_t k2Diff, vDk[64];

    memset(vDk, 0, 64*sizeof(int32_t));

    /* mft only defined for k2 > k0 */
    if (k2 <= k0)
    {
        sbr->N_master = 0;
        return;
    }

    dk = bs_alter_scale ? 2 : 1;
    nrBands = 2 * (int32_t)((float)(k2-k0)/(dk*2) + (-1+dk)/2.0f);
    nrBands = min(nrBands, 64);

    k2Achieved = k0 + nrBands * dk;
    k2Diff = k2 - k2Achieved;
    /* for (k = 0; k <= nrBands; k++) Typo fix */
    for (k = 0; k < nrBands; k++)
        vDk[k] = dk;

    if (k2Diff)
    {
        incr = (k2Diff > 0) ? -1 : 1;
        k = (k2Diff > 0) ? (nrBands-1) : 0;

        while (k2Diff != 0)
        {
            vDk[k] -= incr;
            k += incr;
            k2Diff += incr;
        }
    }

    sbr->f_master[0] = k0;
    for (k = 1; k <= nrBands; k++)
        sbr->f_master[k] = sbr->f_master[k-1] + vDk[k-1];

    sbr->N_master = nrBands;
    sbr->N_master = min(sbr->N_master, 64);

#if 0
    printf("f_master[%d]: ", nrBands);
    for (k = 0; k <= nrBands; k++)
    {
        printf("%d ", sbr->f_master[k]);
    }
    printf("\n");
#endif
}

/*
   version for bs_freq_scale > 0
*/
void master_frequency_table(sbr_info *sbr, uint16_t k0, uint16_t k2,
                            uint8_t bs_freq_scale, uint8_t bs_alter_scale)
{
    uint8_t k, bands, twoRegions;
    uint16_t k1;
    uint32_t nrBand0, nrBand1;
    int32_t vDk0[64], vDk1[64];
    int32_t vk0[64], vk1[64];
    float warp;
    uint8_t temp1[] = { 12, 10, 8 };
    float temp2[] = { 1.0, 1.3 };

    /* without memset code enters infinte loop,
       so there must be some wrong table access */
    memset(vDk0, 0, 64*sizeof(int32_t));
    memset(vDk1, 0, 64*sizeof(int32_t));
    memset(vk0, 0, 64*sizeof(int32_t));
    memset(vk1, 0, 64*sizeof(int32_t));

    /* mft only defined for k2 > k0 */
    if (k2 <= k0)
    {
        sbr->N_master = 0;
        return;
    }

    bands = temp1[bs_freq_scale-1];
    warp = temp2[bs_alter_scale];

    if ((float)k2/(float)k0 > 2.2449)
    {
        twoRegions = 1;
        k1 = 2 * k0;
    } else {
        twoRegions = 0;
        k1 = k2;
    }

    nrBand0 = 2 * (int32_t)(bands * log((float)k1/(float)k0)/(2.0*log(2.0)) + 0.5);
    nrBand0 = min(nrBand0, 64);
    for (k = 0; k <= nrBand0; k++)
    {
        /* MAPLE */
        vDk0[k] = (int32_t)(k0 * pow((float)k1/(float)k0, (k+1)/(float)nrBand0)+0.5) -
            (int32_t)(k0 * pow((float)k1/(float)k0, k/(float)nrBand0)+0.5);
    }

    /* needed? */
    qsort(vDk0, nrBand0, sizeof(vDk0[0]), longcmp);

    vk0[0] = k0;
    for (k = 1; k <= nrBand0; k++)
    {
        vk0[k] = vk0[k-1] + vDk0[k-1];
    }

    if (!twoRegions)
    {
        for (k = 0; k <= nrBand0; k++)
            sbr->f_master[k] = vk0[k];

        sbr->N_master = nrBand0;
        sbr->N_master = min(sbr->N_master, 64);
        return;
    }

    nrBand1 = 2 * (int32_t)(bands * log((float)k2/(float)k1)/(2.0 * log(2.0) * warp) + 0.5);
    nrBand1 = min(nrBand1, 64);

    for (k = 0; k <= nrBand1 - 1; k++)
    {
        vDk1[k] = (int32_t)(k1 * pow((float)k2/(float)k1, (k+1)/(float)nrBand1)+0.5) -
            (int32_t)(k1 * pow((float)k2/(float)k1, k/(float)nrBand1)+0.5);
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
    }

    sbr->N_master = nrBand0 + nrBand1;
    sbr->N_master = min(sbr->N_master, 64);
    for (k = 0; k <= nrBand0; k++)
    {
        sbr->f_master[k] = vk0[k];
    }
    for (k = nrBand0 + 1; k <= sbr->N_master; k++)
    {
        sbr->f_master[k] = vk1[k - nrBand0];
    }

#if 0
    printf("f_master[%d]: ", sbr->N_master);
    for (k = 0; k <= sbr->N_master; k++)
    {
        printf("%d ", sbr->f_master[k]);
    }
    printf("\n");
#endif
}

/* calculate the derived frequency border tables from f_master */
void derived_frequency_table(sbr_info *sbr, uint8_t bs_xover_band,
                             uint16_t k2)
{
    uint8_t k, i;
    uint32_t minus;

    sbr->N_high = sbr->N_master - bs_xover_band;

    /* is this accurate? */
    sbr->N_low = sbr->N_high/2 + (sbr->N_high - 2 * (sbr->N_high/2));

    sbr->n[0] = sbr->N_low;
    sbr->n[1] = sbr->N_high;

    for (k = 0; k <= sbr->N_high; k++)
    {
        sbr->f_table_res[HI_RES][k] = sbr->f_master[k + bs_xover_band];
    }

    sbr->M = sbr->f_table_res[HI_RES][sbr->N_high] - sbr->f_table_res[HI_RES][0];
    sbr->kx = sbr->f_table_res[HI_RES][0];

    /* correct? */
    minus = (sbr->N_high & 1) ? 1 : 0;

    for (k = 0; k <= sbr->N_low; k++)
    {
        if (k == 0)
            i = 0;
        else
            i = 2*k - minus;
        sbr->f_table_res[LO_RES][k] = sbr->f_table_res[HI_RES][i];
    }
    /* end correct? */

#if 0
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
        /* MAPLE */
        sbr->N_Q = max(1, (int32_t)(sbr->bs_noise_bands*(log(k2/(float)sbr->kx)/log(2.0)) + 0.5));
        if (sbr->N_Q == 0)
            sbr->N_Q = 1;
    }
    sbr->N_Q = min(5, sbr->N_Q);

    for (k = 0; k <= sbr->N_Q; k++)
    {
        if (k == 0)
            i = 0;
        else /* is this accurate? */
            i = i + (int32_t)((sbr->N_low - i)/(sbr->N_Q + 1 - k));
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
        printf("%d ", sbr->f_table_noise[k]);
    }
    printf("\n");
#endif
}

/* TODO: blegh, ugly */
/* Modified to calculate for all possible bs_limiter_bands always
 * This reduces the number calls to this functions needed (now only on
 * header reset)
 */
void limiter_frequency_table(sbr_info *sbr)
{
    static real_t limiterBandsPerOctave[] = { REAL_CONST(1.2),
        REAL_CONST(2), REAL_CONST(3) };
    uint8_t k, s;
    int8_t nrLim;
    int32_t limTable[100 /*TODO*/];
    uint8_t patchBorders[64/*??*/];
    real_t limBands;

    sbr->f_table_lim[0][0] = sbr->f_table_res[LO_RES][0] - sbr->kx;
    sbr->f_table_lim[0][1] = sbr->f_table_res[LO_RES][sbr->N_low] - sbr->kx;
    sbr->N_L[0] = 1;

    for (s = 1; s < 4; s++)
    {
        memset(limTable, 0, 100*sizeof(int32_t));

        limBands = limiterBandsPerOctave[s - 1];

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
                nOctaves = REAL_CONST(log((float)limTable[k]/(float)limTable[k-1])/log(2.0));
            else
                nOctaves = 0;

            if ((MUL(nOctaves,limBands)) < REAL_CONST(0.49))
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
