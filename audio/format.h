/*
 * The sample format system used lin libaf is based on bitmasks.
 * The format definition only refers to the storage format,
 * not the resolution.
 *
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MPLAYER_AF_FORMAT_H
#define MPLAYER_AF_FORMAT_H

#include <stdbool.h>

#include "misc/bstr.h"

// Signed/unsigned
#define AF_FORMAT_SI            (0<<0) // Signed
#define AF_FORMAT_US            (1<<0) // Unsigned
#define AF_FORMAT_SIGN_MASK     (1<<0)

// Bits used
// Some code assumes they're sorted by size.
#define AF_FORMAT_8BIT          (0<<1)
#define AF_FORMAT_16BIT         (1<<1)
#define AF_FORMAT_24BIT         (2<<1)
#define AF_FORMAT_32BIT         (3<<1)
#define AF_FORMAT_64BIT         (4<<1)
#define AF_FORMAT_BITS_MASK     (7<<1)

// Fixed/floating point/special
#define AF_FORMAT_I             (1<<4) // Int
#define AF_FORMAT_F             (2<<4) // Foating point
#define AF_FORMAT_S             (4<<4) // special (IEC61937)
#define AF_FORMAT_TYPE_MASK     (7<<4)

// Interleaving (planar formats have data for each channel in separate planes)
#define AF_FORMAT_INTERLEAVED        (0<<7) // must be 0
#define AF_FORMAT_PLANAR             (1<<7)
#define AF_FORMAT_INTERLEAVING_MASK  (1<<7)

#define AF_FORMAT_S_CODEC(n)    ((n)<<8)
#define AF_FORMAT_S_CODEC_MASK  (15 <<8) // 16 codecs max.

#define AF_FORMAT_MASK          ((1<<12)-1)

#define AF_INTP (AF_FORMAT_I|AF_FORMAT_PLANAR)
#define AF_FLTP (AF_FORMAT_F|AF_FORMAT_PLANAR)
#define AF_FORMAT_S_(n) (AF_FORMAT_S_CODEC(n)|AF_FORMAT_S|AF_FORMAT_16BIT)

// actual sample formats
enum af_format {
    AF_FORMAT_UNKNOWN   = 0,

    AF_FORMAT_U8        = AF_FORMAT_I|AF_FORMAT_US|AF_FORMAT_8BIT,
    AF_FORMAT_S8        = AF_FORMAT_I|AF_FORMAT_SI|AF_FORMAT_8BIT,
    AF_FORMAT_U16       = AF_FORMAT_I|AF_FORMAT_US|AF_FORMAT_16BIT,
    AF_FORMAT_S16       = AF_FORMAT_I|AF_FORMAT_SI|AF_FORMAT_16BIT,
    AF_FORMAT_U24       = AF_FORMAT_I|AF_FORMAT_US|AF_FORMAT_24BIT,
    AF_FORMAT_S24       = AF_FORMAT_I|AF_FORMAT_SI|AF_FORMAT_24BIT,
    AF_FORMAT_U32       = AF_FORMAT_I|AF_FORMAT_US|AF_FORMAT_32BIT,
    AF_FORMAT_S32       = AF_FORMAT_I|AF_FORMAT_SI|AF_FORMAT_32BIT,

    AF_FORMAT_FLOAT     = AF_FORMAT_F|AF_FORMAT_32BIT,
    AF_FORMAT_DOUBLE    = AF_FORMAT_F|AF_FORMAT_64BIT,

    // Planar variants
    AF_FORMAT_U8P       = AF_INTP|AF_FORMAT_US|AF_FORMAT_8BIT,
    AF_FORMAT_S16P      = AF_INTP|AF_FORMAT_SI|AF_FORMAT_16BIT,
    AF_FORMAT_S32P      = AF_INTP|AF_FORMAT_SI|AF_FORMAT_32BIT,
    AF_FORMAT_FLOATP    = AF_FLTP|AF_FORMAT_32BIT,
    AF_FORMAT_DOUBLEP   = AF_FLTP|AF_FORMAT_64BIT,

    // All of these use IEC61937 framing, and otherwise pretend to be like PCM.
    AF_FORMAT_S_AAC     = AF_FORMAT_S_(0),
    AF_FORMAT_S_AC3     = AF_FORMAT_S_(1),
    AF_FORMAT_S_DTS     = AF_FORMAT_S_(2),
    AF_FORMAT_S_DTSHD   = AF_FORMAT_S_(3),
    AF_FORMAT_S_EAC3    = AF_FORMAT_S_(4),
    AF_FORMAT_S_MP3     = AF_FORMAT_S_(5),
    AF_FORMAT_S_TRUEHD  = AF_FORMAT_S_(6),
};

#define AF_FORMAT_IS_IEC61937(f) (((f) & AF_FORMAT_TYPE_MASK) == AF_FORMAT_S)
#define AF_FORMAT_IS_SPECIAL(f) AF_FORMAT_IS_IEC61937(f)
#define AF_FORMAT_IS_FLOAT(f) (!!((f) & AF_FORMAT_F))
// false for interleaved and AF_FORMAT_UNKNOWN
#define AF_FORMAT_IS_PLANAR(f) (!!((f) & AF_FORMAT_PLANAR))

struct af_fmt_entry {
    const char *name;
    int format;
};

extern const struct af_fmt_entry af_fmtstr_table[];

int af_str2fmt_short(bstr str);
const char *af_fmt_to_str(int format);

int af_fmt2bps(int format);
int af_fmt2bits(int format);
int af_fmt_change_bits(int format, int bits);

int af_fmt_to_planar(int format);
int af_fmt_from_planar(int format);

// Amount of bytes that contain audio of the given duration, aligned to frames.
int af_fmt_seconds_to_bytes(int format, float seconds, int channels, int samplerate);

bool af_fmt_is_valid(int format);

void af_fill_silence(void *dst, size_t bytes, int format);

int af_format_conversion_score(int dst_format, int src_format);

int af_format_sample_alignment(int format);

#endif /* MPLAYER_AF_FORMAT_H */
