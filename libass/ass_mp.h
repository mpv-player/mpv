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

#ifndef __ASS_OPTIONS_H__
#define __ASS_OPTIONS_H__

#include "subreader.h"

extern ass_library_t* ass_library;
extern int ass_enabled;
extern float ass_font_scale;
extern float ass_line_spacing;
extern int ass_top_margin;
extern int ass_bottom_margin;
extern int extract_embedded_fonts;
extern char **ass_force_style_list;
extern int ass_use_margins;
extern char* ass_color;
extern char* ass_border_color;
extern char* ass_styles_file;

ass_track_t* ass_default_track(ass_library_t* library);
int ass_process_subtitle(ass_track_t* track, subtitle* sub);
ass_track_t* ass_read_subdata(ass_library_t* library, sub_data* subdata, double fps);

void ass_configure(ass_renderer_t* priv, int w, int h);
void ass_configure_fonts(ass_renderer_t* priv);
ass_library_t* ass_init(void);

typedef struct {
	ass_image_t* imgs;
	int changed;
} mp_eosd_images_t;

#endif

