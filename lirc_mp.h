/*

 definitions for LIRC support in mplayer
 written in 2/2001 by Andreas Ackermann
 acki@acki-netz.de

*/

#ifndef LIRC_MP_H_
#define LIRC_MP_H_

#include <lirc/lirc_client.h>

//extern struct lirc_config *lirc_config;
//extern int    lirc_is_setup;


void lirc_mp_setup(void);
void lirc_mp_cleanup(void);
int  lirc_mp_getinput(void);


#endif
