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
** $Id: sbr_dec.c,v 1.5 2003/07/29 08:20:13 menno Exp $
**/

/*
   SBR Decoder overview:

   To achieve a synchronized output signal, the following steps have to be
   acknowledged in the decoder:
    - The bitstream parser divides the bitstream into two parts; the AAC
      core coder part and the SBR part.
    - The SBR bitstream part is fed to the bitstream de-multiplexer followed
      by de-quantization The raw data is Huffman decoded.
    - The AAC bitstream part is fed to the AAC core decoder, where the
      bitstream data of the current frame is decoded, yielding a time domain
      audio signal block of 1024 samples. The block length could easily be
      adapted to other sizes e.g. 960.
    - The core coder audio block is fed to the analysis QMF bank using a
      delay of 1312 samples.
    - The analysis QMF bank performs the filtering of the delayed core coder
      audio signal. The output from the filtering is stored in the matrix
      Xlow. The output from the analysis QMF bank is delayed tHFGen subband
      samples, before being fed to the synthesis QMF bank. To achieve
      synchronization tHFGen = 32, i.e. the value must equal the number of
      subband samples corresponding to one frame.
    - The HF generator calculates XHigh given the matrix XLow. The process
      is guided by the SBR data contained in the current frame.
    - The envelope adjuster calculates the matrix Y given the matrix XHigh
      and the SBR envelope data, extracted from the SBR bitstream. To
      achieve synchronization, tHFAdj has to be set to tHFAdj = 0, i.e. the
      envelope adjuster operates on data delayed tHFGen subband samples.
    - The synthesis QMF bank operates on the delayed output from the analysis
      QMF bank and the output from the envelope adjuster.
 */

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


sbr_info *sbrDecodeInit()
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
                    uint8_t just_seeked)
{
    int16_t i, k, l;

    uint8_t dont_process = 0;
    uint8_t ch, channels, ret;
    real_t *ch_buf;

    qmf_t X[32*64];
#ifdef SBR_LOW_POWER
    real_t deg[64];
#endif

    bitfile *ld = (bitfile*)malloc(sizeof(bitfile));


    sbr->id_aac = id_aac;
    channels = (id_aac == ID_SCE) ? 1 : 2;

    /* initialise and read the bitstream */
    faad_initbits(ld, sbr->data, sbr->data_size);

    ret = sbr_extension_data(ld, sbr, id_aac);

    if (sbr->data) free(sbr->data);
    sbr->data = NULL;

    ret = ld->error ? ld->error : ret;
    faad_endbits(ld);
    if (ld) free(ld);
    ld = NULL;
    if (ret || (sbr->header_count == 0))
    {
        /* don't process just upsample */
        dont_process = 1;
    }

    if (just_seeked)
        sbr->just_seeked = 1;
    else
        sbr->just_seeked = 0;

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

            sbr->Xsbr[ch] = malloc((32+tHFGen)*64 * sizeof(qmf_t));
            sbr->Xcodec[ch] = malloc((32+tHFGen)*32 * sizeof(qmf_t));

            memset(sbr->Xsbr[ch], 0, (32+tHFGen)*64 * sizeof(qmf_t));
            memset(sbr->Xcodec[ch], 0, (32+tHFGen)*32 * sizeof(qmf_t));
        }

        if (ch == 0)
            ch_buf = left_channel;
        else
            ch_buf = right_channel;

        for (i = 0; i < tHFAdj; i++)
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

        /* subband analysis */
        sbr_qmf_analysis_32(sbr->qmfa[ch], ch_buf, sbr->Xcodec[ch], tHFGen);

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
                    QMF_RE(sbr->Xsbr[ch][(tHFAdj + l)*64 + k]) = 0;
                }
            }
#endif

            /* hf adjustment */
            hf_adjustment(sbr, sbr->Xsbr[ch]
#ifdef SBR_LOW_POWER
                ,deg
#endif
                ,ch);
        }

        if ((sbr->just_seeked != 0) || dont_process)
        {
            for (l = 0; l < 32; l++)
            {
                for (k = 0; k < 32; k++)
                {
                    QMF_RE(X[l * 64 + k]) = QMF_RE(sbr->Xcodec[ch][(l + tHFAdj)*32 + k]);
#ifndef SBR_LOW_POWER
                    QMF_IM(X[l * 64 + k]) = QMF_IM(sbr->Xcodec[ch][(l + tHFAdj)*32 + k]);
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
            for (l = 0; l < 32; l++)
            {
                uint8_t xover_band;

                if (l < sbr->t_E[ch][0])
                    xover_band = sbr->kx_prev;
                else
                    xover_band = sbr->kx;

                for (k = 0; k < xover_band; k++)
                {
                    QMF_RE(X[l * 64 + k]) = QMF_RE(sbr->Xcodec[ch][(l + tHFAdj)*32 + k]);
#ifndef SBR_LOW_POWER
                    QMF_IM(X[l * 64 + k]) = QMF_IM(sbr->Xcodec[ch][(l + tHFAdj)*32 + k]);
#endif
                }
                for (k = xover_band; k < 64; k++)
                {
                    QMF_RE(X[l * 64 + k]) = QMF_RE(sbr->Xsbr[ch][(l + tHFAdj)*64 + k]);
#ifndef SBR_LOW_POWER
                    QMF_IM(X[l * 64 + k]) = QMF_IM(sbr->Xsbr[ch][(l + tHFAdj)*64 + k]);
#endif
                }
#ifdef SBR_LOW_POWER
                QMF_RE(X[l * 64 + xover_band - 1]) += QMF_RE(sbr->Xsbr[ch][(l + tHFAdj)*64 + xover_band - 1]);
#endif
            }
        }

        /* subband synthesis */
        sbr_qmf_synthesis_64(sbr->qmfs[ch], (const complex_t*)X, ch_buf);

        for (i = 0; i < 32; i++)
        {
            int8_t j;
            for (j = 0; j < tHFGen; j++)
            {
                QMF_RE(sbr->Xcodec[ch][j*32 + i]) = QMF_RE(sbr->Xcodec[ch][(j+32)*32 + i]);
#ifndef SBR_LOW_POWER
                QMF_IM(sbr->Xcodec[ch][j*32 + i]) = QMF_IM(sbr->Xcodec[ch][(j+32)*32 + i]);
#endif
            }
        }
        for (i = 0; i < 64; i++)
        {
            int8_t j;
            for (j = 0; j < tHFGen; j++)
            {
                QMF_RE(sbr->Xsbr[ch][j*64 + i]) = QMF_RE(sbr->Xsbr[ch][(j+32)*64 + i]);
#ifndef SBR_LOW_POWER
                QMF_IM(sbr->Xsbr[ch][j*64 + i]) = QMF_IM(sbr->Xsbr[ch][(j+32)*64 + i]);
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
