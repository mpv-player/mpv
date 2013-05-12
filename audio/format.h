/*
 * The sample format system used lin libaf is based on bitmasks.
 * The format definition only refers to the storage format,
 * not the resolution.
 *
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

#ifndef MPLAYER_AF_FORMAT_H
#define MPLAYER_AF_FORMAT_H

#include <sys/types.h>
#include "config.h"
#include "core/bstr.h"

// Endianness
#define AF_FORMAT_BE		(0<<0) // Big Endian
#define AF_FORMAT_LE		(1<<0) // Little Endian
#define AF_FORMAT_END_MASK	(1<<0)

#if BYTE_ORDER == BIG_ENDIAN
#define	AF_FORMAT_NE		AF_FORMAT_BE
#else
#define	AF_FORMAT_NE		AF_FORMAT_LE
#endif

// Signed/unsigned
#define AF_FORMAT_SI		(0<<1) // Signed
#define AF_FORMAT_US		(1<<1) // Unsigned
#define AF_FORMAT_SIGN_MASK	(1<<1)

// Fixed or floating point
#define AF_FORMAT_I		(0<<2) // Int
#define AF_FORMAT_F		(1<<2) // Foating point
#define AF_FORMAT_POINT_MASK	(1<<2)

// Bits used
#define AF_FORMAT_8BIT		(0<<3)
#define AF_FORMAT_16BIT		(1<<3)
#define AF_FORMAT_24BIT		(2<<3)
#define AF_FORMAT_32BIT		(3<<3)
#define AF_FORMAT_64BIT		(4<<3)
#define AF_FORMAT_BITS_MASK	(7<<3)

// Special flags refering to non pcm data (note: 1<<6, 2<<6, 5<<6 unused)
#define AF_FORMAT_MPEG2		(3<<6) // MPEG(2) audio
#define AF_FORMAT_AC3		(4<<6) // Dolby Digital AC3
#define AF_FORMAT_IEC61937      (6<<6)
#define AF_FORMAT_SPECIAL_MASK	(7<<6)

#define AF_FORMAT_MASK          ((1<<9)-1)

// PREDEFINED formats

#define AF_FORMAT_U8		(AF_FORMAT_I|AF_FORMAT_US|AF_FORMAT_8BIT|AF_FORMAT_NE)
#define AF_FORMAT_S8		(AF_FORMAT_I|AF_FORMAT_SI|AF_FORMAT_8BIT|AF_FORMAT_NE)
#define AF_FORMAT_U16_LE	(AF_FORMAT_I|AF_FORMAT_US|AF_FORMAT_16BIT|AF_FORMAT_LE)
#define AF_FORMAT_U16_BE	(AF_FORMAT_I|AF_FORMAT_US|AF_FORMAT_16BIT|AF_FORMAT_BE)
#define AF_FORMAT_S16_LE	(AF_FORMAT_I|AF_FORMAT_SI|AF_FORMAT_16BIT|AF_FORMAT_LE)
#define AF_FORMAT_S16_BE	(AF_FORMAT_I|AF_FORMAT_SI|AF_FORMAT_16BIT|AF_FORMAT_BE)
#define AF_FORMAT_U24_LE	(AF_FORMAT_I|AF_FORMAT_US|AF_FORMAT_24BIT|AF_FORMAT_LE)
#define AF_FORMAT_U24_BE	(AF_FORMAT_I|AF_FORMAT_US|AF_FORMAT_24BIT|AF_FORMAT_BE)
#define AF_FORMAT_S24_LE	(AF_FORMAT_I|AF_FORMAT_SI|AF_FORMAT_24BIT|AF_FORMAT_LE)
#define AF_FORMAT_S24_BE	(AF_FORMAT_I|AF_FORMAT_SI|AF_FORMAT_24BIT|AF_FORMAT_BE)
#define AF_FORMAT_U32_LE	(AF_FORMAT_I|AF_FORMAT_US|AF_FORMAT_32BIT|AF_FORMAT_LE)
#define AF_FORMAT_U32_BE	(AF_FORMAT_I|AF_FORMAT_US|AF_FORMAT_32BIT|AF_FORMAT_BE)
#define AF_FORMAT_S32_LE	(AF_FORMAT_I|AF_FORMAT_SI|AF_FORMAT_32BIT|AF_FORMAT_LE)
#define AF_FORMAT_S32_BE	(AF_FORMAT_I|AF_FORMAT_SI|AF_FORMAT_32BIT|AF_FORMAT_BE)

#define AF_FORMAT_FLOAT_LE	(AF_FORMAT_F|AF_FORMAT_32BIT|AF_FORMAT_LE)
#define AF_FORMAT_FLOAT_BE	(AF_FORMAT_F|AF_FORMAT_32BIT|AF_FORMAT_BE)

#define AF_FORMAT_DOUBLE_LE     (AF_FORMAT_F|AF_FORMAT_64BIT|AF_FORMAT_LE)
#define AF_FORMAT_DOUBLE_BE     (AF_FORMAT_F|AF_FORMAT_64BIT|AF_FORMAT_BE)

#define AF_FORMAT_AC3_LE	(AF_FORMAT_AC3|AF_FORMAT_16BIT|AF_FORMAT_LE)
#define AF_FORMAT_AC3_BE	(AF_FORMAT_AC3|AF_FORMAT_16BIT|AF_FORMAT_BE)

#define AF_FORMAT_IEC61937_LE   (AF_FORMAT_IEC61937|AF_FORMAT_16BIT|AF_FORMAT_LE)
#define AF_FORMAT_IEC61937_BE   (AF_FORMAT_IEC61937|AF_FORMAT_16BIT|AF_FORMAT_BE)

#if BYTE_ORDER == BIG_ENDIAN
#define AF_FORMAT_U16_NE AF_FORMAT_U16_BE
#define AF_FORMAT_S16_NE AF_FORMAT_S16_BE
#define AF_FORMAT_U24_NE AF_FORMAT_U24_BE
#define AF_FORMAT_S24_NE AF_FORMAT_S24_BE
#define AF_FORMAT_U32_NE AF_FORMAT_U32_BE
#define AF_FORMAT_S32_NE AF_FORMAT_S32_BE
#define AF_FORMAT_FLOAT_NE AF_FORMAT_FLOAT_BE
#define AF_FORMAT_DOUBLE_NE AF_FORMAT_DOUBLE_BE
#define AF_FORMAT_AC3_NE AF_FORMAT_AC3_BE
#define AF_FORMAT_IEC61937_NE AF_FORMAT_IEC61937_BE
#else
#define AF_FORMAT_U16_NE AF_FORMAT_U16_LE
#define AF_FORMAT_S16_NE AF_FORMAT_S16_LE
#define AF_FORMAT_U24_NE AF_FORMAT_U24_LE
#define AF_FORMAT_S24_NE AF_FORMAT_S24_LE
#define AF_FORMAT_U32_NE AF_FORMAT_U32_LE
#define AF_FORMAT_S32_NE AF_FORMAT_S32_LE
#define AF_FORMAT_FLOAT_NE AF_FORMAT_FLOAT_LE
#define AF_FORMAT_DOUBLE_NE AF_FORMAT_DOUBLE_LE
#define AF_FORMAT_AC3_NE AF_FORMAT_AC3_LE
#define AF_FORMAT_IEC61937_NE AF_FORMAT_IEC61937_LE
#endif

#define AF_FORMAT_UNKNOWN (-1)

#define AF_FORMAT_IS_AC3(fmt) (((fmt) & AF_FORMAT_SPECIAL_MASK) == AF_FORMAT_AC3)
#define AF_FORMAT_IS_IEC61937(fmt) (AF_FORMAT_IS_AC3(fmt) || ((fmt) & AF_FORMAT_SPECIAL_MASK) == AF_FORMAT_IEC61937)

struct af_fmt_entry {
    const char *name;
    int format;
};

extern const struct af_fmt_entry af_fmtstr_table[];

int af_str2fmt_short(bstr str);
int af_fmt2bits(int format);
char* af_fmt2str(int format, char* str, int size);
const char* af_fmt2str_short(int format);

#endif /* MPLAYER_AF_FORMAT_H */
