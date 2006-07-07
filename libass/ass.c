#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef HAVE_ENCA
#include "subreader.h" // for guess_buffer_cp
#endif

#ifdef USE_ICONV
#include <iconv.h>
extern char *sub_cp;
#endif

#include "mp_msg.h"
#include "ass.h"
#include "ass_utils.h"
#include "libvo/sub.h" // for utf8_get_char

char *get_path(char *);

#define ASS_STYLES_ALLOC 20
#define ASS_EVENTS_ALLOC 200

void ass_free_track(ass_track_t* track) {
	int i;
	
	if (track->style_format)
		free(track->style_format);
	if (track->event_format)
		free(track->event_format);
	if (track->styles) {
		for (i = 0; i < track->n_styles; ++i) {
			ass_style_t* style = track->styles + i;
			if (style->Name)
				free(style->Name);
			if (style->FontName)
				free(style->FontName);
		}
		free(track->styles);
	}
	if (track->events) {
		for (i = 0; i < track->n_events; ++i) {
			ass_event_t* event = track->events + i;
			if (event->Name)
				free(event->Name);
			if (event->Effect)
				free(event->Effect);
			if (event->Text)
				free(event->Text);
		}
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

static void free_event(ass_track_t* track, int eid) {
	if (track->n_events > eid + 1) // not last event
		memcpy(track->events + eid, track->events + eid + 1, sizeof(ass_event_t) * (track->n_events - eid - 1));
	track->n_events--;
}

static int events_compare_f(const void* a_, const void* b_) {
	ass_event_t* a = (ass_event_t*)a_;
	ass_event_t* b = (ass_event_t*)b_;
	if (a->Start < b->Start)
		return -1;
	else if (a->Start > b->Start)
		return 1;
	else
		return 0;
}

/// \brief Sort events by start time
/// \param tid track id
static void sort_events(ass_track_t* track) {
	qsort(track->events, track->n_events, sizeof(ass_event_t), events_compare_f);
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
	for (i=0; i<track->n_styles; ++i) {
		// FIXME: mb strcasecmp ?
		if (strcmp(track->styles[i].Name, name) == 0)
			return i;
	}
	i = track->default_style;
	mp_msg(MSGT_GLOBAL, MSGL_WARN, "[%p] Warning: no style named '%s' found, using '%s'\n", track, name, track->styles[i].Name);
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
		mp_msg(MSGT_GLOBAL, MSGL_WARN, "bad timestamp\n");
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
		mp_msg(MSGT_GLOBAL, MSGL_DBG2, "%s = %s\n", #name, token);
#define STRVAL(name) ANYVAL(name,strdup)
#define COLORVAL(name) ANYVAL(name,string2color)
#define INTVAL(name) ANYVAL(name,atoi)
#define FPVAL(name) ANYVAL(name,atof)
#define TIMEVAL(name) ANYVAL(name,string2timecode)
#define STYLEVAL(name) \
	} else if (strcasecmp(tname, #name) == 0) { \
		target->name = lookup_style(track, token); \
		mp_msg(MSGT_GLOBAL, MSGL_DBG2, "%s = %s\n", #name, token);

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

	for (i = 0; i < n_ignored; ++i) {
		NEXT(q, tname);
	}

	while (1) {
		NEXT(q, tname);
		if (strcasecmp(tname, "Text") == 0) {
			char* last;
			event->Text = strdup(p);
			last = event->Text + strlen(event->Text) - 1;
			if (*last == '\r')
				*last = 0;
			mp_msg(MSGT_GLOBAL, MSGL_DBG2, "Text = %s\n", event->Text);
			event->Duration -= event->Start;
			free(format);
			return 0; // "Text" is always the last
		}
		NEXT(p, token);

		ALIAS(End,Duration) // temporarily store end timecode in event->Duration
		if (0) { // cool ;)
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
	
	mp_msg(MSGT_GLOBAL, MSGL_V, "[%p] Style: %s\n", track, str);
	
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
			INTVAL(FontSize)
			INTVAL(Bold)
			INTVAL(Italic)
			INTVAL(Underline)
			INTVAL(StrikeOut)
			INTVAL(Spacing)
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
	if (!style->Name)
		style->Name = strdup("Default");
	if (!style->FontName)
		style->FontName = strdup("Arial");
	free(format);
	return 0;
	
}

/**
 * \brief Parse a header line
 * \param track track
 * \param str string to parse, zero-terminated
*/ 
static int process_header_line(ass_track_t* track, char *str)
{
	static int events_section_started = 0;
	
	mp_msg(MSGT_GLOBAL, MSGL_DBG2, "=== Header: %s\n", str);
	if (strncmp(str, "PlayResX:", 9)==0) {
		track->PlayResX = atoi(str + 9);
	} else if (strncmp(str,"PlayResY:", 9)==0) {
		track->PlayResY = atoi(str + 9);
	} else if (strncmp(str,"Timer:", 6)==0) {
		track->Timer = atof(str + 6);
	} else if (strstr(str,"Styles]")) {
		events_section_started = 0;
		if (strchr(str, '+'))
			track->track_type = TRACK_TYPE_ASS;
		else
			track->track_type = TRACK_TYPE_SSA;
	} else if (strncmp(str,"[Events]", 8)==0) {
		events_section_started = 1;
	} else if (strncmp(str,"Format:", 7)==0) {
		char* p = str + 7;
		skip_spaces(&p);
		if (events_section_started) {
			track->event_format = strdup(p);
			mp_msg(MSGT_GLOBAL, MSGL_DBG2, "Event format: %s\n", track->event_format);
		} else {
			track->style_format = strdup(p);
			mp_msg(MSGT_GLOBAL, MSGL_DBG2, "Style format: %s\n", track->style_format);
		}
	} else if (strncmp(str,"Style:", 6)==0) {
		char* p = str + 6;
		skip_spaces(&p);
		process_style(track, p);
	} else if (strncmp(str,"WrapStyle:", 10)==0) {
		track->WrapStyle = atoi(str + 10);
	}
	return 0;
}

/**
 * \brief Process CodecPrivate section of subtitle stream
 * \param track track
 * \param data string to parse
 * \param size length of data
 CodecPrivate section contains [Stream Info] and [V4+ Styles] sections
*/ 
void ass_process_chunk(ass_track_t* track, char *data, int size)
{
	char* str = malloc(size + 1);
	char* p;
	int sid;

	memcpy(str, data, size);
	str[size] = '\0';

	p = str;
	while(1) {
		char* q;
		for (;((*p=='\r')||(*p=='\n'));++p) {}
		for (q=p; ((*q!='\0')&&(*q!='\r')&&(*q!='\n')); ++q) {};
		if (q==p)
			break;
		if (*q != '\0')
			*(q++) = '\0';
		process_header_line(track, p);
		if (*q == '\0')
			break;
		p = q;
	}
	free(str);

	// add "Default" style to the end
	// will be used if track does not contain a default style (or even does not contain styles at all)
	sid = ass_alloc_style(track);
	track->styles[sid].Name = strdup("Default");
	track->styles[sid].FontName = strdup("Arial");
	
	if (!track->event_format) {
		// probably an mkv produced by ancient mkvtoolnix
		// such files don't have [Events] and Format: headers
		if (track->track_type == TRACK_TYPE_SSA)
			track->event_format = strdup("Format: Marked, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text");
		else
			track->event_format = strdup("Format: Layer, Start, End, Style, Actor, MarginL, MarginR, MarginV, Effect, Text");
	}
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
 * \brief Process a chunk of subtitle stream data. In matroska, this containes exactly 1 event (or a commentary)
 * \param track track
 * \param data string to parse
 * \param size length of data
 * \param timecode starting time of the event (milliseconds)
 * \param duration duration of the event (milliseconds)
*/ 
void ass_process_line(ass_track_t* track, char *data, int size, long long timecode, long long duration)
{
	char* str;
	int eid;
	char* p;
	char* token;
	ass_event_t* event;

	if (!track->event_format) {
		mp_msg(MSGT_GLOBAL, MSGL_WARN, "Event format header missing\n");
		return;
	}
	
	str = malloc(size + 1);
	memcpy(str, data, size);
	str[size] = '\0';
	mp_msg(MSGT_GLOBAL, MSGL_V, "\nline at timecode %lld, duration %lld: \n%s\n", timecode, duration, str);

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
	free_event(track, eid);
	free(str);
}

/**
 * \brief Process a line from external file.
 * \param track track
 * \param str string to parse
 * \param size length of data
*/ 
static void ass_process_external_line(ass_track_t* track, char *str, int size)
{
	int eid;
	ass_event_t* event;

	eid = ass_alloc_event(track);
	event = track->events + eid;

	if (strncmp("Dialogue:", str, 9) != 0)
		return;

	str += 9;
	while (*str == ' ') {++str;}
	
	process_event_tail(track, event, str, 0);
}

#ifdef USE_ICONV
/** \brief recode buffer to utf-8
 * constraint: sub_cp != 0
 * \param data pointer to text buffer
 * \param size buffer size
 * \return a pointer to recoded buffer, caller is responsible for freeing it
**/
static char* sub_recode(char* data, size_t size)
{
	static iconv_t icdsc = (iconv_t)(-1);
	char* tocp = "UTF-8";
	char* outbuf;
	assert(sub_cp);

	{
		char* cp_tmp = sub_cp;
#ifdef HAVE_ENCA
		char enca_lang[3], enca_fallback[100];
		if (sscanf(sub_cp, "enca:%2s:%99s", enca_lang, enca_fallback) == 2
				|| sscanf(sub_cp, "ENCA:%2s:%99s", enca_lang, enca_fallback) == 2) {
			cp_tmp = guess_buffer_cp((unsigned char*)data, size, enca_lang, enca_fallback);
		}
#endif
		if ((icdsc = iconv_open (tocp, cp_tmp)) != (iconv_t)(-1)){
			mp_msg(MSGT_SUBREADER,MSGL_V,"LIBSUB: opened iconv descriptor.\n");
		} else
			mp_msg(MSGT_SUBREADER,MSGL_ERR,"LIBSUB: error opening iconv descriptor.\n");
#ifdef HAVE_ENCA
		if (cp_tmp) free(cp_tmp);
#endif
	}

	{
		size_t osize = size;
		size_t ileft = size;
		size_t oleft = size - 1;
		char* ip;
		char* op;
		size_t rc;
		
		outbuf = malloc(size);
		ip = data;
		op = outbuf;
		
		while (ileft) {
			rc = iconv(icdsc, &ip, &ileft, &op, &oleft);
			if (rc == (size_t)(-1)) {
				if (errno == E2BIG) {
					int offset = op - outbuf;
					outbuf = (char*)realloc(outbuf, osize + size);
					op = outbuf + offset;
					osize += size;
					oleft += size;
				} else {
					mp_msg(MSGT_SUBREADER, MSGL_WARN, "LIBSUB: error recoding file.\n");
					return NULL;
				}
			}
		}
		outbuf[osize - oleft - 1] = 0;
	}

	if (icdsc != (iconv_t)(-1)) {
		(void)iconv_close(icdsc);
		icdsc = (iconv_t)(-1);
		mp_msg(MSGT_SUBREADER,MSGL_V,"LIBSUB: closed iconv descriptor.\n");
	}
	
	return outbuf;
}
#endif // ICONV

/**
 * \brief Read subtitles from file.
 * \param fname file name
 * \return newly allocated track
*/ 
ass_track_t* ass_read_file(char* fname)
{
	int res;
	long sz;
	long bytes_read;
	char* buf;
	char* p;
	int events_reached;
	ass_track_t* track;
	
	FILE* fp = fopen(fname, "rb");
	if (!fp) {
		mp_msg(MSGT_GLOBAL, MSGL_WARN, "ass_read_file(%s): fopen failed\n", fname);
		return 0;
	}
	res = fseek(fp, 0, SEEK_END);
	if (res == -1) {
		mp_msg(MSGT_GLOBAL, MSGL_WARN, "ass_read_file(%s): fseek failed\n", fname);
		fclose(fp);
		return 0;
	}
	
	sz = ftell(fp);
	rewind(fp);

	if (sz > 10*1024*1024) {
		mp_msg(MSGT_GLOBAL, MSGL_INFO, "ass_read_file(%s): Refusing to load subtitles larger than 10M\n", fname);
		fclose(fp);
		return 0;
	}
	
	mp_msg(MSGT_GLOBAL, MSGL_V, "file size: %ld\n", sz);
	
	buf = malloc(sz + 1);
	assert(buf);
	bytes_read = 0;
	do {
		res = fread(buf + bytes_read, 1, sz - bytes_read, fp);
		if (res <= 0) {
			mp_msg(MSGT_GLOBAL, MSGL_INFO, "Read failed, %d: %s\n", errno, strerror(errno));
			fclose(fp);
			free(buf);
			return 0;
		}
		bytes_read += res;
	} while (sz - bytes_read > 0);
	buf[sz] = '\0';
	fclose(fp);
	
#ifdef USE_ICONV
	if (sub_cp) {
		char* tmpbuf = sub_recode(buf, sz);
		free(buf);
		if (!tmpbuf)
			return 0;
		buf = tmpbuf;
	}
#endif
	
	track = ass_new_track();
	track->name = strdup(fname);
	
	// process header
	events_reached = 0;
	p = buf;
	while (p && (*p)) {
		while (*p == '\n') {++p;}
		if (strncmp(p, "[Events]", 8) == 0) {
			events_reached = 1;
		} else if ((strncmp(p, "Format:", 7) == 0) && (events_reached)) {
			p = strchr(p, '\n');
			if (p == 0) {
				mp_msg(MSGT_GLOBAL, MSGL_WARN, "Incomplete subtitles\n");
				free(buf);
				return 0;
			}
			ass_process_chunk(track, buf, p - buf + 1);
			++p;
			break;
		}
		p = strchr(p, '\n');
	}
	// process events
	while (p && (*p)) {
		char* next;
		int len;
		while (*p == '\n') {++p;}
		next = strchr(p, '\n');
		len = 0;
		if (next) {
			len = next - p;
			*next = 0;
		} else {
			len = strlen(p);
		}
		ass_process_external_line(track, p, len);
		if (next) {
			p = next + 1;
			continue;
		} else
			break;
	}
	
	free(buf);

	if (!events_reached) {
		ass_free_track(track);
		return 0;
	}

	mp_msg(MSGT_GLOBAL, MSGL_INFO, "LIBASS: added subtitle file: %s (%d styles, %d events)\n", fname, track->n_styles, track->n_events);
	
	sort_events(track);
		
//	dump_events(forced_tid);
	return track;
}

static char* validate_fname(char* name)
{
	char* fname;
	char* p;
	char* q;
	unsigned code;
	int sz = strlen(name);

	q = fname = malloc(sz + 1);
	p = name;
	while (*p) {
		code = utf8_get_char(&p);
		if (code == 0)
			break;
		if (	(code > 0x7F) ||
			(code == '\\') ||
			(code == '/') ||
			(code == ':') ||
			(code == '*') ||
			(code == '?') ||
			(code == '<') ||
			(code == '>') ||
			(code == '|') ||
			(code == 0))
		{
			*q++ = '_';
		} else {
			*q++ = code;
		}
		if (p - name > sz)
			break;
	}
	*q = 0;
	return fname;
}

/**
 * \brief Process embedded matroska font. Saves it to ~/.mplayer/fonts.
 * \param name attachment name
 * \param data binary font data
 * \param data_size data size
*/ 
void ass_process_font(const char* name, char* data, int data_size)
{
	char buf[1000];
	FILE* fp = 0;
	int rc;
	struct stat st;
	char* fname;

	char* fonts_dir = get_path("fonts");
	rc = stat(fonts_dir, &st);
	if (rc) {
		int res;
#ifndef __MINGW32__
		res = mkdir(fonts_dir, 0700);
#else
		res = mkdir(fonts_dir);
#endif
		if (res) {
			mp_msg(MSGT_GLOBAL, MSGL_WARN, "Failed to create: %s\n", fonts_dir);
		}
	} else if (!S_ISDIR(st.st_mode)) {
		mp_msg(MSGT_GLOBAL, MSGL_WARN, "Not a directory: %s\n", fonts_dir);
	}
	
	fname = validate_fname((char*)name);

	snprintf(buf, 1000, "%s/%s", fonts_dir, fname);
	free(fname);
	free(fonts_dir);

	fp = fopen(buf, "wb");
	if (!fp) return;

	fwrite(data, data_size, 1, fp);
	fclose(fp);
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

ass_track_t* ass_new_track(void) {
	ass_track_t* track = calloc(1, sizeof(ass_track_t));
	return track;
}

