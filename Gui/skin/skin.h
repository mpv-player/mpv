
#ifndef __MY_SKIN
#define __MY_SKIN

#include "../app.h"

extern listItems     * skinAppMPlayer;
//extern listItems     * skinAppTV;
//extern listItems     * skinAppRadio;

extern int skinRead( char * dname  );
extern int skinBPRead( char * fname, txSample * bf );

// ---

extern char * trimleft( char * in );
extern char * strswap( char * in,char what,char whereof );
extern char * trim( char * in );

#endif
