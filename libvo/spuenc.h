/*
 * encode a pixmap with RLE
 *
 * Copyright (C) 2000   Alejandro J. Cura <alecu@protocultura.net>
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

#ifndef MPLAYER_SPUENC_H
#define MPLAYER_SPUENC_H

#include <stdlib.h>
#define DATASIZE 53220

typedef struct {
	unsigned char data[DATASIZE];
	int count;	/* the count of bytes written */
	int oddstart;
	int nibblewaiting;
} encodedata;

void pixbuf_encode_rle(int x, int y, int w, int h, char *inbuf, int stride, encodedata *ed);

#endif /* MPLAYER_SPUENC_H */
