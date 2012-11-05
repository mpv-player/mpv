/*
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

#ifndef MPLAYER_AVIPRINT_H
#define MPLAYER_AVIPRINT_H

#include "ms_hdr.h"
#include "aviheader.h"

void print_avih_flags(MainAVIHeader *h, int verbose_level);
void print_avih(MainAVIHeader *h, int verbose_level);
void print_strh(AVIStreamHeader *h, int verbose_level);
void print_wave_header(WAVEFORMATEX *h, int verbose_level);
void print_video_header(BITMAPINFOHEADER *h, int verbose_level);
void print_vprp(VideoPropHeader *vprp, int verbose_level);
void print_index(AVIINDEXENTRY *idx, int idx_size, int verbose_level);
void print_avistdindex_chunk(avistdindex_chunk *h, int verbose_level);
void print_avisuperindex_chunk(avisuperindex_chunk *h, int verbose_level);

#endif /* MPLAYER_AVIPRINT_H */
