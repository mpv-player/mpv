/*  ----------------------------------------------------------------------

    Copyright (C) 2001  Juha Yrjölä  (jyrjola@cc.hut.fi)

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
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    ---------------------------------------------------------------------- */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "ac3-iec958.h"

struct frmsize_s
{
        unsigned short bit_rate;
        unsigned short frm_size[3];
};
                
static const struct frmsize_s frmsizecod_tbl[64] =
{
        { 32  ,{64   ,69   ,96   } },
        { 32  ,{64   ,70   ,96   } },
        { 40  ,{80   ,87   ,120  } },
        { 40  ,{80   ,88   ,120  } },
        { 48  ,{96   ,104  ,144  } },
        { 48  ,{96   ,105  ,144  } },
        { 56  ,{112  ,121  ,168  } },
        { 56  ,{112  ,122  ,168  } },
        { 64  ,{128  ,139  ,192  } },
        { 64  ,{128  ,140  ,192  } },
        { 80  ,{160  ,174  ,240  } },
        { 80  ,{160  ,175  ,240  } },
        { 96  ,{192  ,208  ,288  } },
        { 96  ,{192  ,209  ,288  } },
        { 112 ,{224  ,243  ,336  } },
        { 112 ,{224  ,244  ,336  } },
        { 128 ,{256  ,278  ,384  } },
        { 128 ,{256  ,279  ,384  } },
        { 160 ,{320  ,348  ,480  } },
        { 160 ,{320  ,349  ,480  } },
        { 192 ,{384  ,417  ,576  } },
        { 192 ,{384  ,418  ,576  } },
        { 224 ,{448  ,487  ,672  } },
        { 224 ,{448  ,488  ,672  } },
        { 256 ,{512  ,557  ,768  } },
        { 256 ,{512  ,558  ,768  } },
        { 320 ,{640  ,696  ,960  } },
        { 320 ,{640  ,697  ,960  } },
        { 384 ,{768  ,835  ,1152 } },
        { 384 ,{768  ,836  ,1152 } },
        { 448 ,{896  ,975  ,1344 } },
        { 448 ,{896  ,976  ,1344 } },
        { 512 ,{1024 ,1114 ,1536 } },
        { 512 ,{1024 ,1115 ,1536 } },
        { 576 ,{1152 ,1253 ,1728 } },
        { 576 ,{1152 ,1254 ,1728 } },
        { 640 ,{1280 ,1393 ,1920 } },
        { 640 ,{1280 ,1394 ,1920 } }
};

struct syncframe {
  struct syncinfo {
    unsigned char syncword[2];
    unsigned char crc1[2];
    unsigned char code;
  } syncinfo;
  struct bsi {
    unsigned char bsidmod;
    unsigned char acmod;
  } bsi;
}; 

int ac3_iec958_build_burst(int length, int data_type, int big_endian, unsigned char * data, unsigned char * out)
{
	const char sync[6] = { 0x72, 0xF8, 0x1F, 0x4E, 0x00, 0x00 };

	memcpy(out, sync, 6);
	if (length)
		out[4] = data_type; /* & 0x1F; */
	else
		out[4] = 0;
	out[6] = (length << 3) & 0xFF;
	out[7] = (length >> 5) & 0xFF;
	if (big_endian)
		swab(data, out + 8, length);
	else
		memcpy(out + 8, data, length);
	memset(out + 8 + length, 0, 6144 - 8 - length);
	return 6144;
}

int ac3_iec958_parse_syncinfo(unsigned char *buf, int size, struct hwac3info *ai, int *skipped)
{
	int samplerates[4] = { 48000, 44100, 32000, -1 };
	unsigned short sync = 0;
	unsigned char *ptr = buf;
	int fscod, frmsizecod;
	struct syncframe *sf;
	
	sync = buf[0] << 8;
	sync |= buf[1];
	ptr = buf + 2;
	*skipped = 0;
	while (sync != 0xb77 && *skipped < size - 8) {
		sync <<= 8;
		sync |= *ptr;
		ptr++;
		*skipped += 1;
	}
	if (sync != 0xb77)
		return -1;
	ptr -= 2;
	sf = (struct syncframe *) ptr;
	fscod = (sf->syncinfo.code >> 6) & 0x03;
	ai->samplerate = samplerates[fscod];
	if (ai->samplerate == -1)
		return -1;
	frmsizecod = sf->syncinfo.code & 0x3f;
	ai->framesize = 2 * frmsizecod_tbl[frmsizecod].frm_size[fscod];
	ai->bitrate = frmsizecod_tbl[frmsizecod].bit_rate;
	if (((sf->bsi.bsidmod >> 3) & 0x1f) != 0x08)
		return -1;
	ai->bsmod = sf->bsi.bsidmod & 0x7;

	return 0;
}
