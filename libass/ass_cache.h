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

#ifndef __ASS_CACHE_H__
#define __ASS_CACHE_H__

void ass_font_cache_init(void);
ass_font_t* ass_font_cache_find(ass_font_desc_t* desc);
void ass_font_cache_add(ass_font_t* font);
void ass_font_cache_done(void);


// describes a glyph; glyphs with equivalents structs are considered identical
typedef struct glyph_hash_key_s {
	char bitmap; // bool : true = bitmap, false = outline
	ass_font_t* font;
	int size; // font size
	uint32_t ch; // character code
	unsigned outline; // border width, 16.16 fixed point value
	int bold, italic;
	char be; // blur edges

	// the following affects bitmap glyphs only
	unsigned scale_x, scale_y; // 16.16
	int frx, fry, frz; // signed 16.16
	
	FT_Vector advance; // subpixel shift vector
} glyph_hash_key_t;

typedef struct glyph_hash_val_s {
	bitmap_t* bm; // the actual glyph bitmaps
	bitmap_t* bm_o;
	bitmap_t* bm_s;
	FT_BBox bbox_scaled; // bbox after scaling, but before rotation
	FT_Vector advance; // 26.6, advance distance to the next glyph in line
} glyph_hash_val_t;

void ass_glyph_cache_init(void);
void cache_add_glyph(glyph_hash_key_t* key, glyph_hash_val_t* val);
glyph_hash_val_t* cache_find_glyph(glyph_hash_key_t* key);
void ass_glyph_cache_reset(void);
void ass_glyph_cache_done(void);

#endif

