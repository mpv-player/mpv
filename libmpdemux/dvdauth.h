#ifdef HAVE_LIBCSS
#ifndef _MPLAYER_CSS_H
#define _MPLAYER_CSS_H

extern char *dvd_auth_device;
extern unsigned char key_disc[];
extern unsigned char key_title[];
extern unsigned char *dvdimportkey;
extern int descrambling;
extern char *css_so;

int dvd_auth ( char *, char * );
int dvd_import_key ( unsigned char * );
int dvd_css_descramble ( u_char *, u_char * );

#endif
#endif
