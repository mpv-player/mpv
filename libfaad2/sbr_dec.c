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
** $Id: sbr_dec.c,v 1.12 2003/09/25 12:04:31 menno Exp $
**/


#include "common.h"
#include "structs.h"

#ifdef SBR_DEC

#include <stdlib.h>

#include "syntax.h"
#include "bits.h"
#include "sbr_syntax.h"
#include "sbr_qmf.h"
#include "sbr_hfgen.h"
#include "sbr_hfadj.h"


sbr_info *sbrDecodeInit(uint16_t framelength
#ifdef DRM
						, uint8_t IsDRM
#endif
                        )
{
    sbr_info *sbr = malloc(sizeof(sbr_info));
    memset(sbr, 0, sizeof(sbr_info));

    sbr->bs_freq_scale = 2;
    sbr->bs_alter_scale = 1;
    sbr->bs_noise_bands = 2;
    sbr->bs_limiter_bands = 2;
    sbr->bs_limiter_gains = 2;
    sbr->bs_interpol_freq = 1;
    sbr->bs_smoothing_mode = 1;
    sbr->bs_start_freq = 5;
    sbr->bs_amp_res = 1;
    sbr->bs_samplerate_mode = 1;
    sbr->prevEnvIsShort[0] = -1;
    sbr->prevEnvIsShort[1] = -1;
    sbr->header_count = 0;

#ifdef DRM
    sbr->Is_DRM_SBR = 0;
    if (IsDRM)
    {
        sbr->Is_DRM_SBR = 1;
        sbr->tHFGen = T_HFGEN_DRM;
        sbr->tHFAdj = T_HFADJ_DRM;

        /* "offset" is different in DRM */
        sbr->bs_samplerate_mode = 0;
    } else
#endif
    {
        sbr->bs_samplerate_mode = 1;
        sbr->tHFGen = T_HFGEN;
        sbr->tHFAdj = T_HFADJ;
    }

    /* force sbr reset */
    sbr->bs_start_freq_prev = -1;

    if (framelength == 960)
    {
        sbr->numTimeSlotsRate = RATE * NO_TIME_SLOTS_960;
        sbr->numTimeSlots = NO_TIME_SLOTS_960;
    } else {
        sbr->numTimeSlotsRate = RATE * NO_TIME_SLOTS;
        sbr->numTimeSlots = NO_TIME_SLOTS;
    }

    return sbr;
}

void sbrDecodeEnd(sbr_info *sbr)
{
    uint8_t j;

    if (sbr)
    {
        qmfa_end(sbr->qmfa[0]);
        qmfs_end(sbr->qmfs[0]);
        if (sbr->id_aac == ID_CPE)
        {
            qmfa_end(sbr->qmfa[1]);
            qmfs_end(sbr->qmfs[1]);
        }

        if (sbr->Xcodec[0]) free(sbr->Xcodec[0]);
        if (sbr->Xsbr[0]) free(sbr->Xsbr[0]);
        if (sbr->Xcodec[1]) free(sbr->Xcodec[1]);
        if (sbr->Xsbr[1]) free(sbr->Xsbr[1]);

        for (j = 0; j < 5; j++)
        {
            if (sbr->G_temp_prev[0][j]) free(sbr->G_temp_prev[0][j]);
            if (sbr->Q_temp_prev[0][j]) free(sbr->Q_temp_prev[0][j]);
            if (sbr->G_temp_prev[1][j]) free(sbr->G_temp_prev[1][j]);
            if (sbr->Q_temp_prev[1][j]) free(sbr->Q_temp_prev[1][j]);
        }

        free(sbr);
    }
}

void sbr_save_prev_data(sbr_info *sbr, uint8_t ch)
{
    uint8_t i;

    /* save data for next frame */
    sbr->kx_prev = sbr->kx;

    sbr->L_E_prev[ch] = sbr->L_E[ch];

    sbr->f_prev[ch] = sbr->f[ch][sbr->L_E[ch] - 1];
    for (i = 0; i < 64; i++)
    {
        sbr->E_prev[ch][i] = sbr->E[ch][i][sbr->L_E[ch] - 1];
        sbr->Q_prev[ch][i] = sbr->Q[ch][i][sbr->L_Q[ch] - 1];
    }

    for (i = 0; i < 64; i++)
    {
        sbr->bs_add_harmonic_prev[ch][i] = sbr->bs_add_harmonic[ch][i];
    }
    sbr->bs_add_harmonic_flag_prev[ch] = sbr->bs_add_harmonic_flag[ch];

    if (sbr->l_A[ch] == sbr->L_E[ch])
        sbr->prevEnvIsShort[ch] = 0;
    else
        sbr->prevEnvIsShort[ch] = -1;
}

void sbrDecodeFrame(sbr_info *sbr, real_t *left_channel,
                    real_t *right_channel, uint8_t id_aac,
                    uint8_t just_seeked, uint8_t upsample_only)
{
    int16_t i, k, l;

    uint8_t dont_process = 0;
    uint8_t ch, channels, ret;
    real_t *ch_buf;

    qmf_t X[32*64];
#ifdef SBR_LOW_POWER
    real_t deg[64];
#endif

    bitfile *ld = NULL;

    sbr->id_aac = id_aac;
    channels = (id_aac == ID_SCE) ? 1 : 2;

    if (sbr->data == NULL || sbr->data_size == 0)
    {
        ret = 1;
    } else {
        ld = (bitfile*)malloc(sizeof(bitfile));

    /* initialise and read the bitstream */
    faad_initbits(ld, sbr->data, sbr->data_size);

    ret = sbr_extension_data(ld, sbr, id_aac);

    ret = ld->error ? ld->error : ret;
    faad_endbits(ld);
    if (ld) free(ld);
    ld = NULL;
    }

    if (sbr->data) free(sbr->data);
    sbr->data = NULL;

    if (ret || (sbr->header_count == 0))
    {
        /* don't process just upsample */
        dont_process = 1;

        /* Re-activate reset for next frame */
        if (ret && sbr->Reset)
            sbr->bs_start_freq_prev = -1;
    }

    if (just_seeked)
    {
        sbr->just_seeked = 1;
    } else {
        sbr->just_seeked = 0;
    }

    for (ch = 0; ch < channels; ch++)
    {
        if (sbr->frame == 0)
        {
            uint8_t j;
            sbr->qmfa[ch] = qmfa_init(32);
            sbr->qmfs[ch] = qmfs_init(64);
            
            for (j = 0; j < 5; j++)
            {
                sbr->G_temp_prev[ch][j] = malloc(64*sizeof(real_t));
                sbr->Q_temp_prev[ch][j] = malloc(64*sizeof(real_t));
            }

            sbr->Xsbr[ch] = malloc((sbr->numTimeSlotsRate+sbr->tHFGen)*64 * sizeof(qmf_t));
            sbr->Xcodec[ch] = malloc((sbr->numTimeSlotsRate+sbr->tHFGen)*32 * sizeof(qmf_t));

            memset(sbr->Xsbr[ch], 0, (sbr->numTimeSlotsRate+sbr->tHFGen)*64 * sizeof(qmf_t));
            memset(sbr->Xcodec[ch], 0, (sbr->numTimeSlotsRate+sbr->tHFGen)*32 * sizeof(qmf_t));
        }

        if (ch == 0)
            ch_buf = left_channel;
        else
            ch_buf = right_channel;

#if 0
        for (i = 0; i < sbr->tHFAdj; i++)
        {
            int8_t j;
            for (j = sbr->kx_prev; j < sbr->kx; j++)
            {
                QMF_RE(sbr->Xcodec[ch][i*32 + j]) = 0;
#ifndef SBR_LOW_POWER
                QMF_IM(sbr->Xcodec[ch][i*32 + j]) = 0;
#endif
            }
        }
#endif

        /* subband analysis */
        if (dont_process)
            sbr_qmf_analysis_32(sbr, sbr->qmfa[ch], ch_buf, sbr->Xcodec[ch], sbr->tHFGen, 32);
        else
            sbr_qmf_analysis_32(sbr, sbr->qmfa[ch], ch_buf, sbr->Xcodec[ch], sbr->tHFGen, sbr->kx);

        if (!dont_process)
        {
            /* insert high frequencies here */
            /* hf generation using patching */
            hf_generation(sbr, sbr->Xcodec[ch], sbr->Xsbr[ch]
#ifdef SBR_LOW_POWER
                ,deg
#endif
                ,ch);

#ifdef SBR_LOW_POWER
            for (l = sbr->t_E[ch][0]; l < sbr->t_E[ch][sbr->L_E[ch]]; l++)
            {
                for (k = 0; k < sbr->kx; k++)
                {
                    QMF_RE(sbr->Xsbr[ch][(sbr->tHFAdj + l)*64 + k]) = 0;
                }
            }
#endif

#if 1
            /* hf adjustment */
            hf_adjustment(sbr, sbr->Xsbr[ch]
#ifdef SBR_LOW_POWER
                ,deg
#endif
                ,ch);
#endif
        }

        if ((sbr->just_seeked != 0) || dont_process)
        {
            for (l = 0; l < sbr->numTimeSlotsRate; l++)
            {
                for (k = 0; k < 32; k++)
                {
                    QMF_RE(X[l * 64 + k]) = QMF_RE(sbr->Xcodec[ch][(l + sbr->tHFAdj)*32 + k]);
#ifndef SBR_LOW_POWER
                    QMF_IM(X[l * 64 + k]) = QMF_IM(sbr->Xcodec[ch][(l + sbr->tHFAdj)*32 + k]);
#endif
                }
                for (k = 32; k < 64; k++)
                {
                    QMF_RE(X[l * 64 + k]) = 0;
#ifndef SBR_LOW_POWER
                    QMF_IM(X[l * 64 + k]) = 0;
#endif
                }
            }
        } else {
            for (l = 0; l < sbr->numTimeSlotsRate; l++)
            {
                uint8_t xover_band;

                if (l < sbr->t_E[ch][0])
                    xover_band = sbr->kx_prev;
                else
                    xover_band = sbr->kx;

#ifdef DRM
				if (sbr->Is_DRM_SBR)
					xover_band = sbr->kx;
#endif

                for (k = 0; k < xover_band; k++)
                {
                    QMF_RE(X[l * 64 + k]) = QMF_RE(sbr->Xcodec[ch][(l + sbr->tHFAdj)*32 + k]);
#ifndef SBR_LOW_POWER
                    QMF_IM(X[l * 64 + k]) = QMF_IM(sbr->Xcodec[ch][(l + sbr->tHFAdj)*32 + k]);
#endif
                }
                for (k = xover_band; k < 64; k++)
                {
                    QMF_RE(X[l * 64 + k]) = QMF_RE(sbr->Xsbr[ch][(l + sbr->tHFAdj)*64 + k]);
#ifndef SBR_LOW_POWER
                    QMF_IM(X[l * 64 + k]) = QMF_IM(sbr->Xsbr[ch][(l + sbr->tHFAdj)*64 + k]);
#endif
                }
#ifdef SBR_LOW_POWER
                QMF_RE(X[l * 64 + xover_band - 1]) += QMF_RE(sbr->Xsbr[ch][(l + sbr->tHFAdj)*64 + xover_band - 1]);
#endif
#ifdef DRM
                if (sbr->Is_DRM_SBR)
                {
                    for (k = xover_band; k < xover_band + 4; k++)
                    {
                        QMF_RE(X[l * 64 + k]) += QMF_RE(sbr->Xcodec[ch][(l + sbr->tHFAdj)*32 + k]);
                        QMF_IM(X[l * 64 + k]) += QMF_IM(sbr->Xcodec[ch][(l + sbr->tHFAdj)*32 + k]);
                    }
                }
#endif
            }
        }

        /* subband synthesis */
        sbr_qmf_synthesis_64(sbr, sbr->qmfs[ch], (const complex_t*)X, ch_buf);

        for (i = 0; i < 32; i++)
        {
            int8_t j;
            for (j = 0; j < sbr->tHFGen; j++)
            {
                QMF_RE(sbr->Xcodec[ch][j*32 + i]) = QMF_RE(sbr->Xcodec[ch][(j+sbr->numTimeSlotsRate)*32 + i]);
#ifndef SBR_LOW_POWER
                QMF_IM(sbr->Xcodec[ch][j*32 + i]) = QMF_IM(sbr->Xcodec[ch][(j+sbr->numTimeSlotsRate)*32 + i]);
#endif
            }
        }
        for (i = 0; i < 64; i++)
        {
            int8_t j;
            for (j = 0; j < sbr->tHFGen; j++)
            {
                QMF_RE(sbr->Xsbr[ch][j*64 + i]) = QMF_RE(sbr->Xsbr[ch][(j+sbr->numTimeSlotsRate)*64 + i]);
#ifndef SBR_LOW_POWER
                QMF_IM(sbr->Xsbr[ch][j*64 + i]) = QMF_IM(sbr->Xsbr[ch][(j+sbr->numTimeSlotsRate)*64 + i]);
#endif
            }
        }
    }

    if (sbr->bs_header_flag)
        sbr->just_seeked = 0;

    if (sbr->header_count != 0)
    {
        for (ch = 0; ch < channels; ch++)
            sbr_save_prev_data(sbr, ch);
    }

    sbr->frame++;
}

#endif
