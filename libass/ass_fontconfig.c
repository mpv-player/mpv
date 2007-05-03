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

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <inttypes.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#include "mputils.h"
#include "ass.h"
#include "ass_library.h"
#include "ass_fontconfig.h"

#ifdef HAVE_FONTCONFIG
#include <fontconfig/fontconfig.h>
#include <fontconfig/fcfreetype.h>
#endif

struct fc_instance_s {
#ifdef HAVE_FONTCONFIG
	FcConfig* config;
#endif
	char* family_default;
	char* path_default;
	int index_default;
};

#ifdef HAVE_FONTCONFIG
/**
 * \brief Low-level font selection.
 * \param priv private data
 * \param family font family
 * \param bold font weight value
 * \param italic font slant value
 * \param index out: font index inside a file
 * \param charset: contains the characters that should be present in the font, can be NULL
 * \return font file path
*/ 
static char* _select_font(fc_instance_t* priv, const char* family, unsigned bold, unsigned italic, int* index,
			  FcCharSet* charset)
{
	FcBool rc;
	FcResult result;
	FcPattern *pat = 0, *rpat;
	int val_i;
	FcChar8* val_s;
	FcBool val_b;
	FcCharSet* val_cs;
	FcFontSet* fset = 0;
	int curf, bestf, bestdiff = 0;
	char* retval = 0;
	
	*index = 0;

	pat = FcPatternCreate();
	if (!pat)
		goto error;
	
	FcPatternAddString(pat, FC_FAMILY, (const FcChar8*)family);
	FcPatternAddBool(pat, FC_OUTLINE, FcTrue);
	FcPatternAddInteger(pat, FC_SLANT, italic);
	FcPatternAddInteger(pat, FC_WEIGHT, bold);

	FcDefaultSubstitute(pat);
	
	rc = FcConfigSubstitute(priv->config, pat, FcMatchPattern);
	if (!rc)
		goto error;

	fset = FcFontSort(priv->config, pat, FcTrue, NULL, &result);

	bestf = -1;
	if (charset)
		bestdiff = FcCharSetCount(charset) + 1;
	for (curf = 0; curf < fset->nfont; ++curf) {
		rpat = fset->fonts[curf];
		
		result = FcPatternGetBool(rpat, FC_OUTLINE, 0, &val_b);
		if (result != FcResultMatch)
			continue;
		if (val_b != FcTrue)
			continue;

		if (charset) {
			int diff;
			result = FcPatternGetCharSet(rpat, FC_CHARSET, 0, &val_cs);
			if (result != FcResultMatch)
				continue;
			diff = FcCharSetSubtractCount(charset, val_cs);
			if (diff < bestdiff) {
				bestdiff = diff;
				bestf = curf;
			}
 			if (diff == 0)
				break;
		} else {
			bestf = curf;
			break;
		}
	}

	if (bestf < 0)
		goto error;

	rpat = fset->fonts[bestf];
	
	result = FcPatternGetInteger(rpat, FC_INDEX, 0, &val_i);
	if (result != FcResultMatch)
		goto error;
	*index = val_i;

	result = FcPatternGetString(rpat, FC_FAMILY, 0, &val_s);
	if (result != FcResultMatch)
		goto error;

	if (strcasecmp((const char*)val_s, family) != 0)
		mp_msg(MSGT_ASS, MSGL_WARN, MSGTR_LIBASS_SelectedFontFamilyIsNotTheRequestedOne,
				(const char*)val_s, family);

	result = FcPatternGetString(rpat, FC_FILE, 0, &val_s);
	if (result != FcResultMatch)
		goto error;
	
	retval = strdup((const char*)val_s);
 error:
	if (pat) FcPatternDestroy(pat);
	if (fset) FcFontSetDestroy(fset);
	return retval;
}

/**
 * \brief Find a font. Use default family or path if necessary.
 * \param priv_ private data
 * \param family font family
 * \param bold font weight value
 * \param italic font slant value
 * \param index out: font index inside a file
 * \param charset: contains the characters that should be present in the font, can be NULL
 * \return font file path
*/ 
char* fontconfig_select_with_charset(fc_instance_t* priv, const char* family, unsigned bold, unsigned italic, int* index,
			FcCharSet* charset)
{
	char* res = 0;
	if (family && *family)
		res = _select_font(priv, family, bold, italic, index, charset);
	if (!res && priv->family_default) {
		res = _select_font(priv, priv->family_default, bold, italic, index, charset);
		if (res)
			mp_msg(MSGT_ASS, MSGL_WARN, MSGTR_LIBASS_UsingDefaultFontFamily, 
					family, bold, italic, res, *index);
	}
	if (!res && priv->path_default) {
		res = priv->path_default;
		*index = priv->index_default;
		mp_msg(MSGT_ASS, MSGL_WARN, MSGTR_LIBASS_UsingDefaultFont, 
		       family, bold, italic, res, *index);
	}
	if (!res) {
		res = _select_font(priv, "Arial", bold, italic, index, charset);
		if (res)
			mp_msg(MSGT_ASS, MSGL_WARN, MSGTR_LIBASS_UsingArialFontFamily, 
					family, bold, italic, res, *index);
	}
	if (res)
		mp_msg(MSGT_ASS, MSGL_V, "fontconfig_select: (%s, %d, %d) -> %s, %d\n", 
				family, bold, italic, res, *index);
	return res;
}

char* fontconfig_select(fc_instance_t* priv, const char* family, unsigned bold, unsigned italic, int* index)
{
	return fontconfig_select_with_charset(priv, family, bold, italic, index, 0);
}

#if (FC_VERSION < 20402)
static char* validate_fname(char* name)
{
	char* fname;
	char* p;
	char* q;
	unsigned code;
	int sz = strlen(name);

	q = fname = malloc(sz + 1);
	p = name;
	while (*p) {
		code = utf8_get_char(&p);
		if (code == 0)
			break;
		if (	(code > 0x7F) ||
			(code == '\\') ||
			(code == '/') ||
			(code == ':') ||
			(code == '*') ||
			(code == '?') ||
			(code == '<') ||
			(code == '>') ||
			(code == '|') ||
			(code == 0))
		{
			*q++ = '_';
		} else {
			*q++ = code;
		}
		if (p - name > sz)
			break;
	}
	*q = 0;
	return fname;
}
#endif

/**
 * \brief Process memory font.
 * \param priv private data
 * \param library library object
 * \param ftlibrary freetype library object
 * \param idx index of the processed font in library->fontdata
 * With FontConfig >= 2.4.2, builds a font pattern in memory via FT_New_Memory_Face/FcFreeTypeQueryFace.
 * With older FontConfig versions, save the font to ~/.mplayer/fonts.
*/ 
static void process_fontdata(fc_instance_t* priv, ass_library_t* library, FT_Library ftlibrary, int idx)
{
	int rc;
	const char* name = library->fontdata[idx].name;
	const char* data = library->fontdata[idx].data;
	int data_size = library->fontdata[idx].size;

#if (FC_VERSION < 20402)
	struct stat st;
	char* fname;
	const char* fonts_dir = library->fonts_dir;
	char buf[1000];
	FILE* fp = 0;

	if (!fonts_dir)
		return;
	rc = stat(fonts_dir, &st);
	if (rc) {
		int res;
#ifndef __MINGW32__
		res = mkdir(fonts_dir, 0700);
#else
		res = mkdir(fonts_dir);
#endif
		if (res) {
			mp_msg(MSGT_ASS, MSGL_WARN, MSGTR_LIBASS_FailedToCreateDirectory, fonts_dir);
		}
	} else if (!S_ISDIR(st.st_mode)) {
		mp_msg(MSGT_ASS, MSGL_WARN, MSGTR_LIBASS_NotADirectory, fonts_dir);
	}
	
	fname = validate_fname((char*)name);

	snprintf(buf, 1000, "%s/%s", fonts_dir, fname);
	free(fname);

	fp = fopen(buf, "wb");
	if (!fp) return;

	fwrite(data, data_size, 1, fp);
	fclose(fp);

#else // (FC_VERSION >= 20402)
	FT_Face face;
	FcPattern* pattern;
	FcFontSet* fset;
	FcBool res;

	rc = FT_New_Memory_Face(ftlibrary, (unsigned char*)data, data_size, 0, &face);
	if (rc) {
		mp_msg(MSGT_ASS, MSGL_WARN, MSGTR_LIBASS_ErrorOpeningMemoryFont, name);
		return;
	}

	pattern = FcFreeTypeQueryFace(face, (unsigned char*)name, 0, FcConfigGetBlanks(priv->config));
	if (!pattern) {
		mp_msg(MSGT_ASS, MSGL_WARN, MSGTR_LIBASS_FunctionCallFailed, "FcFreeTypeQueryFace");
		FT_Done_Face(face);
		return;
	}

	fset = FcConfigGetFonts(priv->config, FcSetSystem); // somehow it failes when asked for FcSetApplication
	if (!fset) {
		mp_msg(MSGT_ASS, MSGL_WARN, MSGTR_LIBASS_FunctionCallFailed, "FcConfigGetFonts");
		FT_Done_Face(face);
		return;
	}

	res = FcFontSetAdd(fset, pattern);
	if (!res) {
		mp_msg(MSGT_ASS, MSGL_WARN, MSGTR_LIBASS_FunctionCallFailed, "FcFontSetAdd");
		FT_Done_Face(face);
		return;
	}

	FT_Done_Face(face);
#endif
}

/**
 * \brief Init fontconfig.
 * \param library libass library object
 * \param ftlibrary freetype library object
 * \param family default font family
 * \param path default font path
 * \return pointer to fontconfig private data
*/ 
fc_instance_t* fontconfig_init(ass_library_t* library, FT_Library ftlibrary, const char* family, const char* path)
{
	int rc;
	fc_instance_t* priv = calloc(1, sizeof(fc_instance_t));
	const char* dir = library->fonts_dir;
	int i;
	
	rc = FcInit();
	assert(rc);

	priv->config = FcConfigGetCurrent();
	if (!priv->config) {
		mp_msg(MSGT_ASS, MSGL_FATAL, MSGTR_LIBASS_FcInitLoadConfigAndFontsFailed);
		return 0;
	}

	for (i = 0; i < library->num_fontdata; ++i)
		process_fontdata(priv, library, ftlibrary, i);

	if (FcDirCacheValid((const FcChar8 *)dir) == FcFalse)
	{
		mp_msg(MSGT_ASS, MSGL_INFO, MSGTR_LIBASS_UpdatingFontCache);
		if (FcGetVersion() >= 20390 && FcGetVersion() < 20400)
			mp_msg(MSGT_ASS, MSGL_WARN,
			       MSGTR_LIBASS_BetaVersionsOfFontconfigAreNotSupported);
		// FontConfig >= 2.4.0 updates cache automatically in FcConfigAppFontAddDir()
		if (FcGetVersion() < 20390) {
			FcFontSet* fcs;
			FcStrSet* fss;
			fcs = FcFontSetCreate();
			fss = FcStrSetCreate();
			rc = FcStrSetAdd(fss, (const FcChar8*)dir);
			if (!rc) {
				mp_msg(MSGT_ASS, MSGL_WARN, MSGTR_LIBASS_FcStrSetAddFailed);
				goto ErrorFontCache;
			}

			rc = FcDirScan(fcs, fss, NULL, FcConfigGetBlanks(priv->config), (const FcChar8 *)dir, FcFalse);
			if (!rc) {
				mp_msg(MSGT_ASS, MSGL_WARN, MSGTR_LIBASS_FcDirScanFailed);
				goto ErrorFontCache;
			}

			rc = FcDirSave(fcs, fss, (const FcChar8 *)dir);
			if (!rc) {
				mp_msg(MSGT_ASS, MSGL_WARN, MSGTR_LIBASS_FcDirSave);
				goto ErrorFontCache;
			}
		ErrorFontCache:
			;
		}
	}

	rc = FcConfigAppFontAddDir(priv->config, (const FcChar8*)dir);
	if (!rc) {
		mp_msg(MSGT_ASS, MSGL_WARN, MSGTR_LIBASS_FcConfigAppFontAddDirFailed);
	}

	priv->family_default = family ? strdup(family) : 0;
	priv->path_default = path ? strdup(path) : 0;
	priv->index_default = 0;

	return priv;
}

#else // HAVE_FONTCONFIG

char* fontconfig_select(fc_instance_t* priv, const char* family, unsigned bold, unsigned italic, int* index)
{
	*index = priv->index_default;
	return priv->path_default;
}

fc_instance_t* fontconfig_init(ass_library_t* library, FT_Library ftlibrary, const char* family, const char* path)
{
	fc_instance_t* priv;

	mp_msg(MSGT_ASS, MSGL_WARN, MSGTR_LIBASS_FontconfigDisabledDefaultFontWillBeUsed);
	
	priv = calloc(1, sizeof(fc_instance_t));
	
	priv->path_default = strdup(path);
	priv->index_default = 0;
	return priv;
}

#endif

void fontconfig_done(fc_instance_t* priv)
{
	// don't call FcFini() here, library can still be used by some code
	if (priv && priv->path_default) free(priv->path_default);
	if (priv && priv->family_default) free(priv->family_default);
	if (priv) free(priv);
}


