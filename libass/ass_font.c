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

#include "config.h"

#include <inttypes.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_SYNTHESIS_H
#include FT_GLYPH_H

#include "ass_font.h"
#include "ass_fontconfig.h"
#include "mputils.h"

/**
 * Select Microfost Unicode CharMap, if the font has one.
 * Otherwise, let FreeType decide.
 */
static void charmap_magic(FT_Face face)
{
	int i;
	for (i = 0; i < face->num_charmaps; ++i) {
		FT_CharMap cmap = face->charmaps[i];
		unsigned pid = cmap->platform_id;
		unsigned eid = cmap->encoding_id;
		if (pid == 3 /*microsoft*/ && (eid == 1 /*unicode bmp*/ || eid == 10 /*full unicode*/)) {
			FT_Set_Charmap(face, cmap);
			break;
		}
	}
}

int ass_font_init(FT_Library ftlibrary, void* fc_priv, ass_font_t* font, ass_font_desc_t* desc)
{
	char* path;
	int index;
	FT_Face face;
	int error;
	
	path = fontconfig_select(fc_priv, desc->family, desc->bold, desc->italic, &index);
	
	error = FT_New_Face(ftlibrary, path, index, &face);
	if (error) {
		mp_msg(MSGT_ASS, MSGL_WARN, MSGTR_LIBASS_ErrorOpeningFont, path, index);
		return 1;
	}

	charmap_magic(face);
	
	font->path = strdup(path);
	font->index = index;
	font->face = face;
	font->desc.family = strdup(desc->family);
	font->desc.bold = desc->bold;
	font->desc.italic = desc->italic;

	font->m.xx = font->m.yy = (FT_Fixed)0x10000L;
	font->m.xy = font->m.yy = 0;
	font->v.x = font->v.y = 0;

	return 0;
}

void ass_font_set_transform(ass_font_t* font, FT_Matrix* m, FT_Vector* v)
{
	font->m.xx = m->xx;
	font->m.xy = m->xy;
	font->m.yx = m->yx;
	font->m.yy = m->yy;
	font->v.x = v->x;
	font->v.y = v->y;
	FT_Set_Transform(font->face, &font->m, &font->v);
}

void ass_font_set_size(ass_font_t* font, int size)
{
	font->size = size;
	FT_Set_Pixel_Sizes(font->face, 0, size);
}

FT_Glyph ass_font_get_glyph(void* fontconfig_priv, ass_font_t* font, uint32_t ch)
{
	int error;
	int index;
	FT_Glyph glyph;
	
	index = FT_Get_Char_Index(font->face, ch);
	error = FT_Load_Glyph(font->face, index, FT_LOAD_NO_BITMAP );
	if (error) {
		mp_msg(MSGT_ASS, MSGL_WARN, MSGTR_LIBASS_ErrorLoadingGlyph);
		return 0;
	}
	
#if (FREETYPE_MAJOR > 2) || \
    ((FREETYPE_MAJOR == 2) && (FREETYPE_MINOR >= 2)) || \
    ((FREETYPE_MAJOR == 2) && (FREETYPE_MINOR == 1) && (FREETYPE_PATCH >= 10))
// FreeType >= 2.1.10 required
	if (!(font->face->style_flags & FT_STYLE_FLAG_ITALIC) && 
			(font->desc.italic > 55)) {
		FT_GlyphSlot_Oblique(font->face->glyph);
	}
#endif
	error = FT_Get_Glyph(font->face->glyph, &glyph);
	if (error) {
		mp_msg(MSGT_ASS, MSGL_WARN, MSGTR_LIBASS_ErrorLoadingGlyph);
		return 0;
	}
	
	return glyph;
}

void ass_font_free(ass_font_t* font)
{
	if (font->face) FT_Done_Face(font->face);
	if (font->path) free(font->path);
	if (font->desc.family) free(font->desc.family);
}
