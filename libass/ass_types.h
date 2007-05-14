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

#ifndef __ASS_TYPES_H__
#define __ASS_TYPES_H__

#define VALIGN_SUB 0
#define VALIGN_CENTER 8
#define VALIGN_TOP 4
#define HALIGN_LEFT 1
#define HALIGN_CENTER 2
#define HALIGN_RIGHT 3

/// ass Style: line
typedef struct ass_style_s {
	char* Name;
	char* FontName;
	double FontSize;
	uint32_t PrimaryColour;
	uint32_t SecondaryColour;
	uint32_t OutlineColour;
	uint32_t BackColour;
	int Bold;
	int Italic;
	int Underline;
	int StrikeOut;
	double ScaleX;
	double ScaleY;
	double Spacing;
	int Angle;
	int BorderStyle;
	double Outline;
	double Shadow;
	int Alignment;
	int MarginL;
	int MarginR;
	int MarginV;
//        int AlphaLevel;
	int Encoding;
} ass_style_t;

typedef struct render_priv_s render_priv_t;

/// ass_event_t corresponds to a single Dialogue line
/// Text is stored as-is, style overrides will be parsed later
typedef struct ass_event_s {
	long long Start; // ms
	long long Duration; // ms

	int ReadOrder;
	int Layer;
	int Style;
	char* Name;
	int MarginL;
	int MarginR;
	int MarginV;
	char* Effect;
	char* Text;

	render_priv_t* render_priv;
} ass_event_t;

typedef struct parser_priv_s parser_priv_t;

typedef struct ass_library_s ass_library_t;

/// ass track represent either an external script or a matroska subtitle stream (no real difference between them)
/// it can be used in rendering after the headers are parsed (i.e. events format line read)
typedef struct ass_track_s {
	int n_styles; // amount used
	int max_styles; // amount allocated
	int n_events;
	int max_events;
	ass_style_t* styles; // array of styles, max_styles length, n_styles used
	ass_event_t* events; // the same as styles

	char* style_format; // style format line (everything after "Format: ")
	char* event_format; // event format line

	enum {TRACK_TYPE_UNKNOWN = 0, TRACK_TYPE_ASS, TRACK_TYPE_SSA} track_type;
	
	// script header fields
	int PlayResX;
	int PlayResY;
	double Timer;
	int WrapStyle;

	
	int default_style; // index of default style
	char* name; // file name in case of external subs, 0 for streams

	ass_library_t* library;
	parser_priv_t* parser_priv;
} ass_track_t;

#endif

