#include <inttypes.h>
#include <string.h>
#include <stdlib.h>

#include "mp_msg.h"

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

ass_track_t* ass_default_track() {
	ass_track_t* track = ass_new_track();
	ass_style_t* style;
	int sid;
	double fs;

	track->track_type = TRACK_TYPE_ASS;
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
			p += snprintf(p, end - p, "%s ", sub->text[j]);

		p--; // remove last ' '
		*p = 0;

	if (check_duplicate_plaintext_event(track)) {
		ass_free_event(track, eid);
		track->n_events--;
		return -1;
	}

	mp_msg(MSGT_GLOBAL, MSGL_V, "plaintext event at %" PRId64 ", +%" PRId64 ": %s  \n",
			(int64_t)event->Start, (int64_t)event->Duration, event->Text);
	
	return eid;
}


/**
 * \brief Convert subdata to ass_track
 * \param subdata subtitles struct from subreader
 * \param fps video framerate
 * \return newly allocated ass_track, filled with subtitles from subdata
 */
ass_track_t* ass_read_subdata(sub_data* subdata, double fps) {
	ass_track_t* track;
	int i;

	track = ass_default_track();
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
	process_force_style(track);
	return track;
}

