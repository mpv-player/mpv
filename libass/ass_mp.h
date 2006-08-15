#ifndef __ASS_OPTIONS_H__
#define __ASS_OPTIONS_H__

#include "subreader.h"

extern int ass_enabled;
extern float ass_font_scale;
extern float ass_line_spacing;
extern int ass_top_margin;
extern int ass_bottom_margin;
extern int extract_embedded_fonts;

ass_track_t* ass_read_subdata(sub_data* subdata, double fps);

#endif

