/*
 * idct_mlib.c
 * Copyright (C) 1999-2001 Håkan Hjort <d95hjort@dtek.chalmers.se>
 *
 * This file is part of mpeg2dec, a free MPEG-2 video stream decoder.
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

#include <inttypes.h>
#include <mlib_types.h>
#include <mlib_status.h>
#include <mlib_sys.h>
#include <mlib_video.h>

#include "mpeg2_internal.h"

void idct_block_copy_mlib (int16_t * block, uint8_t * dest, int stride)
{
    mlib_VideoIDCT8x8_U8_S16 (dest, block, stride);
}

void idct_block_add_mlib (int16_t * block, uint8_t * dest, int stride)
{
    /* Should we use mlib_VideoIDCT_IEEE_S16_S16 here ?? */
    /* it's ~30% slower. */
    mlib_VideoIDCT8x8_S16_S16 (block, block);
    mlib_VideoAddBlock_U8_S16 (dest, block, stride);
}

#endif
