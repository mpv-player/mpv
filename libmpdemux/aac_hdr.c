/*
 * Copyright (C) 2005 Nico Sabbi
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

#include <stdint.h>
#include "aac_hdr.h"

/// \param srate (out) sample rate
/// \param num (out) number of audio frames in this ADTS frame
/// \return size of the ADTS frame in bytes
/// aac_parse_frames needs a buffer at least 8 bytes long
int aac_parse_frame(uint8_t *buf, int *srate, int *num)
{
	int i = 0, sr, fl = 0, id;
	static int srates[] = {96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000, 0, 0, 0};

	if((buf[i] != 0xFF) || ((buf[i+1] & 0xF6) != 0xF0))
		return 0;

	id = (buf[i+1] >> 3) & 0x01;	//id=1 mpeg2, 0: mpeg4
	sr = (buf[i+2] >> 2)  & 0x0F;
	if(sr > 11)
		return 0;
	*srate = srates[sr];

	fl = ((buf[i+3] & 0x03) << 11) | (buf[i+4] << 3) | ((buf[i+5] >> 5) & 0x07);
	*num = (buf[i+6] & 0x02) + 1;

	return fl;
}
