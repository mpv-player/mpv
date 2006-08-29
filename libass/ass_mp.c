#include <inttypes.h>
#include <string.h>
#include <stdlib.h>

#include "ass.h"
#include "ass_mp.h"

// libass-related command line options
int ass_enabled = 0;
float ass_font_scale = 1.;
float ass_line_spacing = 0.;
int ass_top_margin = 0;
int ass_bottom_margin = 0;
int extract_embedded_fonts = 0;
char **ass_force_style_list = NULL;
int ass_use_margins = 0;

extern int font_fontconfig;
extern char* font_name;
extern float text_font_scale_factor;
extern int subtitle_autoscale;

extern double ass_internal_font_size_coeff; 
extern void process_force_style(ass_track_t* track);

/**
 * \brief Convert subdata to ass_track
 * \param subdata subtitles struct from subreader
 * \param fps video framerate
 * \return newly allocated ass_track, filled with subtitles from subdata
 */
ass_track_t* ass_read_subdata(sub_data* subdata, double fps) {
	ass_track_t* track = ass_new_track();
	ass_style_t* style;
	ass_event_t* event;
	subtitle* sub;
	int sid, eid;
	int i;
	double fs;

	track->track_type = TRACK_TYPE_ASS;
	track->name = subdata->filename ? strdup(subdata->filename) : 0;
	track->Timer = 100.;
	track->PlayResX = 384;
	track->PlayResY = 288;
	track->WrapStyle = 0;

	sid = ass_alloc_style(track);
	style = track->styles + sid;
	style->Name = strdup("Default");
	style->FontName = (font_fontconfig && font_name) ? strdup(font_name) : strdup("Sans");

	fs = track->PlayResY * text_font_scale_factor / 100. / ass_internal_font_size_coeff;
	// approximate autoscale coefficients
	if (subtitle_autoscale == 2)
		fs *= 1.3;
	else if (subtitle_autoscale == 3)
		fs *= 1.4;
	style->FontSize = fs;

	style->PrimaryColour = 0xFFFF0000;
	style->SecondaryColour = 0xFFFF0000;
	style->OutlineColour = 0x00000000;
	style->BackColour = 0x00000000;
	style->BorderStyle = 1;
	style->Alignment = 2;
	style->Outline = 2;
	style->MarginL = 30;
	style->MarginR = 30;
	style->MarginV = 20;
	style->ScaleX = 1.;
	style->ScaleY = 1.;

	for (i = 0; i < subdata->sub_num; ++i) {
		int len = 0, j;
		char* p;
		char* end;
		sub = subdata->subtitles + i;
		eid = ass_alloc_event(track);
		event = track->events + eid;

		event->Start = sub->start * 10;
		event->Duration = (sub->end - sub->start) * 10;
		if (!subdata->sub_uses_time) {
			event->Start *= 100. / fps;
			event->Duration *= 100. / fps;
		}

		event->Style = sid;

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
			p += snprintf(p, end - p, "%s ", sub->text[j]);

		p--; // remove last ' '
		*p = 0;
	}
	process_force_style(track);
	return track;
}

