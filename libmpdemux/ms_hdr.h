/*
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef MPLAYER_MS_HDR_H
#define MPLAYER_MS_HDR_H

#include "config.h"

#ifndef _WAVEFORMATEX_
#define _WAVEFORMATEX_
typedef struct __attribute__((__packed__)) _WAVEFORMATEX {
  unsigned short  wFormatTag;
  unsigned short  nChannels;
  unsigned int    nSamplesPerSec;
  unsigned int    nAvgBytesPerSec;
  unsigned short  nBlockAlign;
  unsigned short  wBitsPerSample;
  unsigned short  cbSize;
} WAVEFORMATEX, *PWAVEFORMATEX, *NPWAVEFORMATEX, *LPWAVEFORMATEX;
#endif /* _WAVEFORMATEX_ */

#ifndef _WAVEFORMATEXTENSIBLE_
#define _WAVEFORMATEXTENSIBLE_
typedef struct __attribute__((__packed__)) _WAVEFORMATEXTENSIBLE {
    WAVEFORMATEX   wf;
    unsigned short wValidBitsPerSample;
    unsigned int   dwChannelMask;
    unsigned int   SubFormat; // Only interested in first 32 bits of guid
    unsigned int   _guid_remainder[3];
} WAVEFORMATEXTENSIBLE;
#endif /* _WAVEFORMATEXTENSIBLE_ */

#ifndef _MPEGLAYER3WAVEFORMAT_
#define _MPEGLAYER3WAVEFORMAT_
typedef struct __attribute__((__packed__)) mpeglayer3waveformat_tag {
  WAVEFORMATEX wf;
  unsigned short wID;
  unsigned int   fdwFlags;
  unsigned short nBlockSize;
  unsigned short nFramesPerBlock;
  unsigned short nCodecDelay;
} MPEGLAYER3WAVEFORMAT;
#endif /* _MPEGLAYER3WAVEFORMAT_ */

/* windows.h #includes wingdi.h on MinGW. */
#if !defined(_BITMAPINFOHEADER_) && !defined(_WINGDI_)
#define _BITMAPINFOHEADER_
typedef struct __attribute__((__packed__))
{
    int 	biSize;
    int  	biWidth;
    int  	biHeight;
    short 	biPlanes;
    short 	biBitCount;
    int 	biCompression;
    int 	biSizeImage;
    int  	biXPelsPerMeter;
    int  	biYPelsPerMeter;
    int 	biClrUsed;
    int 	biClrImportant;
} BITMAPINFOHEADER, *PBITMAPINFOHEADER, *LPBITMAPINFOHEADER;
typedef struct {
	BITMAPINFOHEADER bmiHeader;
	int	bmiColors[1];
} BITMAPINFO, *LPBITMAPINFO;
#endif

#ifndef le2me_BITMAPINFOHEADER
#if HAVE_BIGENDIAN
#define le2me_BITMAPINFOHEADER(h) {					\
    (h)->biSize = le2me_32((h)->biSize);				\
    (h)->biWidth = le2me_32((h)->biWidth);				\
    (h)->biHeight = le2me_32((h)->biHeight);				\
    (h)->biPlanes = le2me_16((h)->biPlanes);				\
    (h)->biBitCount = le2me_16((h)->biBitCount);			\
    (h)->biCompression = le2me_32((h)->biCompression);			\
    (h)->biSizeImage = le2me_32((h)->biSizeImage);			\
    (h)->biXPelsPerMeter = le2me_32((h)->biXPelsPerMeter);		\
    (h)->biYPelsPerMeter = le2me_32((h)->biYPelsPerMeter);		\
    (h)->biClrUsed = le2me_32((h)->biClrUsed);				\
    (h)->biClrImportant = le2me_32((h)->biClrImportant);		\
}
#define le2me_WAVEFORMATEX(h) {						\
    (h)->wFormatTag = le2me_16((h)->wFormatTag);			\
    (h)->nChannels = le2me_16((h)->nChannels);				\
    (h)->nSamplesPerSec = le2me_32((h)->nSamplesPerSec);		\
    (h)->nAvgBytesPerSec = le2me_32((h)->nAvgBytesPerSec);		\
    (h)->nBlockAlign = le2me_16((h)->nBlockAlign);			\
    (h)->wBitsPerSample = le2me_16((h)->wBitsPerSample);		\
    (h)->cbSize = le2me_16((h)->cbSize);				\
}
#else
#define le2me_BITMAPINFOHEADER(h)   /**/
#define le2me_WAVEFORMATEX(h)	    /**/
#endif
#endif

#endif /* MPLAYER_MS_HDR_H */
