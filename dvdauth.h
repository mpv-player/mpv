#include "config.h"
#ifdef HAVE_LIBCSS
#ifndef _MPLAYER_CSS_H
#define _MPLAYER_CSS_H

extern char *dvd_device;
extern unsigned char key_disc[];
extern unsigned char key_title[];

int dvd_auth ( char *, int );


#endif
#endif