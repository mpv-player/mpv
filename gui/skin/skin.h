
#ifndef GUI_SKIN_H
#define GUI_SKIN_H

#include "app.h"

extern listItems     * skinAppMPlayer;

extern int skinRead( char * dname  );
extern int skinBPRead( char * fname, txSample * bf );

// ---

extern char * trimleft( char * in );
extern char * strswap( char * in,char what,char whereof );
extern char * trim( char * in );

#endif /* GUI_SKIN_H */
