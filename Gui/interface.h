
#ifndef _INTERFACE_H
#define _INTERFACE_H

#include "mplayer/play.h"
#include "../mplayer.h"

#define guiXEvent 0
#define guiCEvent 1

extern void guiGetEvent( int type,char * arg );
extern void guiEventHandling( void );

#endif