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

#ifndef __ASS_FONTCONFIG_H__
#define __ASS_FONTCONFIG_H__

#ifdef HAVE_FONTCONFIG
#include <fontconfig/fontconfig.h>
#endif

typedef struct fc_instance_s fc_instance_t;

fc_instance_t* fontconfig_init(ass_library_t* library, FT_Library ftlibrary, const char* family, const char* path);
char* fontconfig_select(fc_instance_t* priv, const char* family, unsigned bold, unsigned italic, int* index);
void fontconfig_done(fc_instance_t* priv);

#ifdef HAVE_FONTCONFIG
char* fontconfig_select_with_charset(fc_instance_t* priv, const char* family, unsigned bold, unsigned italic, int* index, FcCharSet* charset);
#endif

#endif

