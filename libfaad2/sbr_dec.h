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
** $Id: sbr_dec.h,v 1.35 2004/09/04 14:56:28 menno Exp $
**/

#ifndef __SBR_DEC_H__
#define __SBR_DEC_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifdef PS_DEC
#include "ps_dec.h"
#endif
#ifdef DRM_PS
#include "drm_dec.h"
#endif

/* MAX_NTSRHFG: maximum of number_time_slots * rate + HFGen. 16*2+8 */
#define MAX_NTSRHFG 40
#define MAX_NTSR    32 /* max number_time_slots * rate, ok for DRM and not DRM mode */

/* MAX_M: maximum value for M */
#define MAX_M       49
/* MAX_L_E: maximum value for L_E */
#define MAX_L_E      5

typedef struct {
    real_t *x;
    int16_t x_index;
    uint8_t channels;
} qmfa_info;

typedef struct {
    real_t *v;
    int16_t v_index;
    uint8_t channels;
} qmfs_info;

typedef struct
{
    uint32_t sample_rate;
    uint32_t maxAACLine;

    uint8_t rate;
    uint8_t just_seeked;
    uint8_t ret;

    uint8_t amp_res[2];

    uint8_t k0;
    uint8_t kx;
    uint8_t M;
    uint8_t N_master;
    uint8_t N_high;
    uint8_t N_low;
    uint8_t N_Q;
    uint8_t N_L[4];
    uint8_t n[2];

    uint8_t f_master[64];
    uint8_t f_table_res[2][64];
    uint8_t f_table_noise[64];
    uint8_t f_table_lim[4][64];
#ifdef SBR_LOW_POWER
    uint8_t f_group[5][64];
    uint8_t N_G[5];
#endif

    uint8_t table_map_k_to_g[64];

    uint8_t abs_bord_lead[2];
    uint8_t abs_bord_trail[2];
    uint8_t n_rel_lead[2];
    uint8_t n_rel_trail[2];

    uint8_t L_E[2];
    uint8_t L_E_prev[2];
    uint8_t L_Q[2];

    uint8_t t_E[2][MAX_L_E+1];
    uint8_t t_Q[2][3];
    uint8_t f[2][MAX_L_E+1];
    uint8_t f_prev[2];

    real_t *G_temp_prev[2][5];
    real_t *Q_temp_prev[2][5];
    int8_t GQ_ringbuf_index[2];

    int16_t E[2][64][MAX_L_E];
    int16_t E_prev[2][64];
#ifndef FIXED_POINT
    real_t E_orig[2][64][MAX_L_E];
#endif
    real_t E_curr[2][64][MAX_L_E];
    int32_t Q[2][64][2];
#ifndef FIXED_POINT
    real_t Q_div[2][64][2];
    real_t Q_div2[2][64][2];
#endif
    int32_t Q_prev[2][64];

    int8_t l_A[2];
    int8_t l_A_prev[2];

    uint8_t bs_invf_mode[2][MAX_L_E];
    uint8_t bs_invf_mode_prev[2][MAX_L_E];
    real_t bwArray[2][64];
    real_t bwArray_prev[2][64];

    uint8_t noPatches;
    uint8_t patchNoSubbands[64];
    uint8_t patchStartSubband[64];

    uint8_t bs_add_harmonic[2][64];
    uint8_t bs_add_harmonic_prev[2][64];

    uint16_t index_noise_prev[2];
    uint8_t psi_is_prev[2];

    uint8_t bs_start_freq_prev;
    uint8_t bs_stop_freq_prev;
    uint8_t bs_xover_band_prev;
    uint8_t bs_freq_scale_prev;
    uint8_t bs_alter_scale_prev;
    uint8_t bs_noise_bands_prev;

    int8_t prevEnvIsShort[2];

    int8_t kx_prev;
    uint8_t bsco;
    uint8_t bsco_prev;
    uint8_t M_prev;
    uint16_t frame_len;

    uint8_t Reset;
    uint32_t frame;
    uint32_t header_count;

    uint8_t id_aac;
    qmfa_info *qmfa[2];
    qmfs_info *qmfs[2];

    qmf_t Xsbr[2][MAX_NTSRHFG][64];

#ifdef DRM
    uint8_t Is_DRM_SBR;
#ifdef DRM_PS
    drm_ps_info *drm_ps;
#endif
#endif

    uint8_t numTimeSlotsRate;
    uint8_t numTimeSlots;
    uint8_t tHFGen;
    uint8_t tHFAdj;

#ifdef PS_DEC
    ps_info *ps;
#endif
#if (defined(PS_DEC) || defined(DRM_PS))
    uint8_t ps_used;
#endif

    /* to get it compiling */
    /* we'll see during the coding of all the tools, whether
       these are all used or not.
    */
    uint8_t bs_header_flag;
    uint8_t bs_crc_flag;
    uint16_t bs_sbr_crc_bits;
    uint8_t bs_protocol_version;
    uint8_t bs_amp_res;
    uint8_t bs_start_freq;
    uint8_t bs_stop_freq;
    uint8_t bs_xover_band;
    uint8_t bs_freq_scale;
    uint8_t bs_alter_scale;
    uint8_t bs_noise_bands;
    uint8_t bs_limiter_bands;
    uint8_t bs_limiter_gains;
    uint8_t bs_interpol_freq;
    uint8_t bs_smoothing_mode;
    uint8_t bs_samplerate_mode;
    uint8_t bs_add_harmonic_flag[2];
    uint8_t bs_add_harmonic_flag_prev[2];
    uint8_t bs_extended_data;
    uint8_t bs_extension_id;
    uint8_t bs_extension_data;
    uint8_t bs_coupling;
    uint8_t bs_frame_class[2];
    uint8_t bs_rel_bord[2][9];
    uint8_t bs_rel_bord_0[2][9];
    uint8_t bs_rel_bord_1[2][9];
    uint8_t bs_pointer[2];
    uint8_t bs_abs_bord_0[2];
    uint8_t bs_abs_bord_1[2];
    uint8_t bs_num_rel_0[2];
    uint8_t bs_num_rel_1[2];
    uint8_t bs_df_env[2][9];
    uint8_t bs_df_noise[2][3];
} sbr_info;

sbr_info *sbrDecodeInit(uint16_t framelength, uint8_t id_aac,
                        uint32_t sample_rate, uint8_t downSampledSBR
#ifdef DRM
                        , uint8_t IsDRM
#endif
                        );
void sbrDecodeEnd(sbr_info *sbr);

uint8_t sbrDecodeCoupleFrame(sbr_info *sbr, real_t *left_chan, real_t *right_chan,
                             const uint8_t just_seeked, const uint8_t downSampledSBR);
uint8_t sbrDecodeSingleFrame(sbr_info *sbr, real_t *channel,
                             const uint8_t just_seeked, const uint8_t downSampledSBR);
#if (defined(PS_DEC) || defined(DRM_PS))
uint8_t sbrDecodeSingleFramePS(sbr_info *sbr, real_t *left_channel, real_t *right_channel,
                               const uint8_t just_seeked, const uint8_t downSampledSBR);
#endif


#ifdef __cplusplus
}
#endif
#endif

