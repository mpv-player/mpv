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

#ifndef __ASS_FONT_H__
#define __ASS_FONT_H__

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_STROKER_H
#include FT_GLYPH_H

#include "ass_bitmap.h"
#include "ass_cache.h"

ass_font_t* ass_font_new(FT_Library ftlibrary, void* fc_priv, ass_font_desc_t* desc);
void ass_font_set_transform(ass_font_t* font, FT_Matrix* m, FT_Vector* v);
void ass_font_set_size(ass_font_t* font, int size);
FT_Glyph ass_font_get_glyph(void* fontconfig_priv, ass_font_t* font, uint32_t ch);
void ass_font_free(ass_font_t* font);

#endif
