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
** $Id: sbr_syntax.c,v 1.2 2003/10/03 22:22:27 alex Exp $
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
#ifdef PS_DEC
#include "ps_dec.h"
#endif
#ifdef DRM_PS
#include "drm_dec.h"
#endif
#include "analysis.h"

/* static function declarations */
static void sbr_header(bitfile *ld, sbr_info *sbr);
static uint8_t sbr_data(bitfile *ld, sbr_info *sbr);
static uint16_t sbr_extension(bitfile *ld, sbr_info *sbr,
                              uint8_t bs_extension_id, uint16_t num_bits_left);
static uint8_t sbr_single_channel_element(bitfile *ld, sbr_info *sbr);
static uint8_t sbr_channel_pair_element(bitfile *ld, sbr_info *sbr);
static uint8_t sbr_grid(bitfile *ld, sbr_info *sbr, uint8_t ch);
static void sbr_dtdf(bitfile *ld, sbr_info *sbr, uint8_t ch);
static void invf_mode(bitfile *ld, sbr_info *sbr, uint8_t ch);
static void sinusoidal_coding(bitfile *ld, sbr_info *sbr, uint8_t ch);


static void sbr_reset(sbr_info *sbr)
{
#if 0
    printf("%d\n", sbr->bs_start_freq_prev);
    printf("%d\n", sbr->bs_stop_freq_prev);
    printf("%d\n", sbr->bs_freq_scale_prev);
    printf("%d\n", sbr->bs_alter_scale_prev);
    printf("%d\n", sbr->bs_xover_band_prev);
    printf("%d\n\n", sbr->bs_noise_bands_prev);
#endif

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
uint8_t sbr_extension_data(bitfile *ld, sbr_info *sbr, uint16_t cnt)
{
    uint8_t result = 0;
    uint16_t num_align_bits = 0;
    uint16_t num_sbr_bits = (uint16_t)faad_get_processed_bits(ld);

#ifdef DRM
    if (!sbr->Is_DRM_SBR)
#endif
    {
        uint8_t bs_extension_type = (uint8_t)faad_getbits(ld, 4
            DEBUGVAR(1,198,"sbr_bitstream(): bs_extension_type"));

        if (bs_extension_type == EXT_SBR_DATA_CRC)
        {
            sbr->bs_sbr_crc_bits = (uint16_t)faad_getbits(ld, 10
                DEBUGVAR(1,199,"sbr_bitstream(): bs_sbr_crc_bits"));
        }
    }

    sbr->bs_header_flag = faad_get1bit(ld
        DEBUGVAR(1,200,"sbr_bitstream(): bs_header_flag"));

    if (sbr->bs_header_flag)
        sbr_header(ld, sbr);

    /* Reset? */
    sbr_reset(sbr);

    /* first frame should have a header */
    //if (!(sbr->frame == 0 && sbr->bs_header_flag == 0))
    if (sbr->header_count != 0)
    {
        if (sbr->Reset || (sbr->bs_header_flag && sbr->just_seeked))
        {
            uint8_t k2;

            /* calculate the Master Frequency Table */
            sbr->k0 = qmf_start_channel(sbr->bs_start_freq, sbr->bs_samplerate_mode,
                sbr->sample_rate);
            k2 = qmf_stop_channel(sbr->bs_stop_freq, sbr->sample_rate, sbr->k0);

            /* check k0 and k2 */
            if (sbr->sample_rate >= 48000)
            {
                if ((k2 - sbr->k0) > 32)
                    result += 1;
            } else if (sbr->sample_rate <= 32000) {
                if ((k2 - sbr->k0) > 48)
                    result += 1;
            } else { /* (sbr->sample_rate == 44100) */
                if ((k2 - sbr->k0) > 45)
                    result += 1;
            }

            if (sbr->bs_freq_scale == 0)
            {
                result += master_frequency_table_fs0(sbr, sbr->k0, k2,
                    sbr->bs_alter_scale);
            } else {
                result += master_frequency_table(sbr, sbr->k0, k2, sbr->bs_freq_scale,
                    sbr->bs_alter_scale);
            }
            result += derived_frequency_table(sbr, sbr->bs_xover_band, k2);

            result = (result > 0) ? 1 : 0;
        }

        if (result == 0)
            result = sbr_data(ld, sbr);
    } else {
        result = 1;
    }

#ifdef DRM
    if (!sbr->Is_DRM_SBR)
#endif
    {
        num_sbr_bits = (uint16_t)faad_get_processed_bits(ld) - num_sbr_bits;
        /* -4 does not apply, bs_extension_type is re-read in this function */
        num_align_bits = 8*cnt /*- 4*/ - num_sbr_bits;

        while (num_align_bits > 7)
        {
            faad_getbits(ld, 8
                DEBUGVAR(1,999,"sbr_bitstream(): num_align_bits"));
            num_align_bits -= 8;
        }
        faad_getbits(ld, num_align_bits
            DEBUGVAR(1,999,"sbr_bitstream(): num_align_bits"));
    }

    return result;
}

/* table 3 */
static void sbr_header(bitfile *ld, sbr_info *sbr)
{
    uint8_t bs_header_extra_1, bs_header_extra_2;

    sbr->header_count++;

    sbr->bs_amp_res = faad_get1bit(ld
        DEBUGVAR(1,203,"sbr_header(): bs_amp_res"));

    /* bs_start_freq and bs_stop_freq must define a fequency band that does
       not exceed 48 channels */
    sbr->bs_start_freq = (uint8_t)faad_getbits(ld, 4
        DEBUGVAR(1,204,"sbr_header(): bs_start_freq"));
    sbr->bs_stop_freq = (uint8_t)faad_getbits(ld, 4
        DEBUGVAR(1,205,"sbr_header(): bs_stop_freq"));
    sbr->bs_xover_band = (uint8_t)faad_getbits(ld, 3
        DEBUGVAR(1,206,"sbr_header(): bs_xover_band"));
    faad_getbits(ld, 2
        DEBUGVAR(1,207,"sbr_header(): bs_reserved_bits_hdr"));
    bs_header_extra_1 = (uint8_t)faad_get1bit(ld
        DEBUGVAR(1,208,"sbr_header(): bs_header_extra_1"));
    bs_header_extra_2 = (uint8_t)faad_get1bit(ld
        DEBUGVAR(1,209,"sbr_header(): bs_header_extra_2"));

    if (bs_header_extra_1)
    {
        sbr->bs_freq_scale = (uint8_t)faad_getbits(ld, 2
            DEBUGVAR(1,211,"sbr_header(): bs_freq_scale"));
        sbr->bs_alter_scale = (uint8_t)faad_get1bit(ld
            DEBUGVAR(1,212,"sbr_header(): bs_alter_scale"));
        sbr->bs_noise_bands = (uint8_t)faad_getbits(ld, 2
            DEBUGVAR(1,213,"sbr_header(): bs_noise_bands"));
    } else {
        /* Default values */
        sbr->bs_freq_scale = 2;
        sbr->bs_alter_scale = 1;
        sbr->bs_noise_bands = 2;
    }

    if (bs_header_extra_2)
    {
        sbr->bs_limiter_bands = (uint8_t)faad_getbits(ld, 2
            DEBUGVAR(1,214,"sbr_header(): bs_limiter_bands"));
        sbr->bs_limiter_gains = (uint8_t)faad_getbits(ld, 2
            DEBUGVAR(1,215,"sbr_header(): bs_limiter_gains"));
        sbr->bs_interpol_freq = (uint8_t)faad_get1bit(ld
            DEBUGVAR(1,216,"sbr_header(): bs_interpol_freq"));
        sbr->bs_smoothing_mode = (uint8_t)faad_get1bit(ld
            DEBUGVAR(1,217,"sbr_header(): bs_smoothing_mode"));
    } else {
        /* Default values */
        sbr->bs_limiter_bands = 2;
        sbr->bs_limiter_gains = 2;
        sbr->bs_interpol_freq = 1;
        sbr->bs_smoothing_mode = 1;
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
static uint8_t sbr_data(bitfile *ld, sbr_info *sbr)
{
    uint8_t result;
#if 0
    sbr->bs_samplerate_mode = faad_get1bit(ld
        DEBUGVAR(1,219,"sbr_data(): bs_samplerate_mode"));
#endif

    sbr->rate = (sbr->bs_samplerate_mode) ? 2 : 1;

    switch (sbr->id_aac)
    {
    case ID_SCE:
		if ((result = sbr_single_channel_element(ld, sbr)) > 0)
			return result;
        break;
    case ID_CPE:
		if ((result = sbr_channel_pair_element(ld, sbr)) > 0)
			return result;
        break;
    }

	return 0;
}

/* table 5 */
static uint8_t sbr_single_channel_element(bitfile *ld, sbr_info *sbr)
{
    uint8_t result;

    if (faad_get1bit(ld
        DEBUGVAR(1,220,"sbr_single_channel_element(): bs_data_extra")))
    {
        faad_getbits(ld, 4
            DEBUGVAR(1,221,"sbr_single_channel_element(): bs_reserved_bits_data"));
    }

#ifdef DRM
    /* bs_coupling, from sbr_channel_pair_base_element(bs_amp_res) */
    if (sbr->Is_DRM_SBR)
        faad_get1bit(ld);
#endif

    if ((result = sbr_grid(ld, sbr, 0)) > 0)
        return result;
    sbr_dtdf(ld, sbr, 0);
    invf_mode(ld, sbr, 0);
    sbr_envelope(ld, sbr, 0);
    sbr_noise(ld, sbr, 0);

    envelope_noise_dequantisation(sbr, 0);

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
        uint16_t cnt = (uint16_t)faad_getbits(ld, 4
            DEBUGVAR(1,225,"sbr_single_channel_element(): bs_extension_size"));
        if (cnt == 15)
        {
            cnt += (uint16_t)faad_getbits(ld, 8
                DEBUGVAR(1,226,"sbr_single_channel_element(): bs_esc_count"));
        }

        nr_bits_left = 8 * cnt;
        while (nr_bits_left > 7)
        {
            sbr->bs_extension_id = (uint8_t)faad_getbits(ld, 2
                DEBUGVAR(1,227,"sbr_single_channel_element(): bs_extension_id"));
            nr_bits_left -= 2;
            nr_bits_left -= sbr_extension(ld, sbr, sbr->bs_extension_id, nr_bits_left);
        }

        /* Corrigendum */
        if (nr_bits_left > 0)
        {
            faad_getbits(ld, nr_bits_left
                DEBUGVAR(1,280,"sbr_single_channel_element(): nr_bits_left"));
        }
    }

    return 0;
}

/* table 6 */
static uint8_t sbr_channel_pair_element(bitfile *ld, sbr_info *sbr)
{
    uint8_t n, result;

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
        if ((result = sbr_grid(ld, sbr, 0)) > 0)
            return result;

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
        if ((result = sbr_grid(ld, sbr, 0)) > 0)
            return result;
        if ((result = sbr_grid(ld, sbr, 1)) > 0)
            return result;
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

    if (sbr->bs_coupling)
        unmap_envelope_noise(sbr);

    sbr->bs_extended_data = faad_get1bit(ld
        DEBUGVAR(1,233,"sbr_channel_pair_element(): bs_extended_data[0]"));
    if (sbr->bs_extended_data)
    {
        uint16_t nr_bits_left;
        uint16_t cnt = (uint16_t)faad_getbits(ld, 4
            DEBUGVAR(1,234,"sbr_channel_pair_element(): bs_extension_size"));
        if (cnt == 15)
        {
            cnt += (uint16_t)faad_getbits(ld, 8
                DEBUGVAR(1,235,"sbr_channel_pair_element(): bs_esc_count"));
        }

        nr_bits_left = 8 * cnt;
        while (nr_bits_left > 7)
        {
            sbr->bs_extension_id = (uint8_t)faad_getbits(ld, 2
                DEBUGVAR(1,236,"sbr_channel_pair_element(): bs_extension_id"));
            nr_bits_left -= 2;
            sbr_extension(ld, sbr, sbr->bs_extension_id, nr_bits_left);
        }

        /* Corrigendum */
        if (nr_bits_left > 0)
        {
            faad_getbits(ld, nr_bits_left
                DEBUGVAR(1,280,"sbr_channel_pair_element(): nr_bits_left"));
        }
    }

    return 0;
}

/* integer log[2](x): input range [0,10) */
static int8_t sbr_log2(const int8_t val)
{
    int8_t log2tab[] = { 0, 0, 1, 2, 2, 3, 3, 3, 3, 4 };
    if (val < 10 && val >= 0)
        return log2tab[val];
    else
        return 0;
}


/* table 7 */
static uint8_t sbr_grid(bitfile *ld, sbr_info *sbr, uint8_t ch)
{
    uint8_t i, env, rel, result;
    uint8_t bs_abs_bord, bs_abs_bord_1;
    uint8_t bs_num_env = 0;

    sbr->bs_frame_class[ch] = (uint8_t)faad_getbits(ld, 2
        DEBUGVAR(1,248,"sbr_grid(): bs_frame_class"));

    switch (sbr->bs_frame_class[ch])
    {
    case FIXFIX:
        i = (uint8_t)faad_getbits(ld, 2
            DEBUGVAR(1,249,"sbr_grid(): bs_num_env_raw"));

        bs_num_env = min(1 << i, 5);

        i = (uint8_t)faad_get1bit(ld
            DEBUGVAR(1,250,"sbr_grid(): bs_freq_res_flag"));
        for (env = 0; env < bs_num_env; env++)
            sbr->f[ch][env] = i;

        sbr->abs_bord_lead[ch] = 0;
        sbr->abs_bord_trail[ch] = sbr->numTimeSlots;
        sbr->n_rel_lead[ch] = bs_num_env - 1;
        sbr->n_rel_trail[ch] = 0;
        break;

    case FIXVAR:
        bs_abs_bord = (uint8_t)faad_getbits(ld, 2
            DEBUGVAR(1,251,"sbr_grid(): bs_abs_bord")) + sbr->numTimeSlots;
        bs_num_env = (uint8_t)faad_getbits(ld, 2
            DEBUGVAR(1,252,"sbr_grid(): bs_num_env")) + 1;

        for (rel = 0; rel < bs_num_env-1; rel++)
        {
            sbr->bs_rel_bord[ch][rel] = 2 * (uint8_t)faad_getbits(ld, 2
                DEBUGVAR(1,253,"sbr_grid(): bs_rel_bord")) + 2;
        }
        i = sbr_log2(bs_num_env + 1);
        sbr->bs_pointer[ch] = (uint8_t)faad_getbits(ld, i
            DEBUGVAR(1,254,"sbr_grid(): bs_pointer"));

        for (env = 0; env < bs_num_env; env++)
        {
            sbr->f[ch][bs_num_env - env - 1] = (uint8_t)faad_get1bit(ld
                DEBUGVAR(1,255,"sbr_grid(): bs_freq_res"));
        }

        sbr->abs_bord_lead[ch] = 0;
        sbr->abs_bord_trail[ch] = bs_abs_bord;
        sbr->n_rel_lead[ch] = 0;
        sbr->n_rel_trail[ch] = bs_num_env - 1;
        break;

    case VARFIX:
        bs_abs_bord = (uint8_t)faad_getbits(ld, 2
            DEBUGVAR(1,256,"sbr_grid(): bs_abs_bord"));
        bs_num_env = (uint8_t)faad_getbits(ld, 2
            DEBUGVAR(1,257,"sbr_grid(): bs_num_env")) + 1;

        for (rel = 0; rel < bs_num_env-1; rel++)
        {
            sbr->bs_rel_bord[ch][rel] = 2 * (uint8_t)faad_getbits(ld, 2
                DEBUGVAR(1,258,"sbr_grid(): bs_rel_bord")) + 2;
        }
        i = sbr_log2(bs_num_env + 1);
        sbr->bs_pointer[ch] = (uint8_t)faad_getbits(ld, i
            DEBUGVAR(1,259,"sbr_grid(): bs_pointer"));

        for (env = 0; env < bs_num_env; env++)
        {
            sbr->f[ch][env] = (uint8_t)faad_get1bit(ld
                DEBUGVAR(1,260,"sbr_grid(): bs_freq_res"));
        }

        sbr->abs_bord_lead[ch] = bs_abs_bord;
        sbr->abs_bord_trail[ch] = sbr->numTimeSlots;
        sbr->n_rel_lead[ch] = bs_num_env - 1;
        sbr->n_rel_trail[ch] = 0;
        break;

    case VARVAR:
        bs_abs_bord = (uint8_t)faad_getbits(ld, 2
            DEBUGVAR(1,261,"sbr_grid(): bs_abs_bord_0"));
        bs_abs_bord_1 = (uint8_t)faad_getbits(ld, 2
            DEBUGVAR(1,262,"sbr_grid(): bs_abs_bord_1")) + sbr->numTimeSlots;
        sbr->bs_num_rel_0[ch] = (uint8_t)faad_getbits(ld, 2
            DEBUGVAR(1,263,"sbr_grid(): bs_num_rel_0"));
        sbr->bs_num_rel_1[ch] = (uint8_t)faad_getbits(ld, 2
            DEBUGVAR(1,264,"sbr_grid(): bs_num_rel_1"));

        bs_num_env = min(5, sbr->bs_num_rel_0[ch] + sbr->bs_num_rel_1[ch] + 1);

        for (rel = 0; rel < sbr->bs_num_rel_0[ch]; rel++)
        {
            sbr->bs_rel_bord_0[ch][rel] = 2 * (uint8_t)faad_getbits(ld, 2
                DEBUGVAR(1,265,"sbr_grid(): bs_rel_bord")) + 2;
        }
        for(rel = 0; rel < sbr->bs_num_rel_1[ch]; rel++)
        {
            sbr->bs_rel_bord_1[ch][rel] = 2 * (uint8_t)faad_getbits(ld, 2
                DEBUGVAR(1,266,"sbr_grid(): bs_rel_bord")) + 2;
        }
        i = sbr_log2(sbr->bs_num_rel_0[ch] + sbr->bs_num_rel_1[ch] + 2);
        sbr->bs_pointer[ch] = (uint8_t)faad_getbits(ld, i
            DEBUGVAR(1,267,"sbr_grid(): bs_pointer"));

        for (env = 0; env < bs_num_env; env++)
        {
            sbr->f[ch][env] = (uint8_t)faad_get1bit(ld
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

    if (sbr->L_E[ch] <= 0)
        return 1;

    if (sbr->L_E[ch] > 1)
        sbr->L_Q[ch] = 2;
    else
        sbr->L_Q[ch] = 1;

    /* TODO: this code can probably be integrated into the code above! */
    if ((result = envelope_time_border_vector(sbr, ch)) > 0)
        return result;
    noise_floor_time_border_vector(sbr, ch);

    return 0;
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
        sbr->bs_invf_mode[ch][n] = (uint8_t)faad_getbits(ld, 2
            DEBUGVAR(1,271,"invf_mode(): bs_invf_mode"));
    }
}

static uint16_t sbr_extension(bitfile *ld, sbr_info *sbr,
                              uint8_t bs_extension_id, uint16_t num_bits_left)
{
    switch (bs_extension_id)
    {
#ifdef PS_DEC
    case EXTENSION_ID_PS:
        sbr->ps_used = 1;
        return ps_data(&(sbr->ps), ld);
#endif
#ifdef DRM_PS
    case DRM_PARAMETRIC_STEREO:
        sbr->ps_used = 1;
        return drm_ps_data(&(sbr->drm_ps), ld);
#endif
    default:
        sbr->bs_extension_data = (uint8_t)faad_getbits(ld, 6
            DEBUGVAR(1,279,"sbr_single_channel_element(): bs_extension_data"));
        return 6;
    }
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
