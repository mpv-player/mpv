
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
#include "../input/input.h"

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

int guiCMDArray[] =
 {
  evLoad,
  evLoadSubtitle,
  evAbout,
  evPlay,
  evStop,
  evPlayList,
  evPreferences,
  evFullScreen,
  evSkinBrowser
 };

void guiGetEvent( int type,char * arg )
{
 switch ( type )
  {
   case guiXEvent:
        wsEvents( wsDisplay,(XEvent *)arg,NULL );
        gtkEventHandling();
        break;
   case guiCEvent:
        switch ( (int)arg )
	 {
          case guiSetPlay:  guiIntfStruct.Playing=1; mplState(); break;
          case guiSetStop:  guiIntfStruct.Playing=0; mplState(); break;
          case guiSetPause: guiIntfStruct.Playing=2; mplState(); break;
	 }
        break;
#ifdef HAVE_NEW_INPUT
   case guiIEvent:
        printf( "cmd: %d\n",(int)arg );
	switch( (int)arg )
	 {
          case MP_CMD_QUIT:
	       mplEventHandling( evExit,0 );
	       break;
	  case MP_CMD_VO_FULLSCREEN:
	       mplEventHandling( evFullScreen,0 );
	       break;
          default:
	       mplEventHandling( guiCMDArray[ (int)arg - MP_CMD_GUI_EVENTS - 1 ],0 );
	 }
	break;
#endif
  }
}

void guiEventHandling( void )
{
 if ( use_gui && !guiIntfStruct.Playing ) wsHandleEvents();
 gtkEventHandling();
 mplTimerHandler(); // handle GUI timer events
}
