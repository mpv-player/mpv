
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "ws.h"
#include "mplayer/play.h"
#include "interface.h"

#include "../mplayer.h"
#include "mplayer/widgets.h"
#include "mplayer/mplayer.h"
#include "app.h"
#include "../libvo/x11_common.h"

guiInterface_t guiIntfStruct;

void guiInit( int argc,char* argv[], char *envp[] )
{
 memset( &guiIntfStruct,0,sizeof( guiIntfStruct ) );
 appInit( argc,argv,envp,(void*)mDisplay );
}

void guiDone( void )
{
 mp_msg( MSGT_GPLAYER,MSGL_V,"[mplayer] exit.\n" );
 mplStop();
 gtkDone();
 wsXDone();
}

void guiGetEvent( int type,char * arg )
{
 switch ( type )
  {
   case guiXEvent:
        wsEvents( wsDisplay,(XEvent *)arg,NULL );
        gtkEventHandling();
        break;
   case guiCEvent:
        break;
  }
}

void guiEventHandling( void )
{
 if ( use_gui && !guiIntfStruct.Playing ) wsHandleEvents();
 gtkEventHandling();
 mplTimerHandler(); // handle GUI timer events
}
