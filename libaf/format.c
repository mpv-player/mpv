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

#include "af.h"
#include "help_mp.h"

// Convert from string to format
int af_str2fmt(const char* str)
{
  int format=0;
  // Scan for endianness
  if(strstr(str,"be") || strstr(str,"BE"))
    format |= AF_FORMAT_BE;
  else if(strstr(str,"le") || strstr(str,"LE"))
    format |= AF_FORMAT_LE;
  else
    format |= AF_FORMAT_NE;

  // Scan for special formats
  if(strstr(str,"mulaw") || strstr(str,"MULAW")){
    format |= AF_FORMAT_MU_LAW; return format;
  }
  if(strstr(str,"alaw") || strstr(str,"ALAW")){
    format |= AF_FORMAT_A_LAW; return format;
  }
  if(strstr(str,"ac3") || strstr(str,"AC3")){
    format |= AF_FORMAT_AC3 | AF_FORMAT_16BIT; return format;
  }
  if(strstr(str,"mpeg2") || strstr(str,"MPEG2")){
    format |= AF_FORMAT_MPEG2; return format;
  }
  if(strstr(str,"imaadpcm") || strstr(str,"IMAADPCM")){
    format |= AF_FORMAT_IMA_ADPCM; return format;
  }

  // Scan for int/float
  if(strstr(str,"float") || strstr(str,"FLOAT")){
    format |= AF_FORMAT_F; return format;
  }
  else
    format |= AF_FORMAT_I;

  // Scan for signed/unsigned
  if(strstr(str,"unsigned") || strstr(str,"UNSIGNED"))
    format |= AF_FORMAT_US;
  else
    format |= AF_FORMAT_SI;

  return format;
}

int af_fmt2bits(int format)
{
    if (AF_FORMAT_IS_AC3(format)) return 16;
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
  int i=0;

  if (size < 1)
    return NULL;
  size--; // reserve one for terminating 0

  // Endianness
  if(AF_FORMAT_LE == (format & AF_FORMAT_END_MASK))
    i+=snprintf(str,size-i,"little-endian ");
  else
    i+=snprintf(str,size-i,"big-endian ");

  if(format & AF_FORMAT_SPECIAL_MASK){
    switch(format & AF_FORMAT_SPECIAL_MASK){
    case(AF_FORMAT_MU_LAW):
      i+=snprintf(&str[i],size-i,"mu-law "); break;
    case(AF_FORMAT_A_LAW):
      i+=snprintf(&str[i],size-i,"A-law "); break;
    case(AF_FORMAT_MPEG2):
      i+=snprintf(&str[i],size-i,"MPEG-2 "); break;
    case(AF_FORMAT_AC3):
      i+=snprintf(&str[i],size-i,"AC3 "); break;
    case(AF_FORMAT_IMA_ADPCM):
      i+=snprintf(&str[i],size-i,"IMA-ADPCM "); break;
    default:
      i+=snprintf(&str[i],size-i,MSGTR_AF_FORMAT_UnknownFormat);
    }
  }
  else{
    // Bits
    i+=snprintf(&str[i],size-i,"%d-bit ", af_fmt2bits(format));

    // Point
    if(AF_FORMAT_F == (format & AF_FORMAT_POINT_MASK))
      i+=snprintf(&str[i],size-i,"float ");
    else{
      // Sign
      if(AF_FORMAT_US == (format & AF_FORMAT_SIGN_MASK))
	i+=snprintf(&str[i],size-i,"unsigned ");
      else
	i+=snprintf(&str[i],size-i,"signed ");

      i+=snprintf(&str[i],size-i,"int ");
    }
  }
  // remove trailing space
  if (i > 0 && str[i - 1] == ' ')
    i--;
  str[i] = 0; // make sure it is 0 terminated.
  return str;
}

static struct {
    const char *name;
    const int format;
} af_fmtstr_table[] = {
    { "mulaw", AF_FORMAT_MU_LAW },
    { "alaw", AF_FORMAT_A_LAW },
    { "mpeg2", AF_FORMAT_MPEG2 },
    { "ac3le", AF_FORMAT_AC3_LE },
    { "ac3be", AF_FORMAT_AC3_BE },
    { "ac3ne", AF_FORMAT_AC3_NE },
    { "imaadpcm", AF_FORMAT_IMA_ADPCM },

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

    { NULL, 0 }
};

const char *af_fmt2str_short(int format)
{
    int i;

    for (i = 0; af_fmtstr_table[i].name; i++)
	if (af_fmtstr_table[i].format == format)
	    return af_fmtstr_table[i].name;

    return "??";
}

int af_str2fmt_short(const char* str)
{
    int i;

    for (i = 0; af_fmtstr_table[i].name; i++)
	if (!strcasecmp(str, af_fmtstr_table[i].name))
	    return af_fmtstr_table[i].format;

    return -1;
}
