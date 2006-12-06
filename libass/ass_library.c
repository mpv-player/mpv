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

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ass.h"
#include "ass_library.h"


ass_library_t* ass_library_init(void)
{
	return calloc(1, sizeof(ass_library_t));
}

void ass_library_done(ass_library_t* priv)
{
	if (priv) {
		ass_set_fonts_dir(priv, NULL);
		ass_set_style_overrides(priv, NULL);
		free(priv);
	}
}

void ass_set_fonts_dir(ass_library_t* priv, const char* fonts_dir)
{
	if (priv->fonts_dir)
		free(priv->fonts_dir);

	priv->fonts_dir = fonts_dir ? strdup(fonts_dir) : 0;
}

void ass_set_extract_fonts(ass_library_t* priv, int extract)
{
	priv->extract_fonts = !!extract;
}

void ass_set_style_overrides(ass_library_t* priv, char** list)
{
	char** p;
	char** q;
	int cnt;
	
	if (priv->style_overrides) {
		for (p = priv->style_overrides; *p; ++p)
			free(*p);
		free(priv->style_overrides);
	}
	
	if (!list) return;

	for (p = list, cnt = 0; *p; ++p, ++cnt) {}

	priv->style_overrides = malloc((cnt + 1) * sizeof(char*));
	for (p = list, q = priv->style_overrides; *p; ++p, ++q)
		*q = strdup(*p);
	priv->style_overrides[cnt] = NULL;
}

static void grow_array(void **array, int nelem, size_t elsize)
{
	if (!(nelem & 31))
		*array = realloc(*array, (nelem + 32) * elsize);
}

void ass_add_font(ass_library_t* priv, char* name, char* data, int size)
{
	grow_array((void**)&priv->fontdata, priv->num_fontdata, sizeof(*priv->fontdata));
	priv->fontdata[priv->num_fontdata].name = name;
	priv->fontdata[priv->num_fontdata].data = data;
	priv->fontdata[priv->num_fontdata].size = size;
	priv->num_fontdata ++;
}

