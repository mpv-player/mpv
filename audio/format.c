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
    return (format & AF_FORMAT_BITS_MASK)+8;
//    return (((format & AF_FORMAT_BITS_MASK)>>3)+1) * 8;
#if 0
    switch(format & AF_FORMAT_BITS_MASK)
    {
	case AF_FORMAT_8BIT: return 8;
	case AF_FORMAT_16BIT: return 16;
	case AF_FORMAT_24BIT: return 24;
	case AF_FORMAT_32BIT: return 32;
	case AF_FORMAT_48BIT: return 48;
    }
#endif
    return -1;
}

int af_bits2fmt(int bits)
{
    return (bits/8 - 1) << 3;
}

/* Convert format to str input str is a buffer for the
   converted string, size is the size of the buffer */
char* af_fmt2str(int format, char* str, int size)
{
    const char *name = af_fmt2str_short(format);
    if (name) {
        snprintf(str, size, "%s", name);
    } else {
        snprintf(str, size, "%#x", format);
    }
    return str;
}

const struct af_fmt_entry af_fmtstr_table[] = {
    { "mpeg2", AF_FORMAT_MPEG2 },
    { "ac3le", AF_FORMAT_AC3_LE },
    { "ac3be", AF_FORMAT_AC3_BE },
    { "ac3ne", AF_FORMAT_AC3_NE },
    { "iec61937le", AF_FORMAT_IEC61937_LE },
    { "iec61937be", AF_FORMAT_IEC61937_BE },
    { "iec61937ne", AF_FORMAT_IEC61937_NE },

    { "u8", AF_FORMAT_U8 },
    { "s8", AF_FORMAT_S8 },
    { "u16le", AF_FORMAT_U16_LE },
    { "u16be", AF_FORMAT_U16_BE },
    { "u16ne", AF_FORMAT_U16_NE },
    { "s16le", AF_FORMAT_S16_LE },
    { "s16be", AF_FORMAT_S16_BE },
    { "s16ne", AF_FORMAT_S16_NE },
    { "u24le", AF_FORMAT_U24_LE },
    { "u24be", AF_FORMAT_U24_BE },
    { "u24ne", AF_FORMAT_U24_NE },
    { "s24le", AF_FORMAT_S24_LE },
    { "s24be", AF_FORMAT_S24_BE },
    { "s24ne", AF_FORMAT_S24_NE },
    { "u32le", AF_FORMAT_U32_LE },
    { "u32be", AF_FORMAT_U32_BE },
    { "u32ne", AF_FORMAT_U32_NE },
    { "s32le", AF_FORMAT_S32_LE },
    { "s32be", AF_FORMAT_S32_BE },
    { "s32ne", AF_FORMAT_S32_NE },
    { "floatle", AF_FORMAT_FLOAT_LE },
    { "floatbe", AF_FORMAT_FLOAT_BE },
    { "floatne", AF_FORMAT_FLOAT_NE },

    {0}
};

const char *af_fmt2str_short(int format)
{
    int i;

    for (i = 0; af_fmtstr_table[i].name; i++)
	if (af_fmtstr_table[i].format == format)
	    return af_fmtstr_table[i].name;

    return "??";
}

static bool af_fmt_valid(int format)
{
    return (format & AF_FORMAT_MASK) == format;
}

int af_str2fmt_short(bstr str)
{
    if (bstr_startswith0(str, "0x")) {
        bstr rest;
        int fmt = bstrtoll(str, &rest, 16);
        if (rest.len == 0 && af_fmt_valid(fmt))
            return fmt;
    }

    for (int i = 0; af_fmtstr_table[i].name; i++)
        if (!bstrcasecmp0(str, af_fmtstr_table[i].name))
            return af_fmtstr_table[i].format;

    return -1;
}
