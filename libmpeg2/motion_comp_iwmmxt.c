/*
 * motion_comp_iwmmxt.c
 * Copyright (C) 2004 AGAWA Koji <i (AT) atty (DOT) jp>
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

#if defined(ARCH_ARM) && defined(HAVE_IWMMXT)

#include <inttypes.h>

#include "mpeg2.h"
#include "attributes.h"
#include "mpeg2_internal.h"

/* defined in libavcodec */

extern void put_pixels16_iwmmxt(uint8_t * dest, const uint8_t * ref, const int stride, int height);
extern void put_pixels16_x2_iwmmxt(uint8_t * dest, const uint8_t * ref, const int stride, int height);
extern void put_pixels16_y2_iwmmxt(uint8_t * dest, const uint8_t * ref, const int stride, int height);
extern void put_pixels16_xy2_iwmmxt(uint8_t * dest, const uint8_t * ref, const int stride, int height);
extern void put_pixels8_iwmmxt(uint8_t * dest, const uint8_t * ref, const int stride, int height);
extern void put_pixels8_x2_iwmmxt(uint8_t * dest, const uint8_t * ref, const int stride, int height);
extern void put_pixels8_y2_iwmmxt(uint8_t * dest, const uint8_t * ref, const int stride, int height);
extern void put_pixels8_xy2_iwmmxt(uint8_t * dest, const uint8_t * ref, const int stride, int height);
extern void avg_pixels16_iwmmxt(uint8_t * dest, const uint8_t * ref, const int stride, int height);
extern void avg_pixels16_x2_iwmmxt(uint8_t * dest, const uint8_t * ref, const int stride, int height);
extern void avg_pixels16_y2_iwmmxt(uint8_t * dest, const uint8_t * ref, const int stride, int height);
extern void avg_pixels16_xy2_iwmmxt(uint8_t * dest, const uint8_t * ref, const int stride, int height);
extern void avg_pixels8_iwmmxt(uint8_t * dest, const uint8_t * ref, const int stride, int height);
extern void avg_pixels8_x2_iwmmxt(uint8_t * dest, const uint8_t * ref, const int stride, int height);
extern void avg_pixels8_y2_iwmmxt(uint8_t * dest, const uint8_t * ref, const int stride, int height);
extern void avg_pixels8_xy2_iwmmxt(uint8_t * dest, const uint8_t * ref, const int stride, int height);

mpeg2_mc_t mpeg2_mc_iwmmxt = {
    {put_pixels16_iwmmxt, put_pixels16_x2_iwmmxt, put_pixels16_y2_iwmmxt, put_pixels16_xy2_iwmmxt,
     put_pixels8_iwmmxt, put_pixels8_x2_iwmmxt,  put_pixels8_y2_iwmmxt,  put_pixels8_xy2_iwmmxt}, \
    {avg_pixels16_iwmmxt, avg_pixels16_x2_iwmmxt, avg_pixels16_y2_iwmmxt, avg_pixels16_xy2_iwmmxt,
     avg_pixels8_iwmmxt, avg_pixels8_x2_iwmmxt,  avg_pixels8_y2_iwmmxt,  avg_pixels8_xy2_iwmmxt}, \
};

#endif /* defined(ARCH_ARM) && defined(HAVE_IWMMXT) */
