// -*- c-basic-offset: 8; indent-tabs-mode: t -*-
// vim:ts=8:sw=8:noet:ai:
/*
  Copyright (C) 2006 Evgeniy Stepanov <eugeni.stepanov@gmail.com>

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
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
*/

#ifndef __ASS_BITMAP_H__
#define __ASS_BITMAP_H__

typedef struct ass_synth_priv_s ass_synth_priv_t;

ass_synth_priv_t* ass_synth_init(void);
void ass_synth_done(ass_synth_priv_t* priv);

typedef struct bitmap_s {
	int left, top;
	int w, h; // width, height
	unsigned char* buffer; // w x h buffer
} bitmap_t;

/**
 * \brief perform glyph rendering
 * \param glyph original glyph
 * \param outline_glyph "border" glyph, produced from original by FreeType's glyph stroker
 * \param bm_g out: pointer to the bitmap of original glyph is returned here
 * \param bm_o out: pointer to the bitmap of outline (border) glyph is returned here
 * \param bm_g out: pointer to the bitmap of glyph shadow is returned here
 * \param be 1 = produces blurred bitmaps, 0 = normal bitmaps
 */
int glyph_to_bitmap(ass_synth_priv_t* priv, FT_Glyph glyph, FT_Glyph outline_glyph, bitmap_t** bm_g, bitmap_t** bm_o, bitmap_t** bm_s, int be);

void ass_free_bitmap(bitmap_t* bm);

#endif

