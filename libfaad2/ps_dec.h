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
** $Id: ps_dec.h,v 1.8 2004/09/04 14:56:28 menno Exp $
**/

#ifndef __PS_DEC_H__
#define __PS_DEC_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "bits.h"

#define EXTENSION_ID_PS 2

#define MAX_PS_ENVELOPES 5
#define NO_ALLPASS_LINKS 3

typedef struct
{
    /* bitstream parameters */
    uint8_t enable_iid;
    uint8_t enable_icc;
    uint8_t enable_ext;

    uint8_t iid_mode;
    uint8_t icc_mode;
    uint8_t nr_iid_par;
    uint8_t nr_ipdopd_par;
    uint8_t nr_icc_par;

    uint8_t frame_class;
    uint8_t num_env;

    uint8_t border_position[MAX_PS_ENVELOPES+1];

    uint8_t iid_dt[MAX_PS_ENVELOPES];
    uint8_t icc_dt[MAX_PS_ENVELOPES];

    uint8_t enable_ipdopd;
    uint8_t ipd_mode;
    uint8_t ipd_dt[MAX_PS_ENVELOPES];
    uint8_t opd_dt[MAX_PS_ENVELOPES];

    /* indices */
    int8_t iid_index_prev[34];
    int8_t icc_index_prev[34];
    int8_t ipd_index_prev[17];
    int8_t opd_index_prev[17];
    int8_t iid_index[MAX_PS_ENVELOPES][34];
    int8_t icc_index[MAX_PS_ENVELOPES][34];
    int8_t ipd_index[MAX_PS_ENVELOPES][17];
    int8_t opd_index[MAX_PS_ENVELOPES][17];

    int8_t ipd_index_1[17];
    int8_t opd_index_1[17];
    int8_t ipd_index_2[17];
    int8_t opd_index_2[17];

    /* ps data was correctly read */
    uint8_t ps_data_available;

    /* a header has been read */
    uint8_t header_read;

    /* hybrid filterbank parameters */
    void *hyb;
    uint8_t use34hybrid_bands;

    /**/
    uint8_t num_groups;
    uint8_t num_hybrid_groups;
    uint8_t nr_par_bands;
    uint8_t nr_allpass_bands;
    uint8_t decay_cutoff;

    uint8_t *group_border;
    uint16_t *map_group2bk;

    /* filter delay handling */
    uint8_t saved_delay;
    uint8_t delay_buf_index_ser[NO_ALLPASS_LINKS];
    uint8_t num_sample_delay_ser[NO_ALLPASS_LINKS];
    uint8_t delay_D[64];
    uint8_t delay_buf_index_delay[64];

    complex_t delay_Qmf[14][64]; /* 14 samples delay max, 64 QMF channels */
    complex_t delay_SubQmf[2][32]; /* 2 samples delay max (SubQmf is always allpass filtered) */
    complex_t delay_Qmf_ser[NO_ALLPASS_LINKS][5][64]; /* 5 samples delay max (table 8.34), 64 QMF channels */
    complex_t delay_SubQmf_ser[NO_ALLPASS_LINKS][5][32]; /* 5 samples delay max (table 8.34) */

    /* transients */
    real_t alpha_decay;
    real_t alpha_smooth;

    real_t P_PeakDecayNrg[34];
    real_t P_prev[34];
    real_t P_SmoothPeakDecayDiffNrg_prev[34];

    /* mixing and phase */
    complex_t h11_prev[50];
    complex_t h12_prev[50];
    complex_t h21_prev[50];
    complex_t h22_prev[50];
    uint8_t phase_hist;
    complex_t ipd_prev[20][2];
    complex_t opd_prev[20][2];

} ps_info;

/* ps_syntax.c */
uint16_t ps_data(ps_info *ps, bitfile *ld, uint8_t *header);

/* ps_dec.c */
ps_info *ps_init(uint8_t sr_index);
void ps_free(ps_info *ps);

uint8_t ps_decode(ps_info *ps, qmf_t X_left[38][64], qmf_t X_right[38][64]);


#ifdef __cplusplus
}
#endif
#endif

