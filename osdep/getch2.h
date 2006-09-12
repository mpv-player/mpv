/* GyS-TermIO v2.0 (for GySmail v3)          (C) 1999 A'rpi/ESP-team */
/* a very small replacement of ncurses library */

/* Screen size. Initialized by load_termcap() and get_screen_size() */
extern int screen_width;
extern int screen_height;

/* Termcap code to erase to end of line */
extern char * erase_to_end_of_line;

/* Get screen-size using IOCTL call. */
extern void get_screen_size(void);

/* Load key definitions from the TERMCAP database. 'termtype' can be NULL */
extern int load_termcap(char *termtype);

/* Enable and disable STDIN line-buffering */
extern void getch2_enable(void);
extern void getch2_disable(void);

/* Read a character or a special key code (see keycodes.h) */
extern int getch2(int halfdelay_time);

#ifdef __MINGW32__
extern int mp_input_win32_slave_cmd_func(int fd,char* dest,int size);
#endif
