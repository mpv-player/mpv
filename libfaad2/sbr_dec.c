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
** Initially modified for use with MPlayer on 2005/12/05
** $Id: sbr_dec.c,v 1.39 2004/09/04 14:56:28 menno Exp $
** detailed changelog at http://svn.mplayerhq.hu/mplayer/trunk/
** local_changes.diff contains the exact changes to this file.
**/


#include "common.h"
#include "structs.h"

#ifdef SBR_DEC

#include <string.h>
#include <stdlib.h>

#include "syntax.h"
#include "bits.h"
#include "sbr_syntax.h"
#include "sbr_qmf.h"
#include "sbr_hfgen.h"
#include "sbr_hfadj.h"


/* static function declarations */
static uint8_t sbr_save_prev_data(sbr_info *sbr, uint8_t ch);
static void sbr_save_matrix(sbr_info *sbr, uint8_t ch);


sbr_info *sbrDecodeInit(uint16_t framelength, uint8_t id_aac,
                        uint32_t sample_rate, uint8_t downSampledSBR
#ifdef DRM
						, uint8_t IsDRM
#endif
                        )
{
    sbr_info *sbr = faad_malloc(sizeof(sbr_info));
    memset(sbr, 0, sizeof(sbr_info));

    /* save id of the parent element */
    sbr->id_aac = id_aac;
    sbr->sample_rate = sample_rate;

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
    sbr->Reset = 1;

#ifdef DRM
    sbr->Is_DRM_SBR = IsDRM;
#endif
    sbr->tHFGen = T_HFGEN;
    sbr->tHFAdj = T_HFADJ;

    sbr->bsco = 0;
    sbr->bsco_prev = 0;
    sbr->M_prev = 0;
    sbr->frame_len = framelength;

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

    sbr->GQ_ringbuf_index[0] = 0;
    sbr->GQ_ringbuf_index[1] = 0;

    if (id_aac == ID_CPE)
    {
        /* stereo */
        uint8_t j;
        sbr->qmfa[0] = qmfa_init(32);
        sbr->qmfa[1] = qmfa_init(32);
        sbr->qmfs[0] = qmfs_init((downSampledSBR)?32:64);
        sbr->qmfs[1] = qmfs_init((downSampledSBR)?32:64);

        for (j = 0; j < 5; j++)
        {
            sbr->G_temp_prev[0][j] = faad_malloc(64*sizeof(real_t));
            sbr->G_temp_prev[1][j] = faad_malloc(64*sizeof(real_t));
            sbr->Q_temp_prev[0][j] = faad_malloc(64*sizeof(real_t));
            sbr->Q_temp_prev[1][j] = faad_malloc(64*sizeof(real_t));
        }

        memset(sbr->Xsbr[0], 0, (sbr->numTimeSlotsRate+sbr->tHFGen)*64 * sizeof(qmf_t));
        memset(sbr->Xsbr[1], 0, (sbr->numTimeSlotsRate+sbr->tHFGen)*64 * sizeof(qmf_t));
    } else {
        /* mono */
        uint8_t j;
        sbr->qmfa[0] = qmfa_init(32);
        sbr->qmfs[0] = qmfs_init((downSampledSBR)?32:64);
        sbr->qmfs[1] = NULL;

        for (j = 0; j < 5; j++)
        {
            sbr->G_temp_prev[0][j] = faad_malloc(64*sizeof(real_t));
            sbr->Q_temp_prev[0][j] = faad_malloc(64*sizeof(real_t));
        }

        memset(sbr->Xsbr[0], 0, (sbr->numTimeSlotsRate+sbr->tHFGen)*64 * sizeof(qmf_t));
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
        if (sbr->qmfs[1] != NULL)
        {
            qmfa_end(sbr->qmfa[1]);
            qmfs_end(sbr->qmfs[1]);
        }

        for (j = 0; j < 5; j++)
        {
            if (sbr->G_temp_prev[0][j]) faad_free(sbr->G_temp_prev[0][j]);
            if (sbr->Q_temp_prev[0][j]) faad_free(sbr->Q_temp_prev[0][j]);
            if (sbr->G_temp_prev[1][j]) faad_free(sbr->G_temp_prev[1][j]);
            if (sbr->Q_temp_prev[1][j]) faad_free(sbr->Q_temp_prev[1][j]);
        }

#ifdef PS_DEC
        if (sbr->ps != NULL)
            ps_free(sbr->ps);
#endif

#ifdef DRM_PS
        if (sbr->drm_ps != NULL)
            drm_ps_free(sbr->drm_ps);
#endif

        faad_free(sbr);
    }
}

static uint8_t sbr_save_prev_data(sbr_info *sbr, uint8_t ch)
{
    uint8_t i;

    /* save data for next frame */
    sbr->kx_prev = sbr->kx;
    sbr->M_prev = sbr->M;
    sbr->bsco_prev = sbr->bsco;

    sbr->L_E_prev[ch] = sbr->L_E[ch];

    /* sbr->L_E[ch] can become 0 on files with bit errors */
    if (sbr->L_E[ch] <= 0)
        return 19;

    sbr->f_prev[ch] = sbr->f[ch][sbr->L_E[ch] - 1];
    for (i = 0; i < MAX_M; i++)
    {
        sbr->E_prev[ch][i] = sbr->E[ch][i][sbr->L_E[ch] - 1];
        sbr->Q_prev[ch][i] = sbr->Q[ch][i][sbr->L_Q[ch] - 1];
    }

    for (i = 0; i < MAX_M; i++)
    {
        sbr->bs_add_harmonic_prev[ch][i] = sbr->bs_add_harmonic[ch][i];
    }
    sbr->bs_add_harmonic_flag_prev[ch] = sbr->bs_add_harmonic_flag[ch];

    if (sbr->l_A[ch] == sbr->L_E[ch])
        sbr->prevEnvIsShort[ch] = 0;
    else
        sbr->prevEnvIsShort[ch] = -1;

    return 0;
}

static void sbr_save_matrix(sbr_info *sbr, uint8_t ch)
{
    uint8_t i;

    for (i = 0; i < sbr->tHFGen; i++)
    {
        memmove(sbr->Xsbr[ch][i], sbr->Xsbr[ch][i+sbr->numTimeSlotsRate], 64 * sizeof(qmf_t));
    }
    for (i = sbr->tHFGen; i < MAX_NTSRHFG; i++)
    {
        memset(sbr->Xsbr[ch][i], 0, 64 * sizeof(qmf_t));
    }
}

static void sbr_process_channel(sbr_info *sbr, real_t *channel_buf, qmf_t X[MAX_NTSR][64],
                                uint8_t ch, uint8_t dont_process,
                                const uint8_t downSampledSBR)
{
    int16_t k, l;

#ifdef SBR_LOW_POWER
    ALIGN real_t deg[64];
#endif

#ifdef DRM
    if (sbr->Is_DRM_SBR)
    {
        sbr->bsco = max((int32_t)sbr->maxAACLine*32/(int32_t)sbr->frame_len - (int32_t)sbr->kx, 0);
    } else {
#endif
        sbr->bsco = 0;
#ifdef DRM
    }
#endif


//#define PRE_QMF_PRINT
#ifdef PRE_QMF_PRINT
    {
        int i;
        for (i = 0; i < 1024; i++)
        {
            printf("%d\n", channel_buf[i]);
        }
    }
#endif


    /* subband analysis */
    if (dont_process)
        sbr_qmf_analysis_32(sbr, sbr->qmfa[ch], channel_buf, sbr->Xsbr[ch], sbr->tHFGen, 32);
    else
        sbr_qmf_analysis_32(sbr, sbr->qmfa[ch], channel_buf, sbr->Xsbr[ch], sbr->tHFGen, sbr->kx);

    if (!dont_process)
    {
#if 1
        /* insert high frequencies here */
        /* hf generation using patching */
        hf_generation(sbr, sbr->Xsbr[ch], sbr->Xsbr[ch]
#ifdef SBR_LOW_POWER
            ,deg
#endif
            ,ch);
#endif

#ifdef SBR_LOW_POWER
        for (l = sbr->t_E[ch][0]; l < sbr->t_E[ch][sbr->L_E[ch]]; l++)
        {
            for (k = 0; k < sbr->kx; k++)
            {
                QMF_RE(sbr->Xsbr[ch][sbr->tHFAdj + l][k]) = 0;
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
                QMF_RE(X[l][k]) = QMF_RE(sbr->Xsbr[ch][l + sbr->tHFAdj][k]);
#ifndef SBR_LOW_POWER
                QMF_IM(X[l][k]) = QMF_IM(sbr->Xsbr[ch][l + sbr->tHFAdj][k]);
#endif
            }
            for (k = 32; k < 64; k++)
            {
                QMF_RE(X[l][k]) = 0;
#ifndef SBR_LOW_POWER
                QMF_IM(X[l][k]) = 0;
#endif
            }
        }
    } else {
        for (l = 0; l < sbr->numTimeSlotsRate; l++)
        {
            uint8_t kx_band, M_band, bsco_band;

            if (l < sbr->t_E[ch][0])
            {
                kx_band = sbr->kx_prev;
                M_band = sbr->M_prev;
                bsco_band = sbr->bsco_prev;
            } else {
                kx_band = sbr->kx;
                M_band = sbr->M;
                bsco_band = sbr->bsco;
            }

#ifndef SBR_LOW_POWER
            for (k = 0; k < kx_band + bsco_band; k++)
            {
                QMF_RE(X[l][k]) = QMF_RE(sbr->Xsbr[ch][l + sbr->tHFAdj][k]);
                QMF_IM(X[l][k]) = QMF_IM(sbr->Xsbr[ch][l + sbr->tHFAdj][k]);
            }
            for (k = kx_band + bsco_band; k < kx_band + M_band; k++)
            {
                QMF_RE(X[l][k]) = QMF_RE(sbr->Xsbr[ch][l + sbr->tHFAdj][k]);
                QMF_IM(X[l][k]) = QMF_IM(sbr->Xsbr[ch][l + sbr->tHFAdj][k]);
            }
            for (k = max(kx_band + bsco_band, kx_band + M_band); k < 64; k++)
            {
                QMF_RE(X[l][k]) = 0;
                QMF_IM(X[l][k]) = 0;
            }
#else
            for (k = 0; k < kx_band + bsco_band; k++)
            {
                QMF_RE(X[l][k]) = QMF_RE(sbr->Xsbr[ch][l + sbr->tHFAdj][k]);
            }
            for (k = kx_band + bsco_band; k < min(kx_band + M_band, 63); k++)
            {
                QMF_RE(X[l][k]) = QMF_RE(sbr->Xsbr[ch][l + sbr->tHFAdj][k]);
            }
            for (k = max(kx_band + bsco_band, kx_band + M_band); k < 64; k++)
            {
                QMF_RE(X[l][k]) = 0;
            }
            QMF_RE(X[l][kx_band - 1 + bsco_band]) +=
                QMF_RE(sbr->Xsbr[ch][l + sbr->tHFAdj][kx_band - 1 + bsco_band]);
#endif
        }
    }
}

uint8_t sbrDecodeCoupleFrame(sbr_info *sbr, real_t *left_chan, real_t *right_chan,
                             const uint8_t just_seeked, const uint8_t downSampledSBR)
{
    uint8_t dont_process = 0;
    uint8_t ret = 0;
    ALIGN qmf_t X[MAX_NTSR][64];

    if (sbr == NULL)
        return 20;

    /* case can occur due to bit errors */
    if (sbr->id_aac != ID_CPE)
        return 21;

    if (sbr->ret || (sbr->header_count == 0))
    {
        /* don't process just upsample */
        dont_process = 1;

        /* Re-activate reset for next frame */
        if (sbr->ret && sbr->Reset)
            sbr->bs_start_freq_prev = -1;
    }

    if (just_seeked)
    {
        sbr->just_seeked = 1;
    } else {
        sbr->just_seeked = 0;
    }

    sbr_process_channel(sbr, left_chan, X, 0, dont_process, downSampledSBR);
    /* subband synthesis */
    if (downSampledSBR)
    {
        sbr_qmf_synthesis_32(sbr, sbr->qmfs[0], X, left_chan);
    } else {
        sbr_qmf_synthesis_64(sbr, sbr->qmfs[0], X, left_chan);
    }

    sbr_process_channel(sbr, right_chan, X, 1, dont_process, downSampledSBR);
    /* subband synthesis */
    if (downSampledSBR)
    {
        sbr_qmf_synthesis_32(sbr, sbr->qmfs[1], X, right_chan);
    } else {
        sbr_qmf_synthesis_64(sbr, sbr->qmfs[1], X, right_chan);
    }

    if (sbr->bs_header_flag)
        sbr->just_seeked = 0;

    if (sbr->header_count != 0 && sbr->ret == 0)
    {
        ret = sbr_save_prev_data(sbr, 0);
        if (ret) return ret;
        ret = sbr_save_prev_data(sbr, 1);
        if (ret) return ret;
    }

    sbr_save_matrix(sbr, 0);
    sbr_save_matrix(sbr, 1);

    sbr->frame++;

//#define POST_QMF_PRINT
#ifdef POST_QMF_PRINT
    {
        int i;
        for (i = 0; i < 2048; i++)
        {
            printf("%d\n", left_chan[i]);
        }
        for (i = 0; i < 2048; i++)
        {
            printf("%d\n", right_chan[i]);
        }
    }
#endif

    return 0;
}

uint8_t sbrDecodeSingleFrame(sbr_info *sbr, real_t *channel,
                             const uint8_t just_seeked, const uint8_t downSampledSBR)
{
    uint8_t dont_process = 0;
    uint8_t ret = 0;
    ALIGN qmf_t X[MAX_NTSR][64];

    if (sbr == NULL)
        return 20;

    /* case can occur due to bit errors */
    if (sbr->id_aac != ID_SCE && sbr->id_aac != ID_LFE)
        return 21;

    if (sbr->ret || (sbr->header_count == 0))
    {
        /* don't process just upsample */
        dont_process = 1;

        /* Re-activate reset for next frame */
        if (sbr->ret && sbr->Reset)
            sbr->bs_start_freq_prev = -1;
    }

    if (just_seeked)
    {
        sbr->just_seeked = 1;
    } else {
        sbr->just_seeked = 0;
    }

    sbr_process_channel(sbr, channel, X, 0, dont_process, downSampledSBR);
    /* subband synthesis */
    if (downSampledSBR)
    {
        sbr_qmf_synthesis_32(sbr, sbr->qmfs[0], X, channel);
    } else {
        sbr_qmf_synthesis_64(sbr, sbr->qmfs[0], X, channel);
    }

    if (sbr->bs_header_flag)
        sbr->just_seeked = 0;

    if (sbr->header_count != 0 && sbr->ret == 0)
    {
        ret = sbr_save_prev_data(sbr, 0);
        if (ret) return ret;
    }

    sbr_save_matrix(sbr, 0);

    sbr->frame++;

//#define POST_QMF_PRINT
#ifdef POST_QMF_PRINT
    {
        int i;
        for (i = 0; i < 2048; i++)
        {
            printf("%d\n", channel[i]);
        }
    }
#endif

    return 0;
}

#if (defined(PS_DEC) || defined(DRM_PS))
uint8_t sbrDecodeSingleFramePS(sbr_info *sbr, real_t *left_channel, real_t *right_channel,
                               const uint8_t just_seeked, const uint8_t downSampledSBR)
{
    uint8_t l, k;
    uint8_t dont_process = 0;
    uint8_t ret = 0;
    ALIGN qmf_t X_left[38][64] = {{{0}}};
    ALIGN qmf_t X_right[38][64] = {{{0}}}; /* must set this to 0 */

    if (sbr == NULL)
        return 20;

    /* case can occur due to bit errors */
    if (sbr->id_aac != ID_SCE && sbr->id_aac != ID_LFE)
        return 21;

    if (sbr->ret || (sbr->header_count == 0))
    {
        /* don't process just upsample */
        dont_process = 1;

        /* Re-activate reset for next frame */
        if (sbr->ret && sbr->Reset)
            sbr->bs_start_freq_prev = -1;
    }

    if (just_seeked)
    {
        sbr->just_seeked = 1;
    } else {
        sbr->just_seeked = 0;
    }

    if (sbr->qmfs[1] == NULL)
    {
        sbr->qmfs[1] = qmfs_init((downSampledSBR)?32:64);
    }

    sbr_process_channel(sbr, left_channel, X_left, 0, dont_process, downSampledSBR);

    /* copy some extra data for PS */
    for (l = 32; l < 38; l++)
    {
        for (k = 0; k < 5; k++)
        {
            QMF_RE(X_left[l][k]) = QMF_RE(sbr->Xsbr[0][sbr->tHFAdj+l][k]);
            QMF_IM(X_left[l][k]) = QMF_IM(sbr->Xsbr[0][sbr->tHFAdj+l][k]);
        }
    }

    /* perform parametric stereo */
#ifdef DRM_PS
    if (sbr->Is_DRM_SBR)
    {
        drm_ps_decode(sbr->drm_ps, (sbr->ret > 0), sbr->sample_rate, X_left, X_right);
    } else {
#endif
#ifdef PS_DEC
        ps_decode(sbr->ps, X_left, X_right);
#endif
#ifdef DRM_PS
    }
#endif

    /* subband synthesis */
    if (downSampledSBR)
    {
        sbr_qmf_synthesis_32(sbr, sbr->qmfs[0], X_left, left_channel);
        sbr_qmf_synthesis_32(sbr, sbr->qmfs[1], X_right, right_channel);
    } else {
        sbr_qmf_synthesis_64(sbr, sbr->qmfs[0], X_left, left_channel);
        sbr_qmf_synthesis_64(sbr, sbr->qmfs[1], X_right, right_channel);
    }

    if (sbr->bs_header_flag)
        sbr->just_seeked = 0;

    if (sbr->header_count != 0 && sbr->ret == 0)
    {
        ret = sbr_save_prev_data(sbr, 0);
        if (ret) return ret;
    }

    sbr_save_matrix(sbr, 0);

    sbr->frame++;

    return 0;
}
#endif

#endif
