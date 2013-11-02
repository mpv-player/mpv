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

#include "compat/mpbswap.h"
#include "video/img_fourcc.h"

// These structs must be binary-compatible to the native win32 types,
// because demux_mkv.c uses them directly.

typedef struct __attribute__((__packed__)) MP_WAVEFORMATEX {
    unsigned short  wFormatTag;
    unsigned short  nChannels;
    unsigned int    nSamplesPerSec;
    unsigned int    nAvgBytesPerSec;
    unsigned short  nBlockAlign;
    unsigned short  wBitsPerSample;
    unsigned short  cbSize;
} MP_WAVEFORMATEX;

typedef struct __attribute__((__packed__)) MP_BITMAPINFOHEADER {
    int     biSize;
    int     biWidth;
    int     biHeight;
    short   biPlanes;
    short   biBitCount;
    int     biCompression;
    int     biSizeImage;
    int     biXPelsPerMeter;
    int     biYPelsPerMeter;
    int     biClrUsed;
    int     biClrImportant;
} MP_BITMAPINFOHEADER;

#endif /* MPLAYER_MS_HDR_H */
