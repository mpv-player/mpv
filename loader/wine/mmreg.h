/*
 *      mmreg.h   -       Declarations for ???
 *
 * Modified for use with MPlayer, detailed changelog at
 * http://svn.mplayerhq.hu/mplayer/trunk/
 * $Id$
 *
 */

#ifndef __WINE_MMREG_H
#define __WINE_MMREG_H

/***********************************************************************
 * Defines/Enums
 */

#ifndef _ACM_WAVEFILTER
#define _ACM_WAVEFILTER

#include "windef.h"

#define WAVE_FILTER_UNKNOWN     0x0000
#define WAVE_FILTER_DEVELOPMENT 0xFFFF

typedef struct __attribute__((__packed__)) _WAVEFILTER {
  DWORD   cbStruct;
  DWORD   dwFilterTag;
  DWORD   fdwFilter;
  DWORD   dwReserved[5];
} WAVEFILTER, *PWAVEFILTER, *NPWAVEFILTER, *LPWAVEFILTER;
#endif /* _ACM_WAVEFILTER */

#ifndef WAVE_FILTER_VOLUME
#define WAVE_FILTER_VOLUME      0x0001

typedef struct __attribute__((__packed__)) _WAVEFILTER_VOLUME {
   WAVEFILTER      wfltr;
   DWORD           dwVolume;
} VOLUMEWAVEFILTER, *PVOLUMEWAVEFILTER, *NPVOLUMEWAVEFILTER, *LPVOLUMEWAVEFILTER;
#endif  /* WAVE_FILTER_VOLUME */

#ifndef WAVE_FILTER_ECHO
#define WAVE_FILTER_ECHO        0x0002

typedef struct __attribute__((__packed__)) WAVEFILTER_ECHO {
   WAVEFILTER      wfltr;
   DWORD           dwVolume;
   DWORD           dwDelay;
} ECHOWAVEFILTER, *PECHOWAVEFILTER, *NPECHOWAVEFILTER, *LPECHOWAVEFILTER;
#endif  /* WAVEFILTER_ECHO */
    
#ifndef _WAVEFORMATEX_
#define _WAVEFORMATEX_
typedef struct __attribute__((__packed__)) _WAVEFORMATEX {
  WORD   wFormatTag;
  WORD   nChannels;
  DWORD  nSamplesPerSec;
  DWORD  nAvgBytesPerSec;
  WORD   nBlockAlign;
  WORD   wBitsPerSample;
  WORD   cbSize;
} WAVEFORMATEX, *PWAVEFORMATEX, *NPWAVEFORMATEX, *LPWAVEFORMATEX;
#endif /* _WAVEFORMATEX_ */

#ifndef GUID_TYPE
#define GUID_TYPE
typedef struct
{
    unsigned long f1;
    unsigned short f2;
    unsigned short f3;
    unsigned char f4[8];
} GUID;
#endif

#ifndef _WAVEFORMATEXTENSIBLE_
#define _WAVEFORMATEXTENSIBLE_
typedef struct {
    WAVEFORMATEX    Format;
    union {
        WORD wValidBitsPerSample;       /* bits of precision  */
        WORD wSamplesPerBlock;          /* valid if wBitsPerSample==0 */
        WORD wReserved;                 /* If neither applies, set to zero. */
    } Samples;
    DWORD           dwChannelMask;      /* which channels are */
                                        /* present in stream  */
    GUID            SubFormat;
} WAVEFORMATEXTENSIBLE, *PWAVEFORMATEXTENSIBLE;
#endif // !_WAVEFORMATEXTENSIBLE_

#ifndef _MPEGLAYER3WAVEFORMAT_
#define _MPEGLAYER3WAVEFORMAT_
typedef struct mpeglayer3waveformat_tag {
  WORD   wFormatTag WINE_PACKED;
  WORD   nChannels WINE_PACKED;
  DWORD  nSamplesPerSec WINE_PACKED;
  DWORD  nAvgBytesPerSec WINE_PACKED;
  WORD   nBlockAlign WINE_PACKED;
  WORD   wBitsPerSample WINE_PACKED;
  WORD   cbSize WINE_PACKED;
  WORD          wID WINE_PACKED;
  DWORD         fdwFlags WINE_PACKED;
  WORD          nBlockSize WINE_PACKED;
  WORD          nFramesPerBlock WINE_PACKED;
  WORD          nCodecDelay WINE_PACKED;
} MPEGLAYER3WAVEFORMAT;
#endif /* !_MPEGLAYER3WAVEFORMAT_ */

/* WAVE form wFormatTag IDs */

#define  WAVE_FORMAT_UNKNOWN                    0x0000 /* Microsoft Corporation */
#define  WAVE_FORMAT_ADPCM                      0x0002 /* Microsoft Corporation */
#define  WAVE_FORMAT_IEEE_FLOAT                 0x0003 /* Microsoft Corporation */
#define  WAVE_FORMAT_VSELP                      0x0004 /* Compaq Computer Corp. */
#define  WAVE_FORMAT_IBM_CVSD                   0x0005 /* IBM Corporation */
#define  WAVE_FORMAT_ALAW                       0x0006 /* Microsoft Corporation */
#define  WAVE_FORMAT_MULAW                      0x0007 /* Microsoft Corporation */
#define  WAVE_FORMAT_DTS                        0x0008 /* Microsoft Corporation */
#define  WAVE_FORMAT_OKI_ADPCM                  0x0010 /* OKI */
#define  WAVE_FORMAT_DVI_ADPCM                  0x0011 /* Intel Corporation */
#define  WAVE_FORMAT_IMA_ADPCM                  (WAVE_FORMAT_DVI_ADPCM) /*  Intel Corporation */
#define  WAVE_FORMAT_MEDIASPACE_ADPCM           0x0012 /* Videologic */
#define  WAVE_FORMAT_SIERRA_ADPCM               0x0013 /* Sierra Semiconductor Corp */
#define  WAVE_FORMAT_G723_ADPCM                 0x0014 /* Antex Electronics Corporation */
#define  WAVE_FORMAT_DIGISTD                    0x0015 /* DSP Solutions, Inc. */
#define  WAVE_FORMAT_DIGIFIX                    0x0016 /* DSP Solutions, Inc. */
#define  WAVE_FORMAT_DIALOGIC_OKI_ADPCM         0x0017 /* Dialogic Corporation */
#define  WAVE_FORMAT_MEDIAVISION_ADPCM          0x0018 /* Media Vision, Inc. */
#define  WAVE_FORMAT_CU_CODEC                   0x0019 /* Hewlett-Packard Company */
#define  WAVE_FORMAT_YAMAHA_ADPCM               0x0020 /* Yamaha Corporation of America */
#define  WAVE_FORMAT_SONARC                     0x0021 /* Speech Compression */
#define  WAVE_FORMAT_DSPGROUP_TRUESPEECH        0x0022 /* DSP Group, Inc */
#define  WAVE_FORMAT_ECHOSC1                    0x0023 /* Echo Speech Corporation */
#define  WAVE_FORMAT_AUDIOFILE_AF36             0x0024 /* Virtual Music, Inc. */
#define  WAVE_FORMAT_APTX                       0x0025 /* Audio Processing Technology */
#define  WAVE_FORMAT_AUDIOFILE_AF10             0x0026 /* Virtual Music, Inc. */
#define  WAVE_FORMAT_PROSODY_1612               0x0027 /* Aculab plc */
#define  WAVE_FORMAT_LRC                        0x0028 /* Merging Technologies S.A. */
#define  WAVE_FORMAT_DOLBY_AC2                  0x0030 /* Dolby Laboratories */
#define  WAVE_FORMAT_GSM610                     0x0031 /* Microsoft Corporation */
#define  WAVE_FORMAT_MSNAUDIO                   0x0032 /* Microsoft Corporation */
#define  WAVE_FORMAT_ANTEX_ADPCME               0x0033 /* Antex Electronics Corporation */
#define  WAVE_FORMAT_CONTROL_RES_VQLPC          0x0034 /* Control Resources Limited */
#define  WAVE_FORMAT_DIGIREAL                   0x0035 /* DSP Solutions, Inc. */
#define  WAVE_FORMAT_DIGIADPCM                  0x0036 /* DSP Solutions, Inc. */
#define  WAVE_FORMAT_CONTROL_RES_CR10           0x0037 /* Control Resources Limited */
#define  WAVE_FORMAT_NMS_VBXADPCM               0x0038 /* Natural MicroSystems */
#define  WAVE_FORMAT_CS_IMAADPCM                0x0039 /* Crystal Semiconductor IMA ADPCM */
#define  WAVE_FORMAT_ECHOSC3                    0x003A /* Echo Speech Corporation */
#define  WAVE_FORMAT_ROCKWELL_ADPCM             0x003B /* Rockwell International */
#define  WAVE_FORMAT_ROCKWELL_DIGITALK          0x003C /* Rockwell International */
#define  WAVE_FORMAT_XEBEC                      0x003D /* Xebec Multimedia Solutions Limited */
#define  WAVE_FORMAT_G721_ADPCM                 0x0040 /* Antex Electronics Corporation */
#define  WAVE_FORMAT_G728_CELP                  0x0041 /* Antex Electronics Corporation */
#define  WAVE_FORMAT_MSG723                     0x0042 /* Microsoft Corporation */
#define  WAVE_FORMAT_MPEG                       0x0050 /* Microsoft Corporation */
#define  WAVE_FORMAT_RT24                       0x0052 /* InSoft, Inc. */
#define  WAVE_FORMAT_PAC                        0x0053 /* InSoft, Inc. */
#define  WAVE_FORMAT_MPEGLAYER3                 0x0055 /* ISO/MPEG Layer3 Format Tag */
#define  WAVE_FORMAT_LUCENT_G723                0x0059 /* Lucent Technologies */
#define  WAVE_FORMAT_CIRRUS                     0x0060 /* Cirrus Logic */
#define  WAVE_FORMAT_ESPCM                      0x0061 /* ESS Technology */
#define  WAVE_FORMAT_VOXWARE                    0x0062 /* Voxware Inc */
#define  WAVE_FORMAT_CANOPUS_ATRAC              0x0063 /* Canopus, co., Ltd. */
#define  WAVE_FORMAT_G726_ADPCM                 0x0064 /* APICOM */
#define  WAVE_FORMAT_G722_ADPCM                 0x0065 /* APICOM */
#define  WAVE_FORMAT_DSAT_DISPLAY               0x0067 /* Microsoft Corporation */
#define  WAVE_FORMAT_VOXWARE_BYTE_ALIGNED       0x0069 /* Voxware Inc */
#define  WAVE_FORMAT_VOXWARE_AC8                0x0070 /* Voxware Inc */
#define  WAVE_FORMAT_VOXWARE_AC10               0x0071 /* Voxware Inc */
#define  WAVE_FORMAT_VOXWARE_AC16               0x0072 /* Voxware Inc */
#define  WAVE_FORMAT_VOXWARE_AC20               0x0073 /* Voxware Inc */
#define  WAVE_FORMAT_VOXWARE_RT24               0x0074 /* Voxware Inc */
#define  WAVE_FORMAT_VOXWARE_RT29               0x0075 /* Voxware Inc */
#define  WAVE_FORMAT_VOXWARE_RT29HW             0x0076 /* Voxware Inc */
#define  WAVE_FORMAT_VOXWARE_VR12               0x0077 /* Voxware Inc */
#define  WAVE_FORMAT_VOXWARE_VR18               0x0078 /* Voxware Inc */
#define  WAVE_FORMAT_VOXWARE_TQ40               0x0079 /* Voxware Inc */
#define  WAVE_FORMAT_SOFTSOUND                  0x0080 /* Softsound, Ltd. */
#define  WAVE_FORMAT_VOXWARE_TQ60               0x0081 /* Voxware Inc */
#define  WAVE_FORMAT_MSRT24                     0x0082 /* Microsoft Corporation */
#define  WAVE_FORMAT_G729A                      0x0083 /* AT&T Labs, Inc. */
#define  WAVE_FORMAT_MVI_MVI2                   0x0084 /* Motion Pixels */
#define  WAVE_FORMAT_DF_G726                    0x0085 /* DataFusion Systems (Pty) (Ltd) */
#define  WAVE_FORMAT_DF_GSM610                  0x0086 /* DataFusion Systems (Pty) (Ltd) */
#define  WAVE_FORMAT_ISIAUDIO                   0x0088 /* Iterated Systems, Inc. */
#define  WAVE_FORMAT_ONLIVE                     0x0089 /* OnLive! Technologies, Inc. */
#define  WAVE_FORMAT_SBC24                      0x0091 /* Siemens Business Communications Sys */
#define  WAVE_FORMAT_DOLBY_AC3_SPDIF            0x0092 /* Sonic Foundry */
#define  WAVE_FORMAT_MEDIASONIC_G723            0x0093 /* MediaSonic */
#define  WAVE_FORMAT_PROSODY_8KBPS              0x0094 /* Aculab plc */
#define  WAVE_FORMAT_ZYXEL_ADPCM                0x0097 /* ZyXEL Communications, Inc. */
#define  WAVE_FORMAT_PHILIPS_LPCBB              0x0098 /* Philips Speech Processing */
#define  WAVE_FORMAT_PACKED                     0x0099 /* Studer Professional Audio AG */
#define  WAVE_FORMAT_MALDEN_PHONYTALK           0x00A0 /* Malden Electronics Ltd. */
#define  WAVE_FORMAT_RHETOREX_ADPCM             0x0100 /* Rhetorex Inc. */
#define  WAVE_FORMAT_IRAT                       0x0101 /* BeCubed Software Inc. */
#define  WAVE_FORMAT_VIVO_G723                  0x0111 /* Vivo Software */
#define  WAVE_FORMAT_VIVO_SIREN                 0x0112 /* Vivo Software */
#define  WAVE_FORMAT_DIGITAL_G723               0x0123 /* Digital Equipment Corporation */
#define  WAVE_FORMAT_SANYO_LD_ADPCM             0x0125 /* Sanyo Electric Co., Ltd. */
#define  WAVE_FORMAT_SIPROLAB_ACEPLNET          0x0130 /* Sipro Lab Telecom Inc. */
#define  WAVE_FORMAT_SIPROLAB_ACELP4800         0x0131 /* Sipro Lab Telecom Inc. */
#define  WAVE_FORMAT_SIPROLAB_ACELP8V3          0x0132 /* Sipro Lab Telecom Inc. */
#define  WAVE_FORMAT_SIPROLAB_G729              0x0133 /* Sipro Lab Telecom Inc. */
#define  WAVE_FORMAT_SIPROLAB_G729A             0x0134 /* Sipro Lab Telecom Inc. */
#define  WAVE_FORMAT_SIPROLAB_KELVIN            0x0135 /* Sipro Lab Telecom Inc. */
#define  WAVE_FORMAT_G726ADPCM                  0x0140 /* Dictaphone Corporation */
#define  WAVE_FORMAT_QUALCOMM_PUREVOICE         0x0150 /* Qualcomm, Inc. */
#define  WAVE_FORMAT_QUALCOMM_HALFRATE          0x0151 /* Qualcomm, Inc. */
#define  WAVE_FORMAT_TUBGSM                     0x0155 /* Ring Zero Systems, Inc. */
#define  WAVE_FORMAT_MSAUDIO1                   0x0160 /* Microsoft Corporation */
#define  WAVE_FORMAT_CREATIVE_ADPCM             0x0200 /* Creative Labs, Inc */
#define  WAVE_FORMAT_CREATIVE_FASTSPEECH8       0x0202 /* Creative Labs, Inc */
#define  WAVE_FORMAT_CREATIVE_FASTSPEECH10      0x0203 /* Creative Labs, Inc */
#define  WAVE_FORMAT_UHER_ADPCM                 0x0210 /* UHER informatic GmbH */
#define  WAVE_FORMAT_QUARTERDECK                0x0220 /* Quarterdeck Corporation */
#define  WAVE_FORMAT_ILINK_VC                   0x0230 /* I-link Worldwide */
#define  WAVE_FORMAT_RAW_SPORT                  0x0240 /* Aureal Semiconductor */
#define  WAVE_FORMAT_IPI_HSX                    0x0250 /* Interactive Products, Inc. */
#define  WAVE_FORMAT_IPI_RPELP                  0x0251 /* Interactive Products, Inc. */
#define  WAVE_FORMAT_CS2                        0x0260 /* Consistent Software */
#define  WAVE_FORMAT_SONY_SCX                   0x0270 /* Sony Corp. */
#define  WAVE_FORMAT_FM_TOWNS_SND               0x0300 /* Fujitsu Corp. */
#define  WAVE_FORMAT_BTV_DIGITAL                0x0400 /* Brooktree Corporation */
#define  WAVE_FORMAT_QDESIGN_MUSIC              0x0450 /* QDesign Corporation */
#define  WAVE_FORMAT_VME_VMPCM                  0x0680 /* AT&T Labs, Inc. */
#define  WAVE_FORMAT_TPC                        0x0681 /* AT&T Labs, Inc. */
#define  WAVE_FORMAT_OLIGSM                     0x1000 /* Ing C. Olivetti & C., S.p.A. */
#define  WAVE_FORMAT_OLIADPCM                   0x1001 /* Ing C. Olivetti & C., S.p.A. */
#define  WAVE_FORMAT_OLICELP                    0x1002 /* Ing C. Olivetti & C., S.p.A. */
#define  WAVE_FORMAT_OLISBC                     0x1003 /* Ing C. Olivetti & C., S.p.A. */
#define  WAVE_FORMAT_OLIOPR                     0x1004 /* Ing C. Olivetti & C., S.p.A. */
#define  WAVE_FORMAT_LH_CODEC                   0x1100 /* Lernout & Hauspie */
#define  WAVE_FORMAT_NORRIS                     0x1400 /* Norris Communications, Inc. */
#define  WAVE_FORMAT_SOUNDSPACE_MUSICOMPRESS    0x1500 /* AT&T Labs, Inc. */
#define  WAVE_FORMAT_DVM                        0x2000 /* FAST Multimedia AG */

#if !defined(WAVE_FORMAT_EXTENSIBLE)
#define  WAVE_FORMAT_EXTENSIBLE                 0xFFFE /* Microsoft */
#endif // !defined(WAVE_FORMAT_EXTENSIBLE)
//
//  the WAVE_FORMAT_DEVELOPMENT format tag can be used during the
//  development phase of a new wave format.  Before shipping, you MUST
//  acquire an official format tag from Microsoft.
//
#define WAVE_FORMAT_DEVELOPMENT         (0xFFFF)


#endif /* __WINE_MMREG_H */
