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
** $Id: sbr_syntax.c,v 1.7 2003/07/29 08:20:13 menno Exp $
**/

#include "common.h"
#include "structs.h"

#ifdef SBR_DEC

#include "sbr_syntax.h"
#include "syntax.h"
#include "sbr_huff.h"
#include "sbr_fbt.h"
#include "sbr_tf_grid.h"
#include "sbr_e_nf.h"
#include "bits.h"
#include "analysis.h"

static void sbr_reset(sbr_info *sbr)
{
    /* if these are different from the previous frame: Reset = 1 */
    if ((sbr->bs_start_freq != sbr->bs_start_freq_prev) ||
        (sbr->bs_stop_freq != sbr->bs_stop_freq_prev) ||
        (sbr->bs_freq_scale != sbr->bs_freq_scale_prev) ||
        (sbr->bs_alter_scale != sbr->bs_alter_scale_prev))
    {
        sbr->Reset = 1;
    } else {
        sbr->Reset = 0;
    }

    if ((sbr->bs_start_freq != sbr->bs_start_freq_prev) ||
        (sbr->bs_stop_freq != sbr->bs_stop_freq_prev) ||
        (sbr->bs_freq_scale != sbr->bs_freq_scale_prev) ||
        (sbr->bs_alter_scale != sbr->bs_alter_scale_prev) ||
        (sbr->bs_xover_band != sbr->bs_xover_band_prev) ||
        (sbr->bs_noise_bands != sbr->bs_noise_bands_prev))
    {
        sbr->Reset = 1;
    } else {
        sbr->Reset = 0;
    }

    sbr->bs_start_freq_prev = sbr->bs_start_freq;
    sbr->bs_stop_freq_prev = sbr->bs_stop_freq;
    sbr->bs_freq_scale_prev = sbr->bs_freq_scale;
    sbr->bs_alter_scale_prev = sbr->bs_alter_scale;
    sbr->bs_xover_band_prev = sbr->bs_xover_band;
    sbr->bs_noise_bands_prev = sbr->bs_noise_bands;

    if (sbr->frame == 0)
    {
        sbr->Reset = 1;
    }
}

/* table 2 */
uint8_t sbr_extension_data(bitfile *ld, sbr_info *sbr, uint8_t id_aac)
{
    uint8_t bs_extension_type = (uint8_t)faad_getbits(ld, 4
        DEBUGVAR(1,198,"sbr_bitstream(): bs_extension_type"));

    if (bs_extension_type == EXT_SBR_DATA_CRC)
    {
        sbr->bs_sbr_crc_bits = (uint16_t)faad_getbits(ld, 10
            DEBUGVAR(1,199,"sbr_bitstream(): bs_sbr_crc_bits"));
    }

    sbr->bs_header_flag = faad_get1bit(ld
        DEBUGVAR(1,200,"sbr_bitstream(): bs_header_flag"));
    if (sbr->bs_header_flag)
        sbr_header(ld, sbr, id_aac);

    /* TODO: Reset? */
    sbr_reset(sbr);

    /* first frame should have a header */
    if (sbr->frame == 0 && sbr->bs_header_flag == 0)
        return 1;


    if (sbr->Reset || (sbr->bs_header_flag && sbr->just_seeked))
    {
        uint16_t k2;

        /* calculate the Master Frequency Table */
        sbr->k0 = qmf_start_channel(sbr->bs_start_freq, sbr->bs_samplerate_mode,
            sbr->sample_rate);
        k2 = qmf_stop_channel(sbr->bs_stop_freq, sbr->sample_rate, sbr->k0);

        /* check k0 and k2 */
        if (sbr->sample_rate >= 48000)
        {
            if ((k2 - sbr->k0) > 32)
                return 1;
        } else if (sbr->sample_rate <= 32000) {
            if ((k2 - sbr->k0) > 48)
                return 1;
        } else { /* (sbr->sample_rate == 44100) */
            if ((k2 - sbr->k0) > 45)
                return 1;
        }

        if (sbr->bs_freq_scale == 0)
        {
            master_frequency_table_fs0(sbr, sbr->k0, k2, sbr->bs_alter_scale);
        } else {
            master_frequency_table(sbr, sbr->k0, k2, sbr->bs_freq_scale,
                sbr->bs_alter_scale);
        }
        derived_frequency_table(sbr, sbr->bs_xover_band, k2);
    }

    sbr_data(ld, sbr, id_aac);

    /* no error */
    return 0;
}

/* table 3 */
static void sbr_header(bitfile *ld, sbr_info *sbr, uint8_t id_aac)
{
    uint8_t bs_header_extra_1, bs_header_extra_2;

    sbr->header_count++;

    sbr->bs_amp_res = faad_get1bit(ld
        DEBUGVAR(1,203,"sbr_header(): bs_amp_res"));

    /* bs_start_freq and bs_stop_freq must define a fequency band that does
       not exceed 48 channels */
    sbr->bs_start_freq = faad_getbits(ld, 4
        DEBUGVAR(1,204,"sbr_header(): bs_start_freq"));
    sbr->bs_stop_freq = faad_getbits(ld, 4
        DEBUGVAR(1,205,"sbr_header(): bs_stop_freq"));
    sbr->bs_xover_band = faad_getbits(ld, 3
        DEBUGVAR(1,206,"sbr_header(): bs_xover_band"));
    faad_getbits(ld, 2
        DEBUGVAR(1,207,"sbr_header(): bs_reserved_bits_hdr"));
    bs_header_extra_1 = faad_get1bit(ld
        DEBUGVAR(1,208,"sbr_header(): bs_header_extra_1"));
    bs_header_extra_2 = faad_get1bit(ld
        DEBUGVAR(1,209,"sbr_header(): bs_header_extra_2"));

    if (bs_header_extra_1)
    {
        sbr->bs_freq_scale = faad_getbits(ld, 2
            DEBUGVAR(1,211,"sbr_header(): bs_freq_scale"));
        sbr->bs_alter_scale = faad_get1bit(ld
            DEBUGVAR(1,212,"sbr_header(): bs_alter_scale"));
        sbr->bs_noise_bands = faad_getbits(ld, 2
            DEBUGVAR(1,213,"sbr_header(): bs_noise_bands"));
    }
    if (bs_header_extra_2)
    {
        sbr->bs_limiter_bands = faad_getbits(ld, 2
            DEBUGVAR(1,214,"sbr_header(): bs_limiter_bands"));
        sbr->bs_limiter_gains = faad_getbits(ld, 2
            DEBUGVAR(1,215,"sbr_header(): bs_limiter_gains"));
        sbr->bs_interpol_freq = faad_get1bit(ld
            DEBUGVAR(1,216,"sbr_header(): bs_interpol_freq"));
        sbr->bs_smoothing_mode = faad_get1bit(ld
            DEBUGVAR(1,217,"sbr_header(): bs_smoothing_mode"));
    }

#if 0
    /* print the header to screen */
    printf("bs_amp_res: %d\n", sbr->bs_amp_res);
    printf("bs_start_freq: %d\n", sbr->bs_start_freq);
    printf("bs_stop_freq: %d\n", sbr->bs_stop_freq);
    printf("bs_xover_band: %d\n", sbr->bs_xover_band);
    if (bs_header_extra_1)
    {
        printf("bs_freq_scale: %d\n", sbr->bs_freq_scale);
        printf("bs_alter_scale: %d\n", sbr->bs_alter_scale);
        printf("bs_noise_bands: %d\n", sbr->bs_noise_bands);
    }
    if (bs_header_extra_2)
    {
        printf("bs_limiter_bands: %d\n", sbr->bs_limiter_bands);
        printf("bs_limiter_gains: %d\n", sbr->bs_limiter_gains);
        printf("bs_interpol_freq: %d\n", sbr->bs_interpol_freq);
        printf("bs_smoothing_mode: %d\n", sbr->bs_smoothing_mode);
    }
    printf("\n");
#endif
}

/* table 4 */
static void sbr_data(bitfile *ld, sbr_info *sbr, uint8_t id_aac)
{
#if 0
    sbr->bs_samplerate_mode = faad_get1bit(ld
        DEBUGVAR(1,219,"sbr_data(): bs_samplerate_mode"));
#endif

    sbr->rate = (sbr->bs_samplerate_mode) ? 2 : 1;

    switch (id_aac)
    {
    case ID_SCE:
        sbr_single_channel_element(ld, sbr);
        break;
    case ID_CPE:
        sbr_channel_pair_element(ld, sbr);
        break;
    }
}

/* table 5 */
static void sbr_single_channel_element(bitfile *ld, sbr_info *sbr)
{
    if (faad_get1bit(ld
        DEBUGVAR(1,220,"sbr_single_channel_element(): bs_data_extra")))
    {
        faad_getbits(ld, 4
            DEBUGVAR(1,221,"sbr_single_channel_element(): bs_reserved_bits_data"));
    }

    sbr_grid(ld, sbr, 0);
    sbr_dtdf(ld, sbr, 0);
    invf_mode(ld, sbr, 0);
    sbr_envelope(ld, sbr, 0);
    sbr_noise(ld, sbr, 0);

    envelope_noise_dequantisation(sbr, 0);

#if 0
// TEMP
    if (sbr->frame == 21)
    {
        int l, k;

        printf("\n");
        for (l = 0; l < sbr->L_E[0]; l++)
        {
            for (k = 0; k < sbr->n[sbr->f[0][l]]; k++)
            {
                //printf("%f\n", sbr->E_orig[0][k][l]);
                printf("%f\n", sbr->E_orig[0][k][l] * 1024.  / (float)(1 << REAL_BITS));
            }
        }
    }
// end TEMP
#endif

#if 0
// TEMP
    {
        int l, k;

        printf("\n");
        for (l = 0; l < sbr->L_Q[0]; l++)
        {
            for (k = 0; k < sbr->N_Q; k++)
            {
                printf("%f\n", sbr->Q_orig[0][k][l]);
            }
        }
    }
// end TEMP
#endif

    memset(sbr->bs_add_harmonic[0], 0, 64*sizeof(uint8_t));

    sbr->bs_add_harmonic_flag[0] = faad_get1bit(ld
        DEBUGVAR(1,223,"sbr_single_channel_element(): bs_add_harmonic_flag[0]"));
    if (sbr->bs_add_harmonic_flag[0])
        sinusoidal_coding(ld, sbr, 0);

    sbr->bs_extended_data = faad_get1bit(ld
        DEBUGVAR(1,224,"sbr_single_channel_element(): bs_extended_data[0]"));
    if (sbr->bs_extended_data)
    {
        uint16_t nr_bits_left;
        uint16_t cnt = faad_getbits(ld, 4
            DEBUGVAR(1,225,"sbr_single_channel_element(): bs_extension_size"));
        if (cnt == 15)
        {
            cnt += faad_getbits(ld, 8
                DEBUGVAR(1,226,"sbr_single_channel_element(): bs_esc_count"));
        }

        nr_bits_left = 8 * cnt;
        while (nr_bits_left > 7)
        {
            sbr->bs_extension_id = faad_getbits(ld, 2
                DEBUGVAR(1,227,"sbr_single_channel_element(): bs_extension_id"));
            nr_bits_left -= 2;
            /* sbr_extension(ld, sbr, 0, nr_bits_left); */
            sbr->bs_extension_data = faad_getbits(ld, 6
                DEBUGVAR(1,279,"sbr_single_channel_element(): bs_extension_data"));
        }
    }
}

/* table 6 */
static void sbr_channel_pair_element(bitfile *ld, sbr_info *sbr)
{
    uint8_t n;

    if (faad_get1bit(ld
        DEBUGVAR(1,228,"sbr_single_channel_element(): bs_data_extra")))
    {
        faad_getbits(ld, 4
            DEBUGVAR(1,228,"sbr_channel_pair_element(): bs_reserved_bits_data"));
        faad_getbits(ld, 4
            DEBUGVAR(1,228,"sbr_channel_pair_element(): bs_reserved_bits_data"));
    }

    sbr->bs_coupling = faad_get1bit(ld
        DEBUGVAR(1,228,"sbr_channel_pair_element(): bs_coupling"));

    if (sbr->bs_coupling)
    {
        sbr_grid(ld, sbr, 0);

        /* need to copy some data from left to right */
        sbr->bs_frame_class[1] = sbr->bs_frame_class[0];
        sbr->L_E[1] = sbr->L_E[0];
        sbr->L_Q[1] = sbr->L_Q[0];
        sbr->bs_pointer[1] = sbr->bs_pointer[0];

        for (n = 0; n <= sbr->L_E[0]; n++)
        {
            sbr->t_E[1][n] = sbr->t_E[0][n];
            sbr->f[1][n] = sbr->f[0][n];
        }
        for (n = 0; n <= sbr->L_Q[0]; n++)
            sbr->t_Q[1][n] = sbr->t_Q[0][n];

        sbr_dtdf(ld, sbr, 0);
        sbr_dtdf(ld, sbr, 1);
        invf_mode(ld, sbr, 0);

        /* more copying */
        for (n = 0; n < sbr->N_Q; n++)
            sbr->bs_invf_mode[1][n] = sbr->bs_invf_mode[0][n];

        sbr_envelope(ld, sbr, 0);
        sbr_noise(ld, sbr, 0);
        sbr_envelope(ld, sbr, 1);
        sbr_noise(ld, sbr, 1);

        memset(sbr->bs_add_harmonic[0], 0, 64*sizeof(uint8_t));
        memset(sbr->bs_add_harmonic[1], 0, 64*sizeof(uint8_t));

        sbr->bs_add_harmonic_flag[0] = faad_get1bit(ld
            DEBUGVAR(1,231,"sbr_channel_pair_element(): bs_add_harmonic_flag[0]"));
        if (sbr->bs_add_harmonic_flag[0])
            sinusoidal_coding(ld, sbr, 0);

        sbr->bs_add_harmonic_flag[1] = faad_get1bit(ld
            DEBUGVAR(1,232,"sbr_channel_pair_element(): bs_add_harmonic_flag[1]"));
        if (sbr->bs_add_harmonic_flag[1])
            sinusoidal_coding(ld, sbr, 1);
    } else {
        sbr_grid(ld, sbr, 0);
        sbr_grid(ld, sbr, 1);
        sbr_dtdf(ld, sbr, 0);
        sbr_dtdf(ld, sbr, 1);
        invf_mode(ld, sbr, 0);
        invf_mode(ld, sbr, 1);
        sbr_envelope(ld, sbr, 0);
        sbr_envelope(ld, sbr, 1);
        sbr_noise(ld, sbr, 0);
        sbr_noise(ld, sbr, 1);

        memset(sbr->bs_add_harmonic[0], 0, 64*sizeof(uint8_t));
        memset(sbr->bs_add_harmonic[1], 0, 64*sizeof(uint8_t));

        sbr->bs_add_harmonic_flag[0] = faad_get1bit(ld
            DEBUGVAR(1,239,"sbr_channel_pair_element(): bs_add_harmonic_flag[0]"));
        if (sbr->bs_add_harmonic_flag[0])
            sinusoidal_coding(ld, sbr, 0);

        sbr->bs_add_harmonic_flag[1] = faad_get1bit(ld
            DEBUGVAR(1,240,"sbr_channel_pair_element(): bs_add_harmonic_flag[1]"));
        if (sbr->bs_add_harmonic_flag[1])
            sinusoidal_coding(ld, sbr, 1);
    }
    envelope_noise_dequantisation(sbr, 0);
    envelope_noise_dequantisation(sbr, 1);

#if 0
// TEMP
    if (sbr->frame == 21)
    {
        int l, k;

        printf("\n");
        for (l = 0; l < sbr->L_E[0]; l++)
        {
            for (k = 0; k < sbr->n[sbr->f[0][l]]; k++)
            {
                printf("%f\n", sbr->E_orig[0][k][l]);
                //printf("%f\n", sbr->E_orig[0][k][l] * 1024.  / (float)(1 << REAL_BITS));
            }
        }
    }
// end TEMP
#endif

    if (sbr->bs_coupling)
        unmap_envelope_noise(sbr);

#if 0
// TEMP
    if (sbr->bs_coupling)
    {
        int l, k;

        printf("\n");
        for (l = 0; l < sbr->L_Q[0]; l++)
        {
            for (k = 0; k < sbr->N_Q; k++)
            {
                printf("%f\n", sbr->Q_orig[0][k][l]);
            }
        }
    }
// end TEMP
#endif

    sbr->bs_extended_data = faad_get1bit(ld
        DEBUGVAR(1,233,"sbr_channel_pair_element(): bs_extended_data[0]"));
    if (sbr->bs_extended_data)
    {
        uint16_t nr_bits_left;
        uint16_t cnt = faad_getbits(ld, 4
            DEBUGVAR(1,234,"sbr_channel_pair_element(): bs_extension_size"));
        if (cnt == 15)
        {
            cnt += faad_getbits(ld, 8
                DEBUGVAR(1,235,"sbr_channel_pair_element(): bs_esc_count"));
        }

        nr_bits_left = 8 * cnt;
        while (nr_bits_left > 7)
        {
            sbr->bs_extension_id = faad_getbits(ld, 2
                DEBUGVAR(1,236,"sbr_channel_pair_element(): bs_extension_id"));
            nr_bits_left -= 2;
            /* sbr_extension(ld, sbr, 0, nr_bits_left); */
            sbr->bs_extension_data = faad_getbits(ld, 6
                DEBUGVAR(1,280,"sbr_single_channel_element(): bs_extension_data"));
        }
    }
}

/* table 7 */
static void sbr_grid(bitfile *ld, sbr_info *sbr, uint8_t ch)
{
    uint8_t i, env, rel;
    uint8_t bs_abs_bord, bs_abs_bord_1;
    uint16_t bs_num_env;

    sbr->bs_frame_class[ch] = faad_getbits(ld, 2
        DEBUGVAR(1,248,"sbr_grid(): bs_frame_class"));

#if 0
    if (sbr->bs_frame_class[ch] != FIXFIX)
        printf("%d", sbr->bs_frame_class[ch]);
#endif

    switch (sbr->bs_frame_class[ch])
    {
    case FIXFIX:
        i = faad_getbits(ld, 2
            DEBUGVAR(1,249,"sbr_grid(): bs_num_env_raw"));

        bs_num_env = min(1 << i, 5);

        i = faad_get1bit(ld
            DEBUGVAR(1,250,"sbr_grid(): bs_freq_res_flag"));
        for (env = 0; env < bs_num_env; env++)
            sbr->f[ch][env] = i;

        sbr->abs_bord_lead[ch] = 0;
        sbr->abs_bord_trail[ch] = NO_TIME_SLOTS;
        sbr->n_rel_lead[ch] = bs_num_env - 1;
        sbr->n_rel_trail[ch] = 0;
        break;

    case FIXVAR:
        bs_abs_bord = faad_getbits(ld, 2
            DEBUGVAR(1,251,"sbr_grid(): bs_abs_bord")) + NO_TIME_SLOTS;
        bs_num_env = faad_getbits(ld, 2
            DEBUGVAR(1,252,"sbr_grid(): bs_num_env")) + 1;

        for (rel = 0; rel < bs_num_env-1; rel++)
        {
            sbr->bs_rel_bord[ch][rel] = 2 * faad_getbits(ld, 2
                DEBUGVAR(1,253,"sbr_grid(): bs_rel_bord")) + 2;
        }
        i = int_log2((int32_t)(bs_num_env + 1));
        sbr->bs_pointer[ch] = faad_getbits(ld, i
            DEBUGVAR(1,254,"sbr_grid(): bs_pointer"));

        for (env = 0; env < bs_num_env; env++)
        {
            sbr->f[ch][bs_num_env - env - 1] = faad_get1bit(ld
                DEBUGVAR(1,255,"sbr_grid(): bs_freq_res"));
        }

        sbr->abs_bord_lead[ch] = 0;
        sbr->abs_bord_trail[ch] = bs_abs_bord;
        sbr->n_rel_lead[ch] = 0;
        sbr->n_rel_trail[ch] = bs_num_env - 1;
        break;

    case VARFIX:
        bs_abs_bord = faad_getbits(ld, 2
            DEBUGVAR(1,256,"sbr_grid(): bs_abs_bord"));
        bs_num_env = faad_getbits(ld, 2
            DEBUGVAR(1,257,"sbr_grid(): bs_num_env")) + 1;

        for (rel = 0; rel < bs_num_env-1; rel++)
        {
            sbr->bs_rel_bord[ch][rel] = 2 * faad_getbits(ld, 2
                DEBUGVAR(1,258,"sbr_grid(): bs_rel_bord")) + 2;
        }
        i = int_log2((int32_t)(bs_num_env + 1));
        sbr->bs_pointer[ch] = faad_getbits(ld, i
            DEBUGVAR(1,259,"sbr_grid(): bs_pointer"));

        for (env = 0; env < bs_num_env; env++)
        {
            sbr->f[ch][env] = faad_get1bit(ld
                DEBUGVAR(1,260,"sbr_grid(): bs_freq_res"));
        }

        sbr->abs_bord_lead[ch] = bs_abs_bord;
        sbr->abs_bord_trail[ch] = NO_TIME_SLOTS;
        sbr->n_rel_lead[ch] = bs_num_env - 1;
        sbr->n_rel_trail[ch] = 0;
        break;

    case VARVAR:
        bs_abs_bord = faad_getbits(ld, 2
            DEBUGVAR(1,261,"sbr_grid(): bs_abs_bord_0"));
        bs_abs_bord_1 = faad_getbits(ld, 2
            DEBUGVAR(1,262,"sbr_grid(): bs_abs_bord_1")) + NO_TIME_SLOTS;
        sbr->bs_num_rel_0[ch] = faad_getbits(ld, 2
            DEBUGVAR(1,263,"sbr_grid(): bs_num_rel_0"));
        sbr->bs_num_rel_1[ch] = faad_getbits(ld, 2
            DEBUGVAR(1,264,"sbr_grid(): bs_num_rel_1"));

        bs_num_env = min(5, sbr->bs_num_rel_0[ch] + sbr->bs_num_rel_1[ch] + 1);

        for (rel = 0; rel < sbr->bs_num_rel_0[ch]; rel++)
        {
            sbr->bs_rel_bord_0[ch][rel] = 2 * faad_getbits(ld, 2
                DEBUGVAR(1,265,"sbr_grid(): bs_rel_bord")) + 2;
        }
        for(rel = 0; rel < sbr->bs_num_rel_1[ch]; rel++)
        {
            sbr->bs_rel_bord_1[ch][rel] = 2 * faad_getbits(ld, 2
                DEBUGVAR(1,266,"sbr_grid(): bs_rel_bord")) + 2;
        }
        i = int_log2((int32_t)(sbr->bs_num_rel_0[ch] + sbr->bs_num_rel_1[ch] + 2));
        sbr->bs_pointer[ch] = faad_getbits(ld, i
            DEBUGVAR(1,267,"sbr_grid(): bs_pointer"));

        for (env = 0; env < bs_num_env; env++)
        {
            sbr->f[ch][env] = faad_get1bit(ld
                DEBUGVAR(1,268,"sbr_grid(): bs_freq_res"));
        }

        sbr->abs_bord_lead[ch] = bs_abs_bord;
        sbr->abs_bord_trail[ch] = bs_abs_bord_1;
        sbr->n_rel_lead[ch] = sbr->bs_num_rel_0[ch];
        sbr->n_rel_trail[ch] = sbr->bs_num_rel_1[ch];
        break;
    }

    if (sbr->bs_frame_class[ch] == VARVAR)
        sbr->L_E[ch] = min(bs_num_env, 5);
    else
        sbr->L_E[ch] = min(bs_num_env, 4);

    if (sbr->L_E[ch] > 1)
        sbr->L_Q[ch] = 2;
    else
        sbr->L_Q[ch] = 1;

    /* TODO: this code can probably be integrated into the code above! */
    envelope_time_border_vector(sbr, ch);
    noise_floor_time_border_vector(sbr, ch);
}

/* table 8 */
static void sbr_dtdf(bitfile *ld, sbr_info *sbr, uint8_t ch)
{
    uint8_t i;

    for (i = 0; i < sbr->L_E[ch]; i++)
    {
        sbr->bs_df_env[ch][i] = faad_get1bit(ld
            DEBUGVAR(1,269,"sbr_dtdf(): bs_df_env"));
    }

    for (i = 0; i < sbr->L_Q[ch]; i++)
    {
        sbr->bs_df_noise[ch][i] = faad_get1bit(ld
            DEBUGVAR(1,270,"sbr_dtdf(): bs_df_noise"));
    }
}

/* table 9 */
static void invf_mode(bitfile *ld, sbr_info *sbr, uint8_t ch)
{
    uint8_t n;

    for (n = 0; n < sbr->N_Q; n++)
    {
        sbr->bs_invf_mode[ch][n] = faad_getbits(ld, 2
            DEBUGVAR(1,271,"invf_mode(): bs_invf_mode"));
    }
}

/* table 10 */
static void sbr_envelope(bitfile *ld, sbr_info *sbr, uint8_t ch)
{
    uint8_t env, band;
    int8_t delta = 0;
    sbr_huff_tab t_huff, f_huff;

    if ((sbr->L_E[ch] == 1) && (sbr->bs_frame_class[ch] == FIXFIX))
        sbr->amp_res[ch] = 0;
    else
        sbr->amp_res[ch] = sbr->bs_amp_res;

    if ((sbr->bs_coupling) && (ch == 1))
    {
        delta = 1;
        if (sbr->amp_res[ch])
        {
            t_huff = t_huffman_env_bal_3_0dB;
            f_huff = f_huffman_env_bal_3_0dB;
        } else {
            t_huff = t_huffman_env_bal_1_5dB;
            f_huff = f_huffman_env_bal_1_5dB;
        }
    } else {
        delta = 0;
        if (sbr->amp_res[ch])
        {
            t_huff = t_huffman_env_3_0dB;
            f_huff = f_huffman_env_3_0dB;
        } else {
            t_huff = t_huffman_env_1_5dB;
            f_huff = f_huffman_env_1_5dB;
        }
    }

    for (env = 0; env < sbr->L_E[ch]; env++)
    {
        if (sbr->bs_df_env[ch][env] == 0)
        {
            if ((sbr->bs_coupling == 1) && (ch == 1))
            {
                if (sbr->amp_res[ch])
                {
                    sbr->E[ch][0][env] = (faad_getbits(ld, 5
                        DEBUGVAR(1,272,"sbr_envelope(): bs_data_env")) << delta);
                } else {
                    sbr->E[ch][0][env] = (faad_getbits(ld, 6
                        DEBUGVAR(1,273,"sbr_envelope(): bs_data_env")) << delta);
                }
            } else {
                if (sbr->amp_res[ch])
                {
                    sbr->E[ch][0][env] = (faad_getbits(ld, 6
                        DEBUGVAR(1,274,"sbr_envelope(): bs_data_env")) << delta);
                } else {
                    sbr->E[ch][0][env] = (faad_getbits(ld, 7
                        DEBUGVAR(1,275,"sbr_envelope(): bs_data_env")) << delta);
                }
            }

            for (band = 1; band < sbr->n[sbr->f[ch][env]]; band++)
            {
                sbr->E[ch][band][env] = (sbr_huff_dec(ld, f_huff) << delta);
            }

        } else {
            for (band = 0; band < sbr->n[sbr->f[ch][env]]; band++)
            {
                sbr->E[ch][band][env] = (sbr_huff_dec(ld, t_huff) << delta);
            }
        }
    }

#if 0
// TEMP
    if (sbr->frame == 19)
    {
        int l, k;

        printf("\n");
        for (l = 0; l < sbr->L_E[ch]; l++)
        {
            for (k = 0; k < sbr->n[sbr->f[ch][l]]; k++)
            {
                printf("l:%d k:%d E:%d\n",l, k, sbr->E[ch][k][l]);
            }
        }
    }
// end TEMP
#endif

    extract_envelope_data(sbr, ch);

#if 0
// TEMP
    if (sbr->frame == 21)
    {
        int l, k;

        printf("\n");
        for (l = 0; l < sbr->L_E[ch]; l++)
        {
            for (k = 0; k < sbr->n[sbr->f[ch][l]]; k++)
            {
                //printf("l:%d k:%d E:%d\n",l,k, sbr->E[ch][k][l]);
                printf("%d\n", sbr->E[ch][k][l]);
            }
        }
    }
// end TEMP
#endif
}

/* table 11 */
static void sbr_noise(bitfile *ld, sbr_info *sbr, uint8_t ch)
{
    uint8_t noise, band;
    int8_t delta = 0;
    sbr_huff_tab t_huff, f_huff;

    if ((sbr->bs_coupling == 1) && (ch == 1))
    {
        delta = 1;
        t_huff = t_huffman_noise_bal_3_0dB;
        f_huff = f_huffman_env_bal_3_0dB;
    } else {
        delta = 0;
        t_huff = t_huffman_noise_3_0dB;
        f_huff = f_huffman_env_3_0dB;
    }

    for (noise = 0; noise < sbr->L_Q[ch]; noise++)
    {
        if(sbr->bs_df_noise[ch][noise] == 0)
        {
            if ((sbr->bs_coupling == 1) && (ch == 1))
            {
                sbr->Q[ch][0][noise] = (faad_getbits(ld, 5
                    DEBUGVAR(1,276,"sbr_noise(): bs_data_noise")) << delta);
            } else {
                sbr->Q[ch][0][noise] = (faad_getbits(ld, 5
                    DEBUGVAR(1,277,"sbr_noise(): bs_data_noise")) << delta);
            }
            for (band = 1; band < sbr->N_Q; band++)
            {
                sbr->Q[ch][band][noise] = (sbr_huff_dec(ld, f_huff) << delta);
            }
        } else {
            for (band = 0; band < sbr->N_Q; band++)
            {
                sbr->Q[ch][band][noise] = (sbr_huff_dec(ld, t_huff) << delta);
            }
        }
    }

    extract_noise_floor_data(sbr, ch);
}

/* table 12 */
static void sinusoidal_coding(bitfile *ld, sbr_info *sbr, uint8_t ch)
{
    uint8_t n;

    for (n = 0; n < sbr->N_high; n++)
    {
        sbr->bs_add_harmonic[ch][n] = faad_get1bit(ld
            DEBUGVAR(1,278,"sinusoidal_coding(): bs_add_harmonic"));
    }
}


#endif /* SBR_DEC */