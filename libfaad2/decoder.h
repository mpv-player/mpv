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
** $Id: decoder.h,v 1.26 2003/07/29 08:20:12 menno Exp $
**/

#ifndef __DECODER_H__
#define __DECODER_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
  #pragma pack(push, 8)
  #ifndef FAADAPI
    #define FAADAPI __cdecl
  #endif
#else
  #ifndef FAADAPI
    #define FAADAPI
  #endif
#endif

#include "bits.h"
#include "syntax.h"
#include "drc.h"
#include "specrec.h"
#include "filtbank.h"
#include "ic_predict.h"


/* library output formats */
#define FAAD_FMT_16BIT  1
#define FAAD_FMT_24BIT  2
#define FAAD_FMT_32BIT  3
#define FAAD_FMT_FLOAT  4
#define FAAD_FMT_DOUBLE 5
#define FAAD_FMT_16BIT_DITHER  6
#define FAAD_FMT_16BIT_L_SHAPE 7
#define FAAD_FMT_16BIT_M_SHAPE 8
#define FAAD_FMT_16BIT_H_SHAPE 9

#define FAAD_FMT_DITHER_LOWEST FAAD_FMT_16BIT_DITHER

#define LC_DEC_CAP            (1<<0)
#define MAIN_DEC_CAP          (1<<1)
#define LTP_DEC_CAP           (1<<2)
#define LD_DEC_CAP            (1<<3)
#define ERROR_RESILIENCE_CAP  (1<<4)
#define FIXED_POINT_CAP       (1<<5)

#define FRONT_CHANNEL_CENTER (1)
#define FRONT_CHANNEL_LEFT   (2)
#define FRONT_CHANNEL_RIGHT  (3)
#define SIDE_CHANNEL_LEFT    (4)
#define SIDE_CHANNEL_RIGHT   (5)
#define BACK_CHANNEL_LEFT    (6)
#define BACK_CHANNEL_RIGHT   (7)
#define BACK_CHANNEL_CENTER  (8)
#define LFE_CHANNEL          (9)
#define UNKNOWN_CHANNEL      (0)

int8_t* FAADAPI faacDecGetErrorMessage(uint8_t errcode);

uint32_t FAADAPI faacDecGetCapabilities();

faacDecHandle FAADAPI faacDecOpen();

faacDecConfigurationPtr FAADAPI faacDecGetCurrentConfiguration(faacDecHandle hDecoder);

uint8_t FAADAPI faacDecSetConfiguration(faacDecHandle hDecoder,
                                    faacDecConfigurationPtr config);

/* Init the library based on info from the AAC file (ADTS/ADIF) */
int32_t FAADAPI faacDecInit(faacDecHandle hDecoder,
                            uint8_t *buffer,
                            uint32_t buffer_size,
                            uint32_t *samplerate,
                            uint8_t *channels);

/* Init the library using a DecoderSpecificInfo */
int8_t FAADAPI faacDecInit2(faacDecHandle hDecoder, uint8_t *pBuffer,
                         uint32_t SizeOfDecoderSpecificInfo,
                         uint32_t *samplerate, uint8_t *channels);

/* Init the library for DRM */
int8_t FAADAPI faacDecInitDRM(faacDecHandle hDecoder, uint32_t samplerate,
                              uint8_t channels);

void FAADAPI faacDecClose(faacDecHandle hDecoder);

void* FAADAPI faacDecDecode(faacDecHandle hDecoder,
                            faacDecFrameInfo *hInfo,
                            uint8_t *buffer,
                            uint32_t buffer_size);

element *decode_sce_lfe(faacDecHandle hDecoder,
                        faacDecFrameInfo *hInfo, bitfile *ld,
                        int16_t **spec_data, real_t **spec_coef,
                        uint8_t id_syn_ele);
element *decode_cpe(faacDecHandle hDecoder,
                    faacDecFrameInfo *hInfo, bitfile *ld,
                    int16_t **spec_data, real_t **spec_coef,
                    uint8_t id_syn_ele);
element **raw_data_block(faacDecHandle hDecoder, faacDecFrameInfo *hInfo,
                         bitfile *ld, element **elements,
                         int16_t **spec_data, real_t **spec_coef,
                         program_config *pce, drc_info *drc);

#ifdef _WIN32
  #pragma pack(pop)
#endif

#ifdef __cplusplus
}
#endif
#endif
