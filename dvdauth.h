#include "config.h"
#ifdef HAVE_LIBCSS
#ifndef _MPLAYER_CSS_H
#define _MPLAYER_CSS_H

extern char *dvd_auth_device;
extern unsigned char key_disc[];
extern unsigned char key_title[];
extern unsigned char *dvdimportkey;
extern int descrambling;

int dvd_auth ( char *, int );
int dvd_import_key ( unsigned char * );

#endif
#endif