#ifndef __MPLAYER_SUBREADER_H
#define __MPLAYER_SUBREADER_H

extern int suboverlap_enabled;
extern int sub_no_text_pp;  // disable text post-processing
extern int subfuzzy_enabled;

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

#define MAX_SUBTITLE_FILES 128

#define SUB_MAX_TEXT 10

typedef struct {

    int lines;

    unsigned long start;
    unsigned long end;
    
    char *text[SUB_MAX_TEXT];
} subtitle;

typedef struct {
    subtitle *subtitles;
    char *filename;
    int sub_uses_time; 
    int sub_num;          // number of subtitle structs
    int sub_errs;
} sub_data;

sub_data* sub_read_file (char *filename, float pts);
subtitle* subcp_recode1 (subtitle *sub);
void subcp_open (void); /* for demux_ogg.c */
void subcp_close (void); /* for demux_ogg.c */
char ** sub_filenames(char *path, char *fname);
void list_sub_file(sub_data* subd);
void dump_srt(sub_data* subd, float fps);
void dump_mpsub(sub_data* subd, float fps);
void dump_microdvd(sub_data* subd, float fps);
void dump_jacosub(sub_data* subd, float fps);
void dump_sami(sub_data* subd, float fps);
void sub_free( sub_data * subd );
void find_sub(sub_data* subd,int key);
void step_sub(sub_data *subd, float pts, int movement);
#endif
