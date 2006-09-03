#ifndef __ASS_OPTIONS_H__
#define __ASS_OPTIONS_H__

#include "subreader.h"

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

ass_track_t* ass_default_track();
int ass_process_subtitle(ass_track_t* track, subtitle* sub);
ass_track_t* ass_read_subdata(sub_data* subdata, double fps);

#endif

