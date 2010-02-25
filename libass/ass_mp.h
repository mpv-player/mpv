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

#ifndef LIBASS_MP_H
#define LIBASS_MP_H

#include "subreader.h"
#if defined(CONFIG_ASS_INTERNAL) || !defined(CONFIG_ASS)
#include "ass.h"
#else
#include <ass/ass.h>
#endif
#if defined(LIBASS_VERSION) && LIBASS_VERSION >= 0x00907010
#define ass_library_t ASS_Library
#define ass_track_t ASS_Track
#define ass_track_s ASS_Track
#define ass_renderer_t ASS_Renderer
#define ass_image_t ASS_Image
#define ass_style_t ASS_Style
#define ass_event_t ASS_Event
#define process_force_style(t) ass_process_force_style(t)
#else
void process_force_style(ass_track_t* track);
#endif

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
extern int ass_hinting;

ass_track_t* ass_default_track(ass_library_t* library);
int ass_process_subtitle(ass_track_t* track, subtitle* sub);
ass_track_t* ass_read_subdata(ass_library_t* library, sub_data* subdata, double fps);
ass_track_t* ass_read_stream(ass_library_t* library, const char *fname, char *charset);

void ass_configure(ass_renderer_t* priv, int w, int h, int hinting);
void ass_configure_fonts(ass_renderer_t* priv);
ass_library_t* ass_init(void);

typedef struct {
	ass_image_t* imgs;
	int changed;
} mp_eosd_images_t;

extern int ass_force_reload;
ass_image_t* ass_mp_render_frame(ass_renderer_t *priv, ass_track_t* track, long long now, int* detect_change);

#endif /* LIBASS_MP_H */
