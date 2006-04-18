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
** $Id: decoder.h,v 1.44 2004/09/04 14:56:28 menno Exp $
**/

#ifndef __DECODER_H__
#define __DECODER_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
  #pragma pack(push, 8)
  #ifndef NEAACDECAPI
    #define NEAACDECAPI __cdecl
  #endif
#else
  #ifndef NEAACDECAPI
    #define NEAACDECAPI
  #endif
#endif


/* library output formats */
#define FAAD_FMT_16BIT  1
#define FAAD_FMT_24BIT  2
#define FAAD_FMT_32BIT  3
#define FAAD_FMT_FLOAT  4
#define FAAD_FMT_FIXED  FAAD_FMT_FLOAT
#define FAAD_FMT_DOUBLE 5

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

char* NEAACDECAPI NeAACDecGetErrorMessage(uint8_t errcode);

uint32_t NEAACDECAPI NeAACDecGetCapabilities(void);

NeAACDecHandle NEAACDECAPI NeAACDecOpen(void);

NeAACDecConfigurationPtr NEAACDECAPI NeAACDecGetCurrentConfiguration(NeAACDecHandle hDecoder);

uint8_t NEAACDECAPI NeAACDecSetConfiguration(NeAACDecHandle hDecoder,
                                             NeAACDecConfigurationPtr config);

/* Init the library based on info from the AAC file (ADTS/ADIF) */
int32_t NEAACDECAPI NeAACDecInit(NeAACDecHandle hDecoder,
                                 uint8_t *buffer,
                                 uint32_t buffer_size,
                                 uint32_t *samplerate,
                                 uint8_t *channels);

/* Init the library using a DecoderSpecificInfo */
int8_t NEAACDECAPI NeAACDecInit2(NeAACDecHandle hDecoder, uint8_t *pBuffer,
                                 uint32_t SizeOfDecoderSpecificInfo,
                                 uint32_t *samplerate, uint8_t *channels);

/* Init the library for DRM */
int8_t NEAACDECAPI NeAACDecInitDRM(NeAACDecHandle *hDecoder, uint32_t samplerate,
                                   uint8_t channels);

void NEAACDECAPI NeAACDecClose(NeAACDecHandle hDecoder);

void NEAACDECAPI NeAACDecPostSeekReset(NeAACDecHandle hDecoder, int32_t frame);

void* NEAACDECAPI NeAACDecDecode(NeAACDecHandle hDecoder,
                                 NeAACDecFrameInfo *hInfo,
                                 uint8_t *buffer,
                                 uint32_t buffer_size);

void* NEAACDECAPI NeAACDecDecode2(NeAACDecHandle hDecoder,
                                  NeAACDecFrameInfo *hInfo,
                                  uint8_t *buffer, uint32_t buffer_size,
                                  void **sample_buffer, uint32_t sample_buffer_size);

#ifdef _WIN32
  #pragma pack(pop)
#endif

#ifdef __cplusplus
}
#endif
#endif
