#ifndef MPLAYER_GUI_SKIN_H
#define MPLAYER_GUI_SKIN_H

#include "app.h"

extern listItems     * skinAppMPlayer;

extern int skinRead( char * dname  );
extern int skinBPRead( char * fname, txSample * bf );

// ---

extern char * trimleft( char * in );
extern char * strswap( char * in,char what,char whereof );
extern char * trim( char * in );

#endif /* MPLAYER_GUI_SKIN_H */
