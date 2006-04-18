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
** $Id$
**/

#ifndef __DRM_DEC_H__
#define __DRM_DEC_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "bits.h"

#define DRM_PARAMETRIC_STEREO    0
#define DRM_NUM_SA_BANDS         8
#define DRM_NUM_PAN_BANDS       20
#define NUM_OF_LINKS             3
#define NUM_OF_QMF_CHANNELS     64
#define NUM_OF_SUBSAMPLES       30
#define MAX_SA_BAND             46
#define MAX_PAN_BAND            64
#define MAX_DELAY                5

typedef struct
{   
    uint8_t drm_ps_data_available;
    uint8_t bs_enable_sa;
    uint8_t bs_enable_pan;

    uint8_t bs_sa_dt_flag;
    uint8_t bs_pan_dt_flag;

    uint8_t g_last_had_sa;
    uint8_t g_last_had_pan;

    int8_t bs_sa_data[DRM_NUM_SA_BANDS];
    int8_t bs_pan_data[DRM_NUM_PAN_BANDS];
        
    int8_t g_sa_index[DRM_NUM_SA_BANDS];
    int8_t g_pan_index[DRM_NUM_PAN_BANDS];                        
    int8_t g_prev_sa_index[DRM_NUM_SA_BANDS];
    int8_t g_prev_pan_index[DRM_NUM_PAN_BANDS];    

    int8_t sa_decode_error;
    int8_t pan_decode_error;

    int8_t g_last_good_sa_index[DRM_NUM_SA_BANDS];
    int8_t g_last_good_pan_index[DRM_NUM_PAN_BANDS];
    
    qmf_t SA[NUM_OF_SUBSAMPLES][MAX_SA_BAND];               

    complex_t d_buff[2][MAX_SA_BAND];
    complex_t d2_buff[NUM_OF_LINKS][MAX_DELAY][MAX_SA_BAND];

    uint8_t delay_buf_index_ser[NUM_OF_LINKS];    
            
    real_t prev_nrg[MAX_SA_BAND];
    real_t prev_peakdiff[MAX_SA_BAND];
    real_t peakdecay_fast[MAX_SA_BAND]; 
} drm_ps_info;


uint16_t drm_ps_data(drm_ps_info *ps, bitfile *ld);

drm_ps_info *drm_ps_init(void);
void drm_ps_free(drm_ps_info *ps);

uint8_t drm_ps_decode(drm_ps_info *ps, uint8_t guess, uint32_t samplerate, qmf_t X_left[38][64], qmf_t X_right[38][64]);

#ifdef __cplusplus
}
#endif
#endif

