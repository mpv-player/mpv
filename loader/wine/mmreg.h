/*
 *      mmreg.h   -       Declarations for ???
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

typedef struct _WAVEFILTER {
  DWORD   cbStruct;
  DWORD   dwFilterTag;
  DWORD   fdwFilter;
  DWORD   dwReserved[5];
} WAVEFILTER, *PWAVEFILTER, *NPWAVEFILTER, *LPWAVEFILTER;
#endif /* _ACM_WAVEFILTER */

#ifndef WAVE_FILTER_VOLUME
#define WAVE_FILTER_VOLUME      0x0001

typedef struct _WAVEFILTER_VOLUME {
   WAVEFILTER      wfltr;
   DWORD           dwVolume;
} VOLUMEWAVEFILTER, *PVOLUMEWAVEFILTER, *NPVOLUMEWAVEFILTER, *LPVOLUMEWAVEFILTER;
#endif  /* WAVE_FILTER_VOLUME */

#ifndef WAVE_FILTER_ECHO
#define WAVE_FILTER_ECHO        0x0002

typedef struct WAVEFILTER_ECHO {
   WAVEFILTER      wfltr;
   DWORD           dwVolume;
   DWORD           dwDelay;
} ECHOWAVEFILTER, *PECHOWAVEFILTER, *NPECHOWAVEFILTER, *LPECHOWAVEFILTER;
#endif  /* WAVEFILTER_ECHO */
    
#ifndef _WAVEFORMATEX_
#define _WAVEFORMATEX_
typedef struct _WAVEFORMATEX {
  WORD   wFormatTag;
  WORD   nChannels;
  DWORD  nSamplesPerSec;
  DWORD  nAvgBytesPerSec;
  WORD   nBlockAlign;
  WORD   wBitsPerSample;
  WORD   cbSize;
} WAVEFORMATEX, *PWAVEFORMATEX, *NPWAVEFORMATEX, *LPWAVEFORMATEX;
#endif /* _WAVEFORMATEX_ */

/* WAVE form wFormatTag IDs */
#define  WAVE_FORMAT_UNKNOWN			0x0000	/*  Microsoft Corporation  */
#define  WAVE_FORMAT_ADPCM			0x0002	/*  Microsoft Corporation  */
#define  WAVE_FORMAT_IBM_CVSD			0x0005	/*  IBM Corporation  */
#define  WAVE_FORMAT_ALAW			0x0006	/*  Microsoft Corporation  */
#define  WAVE_FORMAT_MULAW			0x0007	/*  Microsoft Corporation  */
#define  WAVE_FORMAT_OKI_ADPCM			0x0010	/*  OKI  */
#define  WAVE_FORMAT_DVI_ADPCM			0x0011	/*  Intel Corporation  */
#define  WAVE_FORMAT_IMA_ADPCM			(WAVE_FORMAT_DVI_ADPCM)	/*  Intel Corporation  */
#define  WAVE_FORMAT_MEDIASPACE_ADPCM		0x0012	/*  Videologic  */
#define  WAVE_FORMAT_SIERRA_ADPCM		0x0013	/*  Sierra Semiconductor Corp  */
#define  WAVE_FORMAT_G723_ADPCM			0x0014	/*  Antex Electronics Corporation  */
#define  WAVE_FORMAT_DIGISTD			0x0015	/*  DSP Solutions, Inc.  */
#define  WAVE_FORMAT_DIGIFIX			0x0016	/*  DSP Solutions, Inc.  */
#define  WAVE_FORMAT_DIALOGIC_OKI_ADPCM		0x0017	/*  Dialogic Corporation  */
#define  WAVE_FORMAT_YAMAHA_ADPCM		0x0020	/*  Yamaha Corporation of America  */
#define  WAVE_FORMAT_SONARC			0x0021	/*  Speech Compression  */
#define  WAVE_FORMAT_DSPGROUP_TRUESPEECH	0x0022	/*  DSP Group, Inc  */
#define  WAVE_FORMAT_ECHOSC1			0x0023	/*  Echo Speech Corporation  */
#define  WAVE_FORMAT_AUDIOFILE_AF36		0x0024	/*    */
#define  WAVE_FORMAT_APTX			0x0025	/*  Audio Processing Technology  */
#define  WAVE_FORMAT_AUDIOFILE_AF10		0x0026	/*    */
#define  WAVE_FORMAT_DOLBY_AC2			0x0030	/*  Dolby Laboratories  */
#define  WAVE_FORMAT_GSM610			0x0031	/*  Microsoft Corporation  */
#define  WAVE_FORMAT_ANTEX_ADPCME		0x0033	/*  Antex Electronics Corporation  */
#define  WAVE_FORMAT_CONTROL_RES_VQLPC		0x0034	/*  Control Resources Limited  */
#define  WAVE_FORMAT_DIGIREAL			0x0035	/*  DSP Solutions, Inc.  */
#define  WAVE_FORMAT_DIGIADPCM			0x0036	/*  DSP Solutions, Inc.  */
#define  WAVE_FORMAT_CONTROL_RES_CR10		0x0037	/*  Control Resources Limited  */
#define  WAVE_FORMAT_NMS_VBXADPCM		0x0038	/*  Natural MicroSystems  */
#define  WAVE_FORMAT_G721_ADPCM			0x0040	/*  Antex Electronics Corporation  */
#define  WAVE_FORMAT_MPEG			0x0050	/*  Microsoft Corporation  */
#define  WAVE_FORMAT_CREATIVE_ADPCM		0x0200	/*  Creative Labs, Inc  */
#define  WAVE_FORMAT_CREATIVE_FASTSPEECH8	0x0202	/*  Creative Labs, Inc  */
#define  WAVE_FORMAT_CREATIVE_FASTSPEECH10	0x0203	/*  Creative Labs, Inc  */
#define  WAVE_FORMAT_FM_TOWNS_SND		0x0300	/*  Fujitsu Corp.  */
#define  WAVE_FORMAT_OLIGSM			0x1000	/*  Ing C. Olivetti & C., S.p.A.  */
#define  WAVE_FORMAT_OLIADPCM			0x1001	/*  Ing C. Olivetti & C., S.p.A.  */
#define  WAVE_FORMAT_OLICELP			0x1002	/*  Ing C. Olivetti & C., S.p.A.  */
#define  WAVE_FORMAT_OLISBC			0x1003	/*  Ing C. Olivetti & C., S.p.A.  */
#define  WAVE_FORMAT_OLIOPR			0x1004	/*  Ing C. Olivetti & C., S.p.A.  */

#define WAVE_FORMAT_DEVELOPMENT         	(0xFFFF)

#endif /* __WINE_MMREG_H */
