

#include "ws.h"
#include "mplayer/play.h"
#include "interface.h"
#include "../mplayer.h"

void guiGetEvent( int type,char * arg )
{
 switch ( type )
  {
   case guiXEvent:
        wsEvents( wsDisplay,(XEvent *)arg,NULL );
	break;
   case guiCEvent:
	break;
  }
}

void guiEventHandling( void )
{
 if ( use_gui && !mplShMem->Playing ) wsHandleEvents();
 mplTimerHandler(0); // handle GUI timer events
 if ( mplShMem->SkinChange ) { ChangeSkin(); mplShMem->SkinChange=0;  }
}
