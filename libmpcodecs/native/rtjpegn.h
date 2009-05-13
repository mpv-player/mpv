/*
   RTjpeg (C) Justin Schoeman 1998 (justin@suntiger.ee.up.ac.za)

   With modifications by:
   (c) 1998, 1999 by Joerg Walter <trouble@moes.pmnet.uni-oldenburg.de>
   and
   (c) 1999 by Wim Taymans <wim.taymans@tvd.be>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#ifndef MPLAYER_RTJPEGN_H
#define MPLAYER_RTJPEGN_H

#include <stdint.h>

#define __u8 uint8_t
#define __u16 uint16_t
#define __u32 uint32_t
#define __u64 uint64_t
#define __s8 int8_t
#define __s16 int16_t
#define __s32 int32_t
#define __s64 int64_t

void RTjpeg_init_compress(__u32 *buf, int width, int height, __u8 Q);
int RTjpeg_compressYUV420(__s8 *sp, unsigned char *bp);

void RTjpeg_init_mcompress(void);
int RTjpeg_mcompressYUV420(__s8 *sp, unsigned char *bp, __u16 lmask, __u16 cmask);

#endif /* MPLAYER_RTJPEGN_H */
