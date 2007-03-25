#ifndef __MPLAYER_SUBREADER_H
#define __MPLAYER_SUBREADER_H

#include <stdio.h>

extern int suboverlap_enabled;
extern int sub_no_text_pp;  // disable text post-processing
extern int sub_match_fuzziness;

// subtitle formats
#define SUB_INVALID   -1
#define SUB_MICRODVD  0
#define SUB_SUBRIP    1
#define SUB_SUBVIEWER 2
#define SUB_SAMI      3
#define SUB_VPLAYER   4
#define SUB_RT        5
#define SUB_SSA       6
#define SUB_PJS       7
#define SUB_MPSUB     8
#define SUB_AQTITLE   9
#define SUB_SUBVIEWER2 10
#define SUB_SUBRIP09 11
#define SUB_JACOSUB  12
#define SUB_MPL2     13

// One of the SUB_* constant above
extern int sub_format;

#define MAX_SUBTITLE_FILES 128

#define SUB_MAX_TEXT 12
#define SUB_ALIGNMENT_BOTTOMLEFT       1
#define SUB_ALIGNMENT_BOTTOMCENTER     2
#define SUB_ALIGNMENT_BOTTOMRIGHT      3
#define SUB_ALIGNMENT_MIDDLELEFT       4
#define SUB_ALIGNMENT_MIDDLECENTER     5
#define SUB_ALIGNMENT_MIDDLERIGHT      6
#define SUB_ALIGNMENT_TOPLEFT          7
#define SUB_ALIGNMENT_TOPCENTER        8
#define SUB_ALIGNMENT_TOPRIGHT         9

typedef struct {

    int lines;

    unsigned long start;
    unsigned long end;
    
    char *text[SUB_MAX_TEXT];
    double endpts[SUB_MAX_TEXT];
    unsigned char alignment;
} subtitle;

typedef struct {
    subtitle *subtitles;
    char *filename;
    int sub_uses_time; 
    int sub_num;          // number of subtitle structs
    int sub_errs;
} sub_data;

#ifdef  USE_FRIBIDI
extern char *fribidi_charset;
extern int flip_hebrew;
extern int fribidi_flip_commas;
#endif

sub_data* sub_read_file (char *filename, float pts);
subtitle* subcp_recode (subtitle *sub);
// enca_fd is the file enca uses to determine the codepage.
// setting to NULL disables enca.
struct stream_st;
void subcp_open (struct stream_st *st); /* for demux_ogg.c */
void subcp_close (void); /* for demux_ogg.c */
#ifdef HAVE_ENCA
void* guess_buffer_cp(unsigned char* buffer, int buflen, char *preferred_language, char *fallback);
void* guess_cp(struct stream_st *st, char *preferred_language, char *fallback);
#endif
char ** sub_filenames(const char *path, char *fname);
void list_sub_file(sub_data* subd);
void dump_srt(sub_data* subd, float fps);
void dump_mpsub(sub_data* subd, float fps);
void dump_microdvd(sub_data* subd, float fps);
void dump_jacosub(sub_data* subd, float fps);
void dump_sami(sub_data* subd, float fps);
void sub_free( sub_data * subd );
void find_sub(sub_data* subd,int key);
void step_sub(sub_data *subd, float pts, int movement);
void sub_add_text(subtitle *sub, const char *txt, int len, double endpts);
int sub_clear_text(subtitle *sub, double pts);
#endif
