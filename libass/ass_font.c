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

#include "ass.h"
#include "ass_library.h"
#include "ass_font.h"
#include "ass_bitmap.h"
#include "ass_cache.h"
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
			return;
		}
	}

	if (!face->charmap) {
		if (face->num_charmaps == 0) {
			mp_msg(MSGT_ASS, MSGL_WARN, MSGTR_LIBASS_NoCharmaps);
			return;
		}
		mp_msg(MSGT_ASS, MSGL_WARN, MSGTR_LIBASS_NoCharmapAutodetected);
		FT_Set_Charmap(face, face->charmaps[0]);
		return;
	}
}

/**
 * \brief find a memory font by name
 */
static int find_font(ass_library_t* library, char* name)
{
	int i;
	for (i = 0; i < library->num_fontdata; ++i)
		if (strcasecmp(name, library->fontdata[i].name) == 0)
			return i;
	return -1;
}

/**
 * \brief Create a new ass_font_t according to "desc" argument
 */
ass_font_t* ass_font_new(ass_library_t* library, FT_Library ftlibrary, void* fc_priv, ass_font_desc_t* desc)
{
	char* path;
	int index;
	FT_Face face;
	int error;
	ass_font_t* font;
	int mem_idx;

	font = ass_font_cache_find(desc);
	if (font)
		return font;
	
	path = fontconfig_select(fc_priv, desc->family, desc->bold, desc->italic, &index);
	
	mem_idx = find_font(library, path);
	if (mem_idx >= 0) {
		error = FT_New_Memory_Face(ftlibrary, (unsigned char*)library->fontdata[mem_idx].data,
					   library->fontdata[mem_idx].size, 0, &face);
		if (error) {
			mp_msg(MSGT_ASS, MSGL_WARN, MSGTR_LIBASS_ErrorOpeningMemoryFont, path);
			return 0;
		}
	} else {
		error = FT_New_Face(ftlibrary, path, index, &face);
		if (error) {
			mp_msg(MSGT_ASS, MSGL_WARN, MSGTR_LIBASS_ErrorOpeningFont, path, index);
			return 0;
		}
	}

	charmap_magic(face);
	
	font = calloc(1, sizeof(ass_font_t));
	font->ftlibrary = ftlibrary;
	font->faces[0] = face;
	font->n_faces = 1;
	font->desc.family = strdup(desc->family);
	font->desc.bold = desc->bold;
	font->desc.italic = desc->italic;

	font->m.xx = font->m.yy = (FT_Fixed)0x10000L;
	font->m.xy = font->m.yy = 0;
	font->v.x = font->v.y = 0;
	font->size = 0;

#ifdef HAVE_FONTCONFIG
	font->charset = FcCharSetCreate();
#endif

	ass_font_cache_add(font);
	
	return font;
}

/**
 * \brief Set font transformation matrix and shift vector
 **/
void ass_font_set_transform(ass_font_t* font, FT_Matrix* m, FT_Vector* v)
{
	int i;
	font->m.xx = m->xx;
	font->m.xy = m->xy;
	font->m.yx = m->yx;
	font->m.yy = m->yy;
	font->v.x = v->x;
	font->v.y = v->y;
	for (i = 0; i < font->n_faces; ++i)
		FT_Set_Transform(font->faces[i], &font->m, &font->v);
}

/**
 * \brief Set font size
 **/
void ass_font_set_size(ass_font_t* font, int size)
{
	int i;
	if (font->size != size) {
		font->size = size;
		for (i = 0; i < font->n_faces; ++i)
			FT_Set_Pixel_Sizes(font->faces[i], 0, size);
	}
}

#ifdef HAVE_FONTCONFIG
/**
 * \brief Select a new FT_Face with the given character
 * The new face is added to the end of font->faces.
 **/
static void ass_font_reselect(void* fontconfig_priv, ass_font_t* font, uint32_t ch)
{
	char* path;
	int index;
	FT_Face face;
	int error;

	if (font->n_faces == ASS_FONT_MAX_FACES)
		return;
	
	path = fontconfig_select_with_charset(fontconfig_priv, font->desc.family, font->desc.bold,
					      font->desc.italic, &index, font->charset);

	error = FT_New_Face(font->ftlibrary, path, index, &face);
	if (error) {
		mp_msg(MSGT_ASS, MSGL_WARN, MSGTR_LIBASS_ErrorOpeningFont, path, index);
		return;
	}
	charmap_magic(face);

	error = FT_Get_Char_Index(face, ch);
	if (error == 0) { // the new font face is not better then the old one
		FT_Done_Face(face);
		return;
	}

	font->faces[font->n_faces++] = face;
	
	FT_Set_Transform(face, &font->m, &font->v);
	FT_Set_Pixel_Sizes(face, 0, font->size);
}
#endif

/**
 * \brief Get maximal font ascender and descender.
 * \param ch character code
 * The values are extracted from the font face that provides glyphs for the given character
 **/
void ass_font_get_asc_desc(ass_font_t* font, uint32_t ch, int* asc, int* desc)
{
	int i;
	for (i = 0; i < font->n_faces; ++i) {
		FT_Face face = font->faces[i];
		if (FT_Get_Char_Index(face, ch)) {
			int v, v2;
			v = face->size->metrics.ascender;
			v2 = FT_MulFix(face->bbox.yMax, face->size->metrics.y_scale);
			*asc = (v > v2 * 0.9) ? v : v2;
				
			v = - face->size->metrics.descender;
			v2 = - FT_MulFix(face->bbox.yMin, face->size->metrics.y_scale);
			*desc = (v > v2 * 0.9) ? v : v2;
			return;
		}
	}
	
	*asc = *desc = 0;
}

/**
 * \brief Get a glyph
 * \param ch character code
 **/
FT_Glyph ass_font_get_glyph(void* fontconfig_priv, ass_font_t* font, uint32_t ch)
{
	int error;
	int index = 0;
	int i;
	FT_Glyph glyph;
	FT_Face face = 0;

	if (ch < 0x20)
		return 0;
	if (font->n_faces == 0)
		return 0;

	for (i = 0; i < font->n_faces; ++i) {
		face = font->faces[i];
		index = FT_Get_Char_Index(face, ch);
		if (index)
			break;
	}

#ifdef HAVE_FONTCONFIG
	FcCharSetAddChar(font->charset, ch);
	if (index == 0) {
		mp_msg(MSGT_ASS, MSGL_INFO, MSGTR_LIBASS_GlyphNotFoundReselectingFont,
		       ch, font->desc.family, font->desc.bold, font->desc.italic);
		ass_font_reselect(fontconfig_priv, font, ch);
		face = font->faces[font->n_faces - 1];
		index = FT_Get_Char_Index(face, ch);
		if (index == 0) {
			mp_msg(MSGT_ASS, MSGL_ERR, MSGTR_LIBASS_GlyphNotFound,
			       ch, font->desc.family, font->desc.bold, font->desc.italic);
		}
	}
#endif

	error = FT_Load_Glyph(face, index, FT_LOAD_NO_BITMAP );
	if (error) {
		mp_msg(MSGT_ASS, MSGL_WARN, MSGTR_LIBASS_ErrorLoadingGlyph);
		return 0;
	}
	
#if (FREETYPE_MAJOR > 2) || \
    ((FREETYPE_MAJOR == 2) && (FREETYPE_MINOR >= 2)) || \
    ((FREETYPE_MAJOR == 2) && (FREETYPE_MINOR == 1) && (FREETYPE_PATCH >= 10))
// FreeType >= 2.1.10 required
	if (!(face->style_flags & FT_STYLE_FLAG_ITALIC) && 
			(font->desc.italic > 55)) {
		FT_GlyphSlot_Oblique(face->glyph);
	}
#endif
	error = FT_Get_Glyph(face->glyph, &glyph);
	if (error) {
		mp_msg(MSGT_ASS, MSGL_WARN, MSGTR_LIBASS_ErrorLoadingGlyph);
		return 0;
	}
	
	return glyph;
}

/**
 * \brief Get kerning for the pair of glyphs.
 **/
FT_Vector ass_font_get_kerning(ass_font_t* font, uint32_t c1, uint32_t c2)
{
	FT_Vector v = {0, 0};
	int i;

	for (i = 0; i < font->n_faces; ++i) {
		FT_Face face = font->faces[i];
		int i1 = FT_Get_Char_Index(face, c1);
		int i2 = FT_Get_Char_Index(face, c2);
		if (i1 && i2) {
			if (FT_HAS_KERNING(face))
				FT_Get_Kerning(face, i1, i2, FT_KERNING_DEFAULT, &v);
			return v;
		}
		if (i1 || i2) // these glyphs are from different font faces, no kerning information
			return v;
	}
	return v;
}

/**
 * \brief Deallocate ass_font_t
 **/
void ass_font_free(ass_font_t* font)
{
	int i;
	for (i = 0; i < font->n_faces; ++i)
		if (font->faces[i]) FT_Done_Face(font->faces[i]);
	if (font->desc.family) free(font->desc.family);
#ifdef HAVE_FONTCONFIG
	if (font->charset) FcCharSetDestroy(font->charset);
#endif
	free(font);
}
