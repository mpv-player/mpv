/*
 * straightforward (to be) optimized JPEG encoder for the YUV422 format
 * based on MJPEG code from FFmpeg
 *
 * For an excellent introduction to the JPEG format, see:
 * http://www.ece.purdue.edu/~bouman/grad-labs/lab8/pdf/lab.pdf
 *
 * Copyright (c) 2002, Rik Snel
 * parts from FFmpeg Copyright (c) 2000-2002 Fabrice Bellard
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

#ifndef MPLAYER_JPEG_ENC_H
#define MPLAYER_JPEG_ENC_H

typedef struct {
	struct MpegEncContext *s;
	int cheap_upsample;
	int bw;
	int y_ps;
	int u_ps;
	int v_ps;
	int y_rs;
	int u_rs;
	int v_rs;
} jpeg_enc_t;

jpeg_enc_t *jpeg_enc_init(int w, int h, int y_psize, int y_rsize, 
		int u_psize, int u_rsize, int v_psize, int v_rsize,
		int cu, int q, int b);

int jpeg_enc_frame(jpeg_enc_t *j, unsigned char *y_data, 
		unsigned char *u_data, unsigned char *v_data, char *bufr);

void jpeg_enc_uninit(jpeg_enc_t *j);

#endif /* MPLAYER_JPEG_ENC_H */
