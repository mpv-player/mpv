/*
 * idct_mlib.c
 * Copyright (C) 1999-2002 Håkan Hjort <d95hjort@dtek.chalmers.se>
 *
 * This file is part of mpeg2dec, a free MPEG-2 video stream decoder.
 * See http://libmpeg2.sourceforge.net/ for updates.
 *
 * mpeg2dec is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpeg2dec is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "config.h"

#ifdef LIBMPEG2_MLIB

#include <mlib_types.h>
#include <mlib_status.h>
#include <mlib_sys.h>
#include <mlib_video.h>
#include <string.h>
#include <inttypes.h>

#include "mpeg2.h"
#include "mpeg2_internal.h"

void mpeg2_idct_add_mlib (const int last, int16_t * const block,
			  uint8_t * const dest, const int stride)
{
    mlib_VideoIDCT_IEEE_S16_S16 (block, block);
    mlib_VideoAddBlock_U8_S16 (dest, block, stride);
    memset (block, 0, 64 * sizeof (uint16_t));
}

void mpeg2_idct_copy_mlib_non_ieee (int16_t * const block,
				    uint8_t * const dest, const int stride)
{
    mlib_VideoIDCT8x8_U8_S16 (dest, block, stride);
    memset (block, 0, 64 * sizeof (uint16_t));
}

void mpeg2_idct_add_mlib_non_ieee (const int last, int16_t * const block,
				   uint8_t * const dest, const int stride)
{
    mlib_VideoIDCT8x8_S16_S16 (block, block);
    mlib_VideoAddBlock_U8_S16 (dest, block, stride);
    memset (block, 0, 64 * sizeof (uint16_t));
}

#endif
