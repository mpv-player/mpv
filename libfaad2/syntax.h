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
** $Id: syntax.h,v 1.37 2003/09/24 19:55:34 menno Exp $
**/

#ifndef __SYNTAX_H__
#define __SYNTAX_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "decoder.h"
#include "drc.h"
#include "bits.h"

#define MAIN       1
#define LC         2
#define SSR        3
#define LTP        4
#define HE_AAC     5
#define LD        23
#define ER_LC     17
#define ER_LTP    19
#define DRM_ER_LC 27 /* special object type for DRM */

/* header types */
#define RAW        0
#define ADIF       1
#define ADTS       2

/* SBR signalling */
#define NO_SBR           0
#define SBR_UPSAMPLED    1
#define SBR_DOWNSAMPLED  2
#define NO_SBR_UPSAMPLED 3

/* DRM channel definitions */
#define DRMCH_MONO          1
#define DRMCH_STEREO        2
#define DRMCH_SBR_MONO      3
#define DRMCH_SBR_LC_STEREO 4
#define DRMCH_SBR_STEREO    5


/* First object type that has ER */
#define ER_OBJECT_START 17


/* Bitstream */
#define LEN_SE_ID 3
#define LEN_TAG   4
#define LEN_BYTE  8

#define EXT_FIL            0
#define EXT_FILL_DATA      1
#define EXT_DATA_ELEMENT   2
#define EXT_DYNAMIC_RANGE 11
#define ANC_DATA           0

/* Syntax elements */
#define ID_SCE 0x0
#define ID_CPE 0x1
#define ID_CCE 0x2
#define ID_LFE 0x3
#define ID_DSE 0x4
#define ID_PCE 0x5
#define ID_FIL 0x6
#define ID_END 0x7

#define ONLY_LONG_SEQUENCE   0x0
#define LONG_START_SEQUENCE  0x1
#define EIGHT_SHORT_SEQUENCE 0x2
#define LONG_STOP_SEQUENCE   0x3

#define ZERO_HCB       0
#define FIRST_PAIR_HCB 5
#define ESC_HCB        11
#define QUAD_LEN       4
#define PAIR_LEN       2
#define NOISE_HCB      13
#define INTENSITY_HCB2 14
#define INTENSITY_HCB  15

int8_t GASpecificConfig(bitfile *ld, mp4AudioSpecificConfig *mp4ASC,
                        program_config *pce);

uint8_t adts_frame(adts_header *adts, bitfile *ld);
void get_adif_header(adif_header *adif, bitfile *ld);


/* static functions */
static uint8_t single_lfe_channel_element(faacDecHandle hDecoder,
                                          element *sce, bitfile *ld,
                                          real_t *spec_coef);
static uint8_t channel_pair_element(faacDecHandle hDecoder, element *cpe,
                                    bitfile *ld,
                                    real_t *spec_coef1, real_t *spec_coef2);
static uint8_t coupling_channel_element(faacDecHandle hDecoder, bitfile *ld);
static uint16_t data_stream_element(faacDecHandle hDecoder, bitfile *ld);
static uint8_t program_config_element(program_config *pce, bitfile *ld);
static uint8_t fill_element(faacDecHandle hDecoder, bitfile *ld, drc_info *drc
#ifdef SBR_DEC
                            ,uint8_t sbr_ele
#endif
                            );
static uint8_t individual_channel_stream(faacDecHandle hDecoder, element *ele,
                                         bitfile *ld, ic_stream *ics, uint8_t scal_flag,
                                         int16_t *spec_data);
static uint8_t ics_info(faacDecHandle hDecoder, ic_stream *ics, bitfile *ld,
                        uint8_t common_window);
static uint8_t section_data(faacDecHandle hDecoder, ic_stream *ics, bitfile *ld);
static uint8_t scale_factor_data(faacDecHandle hDecoder, ic_stream *ics, bitfile *ld);
static void gain_control_data(bitfile *ld, ic_stream *ics);
static uint8_t spectral_data(faacDecHandle hDecoder, ic_stream *ics, bitfile *ld,
                             int16_t *spectral_data);
static uint16_t extension_payload(bitfile *ld, drc_info *drc, uint16_t count);
#ifdef ERROR_RESILIENCE
uint8_t reordered_spectral_data(faacDecHandle hDecoder, ic_stream *ics,
                                bitfile *ld, int16_t *spectral_data);
#endif
static uint8_t pulse_data(ic_stream *ics, pulse_info *pul, bitfile *ld);
static void tns_data(ic_stream *ics, tns_info *tns, bitfile *ld);
static void ltp_data(faacDecHandle hDecoder, ic_stream *ics, ltp_info *ltp, bitfile *ld);
static uint8_t adts_fixed_header(adts_header *adts, bitfile *ld);
static void adts_variable_header(adts_header *adts, bitfile *ld);
static void adts_error_check(adts_header *adts, bitfile *ld);
static uint8_t dynamic_range_info(bitfile *ld, drc_info *drc);
static uint8_t excluded_channels(bitfile *ld, drc_info *drc);


#ifdef __cplusplus
}
#endif
#endif
