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
** $Id: faad.h,v 1.33 2003/09/24 19:55:34 menno Exp $
**/

#ifndef __AACDEC_H__
#define __AACDEC_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

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

#define FAAD2_VERSION "2.0 RC1 "

/* object types for AAC */
#define MAIN       1
#define LC         2
#define SSR        3
#define LTP        4
#define HE_AAC     5
#define ER_LC     17
#define ER_LTP    19
#define LD        23
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

/* Capabilities */
#define LC_DEC_CAP            (1<<0)
#define MAIN_DEC_CAP          (1<<1)
#define LTP_DEC_CAP           (1<<2)
#define LD_DEC_CAP            (1<<3)
#define ERROR_RESILIENCE_CAP  (1<<4)
#define FIXED_POINT_CAP       (1<<5)

/* Channel definitions */
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

/* DRM channel definitions */
#define DRMCH_MONO          1
#define DRMCH_STEREO        2
#define DRMCH_SBR_MONO      3
#define DRMCH_SBR_LC_STEREO 4
#define DRMCH_SBR_STEREO    5


/* A decode call can eat up to FAAD_MIN_STREAMSIZE bytes per decoded channel,
   so at least so much bytes per channel should be available in this stream */
#define FAAD_MIN_STREAMSIZE 768 /* 6144 bits/channel */


typedef void *faacDecHandle;

typedef struct mp4AudioSpecificConfig
{
    /* Audio Specific Info */
    unsigned char objectTypeIndex;
    unsigned char samplingFrequencyIndex;
    unsigned long samplingFrequency;
    unsigned char channelsConfiguration;

    /* GA Specific Info */
    unsigned char frameLengthFlag;
    unsigned char dependsOnCoreCoder;
    unsigned short coreCoderDelay;
    unsigned char extensionFlag;
    unsigned char aacSectionDataResilienceFlag;
    unsigned char aacScalefactorDataResilienceFlag;
    unsigned char aacSpectralDataResilienceFlag;
    unsigned char epConfig;

    char sbr_present_flag;
    char forceUpSampling;
} mp4AudioSpecificConfig;

typedef struct faacDecConfiguration
{
    unsigned char defObjectType;
    unsigned long defSampleRate;
    unsigned char outputFormat;
    unsigned char downMatrix;
    unsigned char useOldADTSFormat;
} faacDecConfiguration, *faacDecConfigurationPtr;

typedef struct faacDecFrameInfo
{
    unsigned long bytesconsumed;
    unsigned long samples;
    unsigned char channels;
    unsigned char error;
    unsigned long samplerate;

    /* SBR: 0: off, 1: on; upsample, 2: on; downsampled, 3: off; upsampled */
    unsigned char sbr;

    /* MPEG-4 ObjectType */
    unsigned char object_type;

    /* AAC header type; MP4 will be signalled as RAW also */
    unsigned char header_type;

    /* multichannel configuration */
    unsigned char num_front_channels;
    unsigned char num_side_channels;
    unsigned char num_back_channels;
    unsigned char num_lfe_channels;
    unsigned char channel_position[64];
} faacDecFrameInfo;

char* FAADAPI faacDecGetErrorMessage(unsigned char errcode);

unsigned long FAADAPI faacDecGetCapabilities();

faacDecHandle FAADAPI faacDecOpen();

faacDecConfigurationPtr FAADAPI faacDecGetCurrentConfiguration(faacDecHandle hDecoder);

unsigned char FAADAPI faacDecSetConfiguration(faacDecHandle hDecoder,
                                    faacDecConfigurationPtr config);

/* Init the library based on info from the AAC file (ADTS/ADIF) */
long FAADAPI faacDecInit(faacDecHandle hDecoder,
                        unsigned char *buffer,
                        unsigned long buffer_size,
                        unsigned long *samplerate,
                        unsigned char *channels);

/* Init the library using a DecoderSpecificInfo */
char FAADAPI faacDecInit2(faacDecHandle hDecoder, unsigned char *pBuffer,
                         unsigned long SizeOfDecoderSpecificInfo,
                         unsigned long *samplerate, unsigned char *channels);

/* Init the library for DRM */
char FAADAPI faacDecInitDRM(faacDecHandle hDecoder, unsigned long samplerate,
                            unsigned char channels);

void FAADAPI faacDecPostSeekReset(faacDecHandle hDecoder, long frame);

void FAADAPI faacDecClose(faacDecHandle hDecoder);

void* FAADAPI faacDecDecode(faacDecHandle hDecoder,
                            faacDecFrameInfo *hInfo,
                            unsigned char *buffer,
                            unsigned long buffer_size);

char FAADAPI AudioSpecificConfig(unsigned char *pBuffer,
                                 unsigned long buffer_size,
                                 mp4AudioSpecificConfig *mp4ASC);

#ifdef _WIN32
  #pragma pack(pop)
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
