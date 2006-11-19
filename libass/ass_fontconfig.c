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

#include "mputils.h"
#include "ass_fontconfig.h"

#ifdef HAVE_FONTCONFIG
#include <fontconfig/fontconfig.h>
#endif

struct fc_instance_s {
#ifdef HAVE_FONTCONFIG
	FcConfig* config;
#endif
	char* family_default;
	char* path_default;
	int index_default;
};

extern int no_more_font_messages;

#ifdef HAVE_FONTCONFIG
/**
 * \brief Low-level font selection.
 * \param priv private data
 * \param family font family
 * \param bold font weight value
 * \param italic font slant value
 * \param index out: font index inside a file
 * \return font file path
*/ 
static char* _select_font(fc_instance_t* priv, const char* family, unsigned bold, unsigned italic, int* index)
{
	FcBool rc;
	FcResult result;
	FcPattern *pat, *rpat;
	int val_i;
	FcChar8* val_s;
	FcBool val_b;
	
	*index = 0;

	pat = FcPatternCreate();
	if (!pat)
		return 0;
	
	FcPatternAddString(pat, FC_FAMILY, (const FcChar8*)family);
	FcPatternAddBool(pat, FC_OUTLINE, FcTrue);
	FcPatternAddInteger(pat, FC_SLANT, italic);
	FcPatternAddInteger(pat, FC_WEIGHT, bold);

	FcDefaultSubstitute(pat);
	
	rc = FcConfigSubstitute(priv->config, pat, FcMatchPattern);
	if (!rc)
		return 0;
	
	rpat = FcFontMatch(priv->config, pat, &result);
	if (!rpat)
		return 0;
	
	result = FcPatternGetBool(rpat, FC_OUTLINE, 0, &val_b);
	if (result != FcResultMatch)
		return 0;
	if (val_b != FcTrue)
		return 0;
	
	result = FcPatternGetInteger(rpat, FC_INDEX, 0, &val_i);
	if (result != FcResultMatch)
		return 0;
	*index = val_i;

	result = FcPatternGetString(rpat, FC_FAMILY, 0, &val_s);
	if (result != FcResultMatch)
		return 0;

	if (strcasecmp((const char*)val_s, family) != 0)
		mp_msg(MSGT_ASS, MSGL_WARN, MSGTR_LIBASS_SelectedFontFamilyIsNotTheRequestedOne,
				(const char*)val_s, family);

	result = FcPatternGetString(rpat, FC_FILE, 0, &val_s);
	if (result != FcResultMatch)
		return 0;
	
	return strdup((const char*)val_s);
}

/**
 * \brief Find a font. Use default family or path if necessary.
 * \param priv_ private data
 * \param family font family
 * \param bold font weight value
 * \param italic font slant value
 * \param index out: font index inside a file
 * \return font file path
*/ 
char* fontconfig_select(fc_instance_t* priv, const char* family, unsigned bold, unsigned italic, int* index)
{
	char* res = 0;
	if (family && *family)
		res = _select_font(priv, family, bold, italic, index);
	if (!res && priv->family_default) {
		res = _select_font(priv, priv->family_default, bold, italic, index);
		if (res && !no_more_font_messages)
			mp_msg(MSGT_ASS, MSGL_WARN, MSGTR_LIBASS_UsingDefaultFontFamily, 
					family, bold, italic, res, *index);
	}
	if (!res && priv->path_default) {
		res = priv->path_default;
		*index = priv->index_default;
		if (!no_more_font_messages)
			mp_msg(MSGT_ASS, MSGL_WARN, MSGTR_LIBASS_UsingDefaultFont, 
					family, bold, italic, res, *index);
	}
	if (!res) {
		res = _select_font(priv, "Arial", bold, italic, index);
		if (res && !no_more_font_messages)
			mp_msg(MSGT_ASS, MSGL_WARN, MSGTR_LIBASS_UsingArialFontFamily, 
					family, bold, italic, res, *index);
	}
	if (res)
		mp_msg(MSGT_ASS, MSGL_V, "fontconfig_select: (%s, %d, %d) -> %s, %d\n", 
				family, bold, italic, res, *index);
	return res;
}

/**
 * \brief Init fontconfig.
 * \param dir additional directoryu for fonts
 * \param family default font family
 * \param path default font path
 * \return pointer to fontconfig private data
*/ 
fc_instance_t* fontconfig_init(const char* dir, const char* family, const char* path)
{
	int rc;
	struct stat st;
	fc_instance_t* priv = calloc(1, sizeof(fc_instance_t));
	
	rc = FcInit();
	assert(rc);

	priv->config = FcConfigGetCurrent();
	if (!priv->config) {
		mp_msg(MSGT_ASS, MSGL_FATAL, MSGTR_LIBASS_FcInitLoadConfigAndFontsFailed);
		return 0;
	}

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
	priv->index_default = 0;
	
	rc = stat(path, &st);
	if (!rc && S_ISREG(st.st_mode))
		priv->path_default = path ? strdup(path) : 0;
	else
		priv->path_default = 0;

	return priv;
}

#else

char* fontconfig_select(fc_instance_t* priv, const char* family, unsigned bold, unsigned italic, int* index)
{
	*index = priv->index_default;
	return priv->path_default;
}

fc_instance_t* fontconfig_init(const char* dir, const char* family, const char* path)
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


