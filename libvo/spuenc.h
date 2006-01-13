/*
 * subpic_encode.c - encodes a pixmap with RLE
 *
 * Copyright (C) 2000   Alejandro J. Cura <alecu@protocultura.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include <stdlib.h>
#define DATASIZE 53220


typedef struct {
	int x, y;
	unsigned int rgb[4];
	unsigned char* pixels;
} pixbuf;

typedef struct {
	unsigned char data[DATASIZE];
	int count;	/* the count of bytes written */
	int oddstart;
	int nibblewaiting;
} encodedata;

void pixbuf_encode_rle(int x, int y, int w, int h, char *inbuf, int stride, encodedata *ed);
void pixbuf_delete(pixbuf* pb);
