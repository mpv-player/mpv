
#include <stdlib.h>
#include <stdio.h>

#include <unistd.h>
#include <signal.h>

int    mplParent = 1;

int    mplx,mply,mplwidth,mplheight;

#include "../app.h"

#include "../wm/ws.h"
#include "../wm/wskeys.h"
#include "../wm/widget.h"

#include "../../config.h"
#include "../../libvo/x11_common.h"

#include "widgets.h"
#include "./mplayer.h"
#include "psignal.h"
#include "play.h"

mplCommStruct * mplShMem;
char          * Filename = NULL;

extern float rel_seek_secs;
extern int abs_seek_pos;


void mplPlayerThread( void )
{
// mplayer( 0,NULL,NULL );
}

void mplFullScreen( void )
{
 if ( appMPlayer.subWindow.isFullScreen )
  {
   if ( mplShMem->Playing )
    {
     appMPlayer.subWindow.OldX=mplx;
     appMPlayer.subWindow.OldY=mply;
     appMPlayer.subWindow.OldWidth=mplwidth;
     appMPlayer.subWindow.OldHeight=mplheight;
    }
    else
     {
      appMPlayer.subWindow.OldWidth=appMPlayer.sub.width;
      appMPlayer.subWindow.OldHeight=appMPlayer.sub.height;
      appMPlayer.subWindow.OldX=( wsMaxX - appMPlayer.sub.width ) / 2;
      appMPlayer.subWindow.OldY=( wsMaxY - appMPlayer.sub.height ) / 2;
     }
  }
  else
   {
     mplx=appMPlayer.subWindow.X;
     mply=appMPlayer.subWindow.Y;
     mplwidth=appMPlayer.subWindow.Width;
     mplheight=appMPlayer.subWindow.Height;
   }
 wsFullScreen( &appMPlayer.subWindow );
// wsMoveTopWindow( &appMPlayer.subWindow );
 mplResize( 0,0,appMPlayer.subWindow.Width,appMPlayer.subWindow.Height );
}

extern int mplSubRender;

void mplStop()
{
 if ( !mplShMem->Playing ) return;
// ---
printf("%%%%%% STOP  \n");
// ---
 mplShMem->Playing=0;
}

void mplPlay( void )
{
 if ( !Filename ) return;
 if ( mplShMem->Playing ) mplStop();
// ---
printf("%%%%%% PLAY  \n");
// ---
 mplShMem->Playing=1;
}

void mplPause( void )
{
 if ( mplShMem->Playing != 1 ) return;
// ---
printf("%%%%%% PAUSE  \n");
// ---
 mplShMem->Playing=2;
}

void mplResize( unsigned int X,unsigned int Y,unsigned int width,unsigned int height )
{

printf("mplResize(%d,%d,%d,%d)  \n",X,Y,width,height);
	vo_setwindowsize( width,height );
        vo_resize=1;

}

void mplMPlayerInit( int argc,char* argv[], char *envp[] )
{
#if 0
 mplShMem=shmem_alloc( ShMemSize );
#else
 mplShMem=calloc( 1,ShMemSize );
#endif
 signal( SIGTYPE,mplMainSigHandler );
// signal( SIGCHLD,SIG_IGN );

 mplShMem->Playing=0;
 mplShMem->Volume=0.0f;
 mplShMem->Position=0.0f;
 mplShMem->Balance=50.0f;
 mplShMem->Track=0;
 mplShMem->AudioType=0;
 mplShMem->StreamType=0;
 mplShMem->TimeSec=0;
 mplShMem->LengthInSec=0;

// ---
// ---
}

float mplGetPosition( void )
{ // return 0.0 ... 100.0
 return mplShMem->Position;
}

void mplRelSeek( float s )
{ // -+s
// ---
//printf("%%%%%% RelSEEK=%5.3f  \n",s);
// ---
 rel_seek_secs=s; abs_seek_pos=0;
}

void mplAbsSeek( float s )
{ // 0.0 ... 100.0
// ---
//printf("%%%%%% AbsSEEK=%5.3f  \n",s);
 rel_seek_secs=0.01*s; abs_seek_pos=3;
// ---
}

void mplIncAudioBufDelay( void )
{
}

void mplDecAudioBufDelay( void )
{
}
