/*
 * main window
 *
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include "config.h"
#include "gmplayer.h"
#include "gui/app.h"
#include "gui/skin/font.h"
#include "gui/skin/skin.h"
#include "gui/wm/ws.h"

#include "help_mp.h"
#include "libvo/x11_common.h"
#include "libvo/fastmemcpy.h"

#include "stream/stream.h"
#include "stream/url.h"
#include "mixer.h"
#include "libvo/sub.h"
#include "access_mpcontext.h"

#include "libmpdemux/demuxer.h"
#include "libmpdemux/stheader.h"
#include "codec-cfg.h"
#include "m_option.h"
#include "m_property.h"

#define GUI_REDRAW_WAIT 375

#include "play.h"
#include "widgets.h"

unsigned int GetTimerMS( void );

unsigned char * mplDrawBuffer = NULL;
int             mplMainRender = 1;

int             mplMainAutoPlay = 0;
int             mplMiddleMenu = 0;

int             mainVisible = 1;

int             boxMoved = 0;
int             sx = 0,sy = 0;
int             i,pot = 0;

#include "gui_common.h"

void mplMainDraw( void )
{

 if ( appMPlayer.mainWindow.State == wsWindowClosed ) exit_player( MSGTR_Exit_quit );
 
 if ( appMPlayer.mainWindow.Visible == wsWindowNotVisible ||
      !mainVisible ) return;
//      !appMPlayer.mainWindow.Mapped ) return;

 if ( mplMainRender && appMPlayer.mainWindow.State == wsWindowExpose )
  {
   btnModify( evSetMoviePosition,guiIntfStruct.Position );
   btnModify( evSetVolume,guiIntfStruct.Volume );

   fast_memcpy( mplDrawBuffer,appMPlayer.main.Bitmap.Image,appMPlayer.main.Bitmap.ImageSize );
   Render( &appMPlayer.mainWindow,appMPlayer.Items,appMPlayer.NumberOfItems,mplDrawBuffer,appMPlayer.main.Bitmap.ImageSize );
   mplMainRender=0;
  }
 wsPutImage( &appMPlayer.mainWindow );
// XFlush( wsDisplay );
}

static unsigned last_redraw_time = 0;

void mplEventHandling( int msg,float param )
{
 int iparam = (int)param;
 mixer_t *mixer = mpctx_get_mixer(guiIntfStruct.mpcontext);

 switch( msg )
  {
// --- user events
   case evExit:
        exit_player( "Exit" );
        break;

   case evPlayNetwork:
        gfree( (void **)&guiIntfStruct.Subtitlename );
	gfree( (void **)&guiIntfStruct.AudioFile );
	guiIntfStruct.StreamType=STREAMTYPE_STREAM;
        goto play;
   case evSetURL:
        gtkShow( evPlayNetwork,NULL );
	break;

   case evSetAudio:
        if ( !guiIntfStruct.demuxer || audio_id == iparam ) break;
	audio_id=iparam;
	goto play;

   case evSetVideo:
        if ( !guiIntfStruct.demuxer || video_id == iparam ) break;
	video_id=iparam;
	goto play;

   case evSetSubtitle:
        mp_property_do("sub",M_PROPERTY_SET,&iparam,guiIntfStruct.mpcontext); 
	break;

#ifdef CONFIG_VCD
   case evSetVCDTrack:
        guiIntfStruct.Track=iparam;
   case evPlayVCD:
 	gtkSet( gtkClearStruct,0,(void *)guiALL );
	guiIntfStruct.StreamType=STREAMTYPE_VCD;
	goto play;
#endif
#ifdef CONFIG_DVDREAD
   case evPlayDVD:
        guiIntfStruct.DVD.current_title=1;
        guiIntfStruct.DVD.current_chapter=1;
        guiIntfStruct.DVD.current_angle=1;
play_dvd_2:
 	gtkSet( gtkClearStruct,0,(void *)(guiALL - guiDVD) );
        guiIntfStruct.StreamType=STREAMTYPE_DVD;
	goto play;
#endif
   case evPlay:
   case evPlaySwitchToPause:
play:

        if ( ( msg == evPlaySwitchToPause )&&( guiIntfStruct.Playing == 2 ) ) goto NoPause;

	if ( gtkSet( gtkGetCurrPlItem,0,NULL ) &&( guiIntfStruct.StreamType == STREAMTYPE_FILE ) )
	 {
	  plItem * next = gtkSet( gtkGetCurrPlItem,0,NULL );
	  plLastPlayed=next;
	  mplSetFileName( next->path,next->name,STREAMTYPE_FILE );
	 }

        switch ( guiIntfStruct.StreamType )
         {
	  case STREAMTYPE_STREAM:
	  case STREAMTYPE_FILE:
	       gtkSet( gtkClearStruct,0,(void *)(guiALL - guiFilenames) );
	       break;
#ifdef CONFIG_VCD
          case STREAMTYPE_VCD:
	       gtkSet( gtkClearStruct,0,(void *)(guiALL - guiVCD - guiFilenames) );
	       if ( !cdrom_device ) cdrom_device=gstrdup( DEFAULT_CDROM_DEVICE );
	       mplSetFileName( NULL,cdrom_device,STREAMTYPE_VCD );
	       if ( guiIntfStruct.Playing != 2 )
	        {
		 if ( !guiIntfStruct.Track )
		  {
		   if ( guiIntfStruct.VCDTracks > 1 ) guiIntfStruct.Track=2;
		    else guiIntfStruct.Track=1;
		  }
                 guiIntfStruct.DiskChanged=1;
		}
	       break;
#endif
#ifdef CONFIG_DVDREAD
          case STREAMTYPE_DVD:
	       gtkSet( gtkClearStruct,0,(void *)(guiALL - guiDVD - guiFilenames) );
	       if ( !dvd_device ) dvd_device=gstrdup( DEFAULT_DVD_DEVICE );
	       mplSetFileName( NULL,dvd_device,STREAMTYPE_DVD );
	       if ( guiIntfStruct.Playing != 2 )
	        {
		 guiIntfStruct.Title=guiIntfStruct.DVD.current_title;
		 guiIntfStruct.Chapter=guiIntfStruct.DVD.current_chapter;
		 guiIntfStruct.Angle=guiIntfStruct.DVD.current_angle;
                 guiIntfStruct.DiskChanged=1;
		} 
               break;
#endif
         }
	guiIntfStruct.NewPlay=1;
        mplPlay();
        break;
#ifdef CONFIG_DVDREAD
   case evSetDVDSubtitle:
        dvdsub_id=iparam;
        goto play_dvd_2;
        break;
   case evSetDVDAudio:
        audio_id=iparam;
        goto play_dvd_2;
        break;
   case evSetDVDChapter:
        guiIntfStruct.DVD.current_chapter=iparam;
        goto play_dvd_2;
        break;
   case evSetDVDTitle:
        guiIntfStruct.DVD.current_title=iparam;
	guiIntfStruct.DVD.current_chapter=1;
	guiIntfStruct.DVD.current_angle=1;
        goto play_dvd_2;
        break;
#endif

   case evPause:
   case evPauseSwitchToPlay:
NoPause:
        mplPause();
        break;

   case evStop: 
	guiIntfStruct.Playing=guiSetStop; 
	mplState(); 
	guiIntfStruct.NoWindow=False;
	break;

   case evLoadPlay:
        mplMainAutoPlay=1;
//	guiIntfStruct.StreamType=STREAMTYPE_FILE;
   case evLoad:
	gtkSet( gtkDelPl,0,NULL );
        gtkShow( evLoad,NULL );
        break;
   case evLoadSubtitle:  gtkShow( evLoadSubtitle,NULL );  break;
   case evDropSubtitle:
	gfree( (void **)&guiIntfStruct.Subtitlename );
	guiLoadSubtitle( NULL );
	break;
   case evLoadAudioFile: gtkShow( evLoadAudioFile,NULL ); break;
   case evPrev: mplPrev(); break;
   case evNext: mplNext(); break;

   case evPlayList:    gtkShow( evPlayList,NULL );        break;
   case evSkinBrowser: gtkShow( evSkinBrowser,skinName ); break;
   case evAbout:       gtkShow( evAbout,NULL );           break;
   case evPreferences: gtkShow( evPreferences,NULL );     break;
   case evEqualizer:   gtkShow( evEqualizer,NULL );       break;

   case evForward10min:	    mplRelSeek( 600 ); break;
   case evBackward10min:    mplRelSeek( -600 );break;
   case evForward1min:      mplRelSeek( 60 );  break;
   case evBackward1min:     mplRelSeek( -60 ); break;
   case evForward10sec:     mplRelSeek( 10 );  break;
   case evBackward10sec:    mplRelSeek( -10 ); break;
   case evSetMoviePosition: mplAbsSeek( param ); break;

   case evIncVolume:  vo_x11_putkey( wsGrayMul ); break;
   case evDecVolume:  vo_x11_putkey( wsGrayDiv ); break;
   case evMute:       mixer_mute( mixer ); break;

   case evSetVolume:
        guiIntfStruct.Volume=param;
	goto set_volume;
   case evSetBalance: 
        guiIntfStruct.Balance=param;
set_volume:
        {
	 float l = guiIntfStruct.Volume * ( ( 100.0 - guiIntfStruct.Balance ) / 50.0 );
	 float r = guiIntfStruct.Volume * ( ( guiIntfStruct.Balance ) / 50.0 );
	 if ( l > guiIntfStruct.Volume ) l=guiIntfStruct.Volume;
	 if ( r > guiIntfStruct.Volume ) r=guiIntfStruct.Volume;
//	 printf( "!!! v: %.2f b: %.2f -> %.2f x %.2f\n",guiIntfStruct.Volume,guiIntfStruct.Balance,l,r );
         mixer_setvolume( mixer,l,r );
	}
	if ( osd_level )
	 {
	  osd_visible=(GetTimerMS() + 1000) | 1;
	  vo_osd_progbar_type=OSD_VOLUME;
	  vo_osd_progbar_value=( ( guiIntfStruct.Volume ) * 256.0 ) / 100.0;
	  vo_osd_changed( OSDTYPE_PROGBAR );
	 }
        break;


   case evIconify:
        switch ( iparam )
         {
          case 0: wsIconify( appMPlayer.mainWindow ); break;
          case 1: wsIconify( appMPlayer.subWindow ); break;
         }
        break;
   case evHalfSize:
        btnSet( evFullScreen,btnReleased );
        if ( guiIntfStruct.Playing )
         {
          if ( appMPlayer.subWindow.isFullScreen )
           {
            mplFullScreen();
           }
          wsResizeWindow( &appMPlayer.subWindow, guiIntfStruct.MovieWidth / 2, guiIntfStruct.MovieHeight / 2 );
          wsMoveWindow( &appMPlayer.subWindow, 0,
                        ( wsMaxX - guiIntfStruct.MovieWidth/2  )/2 + wsOrgX,
                        ( wsMaxY - guiIntfStruct.MovieHeight/2 )/2 + wsOrgY  );
         }
        break;
   case evDoubleSize:
    	btnSet( evFullScreen,btnReleased );
        if ( guiIntfStruct.Playing )
         {
          if ( appMPlayer.subWindow.isFullScreen )
           {
            mplFullScreen();
           }
          wsResizeWindow( &appMPlayer.subWindow, guiIntfStruct.MovieWidth * 2, guiIntfStruct.MovieHeight * 2 );
          wsMoveWindow( &appMPlayer.subWindow, 0,
                        ( wsMaxX - guiIntfStruct.MovieWidth*2  )/2 + wsOrgX,
                        ( wsMaxY - guiIntfStruct.MovieHeight*2 )/2 + wsOrgY  );
         }
        break;
   case evNormalSize:
	btnSet( evFullScreen,btnReleased );
        if ( guiIntfStruct.Playing )
         {
          if ( appMPlayer.subWindow.isFullScreen )
           {
            mplFullScreen();
           }
          wsResizeWindow( &appMPlayer.subWindow, guiIntfStruct.MovieWidth, guiIntfStruct.MovieHeight );
          wsMoveWindow( &appMPlayer.subWindow, 0,
                        ( wsMaxX - guiIntfStruct.MovieWidth  )/2 + wsOrgX,
                        ( wsMaxY - guiIntfStruct.MovieHeight )/2 + wsOrgY  );
	  break;
         } else if ( !appMPlayer.subWindow.isFullScreen ) break;
   case evFullScreen:
        if ( !guiIntfStruct.Playing && !gtkShowVideoWindow ) break;
        mplFullScreen();
	if ( appMPlayer.subWindow.isFullScreen ) btnSet( evFullScreen,btnPressed );
	 else btnSet( evFullScreen,btnReleased );
        break;

   case evSetAspect:
	switch ( iparam )
	 {
	  case 2:  movie_aspect=16.0f / 9.0f; break;
	  case 3:  movie_aspect=4.0f / 3.0f;  break;
	  case 4:  movie_aspect=2.35;         break;
	  case 1:
	  default: movie_aspect=-1;
	 }
	wsClearWindow( appMPlayer.subWindow );
#ifdef CONFIG_DVDREAD
	if ( guiIntfStruct.StreamType == STREAMTYPE_DVD || guiIntfStruct.StreamType == STREAMTYPE_VCD ) goto play_dvd_2;
	 else 
#endif
	 guiIntfStruct.NewPlay=1;
	break;

// --- timer events
   case evRedraw:
        {
          unsigned now = GetTimerMS();
          extern int mplPBFade;
          if ((now > last_redraw_time) &&
              (now < last_redraw_time + GUI_REDRAW_WAIT) &&
              !mplPBFade)
            break;
          last_redraw_time = now;
        }
        mplMainRender=1;
        wsPostRedisplay( &appMPlayer.mainWindow );
	wsPostRedisplay( &appMPlayer.barWindow );
        break;
// --- system events
#ifdef MP_DEBUG
   case evNone:
        mp_msg( MSGT_GPLAYER,MSGL_STATUS,"[mw] event none received.\n" );
        break;
   default:
        mp_msg( MSGT_GPLAYER,MSGL_STATUS,"[mw] unknown event received ( %d,%.2f ).\n",msg,param );
        break;
#endif
  }
}

#define itPLMButton (itNULL - 1)
#define itPRMButton (itNULL - 2)

void mplMainMouseHandle( int Button,int X,int Y,int RX,int RY )
{
 static int     itemtype = 0;
        int     i;
        wItem * item = NULL;
        float   value = 0.0f;

 static int     SelectedItem = -1;
        int     currentselected = -1;

 for ( i=0;i < appMPlayer.NumberOfItems + 1;i++ )
  if ( ( appMPlayer.Items[i].pressed != btnDisabled )&&
       ( wgIsRect( X,Y,appMPlayer.Items[i].x,appMPlayer.Items[i].y,appMPlayer.Items[i].x+appMPlayer.Items[i].width,appMPlayer.Items[i].y+appMPlayer.Items[i].height ) ) )
   { currentselected=i; break; }

 switch ( Button )
  {
   case wsPMMouseButton:
	  gtkShow( evHidePopUpMenu,NULL );
          mplShowMenu( RX,RY );
          itemtype=itPRMButton;
          break;
   case wsRMMouseButton:
          mplHideMenu( RX,RY,0 );
          break;

   case wsPLMouseButton:
	  gtkShow( evHidePopUpMenu,NULL );
          sx=X; sy=Y; boxMoved=1; itemtype=itPLMButton;
          SelectedItem=currentselected;
          if ( SelectedItem == -1 ) break;
          boxMoved=0; 
          item=&appMPlayer.Items[SelectedItem];
          itemtype=item->type;
          item->pressed=btnPressed;
          switch( item->type )
           {
            case itButton:
                 if ( ( SelectedItem > -1 ) &&
                    ( ( ( item->msg == evPlaySwitchToPause && item->msg == evPauseSwitchToPlay ) ) ||
                      ( ( item->msg == evPauseSwitchToPlay && item->msg == evPlaySwitchToPause ) ) ) )
                  { item->pressed=btnDisabled; }
                 break;
           }
          break;
   case wsRLMouseButton:
          boxMoved=0;
          item=&appMPlayer.Items[SelectedItem];
          item->pressed=btnReleased;
          SelectedItem=-1;
          if ( currentselected == - 1 ) { itemtype=0; break; }
          value=0;
          switch( itemtype )
           {
            case itPotmeter:
            case itHPotmeter:
                 btnModify( item->msg,(float)( X - item->x ) / item->width * 100.0f );
		 mplEventHandling( item->msg,item->value );
                 value=item->value;
                 break;
	    case itVPotmeter:
                 btnModify( item->msg, ( 1. - (float)( Y - item->y ) / item->height) * 100.0f );
		 mplEventHandling( item->msg,item->value );
                 value=item->value;
                 break;
           }
          mplEventHandling( item->msg,value );
          itemtype=0;
          break;

   case wsRRMouseButton:
        gtkShow( evShowPopUpMenu,NULL );
        break;

// --- rolled mouse ... de szar :)))
   case wsP5MouseButton: value=-2.5f; goto rollerhandled;
   case wsP4MouseButton: value= 2.5f;
rollerhandled:
          item=&appMPlayer.Items[currentselected];
          if ( ( item->type == itHPotmeter )||( item->type == itVPotmeter )||( item->type == itPotmeter ) )
           {
            item->value+=value;
            btnModify( item->msg,item->value );
            mplEventHandling( item->msg,item->value );
           }
          break;

// --- moving
   case wsMoveMouse:
          item=&appMPlayer.Items[SelectedItem];
          switch ( itemtype )
           {
            case itPLMButton:
                 wsMoveWindow( &appMPlayer.mainWindow,False,RX - abs( sx ),RY - abs( sy ) );
                 mplMainRender=0;
                 break;
            case itPRMButton:
                 mplMenuMouseHandle( X,Y,RX,RY );
                 break;
            case itPotmeter:
                 item->value=(float)( X - item->x ) / item->width * 100.0f;
                 goto potihandled;
            case itVPotmeter:
                 item->value=(1. - (float)( Y - item->y ) / item->height) * 100.0f;
                 goto potihandled;
            case itHPotmeter:
                 item->value=(float)( X - item->x ) / item->width * 100.0f;
potihandled:
                 if ( item->value > 100.0f ) item->value=100.0f;
                 if ( item->value < 0.0f ) item->value=0.0f;
                 mplEventHandling( item->msg,item->value );
                 break;
           }
          break;
  }
}

int keyPressed = 0;

void mplMainKeyHandle( int KeyCode,int Type,int Key )
{
 int msg = evNone;

 if ( Type != wsKeyPressed ) return;
 
 if ( !Key )
  {
   switch ( KeyCode )
    {
     case wsXFMMPrev:     msg=evPrev;              break;
     case wsXFMMStop:	  msg=evStop;              break;
     case wsXFMMPlay:	  msg=evPlaySwitchToPause; break;
     case wsXFMMNext:	  msg=evNext;	           break;
     case wsXFMMVolUp:	  msg=evIncVolume;         break;
     case wsXFMMVolDown:  msg=evDecVolume;         break;
     case wsXFMMMute: 	  msg=evMute;	           break;
    }
  }
  else
   {
    switch ( Key )
     {
      case wsEnter:            msg=evPlay; break;
      case wsXF86LowerVolume:  msg=evDecVolume; break;
      case wsXF86RaiseVolume:  msg=evIncVolume; break;
      case wsXF86Mute:         msg=evMute; break;
      case wsXF86Play:         msg=evPlaySwitchToPause; break;
      case wsXF86Stop:         msg=evStop; break;
      case wsXF86Prev:         msg=evPrev; break;
      case wsXF86Next:         msg=evNext; break;
      case wsXF86Media:        msg=evLoad; break;
      case wsEscape: 
    	    if ( appMPlayer.subWindow.isFullScreen )
	     { 
	      if ( guiIntfStruct.event_struct ) ((XEvent *)guiIntfStruct.event_struct)->type=None; 
	      mplEventHandling( evNormalSize,0 ); 
	      return;
	     }
      default:          vo_x11_putkey( Key ); return;
     }
   }
 if ( msg != evNone ) mplEventHandling( msg,0 );
}

/* this will be used to handle Drag&Drop files */
void mplDandDHandler(int num,char** files)
{
  struct stat buf;
  int f = 0;

  char* subtitles = NULL;
  char* filename = NULL;

  if (num <= 0)
    return;


  /* now fill it with new items */
  for(f=0; f < num; f++){
    char* str = strdup( files[f] );
    plItem* item;

    url_unescape_string(str, files[f]);

    if(stat(str,&buf) == 0 && S_ISDIR(buf.st_mode) == 0) {
      /* this is not a directory so try to play it */
      mp_msg( MSGT_GPLAYER,MSGL_V,"Received D&D %s\n",str );
      
      /* check if it is a subtitle file */
      {
	char* ext = strrchr(str,'.');
	if (ext) {
	  static char supported[] = "utf/sub/srt/smi/rt//txt/ssa/aqt/";
	  char* type;
	  int len;
	  if((len=strlen(++ext)) && (type=strstr(supported,ext)) &&\
	     (type-supported)%4 == 0 && *(type+len) == '/'){
	    /* handle subtitle file */
	    gfree((void**)&subtitles);
	    subtitles = str;
	    continue;
	  }
	}
      }

      /* clear playlist */
      if (filename == NULL) {
	filename = files[f];
	gtkSet(gtkDelPl,0,NULL);
      }

      item = calloc(1,sizeof(plItem));
      
      /* FIXME: decompose file name ? */
      /* yes -- Pontscho */
      if ( strrchr( str,'/' ) ) {
	char * s = strrchr( str,'/' ); *s=0; s++;
	item->name = gstrdup( s );
	item->path = gstrdup( str );
      } else {
	item->name = strdup(str);
	item->path = strdup("");
      }
      gtkSet(gtkAddPlItem,0,(void*)item);
    } else {
      mp_msg( MSGT_GPLAYER,MSGL_WARN,MSGTR_NotAFile,str );
    }
    free( str );
  }

  if (filename) {
    mplSetFileName( NULL,filename,STREAMTYPE_FILE );
    if ( guiIntfStruct.Playing == 1 ) mplEventHandling( evStop,0 );
    mplEventHandling( evPlay,0 );
  }
  if (subtitles) {
    gfree((void**)&guiIntfStruct.Subtitlename);
    guiIntfStruct.Subtitlename = subtitles;
    guiLoadSubtitle(guiIntfStruct.Subtitlename);
  }
}
