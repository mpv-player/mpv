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

#include <inttypes.h>
#include <string.h>
#include <stdlib.h>

#include "mp_msg.h"
#include "path.h"

#include "ass_mp.h"
#include "help_mp.h"
#include "stream/stream.h"

#ifdef CONFIG_FONTCONFIG
#include <fontconfig/fontconfig.h>
#endif

// libass-related command line options
ass_library_t* ass_library;
int ass_enabled = 0;
float ass_font_scale = 1.;
float ass_line_spacing = 0.;
int ass_top_margin = 0;
int ass_bottom_margin = 0;
#if defined(FC_VERSION) && (FC_VERSION >= 20402)
int extract_embedded_fonts = 1;
#else
int extract_embedded_fonts = 0;
#endif
char **ass_force_style_list = NULL;
int ass_use_margins = 0;
char* ass_color = NULL;
char* ass_border_color = NULL;
char* ass_styles_file = NULL;
int ass_hinting = ASS_HINTING_NATIVE + 4; // native hinting for unscaled osd

#ifdef CONFIG_FONTCONFIG
extern int font_fontconfig;
#else
static int font_fontconfig = -1;
#endif
extern char* font_name;
extern char* sub_font_name;
extern float text_font_scale_factor;
extern int subtitle_autoscale;

#ifdef CONFIG_ICONV
extern char* sub_cp;
#else
static char* sub_cp = 0;
#endif

ass_track_t* ass_default_track(ass_library_t* library) {
	ass_track_t* track = ass_new_track(library);

	track->track_type = TRACK_TYPE_ASS;
	track->Timer = 100.;
	track->PlayResY = 288;
	track->WrapStyle = 0;

	if (ass_styles_file)
		ass_read_styles(track, ass_styles_file, sub_cp);

	if (track->n_styles == 0) {
		ass_style_t* style;
		int sid;
		double fs;
		uint32_t c1, c2;

		sid = ass_alloc_style(track);
		style = track->styles + sid;
		style->Name = strdup("Default");
		style->FontName = (font_fontconfig >= 0 && sub_font_name) ? strdup(sub_font_name) : (font_fontconfig >= 0 && font_name) ? strdup(font_name) : strdup("Sans");
		style->treat_fontname_as_pattern = 1;

		fs = track->PlayResY * text_font_scale_factor / 100.;
		// approximate autoscale coefficients
		if (subtitle_autoscale == 2)
			fs *= 1.3;
		else if (subtitle_autoscale == 3)
			fs *= 1.4;
		style->FontSize = fs;

		if (ass_color) c1 = strtoll(ass_color, NULL, 16);
		else c1 = 0xFFFF0000;
		if (ass_border_color) c2 = strtoll(ass_border_color, NULL, 16);
		else c2 = 0x00000000;

		style->PrimaryColour = c1;
		style->SecondaryColour = c1;
		style->OutlineColour = c2;
		style->BackColour = 0x00000000;
		style->BorderStyle = 1;
		style->Alignment = 2;
		style->Outline = 2;
		style->MarginL = 10;
		style->MarginR = 10;
		style->MarginV = 5;
		style->ScaleX = 1.;
		style->ScaleY = 1.;
	}

	process_force_style(track);
	return track;
}

static int check_duplicate_plaintext_event(ass_track_t* track)
{
	int i;
	ass_event_t* evt = track->events + track->n_events - 1;

	for (i = 0; i<track->n_events - 1; ++i) // ignoring last event, it is the one we are comparing with
		if (track->events[i].Start == evt->Start &&
		    track->events[i].Duration == evt->Duration &&
		    strcmp(track->events[i].Text, evt->Text) == 0)
			return 1;
	return 0;
}

/**
 * \brief Convert subtitle to ass_event_t for the given track
 * \param ass_track_t track
 * \param sub subtitle to convert
 * \return event id
 * note: assumes that subtitle is _not_ fps-based; caller must manually correct
 *   Start and Duration in other case.
 **/
int ass_process_subtitle(ass_track_t* track, subtitle* sub)
{
        int eid;
        ass_event_t* event;
	int len = 0, j;
	char* p;
	char* end;

	eid = ass_alloc_event(track);
	event = track->events + eid;

	event->Start = sub->start * 10;
	event->Duration = (sub->end - sub->start) * 10;
	event->Style = 0;

	for (j = 0; j < sub->lines; ++j)
		len += sub->text[j] ? strlen(sub->text[j]) : 0;

	len += 2 * sub->lines; // '\N', including the one after the last line
	len += 6; // {\anX}
	len += 1; // '\0'

	event->Text = malloc(len);
	end = event->Text + len;
	p = event->Text;

	if (sub->alignment)
		p += snprintf(p, end - p, "{\\an%d}", sub->alignment);

	for (j = 0; j < sub->lines; ++j)
		p += snprintf(p, end - p, "%s\\N", sub->text[j]);

	if (sub->lines > 0) p-=2; // remove last "\N"
	*p = 0;

	if (check_duplicate_plaintext_event(track)) {
		ass_free_event(track, eid);
		track->n_events--;
		return -1;
	}

	mp_msg(MSGT_ASS, MSGL_V, "plaintext event at %" PRId64 ", +%" PRId64 ": %s  \n",
			(int64_t)event->Start, (int64_t)event->Duration, event->Text);

	return eid;
}


/**
 * \brief Convert subdata to ass_track
 * \param subdata subtitles struct from subreader
 * \param fps video framerate
 * \return newly allocated ass_track, filled with subtitles from subdata
 */
ass_track_t* ass_read_subdata(ass_library_t* library, sub_data* subdata, double fps) {
	ass_track_t* track;
	int i;

	track = ass_default_track(library);
	track->name = subdata->filename ? strdup(subdata->filename) : 0;

	for (i = 0; i < subdata->sub_num; ++i) {
		int eid = ass_process_subtitle(track, subdata->subtitles + i);
		if (eid < 0)
			continue;
		if (!subdata->sub_uses_time) {
			track->events[eid].Start *= 100. / fps;
			track->events[eid].Duration *= 100. / fps;
		}
	}
	return track;
}

ass_track_t* ass_read_stream(ass_library_t* library, const char *fname, char *charset) {
	int i;
	char *buf = NULL;
	ass_track_t *track;
	size_t sz = 0;
	size_t buf_alloc = 0;
	stream_t *fd;

	fd = open_stream(fname, NULL, &i);
	if (!fd) {
		mp_msg(MSGT_ASS, MSGL_WARN, MSGTR_LIBASS_FopenFailed, fname);
		return NULL;
	}
	if (fd->end_pos > STREAM_BUFFER_SIZE)
		/* read entire file if size is known */
		buf_alloc = fd->end_pos;
	for (;;) {
		if (buf_alloc >= 100*1024*1024) {
			mp_msg(MSGT_ASS, MSGL_INFO, MSGTR_LIBASS_RefusingToLoadSubtitlesLargerThan100M, fname);
			sz = 0;
			break;
		}
		if (buf_alloc < sz + STREAM_BUFFER_SIZE)
			buf_alloc += STREAM_BUFFER_SIZE;
		buf = realloc(buf, buf_alloc + 1);
		i = stream_read(fd, buf + sz, buf_alloc - sz);
		if (i <= 0) break;
		sz += i;
	}
	free_stream(fd);
	if (!sz) {
		free(buf);
		return NULL;
	}
	buf[sz] = 0;
	buf = realloc(buf, sz + 1);
	track = ass_read_memory(library, buf, sz, charset);
	if (track) {
		free(track->name);
		track->name = strdup(fname);
	}
	free(buf);
	return track;
}

void ass_configure(ass_renderer_t* priv, int w, int h, int unscaled) {
	int hinting;
	ass_set_frame_size(priv, w, h);
	ass_set_margins(priv, ass_top_margin, ass_bottom_margin, 0, 0);
	ass_set_use_margins(priv, ass_use_margins);
	ass_set_font_scale(priv, ass_font_scale);
	if (!unscaled && (ass_hinting & 4))
		hinting = 0;
	else
		hinting = ass_hinting & 3;
	ass_set_hinting(priv, hinting);
	ass_set_line_spacing(priv, ass_line_spacing);
}

void ass_configure_fonts(ass_renderer_t* priv) {
	char *dir, *path, *family;
	dir = get_path("fonts");
	if (font_fontconfig < 0 && sub_font_name) path = strdup(sub_font_name);
	else if (font_fontconfig < 0 && font_name) path = strdup(font_name);
	else path = get_path("subfont.ttf");
	if (font_fontconfig >= 0 && sub_font_name) family = strdup(sub_font_name);
	else if (font_fontconfig >= 0 && font_name) family = strdup(font_name);
	else family = 0;

#if defined(LIBASS_VERSION) && LIBASS_VERSION >= 0x00907010
        ass_set_fonts(priv, path, family, font_fontconfig, NULL, 1);
#else
	if (font_fontconfig >= 0)
		ass_set_fonts(priv, path, family);
	else
		ass_set_fonts_nofc(priv, path, family);
#endif

	free(dir);
	free(path);
	free(family);
}

ass_library_t* ass_init(void) {
	ass_library_t* priv;
	char* path = get_path("fonts");
	priv = ass_library_init();
	ass_set_fonts_dir(priv, path);
	ass_set_extract_fonts(priv, extract_embedded_fonts);
	ass_set_style_overrides(priv, ass_force_style_list);
	free(path);
	return priv;
}

int ass_force_reload = 0; // flag set if global ass-related settings were changed

ass_image_t* ass_mp_render_frame(ass_renderer_t *priv, ass_track_t* track, long long now, int* detect_change) {
	if (ass_force_reload) {
		ass_set_margins(priv, ass_top_margin, ass_bottom_margin, 0, 0);
		ass_set_use_margins(priv, ass_use_margins);
		ass_set_font_scale(priv, ass_font_scale);
		ass_force_reload = 0;
	}
	return ass_render_frame(priv, track, now, detect_change);
}
