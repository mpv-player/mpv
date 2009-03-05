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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <inttypes.h>

#ifdef CONFIG_ICONV
#include <iconv.h>
#endif

#include "ass.h"
#include "ass_utils.h"
#include "ass_library.h"
#include "mputils.h"

typedef enum {PST_UNKNOWN = 0, PST_INFO, PST_STYLES, PST_EVENTS, PST_FONTS} parser_state_t;

struct parser_priv_s {
	parser_state_t state;
	char* fontname;
	char* fontdata;
	int fontdata_size;
	int fontdata_used;
};

#define ASS_STYLES_ALLOC 20
#define ASS_EVENTS_ALLOC 200

void ass_free_track(ass_track_t* track) {
	int i;
	
	if (track->parser_priv) {
		if (track->parser_priv->fontname)
			free(track->parser_priv->fontname);
		if (track->parser_priv->fontdata)
			free(track->parser_priv->fontdata);
		free(track->parser_priv);
	}
	if (track->style_format)
		free(track->style_format);
	if (track->event_format)
		free(track->event_format);
	if (track->styles) {
		for (i = 0; i < track->n_styles; ++i)
			ass_free_style(track, i);
		free(track->styles);
	}
	if (track->events) {
		for (i = 0; i < track->n_events; ++i)
			ass_free_event(track, i);
		free(track->events);
	}
}

/// \brief Allocate a new style struct
/// \param track track
/// \return style id
int ass_alloc_style(ass_track_t* track) {
	int sid;
	
	assert(track->n_styles <= track->max_styles);

	if (track->n_styles == track->max_styles) {
		track->max_styles += ASS_STYLES_ALLOC;
		track->styles = (ass_style_t*)realloc(track->styles, sizeof(ass_style_t)*track->max_styles);
	}
	
	sid = track->n_styles++;
	memset(track->styles + sid, 0, sizeof(ass_style_t));
	return sid;
}

/// \brief Allocate a new event struct
/// \param track track
/// \return event id
int ass_alloc_event(ass_track_t* track) {
	int eid;
	
	assert(track->n_events <= track->max_events);

	if (track->n_events == track->max_events) {
		track->max_events += ASS_EVENTS_ALLOC;
		track->events = (ass_event_t*)realloc(track->events, sizeof(ass_event_t)*track->max_events);
	}
	
	eid = track->n_events++;
	memset(track->events + eid, 0, sizeof(ass_event_t));
	return eid;
}

void ass_free_event(ass_track_t* track, int eid) {
	ass_event_t* event = track->events + eid;
	if (event->Name)
		free(event->Name);
	if (event->Effect)
		free(event->Effect);
	if (event->Text)
		free(event->Text);
	if (event->render_priv)
		free(event->render_priv);
}

void ass_free_style(ass_track_t* track, int sid) {
	ass_style_t* style = track->styles + sid;
	if (style->Name)
		free(style->Name);
	if (style->FontName)
		free(style->FontName);
}

// ==============================================================================================

static void skip_spaces(char** str) {
	char* p = *str;
	while ((*p==' ') || (*p=='\t'))
		++p;
	*str = p;
}

static void rskip_spaces(char** str, char* limit) {
	char* p = *str;
	while ((p >= limit) && ((*p==' ') || (*p=='\t')))
		--p;
	*str = p;
}

/**
 * \brief find style by name
 * \param track track
 * \param name style name
 * \return index in track->styles
 * Returnes 0 if no styles found => expects at least 1 style.
 * Parsing code always adds "Default" style in the end.
 */
static int lookup_style(ass_track_t* track, char* name) {
	int i;
	if (*name == '*') ++name; // FIXME: what does '*' really mean ?
	for (i = track->n_styles - 1; i >= 0; --i) {
		// FIXME: mb strcasecmp ?
		if (strcmp(track->styles[i].Name, name) == 0)
			return i;
	}
	i = track->default_style;
	mp_msg(MSGT_ASS, MSGL_WARN, MSGTR_LIBASS_NoStyleNamedXFoundUsingY, track, name, track->styles[i].Name);
	return i; // use the first style
}

static uint32_t string2color(char* p) {
	uint32_t tmp;
	(void)strtocolor(&p, &tmp);
	return tmp;
}

static long long string2timecode(char* p) {
	unsigned h, m, s, ms;
	long long tm;
	int res = sscanf(p, "%1d:%2d:%2d.%2d", &h, &m, &s, &ms);
	if (res < 4) {
		mp_msg(MSGT_ASS, MSGL_WARN, MSGTR_LIBASS_BadTimestamp);
		return 0;
	}
	tm = ((h * 60 + m) * 60 + s) * 1000 + ms * 10;
	return tm;
}

/**
 * \brief converts numpad-style align to align.
 */
static int numpad2align(int val) {
	int res, v;
	v = (val - 1) / 3; // 0, 1 or 2 for vertical alignment
	if (v != 0) v = 3 - v;
	res = ((val - 1) % 3) + 1; // horizontal alignment
	res += v*4;
	return res;
}

#define NEXT(str,token) \
	token = next_token(&str); \
	if (!token) break;

#define ANYVAL(name,func) \
	} else if (strcasecmp(tname, #name) == 0) { \
		target->name = func(token); \
		mp_msg(MSGT_ASS, MSGL_DBG2, "%s = %s\n", #name, token);

#define STRVAL(name) \
	} else if (strcasecmp(tname, #name) == 0) { \
		if (target->name != NULL) free(target->name); \
		target->name = strdup(token); \
		mp_msg(MSGT_ASS, MSGL_DBG2, "%s = %s\n", #name, token);
		
#define COLORVAL(name) ANYVAL(name,string2color)
#define INTVAL(name) ANYVAL(name,atoi)
#define FPVAL(name) ANYVAL(name,atof)
#define TIMEVAL(name) ANYVAL(name,string2timecode)
#define STYLEVAL(name) \
	} else if (strcasecmp(tname, #name) == 0) { \
		target->name = lookup_style(track, token); \
		mp_msg(MSGT_ASS, MSGL_DBG2, "%s = %s\n", #name, token);

#define ALIAS(alias,name) \
	if (strcasecmp(tname, #alias) == 0) {tname = #name;}

static char* next_token(char** str) {
	char* p = *str;
	char* start;
	skip_spaces(&p);
	if (*p == '\0') {
		*str = p;
		return 0;
	}
	start = p; // start of the token
	for (; (*p != '\0') && (*p != ','); ++p) {}
	if (*p == '\0') {
		*str = p; // eos found, str will point to '\0' at exit
	} else {
		*p = '\0';
		*str = p + 1; // ',' found, str will point to the next char (beginning of the next token)
	}
	--p; // end of current token
	rskip_spaces(&p, start);
	if (p < start)
		p = start; // empty token
	else
		++p; // the first space character, or '\0'
	*p = '\0';
	return start;
}
/**
 * \brief Parse the tail of Dialogue line
 * \param track track
 * \param event parsed data goes here
 * \param str string to parse, zero-terminated
 * \param n_ignored number of format options to skip at the beginning
*/ 
static int process_event_tail(ass_track_t* track, ass_event_t* event, char* str, int n_ignored)
{
	char* token;
	char* tname;
	char* p = str;
	int i;
	ass_event_t* target = event;

	char* format = strdup(track->event_format);
	char* q = format; // format scanning pointer

	if (track->n_styles == 0) {
		// add "Default" style to the end
		// will be used if track does not contain a default style (or even does not contain styles at all)
		int sid = ass_alloc_style(track);
		track->styles[sid].Name = strdup("Default");
		track->styles[sid].FontName = strdup("Arial");
	}

	for (i = 0; i < n_ignored; ++i) {
		NEXT(q, tname);
	}

	while (1) {
		NEXT(q, tname);
		if (strcasecmp(tname, "Text") == 0) {
			char* last;
			event->Text = strdup(p);
			if (*event->Text != 0) {
				last = event->Text + strlen(event->Text) - 1;
				if (last >= event->Text && *last == '\r')
					*last = 0;
			}
			mp_msg(MSGT_ASS, MSGL_DBG2, "Text = %s\n", event->Text);
			event->Duration -= event->Start;
			free(format);
			return 0; // "Text" is always the last
		}
		NEXT(p, token);

		ALIAS(End,Duration) // temporarily store end timecode in event->Duration
		if (0) { // cool ;)
			INTVAL(Layer)
			STYLEVAL(Style)
			STRVAL(Name)
			STRVAL(Effect)
			INTVAL(MarginL)
			INTVAL(MarginR)
			INTVAL(MarginV)
			TIMEVAL(Start)
			TIMEVAL(Duration)
		}
	}
	free(format);
	return 1;
}

/**
 * \brief Parse command line style overrides (--ass-force-style option)
 * \param track track to apply overrides to
 * The format for overrides is [StyleName.]Field=Value
 */
void process_force_style(ass_track_t* track) {
	char **fs, *eq, *dt, *style, *tname, *token;
	ass_style_t* target;
	int sid;
	char** list = track->library->style_overrides;
	
	if (!list) return;
	
	for (fs = list; *fs; ++fs) {
		eq = strrchr(*fs, '=');
		if (!eq)
			continue;
		*eq = '\0';
		token = eq + 1;

		if(!strcasecmp(*fs, "PlayResX"))
			track->PlayResX = atoi(token);
		else if(!strcasecmp(*fs, "PlayResY"))
			track->PlayResY = atoi(token);
		else if(!strcasecmp(*fs, "Timer"))
			track->Timer = atof(token);
		else if(!strcasecmp(*fs, "WrapStyle"))
			track->WrapStyle = atoi(token);
		else if(!strcasecmp(*fs, "ScaledBorderAndShadow"))
			track->ScaledBorderAndShadow = parse_bool(token);

		dt = strrchr(*fs, '.');
		if (dt) {
			*dt = '\0';
			style = *fs;
			tname = dt + 1;
		} else {
			style = NULL;
			tname = *fs;
		}
		for (sid = 0; sid < track->n_styles; ++sid) {
			if (style == NULL || strcasecmp(track->styles[sid].Name, style) == 0) {
				target = track->styles + sid;
				if (0) {
					STRVAL(FontName)
					COLORVAL(PrimaryColour)
					COLORVAL(SecondaryColour)
					COLORVAL(OutlineColour)
					COLORVAL(BackColour)
					FPVAL(FontSize)
					INTVAL(Bold)
					INTVAL(Italic)
					INTVAL(Underline)
					INTVAL(StrikeOut)
					FPVAL(Spacing)
					INTVAL(Angle)
					INTVAL(BorderStyle)
					INTVAL(Alignment)
					INTVAL(MarginL)
					INTVAL(MarginR)
					INTVAL(MarginV)
					INTVAL(Encoding)
					FPVAL(ScaleX)
					FPVAL(ScaleY)
					FPVAL(Outline)
					FPVAL(Shadow)
				}
			}
		}
		*eq = '=';
		if (dt) *dt = '.';
	}
}

/**
 * \brief Parse the Style line
 * \param track track
 * \param str string to parse, zero-terminated
 * Allocates a new style struct.
*/ 
static int process_style(ass_track_t* track, char *str)
{

	char* token;
	char* tname;
	char* p = str;
	char* format;
	char* q; // format scanning pointer
	int sid;
	ass_style_t* style;
	ass_style_t* target;

	if (!track->style_format) {
		// no style format header
		// probably an ancient script version
		if (track->track_type == TRACK_TYPE_SSA)
			track->style_format = strdup("Name, Fontname, Fontsize, PrimaryColour, SecondaryColour,"
					"TertiaryColour, BackColour, Bold, Italic, BorderStyle, Outline,"
					"Shadow, Alignment, MarginL, MarginR, MarginV, AlphaLevel, Encoding");
		else
			track->style_format = strdup("Name, Fontname, Fontsize, PrimaryColour, SecondaryColour,"
					"OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut,"
					"ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow,"
					"Alignment, MarginL, MarginR, MarginV, Encoding");
	}

	q = format = strdup(track->style_format);
	
	mp_msg(MSGT_ASS, MSGL_V, "[%p] Style: %s\n", track, str);
	
	sid = ass_alloc_style(track);

	style = track->styles + sid;
	target = style;
// fill style with some default values
	style->ScaleX = 100.;
	style->ScaleY = 100.;
	
	while (1) {
		NEXT(q, tname);
		NEXT(p, token);
		
//		ALIAS(TertiaryColour,OutlineColour) // ignore TertiaryColour; it appears only in SSA, and is overridden by BackColour
			
		if (0) { // cool ;)
			STRVAL(Name)
				if ((strcmp(target->Name, "Default")==0) || (strcmp(target->Name, "*Default")==0))
					track->default_style = sid;
			STRVAL(FontName)
			COLORVAL(PrimaryColour)
			COLORVAL(SecondaryColour)
			COLORVAL(OutlineColour) // TertiaryColor
			COLORVAL(BackColour)
				// SSA uses BackColour for both outline and shadow
				// this will destroy SSA's TertiaryColour, but i'm not going to use it anyway
				if (track->track_type == TRACK_TYPE_SSA)
					target->OutlineColour = target->BackColour;
			FPVAL(FontSize)
			INTVAL(Bold)
			INTVAL(Italic)
			INTVAL(Underline)
			INTVAL(StrikeOut)
			FPVAL(Spacing)
			INTVAL(Angle)
			INTVAL(BorderStyle)
			INTVAL(Alignment)
				if (track->track_type == TRACK_TYPE_ASS)
					target->Alignment = numpad2align(target->Alignment);
			INTVAL(MarginL)
			INTVAL(MarginR)
			INTVAL(MarginV)
			INTVAL(Encoding)
			FPVAL(ScaleX)
			FPVAL(ScaleY)
			FPVAL(Outline)
			FPVAL(Shadow)
		}
	}
	style->ScaleX /= 100.;
	style->ScaleY /= 100.;
	style->Bold = !!style->Bold;
	style->Italic = !!style->Italic;
	style->Underline = !!style->Underline;
	if (!style->Name)
		style->Name = strdup("Default");
	if (!style->FontName)
		style->FontName = strdup("Arial");
	// skip '@' at the start of the font name
	if (*style->FontName == '@') {
		p = style->FontName;
		style->FontName = strdup(p + 1);
		free(p);
	}
	free(format);
	return 0;
	
}

static int process_styles_line(ass_track_t* track, char *str)
{
	if (!strncmp(str,"Format:", 7)) {
		char* p = str + 7;
		skip_spaces(&p);
		track->style_format = strdup(p);
		mp_msg(MSGT_ASS, MSGL_DBG2, "Style format: %s\n", track->style_format);
	} else if (!strncmp(str,"Style:", 6)) {
		char* p = str + 6;
		skip_spaces(&p);
		process_style(track, p);
	}
	return 0;
}

static int process_info_line(ass_track_t* track, char *str)
{
	if (!strncmp(str, "PlayResX:", 9)) {
		track->PlayResX = atoi(str + 9);
	} else if (!strncmp(str,"PlayResY:", 9)) {
		track->PlayResY = atoi(str + 9);
	} else if (!strncmp(str,"Timer:", 6)) {
		track->Timer = atof(str + 6);
	} else if (!strncmp(str,"WrapStyle:", 10)) {
		track->WrapStyle = atoi(str + 10);
	} else if (!strncmp(str, "ScaledBorderAndShadow:", 22)) {
		track->ScaledBorderAndShadow = parse_bool(str + 22);
	}
	return 0;
}

static int process_events_line(ass_track_t* track, char *str)
{
	if (!strncmp(str, "Format:", 7)) {
		char* p = str + 7;
		skip_spaces(&p);
		track->event_format = strdup(p);
		mp_msg(MSGT_ASS, MSGL_DBG2, "Event format: %s\n", track->event_format);
	} else if (!strncmp(str, "Dialogue:", 9)) {
		// This should never be reached for embedded subtitles.
		// They have slightly different format and are parsed in ass_process_chunk,
		// called directly from demuxer
		int eid;
		ass_event_t* event;
		
		str += 9;
		skip_spaces(&str);

		eid = ass_alloc_event(track);
		event = track->events + eid;

		process_event_tail(track, event, str, 0);
	} else {
		mp_msg(MSGT_ASS, MSGL_V, "Not understood: %s  \n", str);
	}
	return 0;
}

// Copied from mkvtoolnix
static unsigned char* decode_chars(unsigned char c1, unsigned char c2,
		unsigned char c3, unsigned char c4, unsigned char* dst, int cnt)
{
	uint32_t value;
	unsigned char bytes[3];
	int i;

	value = ((c1 - 33) << 18) + ((c2 - 33) << 12) + ((c3 - 33) << 6) + (c4 - 33);
	bytes[2] = value & 0xff;
	bytes[1] = (value & 0xff00) >> 8;
	bytes[0] = (value & 0xff0000) >> 16;

	for (i = 0; i < cnt; ++i)
		*dst++ = bytes[i];
	return dst;
}

static int decode_font(ass_track_t* track)
{
	unsigned char* p;
	unsigned char* q;
	int i;
	int size; // original size
	int dsize; // decoded size
	unsigned char* buf = 0;

	mp_msg(MSGT_ASS, MSGL_V, "font: %d bytes encoded data \n", track->parser_priv->fontdata_used);
	size = track->parser_priv->fontdata_used;
	if (size % 4 == 1) {
		mp_msg(MSGT_ASS, MSGL_ERR, MSGTR_LIBASS_BadEncodedDataSize);
		goto error_decode_font;
	}
	buf = malloc(size / 4 * 3 + 2);
	q = buf;
	for (i = 0, p = (unsigned char*)track->parser_priv->fontdata; i < size / 4; i++, p+=4) {
		q = decode_chars(p[0], p[1], p[2], p[3], q, 3);
	}
	if (size % 4 == 2) {
		q = decode_chars(p[0], p[1], 0, 0, q, 1);
	} else if (size % 4 == 3) {
		q = decode_chars(p[0], p[1], p[2], 0, q, 2);
	}
	dsize = q - buf;
	assert(dsize <= size / 4 * 3 + 2);
	
	if (track->library->extract_fonts) {
		ass_add_font(track->library, track->parser_priv->fontname, (char*)buf, dsize);
		buf = 0;
	}

error_decode_font:
	if (buf) free(buf);
	free(track->parser_priv->fontname);
	free(track->parser_priv->fontdata);
	track->parser_priv->fontname = 0;
	track->parser_priv->fontdata = 0;
	track->parser_priv->fontdata_size = 0;
	track->parser_priv->fontdata_used = 0;
	return 0;
}

static int process_fonts_line(ass_track_t* track, char *str)
{
	int len;

	if (!strncmp(str, "fontname:", 9)) {
		char* p = str + 9;
		skip_spaces(&p);
		if (track->parser_priv->fontname) {
			decode_font(track);
		}
		track->parser_priv->fontname = strdup(p);
		mp_msg(MSGT_ASS, MSGL_V, "fontname: %s\n", track->parser_priv->fontname);
		return 0;
	}
	
	if (!track->parser_priv->fontname) {
		mp_msg(MSGT_ASS, MSGL_V, "Not understood: %s  \n", str);
		return 0;
	}

	len = strlen(str);
	if (len > 80) {
		mp_msg(MSGT_ASS, MSGL_WARN, MSGTR_LIBASS_FontLineTooLong, len, str);
		return 0;
	}
	if (track->parser_priv->fontdata_used + len > track->parser_priv->fontdata_size) {
		track->parser_priv->fontdata_size += 100 * 1024;
		track->parser_priv->fontdata = realloc(track->parser_priv->fontdata, track->parser_priv->fontdata_size);
	}
	memcpy(track->parser_priv->fontdata + track->parser_priv->fontdata_used, str, len);
	track->parser_priv->fontdata_used += len;
	
	return 0;
}

/**
 * \brief Parse a header line
 * \param track track
 * \param str string to parse, zero-terminated
*/ 
static int process_line(ass_track_t* track, char *str)
{
	if (!strncasecmp(str, "[Script Info]", 13)) {
		track->parser_priv->state = PST_INFO;
	} else if (!strncasecmp(str, "[V4 Styles]", 11)) {
		track->parser_priv->state = PST_STYLES;
		track->track_type = TRACK_TYPE_SSA;
	} else if (!strncasecmp(str, "[V4+ Styles]", 12)) {
		track->parser_priv->state = PST_STYLES;
		track->track_type = TRACK_TYPE_ASS;
	} else if (!strncasecmp(str, "[Events]", 8)) {
		track->parser_priv->state = PST_EVENTS;
	} else if (!strncasecmp(str, "[Fonts]", 7)) {
		track->parser_priv->state = PST_FONTS;
	} else {
		switch (track->parser_priv->state) {
		case PST_INFO:
			process_info_line(track, str);
			break;
		case PST_STYLES:
			process_styles_line(track, str);
			break;
		case PST_EVENTS:
			process_events_line(track, str);
			break;
		case PST_FONTS:
			process_fonts_line(track, str);
			break;
		default:
			break;
		}
	}

	// there is no explicit end-of-font marker in ssa/ass
	if ((track->parser_priv->state != PST_FONTS) && (track->parser_priv->fontname))
		decode_font(track);

	return 0;
}

static int process_text(ass_track_t* track, char* str)
{
	char* p = str;
	while(1) {
		char* q;
		while (1) {
			if ((*p=='\r')||(*p=='\n')) ++p;
			else if (p[0]=='\xef' && p[1]=='\xbb' && p[2]=='\xbf') p+=3; // U+FFFE (BOM)
			else break;
		}
		for (q=p; ((*q!='\0')&&(*q!='\r')&&(*q!='\n')); ++q) {};
		if (q==p)
			break;
		if (*q != '\0')
			*(q++) = '\0';
		process_line(track, p);
		if (*q == '\0')
			break;
		p = q;
	}
	return 0;
}

/**
 * \brief Process a chunk of subtitle stream data.
 * \param track track
 * \param data string to parse
 * \param size length of data
*/
void ass_process_data(ass_track_t* track, char* data, int size)
{
	char* str = malloc(size + 1);

	memcpy(str, data, size);
	str[size] = '\0';

	mp_msg(MSGT_ASS, MSGL_V, "event: %s\n", str);
	process_text(track, str);
	free(str);
}

/**
 * \brief Process CodecPrivate section of subtitle stream
 * \param track track
 * \param data string to parse
 * \param size length of data
 CodecPrivate section contains [Stream Info] and [V4+ Styles] ([V4 Styles] for SSA) sections
*/
void ass_process_codec_private(ass_track_t* track, char *data, int size)
{
	ass_process_data(track, data, size);

	if (!track->event_format) {
		// probably an mkv produced by ancient mkvtoolnix
		// such files don't have [Events] and Format: headers
		track->parser_priv->state = PST_EVENTS;
		if (track->track_type == TRACK_TYPE_SSA)
			track->event_format = strdup("Format: Marked, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text");
		else
			track->event_format = strdup("Format: Layer, Start, End, Style, Actor, MarginL, MarginR, MarginV, Effect, Text");
	}

	process_force_style(track);
}

static int check_duplicate_event(ass_track_t* track, int ReadOrder)
{
	int i;
	for (i = 0; i<track->n_events - 1; ++i) // ignoring last event, it is the one we are comparing with
		if (track->events[i].ReadOrder == ReadOrder)
			return 1;
	return 0;
}

/**
 * \brief Process a chunk of subtitle stream data. In Matroska, this contains exactly 1 event (or a commentary).
 * \param track track
 * \param data string to parse
 * \param size length of data
 * \param timecode starting time of the event (milliseconds)
 * \param duration duration of the event (milliseconds)
*/ 
void ass_process_chunk(ass_track_t* track, char *data, int size, long long timecode, long long duration)
{
	char* str;
	int eid;
	char* p;
	char* token;
	ass_event_t* event;

	if (!track->event_format) {
		mp_msg(MSGT_ASS, MSGL_WARN, MSGTR_LIBASS_EventFormatHeaderMissing);
		return;
	}
	
	str = malloc(size + 1);
	memcpy(str, data, size);
	str[size] = '\0';
	mp_msg(MSGT_ASS, MSGL_V, "event at %" PRId64 ", +%" PRId64 ": %s  \n", (int64_t)timecode, (int64_t)duration, str);

	eid = ass_alloc_event(track);
	event = track->events + eid;

	p = str;
	
	do { 
		NEXT(p, token);
		event->ReadOrder = atoi(token);
		if (check_duplicate_event(track, event->ReadOrder))
			break;

		NEXT(p, token);
		event->Layer = atoi(token);

		process_event_tail(track, event, p, 3);

		event->Start = timecode;
		event->Duration = duration;
		
		free(str);
		return;
//		dump_events(tid);
	} while (0);
	// some error
	ass_free_event(track, eid);
	track->n_events--;
	free(str);
}

#ifdef CONFIG_ICONV
/** \brief recode buffer to utf-8
 * constraint: codepage != 0
 * \param data pointer to text buffer
 * \param size buffer size
 * \return a pointer to recoded buffer, caller is responsible for freeing it
**/
static char* sub_recode(char* data, size_t size, char* codepage)
{
	static iconv_t icdsc = (iconv_t)(-1);
	char* tocp = "UTF-8";
	char* outbuf;
	assert(codepage);

	{
		const char* cp_tmp = codepage;
#ifdef CONFIG_ENCA
		char enca_lang[3], enca_fallback[100];
		if (sscanf(codepage, "enca:%2s:%99s", enca_lang, enca_fallback) == 2
				|| sscanf(codepage, "ENCA:%2s:%99s", enca_lang, enca_fallback) == 2) {
			cp_tmp = guess_buffer_cp((unsigned char*)data, size, enca_lang, enca_fallback);
		}
#endif
		if ((icdsc = iconv_open (tocp, cp_tmp)) != (iconv_t)(-1)){
			mp_msg(MSGT_ASS,MSGL_V,"LIBSUB: opened iconv descriptor.\n");
		} else
			mp_msg(MSGT_ASS,MSGL_ERR,MSGTR_LIBASS_ErrorOpeningIconvDescriptor);
	}

	{
		size_t osize = size;
		size_t ileft = size;
		size_t oleft = size - 1;
		char* ip;
		char* op;
		size_t rc;
		int clear = 0;
		
		outbuf = malloc(osize);
		ip = data;
		op = outbuf;
		
		while (1) {
			if (ileft)
				rc = iconv(icdsc, &ip, &ileft, &op, &oleft);
			else {// clear the conversion state and leave
				clear = 1;
				rc = iconv(icdsc, NULL, NULL, &op, &oleft);
			}
			if (rc == (size_t)(-1)) {
				if (errno == E2BIG) {
					size_t offset = op - outbuf;
					outbuf = (char*)realloc(outbuf, osize + size);
					op = outbuf + offset;
					osize += size;
					oleft += size;
				} else {
					mp_msg(MSGT_ASS, MSGL_WARN, MSGTR_LIBASS_ErrorRecodingFile);
					return NULL;
				}
			} else
				if (clear)
					break;
		}
		outbuf[osize - oleft - 1] = 0;
	}

	if (icdsc != (iconv_t)(-1)) {
		(void)iconv_close(icdsc);
		icdsc = (iconv_t)(-1);
		mp_msg(MSGT_ASS,MSGL_V,"LIBSUB: closed iconv descriptor.\n");
	}
	
	return outbuf;
}
#endif // ICONV

/**
 * \brief read file contents into newly allocated buffer
 * \param fname file name
 * \param bufsize out: file size
 * \return pointer to file contents. Caller is responsible for its deallocation.
 */
static char* read_file(char* fname, size_t *bufsize)
{
	int res;
	long sz;
	long bytes_read;
	char* buf;

	FILE* fp = fopen(fname, "rb");
	if (!fp) {
		mp_msg(MSGT_ASS, MSGL_WARN, MSGTR_LIBASS_FopenFailed, fname);
		return 0;
	}
	res = fseek(fp, 0, SEEK_END);
	if (res == -1) {
		mp_msg(MSGT_ASS, MSGL_WARN, MSGTR_LIBASS_FseekFailed, fname);
		fclose(fp);
		return 0;
	}
	
	sz = ftell(fp);
	rewind(fp);

	if (sz > 10*1024*1024) {
		mp_msg(MSGT_ASS, MSGL_INFO, MSGTR_LIBASS_RefusingToLoadSubtitlesLargerThan10M, fname);
		fclose(fp);
		return 0;
	}
	
	mp_msg(MSGT_ASS, MSGL_V, "file size: %ld\n", sz);
	
	buf = malloc(sz + 1);
	assert(buf);
	bytes_read = 0;
	do {
		res = fread(buf + bytes_read, 1, sz - bytes_read, fp);
		if (res <= 0) {
			mp_msg(MSGT_ASS, MSGL_INFO, MSGTR_LIBASS_ReadFailed, errno, strerror(errno));
			fclose(fp);
			free(buf);
			return 0;
		}
		bytes_read += res;
	} while (sz - bytes_read > 0);
	buf[sz] = '\0';
	fclose(fp);
	
	if (bufsize)
		*bufsize = sz;
	return buf;
}

/*
 * \param buf pointer to subtitle text in utf-8
 */
static ass_track_t* parse_memory(ass_library_t* library, char* buf)
{
	ass_track_t* track;
	int i;
	
	track = ass_new_track(library);
	
	// process header
	process_text(track, buf);

	// external SSA/ASS subs does not have ReadOrder field
	for (i = 0; i < track->n_events; ++i)
		track->events[i].ReadOrder = i;

	// there is no explicit end-of-font marker in ssa/ass
	if (track->parser_priv->fontname)
		decode_font(track);

	if (track->track_type == TRACK_TYPE_UNKNOWN) {
		ass_free_track(track);
		return 0;
	}

	process_force_style(track);

	return track;
}

/**
 * \brief Read subtitles from memory.
 * \param library libass library object
 * \param buf pointer to subtitles text
 * \param bufsize size of buffer
 * \param codepage recode buffer contents from given codepage
 * \return newly allocated track
*/ 
ass_track_t* ass_read_memory(ass_library_t* library, char* buf, size_t bufsize, char* codepage)
{
	ass_track_t* track;
	int need_free = 0;
	
	if (!buf)
		return 0;
	
#ifdef CONFIG_ICONV
	if (codepage)
		buf = sub_recode(buf, bufsize, codepage);
	if (!buf)
		return 0;
	else
		need_free = 1;
#endif
	track = parse_memory(library, buf);
	if (need_free)
		free(buf);
	if (!track)
		return 0;

	mp_msg(MSGT_ASS, MSGL_INFO, MSGTR_LIBASS_AddedSubtitleFileMemory, track->n_styles, track->n_events);
	return track;
}

char* read_file_recode(char* fname, char* codepage, size_t* size)
{
	char* buf;
	size_t bufsize;
	
	buf = read_file(fname, &bufsize);
	if (!buf)
		return 0;
#ifdef CONFIG_ICONV
	if (codepage) {
		 char* tmpbuf = sub_recode(buf, bufsize, codepage);
		 free(buf);
		 buf = tmpbuf;
	}
	if (!buf)
		return 0;
#endif
	*size = bufsize;
	return buf;
}

/**
 * \brief Read subtitles from file.
 * \param library libass library object
 * \param fname file name
 * \param codepage recode buffer contents from given codepage
 * \return newly allocated track
*/ 
ass_track_t* ass_read_file(ass_library_t* library, char* fname, char* codepage)
{
	char* buf;
	ass_track_t* track;
	size_t bufsize;

	buf = read_file_recode(fname, codepage, &bufsize);
	if (!buf)
		return 0;
	track = parse_memory(library, buf);
	free(buf);
	if (!track)
		return 0;
	
	track->name = strdup(fname);

	mp_msg(MSGT_ASS, MSGL_INFO, MSGTR_LIBASS_AddedSubtitleFileFname, fname, track->n_styles, track->n_events);
	
//	dump_events(forced_tid);
	return track;
}

/**
 * \brief read styles from file into already initialized track
 */
int ass_read_styles(ass_track_t* track, char* fname, char* codepage)
{
	char* buf;
	parser_state_t old_state;
	size_t sz;

	buf = read_file(fname, &sz);
	if (!buf)
		return 1;
#ifdef CONFIG_ICONV
	if (codepage) {
		char* tmpbuf;
		tmpbuf = sub_recode(buf, sz, codepage);
		free(buf);
		buf = tmpbuf;
	}
	if (!buf)
		return 0;
#endif

	old_state = track->parser_priv->state;
	track->parser_priv->state = PST_STYLES;
	process_text(track, buf);
	track->parser_priv->state = old_state;

	return 0;
}

long long ass_step_sub(ass_track_t* track, long long now, int movement) {
	int i;

	if (movement == 0) return 0;
	if (track->n_events == 0) return 0;
	
	if (movement < 0)
		for (i = 0; (i < track->n_events) && ((long long)(track->events[i].Start + track->events[i].Duration) <= now); ++i) {}
	else
		for (i = track->n_events - 1; (i >= 0) && ((long long)(track->events[i].Start) > now); --i) {}
	
	// -1 and n_events are ok
	assert(i >= -1); assert(i <= track->n_events);
	i += movement;
	if (i < 0) i = 0;
	if (i >= track->n_events) i = track->n_events - 1;
	return ((long long)track->events[i].Start) - now;
}

ass_track_t* ass_new_track(ass_library_t* library) {
	ass_track_t* track = calloc(1, sizeof(ass_track_t));
	track->library = library;
	track->parser_priv = calloc(1, sizeof(parser_priv_t));
	return track;
}

