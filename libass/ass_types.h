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
	int FontSize;
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
	int Spacing;
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
} ass_event_t;

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

	enum {TRACK_TYPE_ASS, TRACK_TYPE_SSA} track_type;
	
	// script header fields
	int PlayResX;
	int PlayResY;
	double Timer;
	int WrapStyle;

	
	int default_style; // index of default style
	char* name; // file name in case of external subs, 0 for streams
} ass_track_t;

#endif

