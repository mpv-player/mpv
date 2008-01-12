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
#include FT_TRUETYPE_TABLES_H

#include "ass.h"
#include "ass_library.h"
#include "ass_font.h"
#include "ass_bitmap.h"
#include "ass_cache.h"
#include "ass_fontconfig.h"
#include "ass_utils.h"
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

static void update_transform(ass_font_t* font)
{
	int i;
	FT_Matrix m;
	m.xx = double_to_d16(font->scale_x);
	m.yy = double_to_d16(font->scale_y);
	m.xy = m.yx = 0;
	for (i = 0; i < font->n_faces; ++i)
		FT_Set_Transform(font->faces[i], &m, &font->v);
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

static void face_set_size(FT_Face face, double size);

static void buggy_font_workaround(FT_Face face)
{
	// Some fonts have zero Ascender/Descender fields in 'hhea' table.
	// In this case, get the information from 'os2' table or, as
	// a last resort, from face.bbox.
	if (face->ascender + face->descender == 0 || face->height == 0) {
		TT_OS2 *os2 = FT_Get_Sfnt_Table(face, ft_sfnt_os2);
		if (os2) {
			face->ascender = os2->sTypoAscender;
			face->descender = os2->sTypoDescender;
			face->height = face->ascender - face->descender;
		} else {
			face->ascender = face->bbox.yMax;
			face->descender = face->bbox.yMin;
			face->height = face->ascender - face->descender;
		}
	}
}

/**
 * \brief Select a face with the given charcode and add it to ass_font_t
 * \return index of the new face in font->faces, -1 if failed
 */
static int add_face(void* fc_priv, ass_font_t* font, uint32_t ch)
{
	char* path;
	int index;
	FT_Face face;
	int error;
	int mem_idx;
	
	if (font->n_faces == ASS_FONT_MAX_FACES)
		return -1;
	
	path = fontconfig_select(fc_priv, font->desc.family, font->desc.bold,
					      font->desc.italic, &index, ch);

	mem_idx = find_font(font->library, path);
	if (mem_idx >= 0) {
		error = FT_New_Memory_Face(font->ftlibrary, (unsigned char*)font->library->fontdata[mem_idx].data,
					   font->library->fontdata[mem_idx].size, 0, &face);
		if (error) {
			mp_msg(MSGT_ASS, MSGL_WARN, MSGTR_LIBASS_ErrorOpeningMemoryFont, path);
			return -1;
		}
	} else {
		error = FT_New_Face(font->ftlibrary, path, index, &face);
		if (error) {
			mp_msg(MSGT_ASS, MSGL_WARN, MSGTR_LIBASS_ErrorOpeningFont, path, index);
			return -1;
		}
	}
	charmap_magic(face);
	buggy_font_workaround(face);
	
	font->faces[font->n_faces++] = face;
	update_transform(font);
	face_set_size(face, font->size);
	return font->n_faces - 1;
}

/**
 * \brief Create a new ass_font_t according to "desc" argument
 */
ass_font_t* ass_font_new(ass_library_t* library, FT_Library ftlibrary, void* fc_priv, ass_font_desc_t* desc)
{
	int error;
	ass_font_t* fontp;
	ass_font_t font;

	fontp = ass_font_cache_find(desc);
	if (fontp)
		return fontp;
	
	font.library = library;
	font.ftlibrary = ftlibrary;
	font.n_faces = 0;
	font.desc.family = strdup(desc->family);
	font.desc.bold = desc->bold;
	font.desc.italic = desc->italic;

	font.scale_x = font.scale_y = 1.;
	font.v.x = font.v.y = 0;
	font.size = 0.;

	error = add_face(fc_priv, &font, 0);
	if (error == -1) {
		free(font.desc.family);
		return 0;
	} else
		return ass_font_cache_add(&font);
}

/**
 * \brief Set font transformation matrix and shift vector
 **/
void ass_font_set_transform(ass_font_t* font, double scale_x, double scale_y, FT_Vector* v)
{
	font->scale_x = scale_x;
	font->scale_y = scale_y;
	font->v.x = v->x;
	font->v.y = v->y;
	update_transform(font);
}

static void face_set_size(FT_Face face, double size)
{
#if (FREETYPE_MAJOR > 2) || ((FREETYPE_MAJOR == 2) && (FREETYPE_MINOR > 1))
	TT_HoriHeader *hori = FT_Get_Sfnt_Table(face, ft_sfnt_hhea);
	TT_OS2 *os2 = FT_Get_Sfnt_Table(face, ft_sfnt_os2);
	double mscale = 1.;
	FT_Size_RequestRec rq;
	FT_Size_Metrics *m = &face->size->metrics;
	// VSFilter uses metrics from TrueType OS/2 table
	// The idea was borrowed from asa (http://asa.diac24.net)
	if (hori && os2) {
		int hori_height = hori->Ascender - hori->Descender;
		int os2_height = os2->usWinAscent + os2->usWinDescent;
		if (hori_height && os2_height)
			mscale = (double)hori_height / os2_height;
	}
	memset(&rq, 0, sizeof(rq));
	rq.type = FT_SIZE_REQUEST_TYPE_REAL_DIM;
	rq.width = 0;
	rq.height = double_to_d6(size * mscale);
	rq.horiResolution = rq.vertResolution = 0;
	FT_Request_Size(face, &rq);
	m->ascender /= mscale;
	m->descender /= mscale;
	m->height /= mscale;
#else
	FT_Set_Char_Size(face, 0, double_to_d6(size), 0, 0);
#endif
}

/**
 * \brief Set font size
 **/
void ass_font_set_size(ass_font_t* font, double size)
{
	int i;
	if (font->size != size) {
		font->size = size;
		for (i = 0; i < font->n_faces; ++i)
			face_set_size(font->faces[i], size);
	}
}

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
			*asc = face->size->metrics.ascender;
			*desc = - face->size->metrics.descender;
			return;
		}
	}
	
	*asc = *desc = 0;
}

/**
 * \brief Get a glyph
 * \param ch character code
 **/
FT_Glyph ass_font_get_glyph(void* fontconfig_priv, ass_font_t* font, uint32_t ch, ass_hinting_t hinting)
{
	int error;
	int index = 0;
	int i;
	FT_Glyph glyph;
	FT_Face face = 0;
	int flags = 0;

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
	if (index == 0) {
		int face_idx;
		mp_msg(MSGT_ASS, MSGL_INFO, MSGTR_LIBASS_GlyphNotFoundReselectingFont,
		       ch, font->desc.family, font->desc.bold, font->desc.italic);
		face_idx = add_face(fontconfig_priv, font, ch);
		if (face_idx >= 0) {
			face = font->faces[face_idx];
			index = FT_Get_Char_Index(face, ch);
			if (index == 0) {
				mp_msg(MSGT_ASS, MSGL_ERR, MSGTR_LIBASS_GlyphNotFound,
				       ch, font->desc.family, font->desc.bold, font->desc.italic);
			}
		}
	}
#endif

	switch (hinting) {
	case ASS_HINTING_NONE: flags = FT_LOAD_NO_HINTING; break;
	case ASS_HINTING_LIGHT: flags = FT_LOAD_FORCE_AUTOHINT | FT_LOAD_TARGET_LIGHT; break;
	case ASS_HINTING_NORMAL: flags = FT_LOAD_FORCE_AUTOHINT; break;
	case ASS_HINTING_NATIVE: flags = 0; break;
	}
	
	error = FT_Load_Glyph(face, index, FT_LOAD_NO_BITMAP | flags);
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
	free(font);
}
