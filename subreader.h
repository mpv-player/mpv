
#ifndef __MPLAYER_SUBREADER_H
#define __MPLAYER_SUBREADER_H

extern int sub_uses_time;
extern int sub_errs;
extern int sub_num;         // number of subtitle structs
extern int sub_format;     // 0 for microdvd
			  // 1 for SubRip
			 // 2 for the third format
			// 3 for SAMI (smi)
		       // 4 for vplayer format

#define SUB_MAX_TEXT 5

typedef struct {

    int lines;

    unsigned long start;
    unsigned long end;
    
    char *text[SUB_MAX_TEXT];
} subtitle;

subtitle* sub_read_file (char *filename);
char * sub_filename(char *path, char * fname );

#endif
