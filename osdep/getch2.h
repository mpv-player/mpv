/* GyS-TermIO v2.0 (for GySmail v3)          (C) 1999 A'rpi/ESP-team */
/* a very small replacement of ncurses library */

#ifndef MPLAYER_GETCH2_H
#define MPLAYER_GETCH2_H

/* Screen size. Initialized by load_termcap() and get_screen_size() */
extern int screen_width;
extern int screen_height;

/* Termcap code to erase to end of line */
extern char * erase_to_end_of_line;

/* Get screen-size using IOCTL call. */
void get_screen_size(void);

/* Load key definitions from the TERMCAP database. 'termtype' can be NULL */
int load_termcap(char *termtype);

/* Enable and disable STDIN line-buffering */
void getch2_enable(void);
void getch2_disable(void);

/* Read a character or a special key code (see keycodes.h) */
void getch2(void);

/* slave cmd function for Windows and OS/2 */
int mp_input_slave_cmd_func(int fd,char* dest,int size);

#if defined(__MINGW32__) || defined(__OS2__)
#define USE_SELECT  0
#define MP_INPUT_SLAVE_CMD_FUNC     mp_input_slave_cmd_func
#else
#define USE_SELECT  1
#define MP_INPUT_SLAVE_CMD_FUNC     NULL
#endif

#endif /* MPLAYER_GETCH2_H */
