 
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "ws.h"
#include "mplayer/play.h"
#include "interface.h"
#include "skin/skin.h"

#include "../mplayer.h"
#include "mplayer/widgets.h"
#include "mplayer/mplayer.h"
#include "app.h"
#include "../libvo/x11_common.h"
#include "../libvo/video_out.h"
#include "../input/input.h"
#include "../libao2/audio_out.h"

#include <inttypes.h>
#include <sys/types.h>

#include "../libmpdemux/stream.h"
#include "../libmpdemux/demuxer.h"

guiInterface_t guiIntfStruct;

void guiInit( void )
{
 memset( &guiIntfStruct,0,sizeof( guiIntfStruct ) );
 appInit( (void*)mDisplay );
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

typedef struct 
{
 demux_stream_t *ds;
 unsigned int format;
 struct codecs_st *codec;
 int inited;
 // output format:                                                                                
 float timer;
 float fps;
 float frametime;
 int i_bps;
 int disp_w,disp_h;
} tmp_sh_video_t;

extern ao_functions_t * audio_out;

void guiGetEvent( int type,char * arg )
{
 stream_t * stream = (stream_t *) arg;
#ifdef USE_DVDREAD
 dvd_priv_t * dvdp = (dvd_priv_t *) arg;
#endif 

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
   case guiSetState:
	mplState();
        break;
   case guiSetFileName:
        if ( arg ) guiSetFilename( guiIntfStruct.Filename,arg );
        break;
   case guiSetAudioOnly:
	guiIntfStruct.AudioOnly=(int)arg;
	if ( (int)arg ) wsVisibleWindow( &appMPlayer.subWindow,wsHideWindow );
	  else wsVisibleWindow( &appMPlayer.subWindow,wsShowWindow );
	break;
   case guiReDrawSubWindow:
	wsPostRedisplay( &appMPlayer.subWindow );
	if ( guiIntfStruct.Playing == 1 ) wsSetBackgroundRGB( &appMPlayer.subWindow,0,0,0 );
	break;
   case guiSetShVideo:
	 {
	  if ( !appMPlayer.subWindow.isFullScreen )
	   {
	    wsResizeWindow( &appMPlayer.subWindow,vo_dwidth,vo_dheight );
            wsMoveWindow( &appMPlayer.subWindow,True,appMPlayer.sub.x,appMPlayer.sub.y );
	   }
	  guiIntfStruct.MovieWidth=vo_dwidth;
	  guiIntfStruct.MovieHeight=vo_dheight;
         }
	break;
#ifdef USE_DVDREAD
   case guiSetDVD:
        guiIntfStruct.DVD.titles=dvdp->vmg_file->tt_srpt->nr_of_srpts;
        guiIntfStruct.DVD.chapters=dvdp->vmg_file->tt_srpt->title[dvd_title].nr_of_ptts;
        guiIntfStruct.DVD.angles=dvdp->vmg_file->tt_srpt->title[dvd_title].nr_of_angles;
        guiIntfStruct.DVD.nr_of_audio_channels=dvdp->nr_of_channels;
        memcpy( guiIntfStruct.DVD.audio_streams,dvdp->audio_streams,sizeof( dvdp->audio_streams ) );
        guiIntfStruct.DVD.nr_of_subtitles=dvdp->nr_of_subtitles;
        memcpy( guiIntfStruct.DVD.subtitles,dvdp->subtitles,sizeof( dvdp->subtitles ) );
        guiIntfStruct.DVD.current_title=dvd_title + 1;
        guiIntfStruct.DVD.current_chapter=dvd_chapter + 1;
        guiIntfStruct.DVD.current_angle=dvd_angle + 1;
        guiIntfStruct.Track=dvd_title + 1;
        break;
#endif
   case guiSetStream:
	guiIntfStruct.StreamType=stream->type;
	switch( stream->type )
	 {
	  case STREAMTYPE_DVD: 
	       guiGetEvent( guiSetDVD,(char *)stream->priv );
	       break;
#ifdef HAVE_VCD
	  case STREAMTYPE_VCD: 
	       {
	        int i;
		for ( i=1;i < 100;i++ )
		  if ( vcd_seek_to_track( stream->fd,i ) < 0 ) break;
		vcd_seek_to_track( stream->fd,vcd_track );
		guiIntfStruct.VCDTracks=--i;
		mp_msg( MSGT_GPLAYER,MSGL_INFO,"[interface] vcd tracks: %d\n",guiIntfStruct.VCDTracks );
		guiIntfStruct.Track=vcd_track;
	        break;
	       }
#endif
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
   case guiClearStruct:
#ifdef USE_DVDREAD
	if ( (unsigned int)arg & guiDVD ) memset( &guiIntfStruct.DVD,0,sizeof( guiDVDStruct ) );
#endif
#ifdef HAVE_VCD
	if ( (unsigned int)arg & guiVCD ) guiIntfStruct.VCDTracks=0;
#endif
	break;
   case guiReDraw:
	mplEventHandling( evRedraw,0 );
	break;
   case guiSetVolume:
        if ( audio_out )
	{
	 float l,r;
	 mixer_getvolume( &l,&r );
	 guiIntfStruct.Volume=(r>l?r:l);
	 if ( r != l ) guiIntfStruct.Balance=( ( r - l ) + 100 ) * 0.5f;
	   else guiIntfStruct.Balance=0.0f;
	 btnModify( evSetVolume,guiIntfStruct.Volume );
	 btnModify( evSetBalance,guiIntfStruct.Balance );
	}
        break;
  }
}

extern unsigned int GetTimerMS( void );
extern int mplTimer;

void guiEventHandling( void )
{
 if ( !guiIntfStruct.Playing || guiIntfStruct.AudioOnly ) wsHandleEvents();
 gtkEventHandling();
 mplTimer=GetTimerMS() / 20;
// if ( !( GetTimerMS()%2 ) ) 
}
