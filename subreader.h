#ifndef __MPLAYER_SUBREADER_H
#define __MPLAYER_SUBREADER_H

extern int sub_uses_time;
extern int sub_errs;
extern int sub_num;         // number of subtitle structs

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

// One of the SUB_* constant above
extern int sub_format;

#define SUB_MAX_TEXT 5

typedef struct {

    int lines;

    unsigned long start;
    unsigned long end;
    
    char *text[SUB_MAX_TEXT];
} subtitle;

subtitle* sub_read_file (char *filename, float pts);
char * sub_filename(char *path, char * fname);
void list_sub_file(subtitle* subs);
void dump_mpsub(subtitle* subs, float fps);
void sub_free( subtitle * subs );
void find_sub(subtitle* subtitles,int key);
#endif
