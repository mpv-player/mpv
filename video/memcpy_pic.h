/*
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with MPlayer; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef MPLAYER_FASTMEMCPY_H
#define MPLAYER_FASTMEMCPY_H

#include <inttypes.h>
#include <string.h>
#include <stddef.h>

#define my_memcpy_pic memcpy_pic
#define memcpy_pic2(d, s, b, h, ds, ss, unused) memcpy_pic(d, s, b, h, ds, ss)

static inline void memcpy_pic(void *dst, const void *src,
                              int bytesPerLine, int height,
                              int dstStride, int srcStride)
{
    if (bytesPerLine == dstStride && dstStride == srcStride) {
        if (srcStride < 0) {
            src = (uint8_t*)src + (height - 1) * srcStride;
            dst = (uint8_t*)dst + (height - 1) * dstStride;
            srcStride = -srcStride;
        }

        memcpy(dst, src, srcStride * height);
    } else {
        for (int i = 0; i < height; i++) {
            memcpy(dst, src, bytesPerLine);
            src = (uint8_t*)src + srcStride;
            dst = (uint8_t*)dst + dstStride;
        }
    }
}

static inline void memset_pic(void *dst, int fill, int bytesPerLine, int height,
                              int stride)
{
    if (bytesPerLine == stride) {
        memset(dst, fill, stride * height);
    } else {
        for (int i = 0; i < height; i++) {
            memset(dst, fill, bytesPerLine);
            dst = (uint8_t *)dst + stride;
        }
    }
}

static inline void memset16_pic(void *dst, int fill, int unitsPerLine,
                                int height, int stride)
{
    if (fill == 0) {
        memset_pic(dst, 0, unitsPerLine * 2, height, stride);
    } else {
        for (int i = 0; i < height; i++) {
            uint16_t *line = dst;
            uint16_t *end = line + unitsPerLine;
            while (line < end)
                *line++ = fill;
            dst = (uint8_t *)dst + stride;
        }
    }
}

#endif /* MPLAYER_FASTMEMCPY_H */
