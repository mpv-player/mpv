// -*- c-basic-offset: 8; indent-tabs-mode: t -*-
// vim:ts=8:sw=8:noet:ai:
/*
 * Copyright (C) 2006 Evgeniy Stepanov <eugeni.stepanov@gmail.com>
 *
 * This file is part of libass.
 *
 * libass is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * libass is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with libass; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
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

#ifdef CONFIG_FONTCONFIG
#include <fontconfig/fontconfig.h>
#include <fontconfig/fcfreetype.h>
#endif

struct fc_instance_s {
#ifdef CONFIG_FONTCONFIG
	FcConfig* config;
#endif
	char* family_default;
	char* path_default;
	int index_default;
};

#ifdef CONFIG_FONTCONFIG

// 4yo fontconfig does not have these.
// They are only needed for debug output, anyway.
#ifndef FC_FULLNAME
#define FC_FULLNAME "fullname"
#endif
#ifndef FC_EMBOLDEN
#define FC_EMBOLDEN "embolden"
#endif

/**
 * \brief Low-level font selection.
 * \param priv private data
 * \param family font family
 * \param bold font weight value
 * \param italic font slant value
 * \param index out: font index inside a file
 * \param code: the character that should be present in the font, can be 0
 * \return font file path
*/ 
static char* _select_font(fc_instance_t* priv, const char* family, unsigned bold, unsigned italic, int* index,
			  uint32_t code)
{
	FcBool rc;
	FcResult result;
	FcPattern *pat = NULL, *rpat = NULL;
	int r_index, r_slant, r_weight;
	FcChar8 *r_family, *r_style, *r_file, *r_fullname;
	FcBool r_outline, r_embolden;
	FcCharSet* r_charset;
	FcFontSet* fset = NULL;
	int curf;
	char* retval = NULL;
	int family_cnt;
	
	*index = 0;

	pat = FcPatternCreate();
	if (!pat)
		goto error;
	
	FcPatternAddString(pat, FC_FAMILY, (const FcChar8*)family);

	// In SSA/ASS fonts are sometimes referenced by their "full name",
	// which is usually a concatenation of family name and font
	// style (ex. Ottawa Bold). Full name is available from
	// FontConfig pattern element FC_FULLNAME, but it is never
	// used for font matching.
	// Therefore, I'm removing words from the end of the name one
	// by one, and adding shortened names to the pattern. It seems
	// that the first value (full name in this case) has
	// precedence in matching.
	// An alternative approach could be to reimplement FcFontSort
	// using FC_FULLNAME instead of FC_FAMILY.
	family_cnt = 1;
	{
		char* s = strdup(family);
		char* p = s + strlen(s);
		while (--p > s)
			if (*p == ' ' || *p == '-') {
				*p = '\0';
				FcPatternAddString(pat, FC_FAMILY, (const FcChar8*)s);
				++ family_cnt;
			}
		free(s);
	}
	FcPatternAddBool(pat, FC_OUTLINE, FcTrue);
	FcPatternAddInteger(pat, FC_SLANT, italic);
	FcPatternAddInteger(pat, FC_WEIGHT, bold);

	FcDefaultSubstitute(pat);
	
	rc = FcConfigSubstitute(priv->config, pat, FcMatchPattern);
	if (!rc)
		goto error;

	fset = FcFontSort(priv->config, pat, FcTrue, NULL, &result);
	if (!fset)
		goto error;

	for (curf = 0; curf < fset->nfont; ++curf) {
		FcPattern* curp = fset->fonts[curf];

		result = FcPatternGetBool(curp, FC_OUTLINE, 0, &r_outline);
		if (result != FcResultMatch)
			continue;
		if (r_outline != FcTrue)
			continue;
		if (!code)
			break;
		result = FcPatternGetCharSet(curp, FC_CHARSET, 0, &r_charset);
		if (result != FcResultMatch)
			continue;
		if (FcCharSetHasChar(r_charset, code))
			break;
	}

	if (curf >= fset->nfont)
		goto error;

#if (FC_VERSION >= 20297)
	// Remove all extra family names from original pattern.
	// After this, FcFontRenderPrepare will select the most relevant family
	// name in case there are more than one of them.
	for (; family_cnt > 1; --family_cnt)
		FcPatternRemove(pat, FC_FAMILY, family_cnt - 1);
#endif

	rpat = FcFontRenderPrepare(priv->config, pat, fset->fonts[curf]);
	if (!rpat)
		goto error;

	result = FcPatternGetInteger(rpat, FC_INDEX, 0, &r_index);
	if (result != FcResultMatch)
		goto error;
	*index = r_index;

	result = FcPatternGetString(rpat, FC_FILE, 0, &r_file);
	if (result != FcResultMatch)
		goto error;
	retval = strdup((const char*)r_file);

	result = FcPatternGetString(rpat, FC_FAMILY, 0, &r_family);
	if (result != FcResultMatch)
		r_family = NULL;

	result = FcPatternGetString(rpat, FC_FULLNAME, 0, &r_fullname);
	if (result != FcResultMatch)
		r_fullname = NULL;

	if (!(r_family && strcasecmp((const char*)r_family, family) == 0) &&
	    !(r_fullname && strcasecmp((const char*)r_fullname, family) == 0))
		mp_msg(MSGT_ASS, MSGL_WARN, MSGTR_LIBASS_SelectedFontFamilyIsNotTheRequestedOne,
		       (const char*)(r_fullname ? r_fullname : r_family), family);

	result = FcPatternGetString(rpat, FC_STYLE, 0, &r_style);
	if (result != FcResultMatch)
		r_style = NULL;

	result = FcPatternGetInteger(rpat, FC_SLANT, 0, &r_slant);
	if (result != FcResultMatch)
		r_slant = 0;

	result = FcPatternGetInteger(rpat, FC_WEIGHT, 0, &r_weight);
	if (result != FcResultMatch)
		r_weight = 0;

	result = FcPatternGetBool(rpat, FC_EMBOLDEN, 0, &r_embolden);
	if (result != FcResultMatch)
		r_embolden = 0;

	mp_msg(MSGT_ASS, MSGL_V, "[ass] Font info: family '%s', style '%s', fullname '%s',"
	       " slant %d, weight %d%s\n",
	       (const char*)r_family, (const char*)r_style, (const char*)r_fullname,
	       r_slant, r_weight, r_embolden ? ", embolden" : "");

 error:
	if (pat) FcPatternDestroy(pat);
	if (rpat) FcPatternDestroy(rpat);
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
 * \param code: the character that should be present in the font, can be 0
 * \return font file path
*/ 
char* fontconfig_select(fc_instance_t* priv, const char* family, unsigned bold, unsigned italic, int* index,
			uint32_t code)
{
	char* res = 0;
	if (!priv->config) {
		*index = priv->index_default;
		return priv->path_default;
	}
	if (family && *family)
		res = _select_font(priv, family, bold, italic, index, code);
	if (!res && priv->family_default) {
		res = _select_font(priv, priv->family_default, bold, italic, index, code);
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
		res = _select_font(priv, "Arial", bold, italic, index, code);
		if (res)
			mp_msg(MSGT_ASS, MSGL_WARN, MSGTR_LIBASS_UsingArialFontFamily, 
					family, bold, italic, res, *index);
	}
	if (res)
		mp_msg(MSGT_ASS, MSGL_V, "fontconfig_select: (%s, %d, %d) -> %s, %d\n", 
				family, bold, italic, res, *index);
	return res;
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
	FILE* fp = NULL;

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
	int face_index, num_faces = 1;

	for (face_index = 0; face_index < num_faces; ++face_index) {
		rc = FT_New_Memory_Face(ftlibrary, (unsigned char*)data, data_size, face_index, &face);
		if (rc) {
			mp_msg(MSGT_ASS, MSGL_WARN, MSGTR_LIBASS_ErrorOpeningMemoryFont, name);
			return;
		}
		num_faces = face->num_faces;

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
	}
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
fc_instance_t* fontconfig_init(ass_library_t* library, FT_Library ftlibrary, const char* family, const char* path, int fc)
{
	int rc;
	fc_instance_t* priv = calloc(1, sizeof(fc_instance_t));
	const char* dir = library->fonts_dir;
	int i;
	
	if (!fc) {
		mp_msg(MSGT_ASS, MSGL_WARN,
		       MSGTR_LIBASS_FontconfigDisabledDefaultFontWillBeUsed);
		goto exit;
	}

	rc = FcInit();
	assert(rc);

	priv->config = FcConfigGetCurrent();
	if (!priv->config) {
		mp_msg(MSGT_ASS, MSGL_FATAL, MSGTR_LIBASS_FcInitLoadConfigAndFontsFailed);
		goto exit;
	}

	for (i = 0; i < library->num_fontdata; ++i)
		process_fontdata(priv, library, ftlibrary, i);

	if (dir) {
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

					rc = FcDirScan(fcs, fss, NULL, FcConfigGetBlanks(priv->config),
						       (const FcChar8 *)dir, FcFalse);
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
	}

	priv->family_default = family ? strdup(family) : NULL;
exit:
	priv->path_default = path ? strdup(path) : NULL;
	priv->index_default = 0;

	return priv;
}

#else /* CONFIG_FONTCONFIG */

char* fontconfig_select(fc_instance_t* priv, const char* family, unsigned bold, unsigned italic, int* index,
			uint32_t code)
{
	*index = priv->index_default;
	return priv->path_default;
}

fc_instance_t* fontconfig_init(ass_library_t* library, FT_Library ftlibrary, const char* family, const char* path, int fc)
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


