#ifndef __MPLAYER_SUBREADER_H
#define __MPLAYER_SUBREADER_H

extern int sub_uses_time;
extern int sub_errs;
extern int sub_num;         // number of subtitle structs
extern int suboverlap_enabled;
extern int sub_no_text_pp;  // disable text post-processing

// subtitle formats
#define SUB_INVALID   -1
#define SUB_MICRODVD  0
#define SUB_SUBRIP    1
#define SUB_SUBVIEWER 2
#define SUB_SAMI      3
#define SUB_VPLAYER   4
#define SUB_RT        5
#define SUB_SSA       6
#define SUB_DUNNOWHAT 7		// FIXME what format is it ?
#define SUB_MPSUB     8
#define SUB_AQTITLE   9
#define SUB_SUBVIEWER2 10
#define SUB_SUBRIP09 11
#define SUB_JACOSUB  12

// One of the SUB_* constant above
extern int sub_format;

#define SUB_MAX_TEXT 10

typedef struct {

    int lines;

    unsigned long start;
    unsigned long end;
    
    char *text[SUB_MAX_TEXT];
} subtitle;

subtitle* sub_read_file (char *filename, float pts);
subtitle* subcp_recode1 (subtitle *sub);
void subcp_open (void); /* for demux_ogg.c */
void subcp_close (void); /* for demux_ogg.c */
char * sub_filename(char *path, char * fname);
void list_sub_file(subtitle* subs);
void dump_srt(subtitle* subs, float fps);
void dump_mpsub(subtitle* subs, float fps);
void dump_microdvd(subtitle* subs, float fps);
void dump_jacosub(subtitle* subs, float fps);
void dump_sami(subtitle* subs, float fps);
void sub_free( subtitle * subs );
void find_sub(subtitle* subtitles,int key);
void step_sub(subtitle *subtitles, float pts, int movement);
#endif
