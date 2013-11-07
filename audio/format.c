/*
 * Copyright (C) 2005 Alex Beregszaszi
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <limits.h>

#include "audio/filter/af.h"

int af_fmt2bits(int format)
{
    if (AF_FORMAT_IS_AC3(format)) return 16;
    if (format == AF_FORMAT_UNKNOWN)
        return 0;
    switch (format & AF_FORMAT_BITS_MASK) {
    case AF_FORMAT_8BIT:  return 8;
    case AF_FORMAT_16BIT: return 16;
    case AF_FORMAT_24BIT: return 24;
    case AF_FORMAT_32BIT: return 32;
    case AF_FORMAT_64BIT: return 64;
    }
    return 0;
}

static int bits_to_mask(int bits)
{
    switch (bits) {
    case 8:  return AF_FORMAT_8BIT;
    case 16: return AF_FORMAT_16BIT;
    case 24: return AF_FORMAT_24BIT;
    case 32: return AF_FORMAT_32BIT;
    case 64: return AF_FORMAT_64BIT;
    }
    return 0;
}

int af_fmt_change_bits(int format, int bits)
{
    if (!af_fmt_is_valid(format) || (format & AF_FORMAT_SPECIAL_MASK))
        return 0;
    int mask = bits_to_mask(bits);
    format = (format & ~AF_FORMAT_BITS_MASK) | mask;
    return af_fmt_is_valid(format) ? format : 0;
}

#define FMT(string, id)                                 \
    {string,            id},

#define FMT_ENDIAN(string, id)                          \
    {string,            id},                            \
    {string "ne",       id},                            \
    {string "le",       MP_CONCAT(id, _LE)},            \
    {string "be",       MP_CONCAT(id, _BE)},            \

const struct af_fmt_entry af_fmtstr_table[] = {
    FMT("mpeg2",                AF_FORMAT_MPEG2)
    FMT_ENDIAN("ac3",           AF_FORMAT_AC3)
    FMT_ENDIAN("iec61937",      AF_FORMAT_IEC61937)

    FMT("u8",                   AF_FORMAT_U8)
    FMT("s8",                   AF_FORMAT_S8)
    FMT_ENDIAN("u16",           AF_FORMAT_U16)
    FMT_ENDIAN("s16",           AF_FORMAT_S16)
    FMT_ENDIAN("u24",           AF_FORMAT_U24)
    FMT_ENDIAN("s24",           AF_FORMAT_S24)
    FMT_ENDIAN("u32",           AF_FORMAT_U32)
    FMT_ENDIAN("s32",           AF_FORMAT_S32)
    FMT_ENDIAN("float",         AF_FORMAT_FLOAT)
    FMT_ENDIAN("double",        AF_FORMAT_DOUBLE)

    {0}
};

bool af_fmt_is_valid(int format)
{
    for (int i = 0; af_fmtstr_table[i].name; i++) {
        if (af_fmtstr_table[i].format == format)
            return true;
    }
    return false;
}

const char *af_fmt_to_str(int format)
{
    for (int i = 0; af_fmtstr_table[i].name; i++) {
        if (af_fmtstr_table[i].format == format)
            return af_fmtstr_table[i].name;
    }

    return "??";
}

int af_fmt_seconds_to_bytes(int format, float seconds, int channels, int samplerate)
{
    int bps      = (af_fmt2bits(format) / 8);
    int framelen = channels * bps;
    int bytes    = seconds  * bps * samplerate;
    if (bytes % framelen)
        bytes += framelen - (bytes % framelen);
    return bytes;
}

int af_str2fmt_short(bstr str)
{
    for (int i = 0; af_fmtstr_table[i].name; i++) {
        if (!bstrcasecmp0(str, af_fmtstr_table[i].name))
            return af_fmtstr_table[i].format;
    }
    return 0;
}
