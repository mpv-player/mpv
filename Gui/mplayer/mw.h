
// main window

#include "../../libmpdemux/stream.h"

unsigned char * mplDrawBuffer = NULL;
int             mplMainRender = 1;

int             mplMainAutoPlay = 0;
int             mplMiddleMenu = 0;

int             mainVisible = 1;

int             boxMoved = 0;
int             sx = 0,sy = 0;
int             i,pot = 0;

inline void TranslateFilename( int c,char * tmp )
{
 int i;
 switch ( guiIntfStruct.StreamType )
  {
   case STREAMTYPE_FILE:
          if ( ( guiIntfStruct.Filename )&&( guiIntfStruct.Filename[0] ) )
           {
	    if ( strrchr( guiIntfStruct.Filename,'/' ) ) strcpy( tmp,strrchr( guiIntfStruct.Filename,'/' ) + 1 );
	     else strcpy( tmp,guiIntfStruct.Filename );
            if ( tmp[strlen( tmp ) - 4] == '.' ) tmp[strlen( tmp ) - 4]=0;
            if ( tmp[strlen( tmp ) - 5] == '.' ) tmp[strlen( tmp ) - 5]=0;
           } else strcpy( tmp,"no file loaded" );
          break;
#ifdef USE_DVDREAD
   case STREAMTYPE_DVD:
          if ( guiIntfStruct.DVD.current_chapter ) sprintf( tmp,"chapter %d",guiIntfStruct.DVD.current_chapter );
            else strcat( tmp,"no chapter" );
          break;
#endif
   default: strcpy( tmp,"no media opened" );
  }
 if ( c )
  {
   for ( i=0;i < (int)strlen( tmp );i++ )
    {
     int t=0;
     if ( c == 1 ) { if ( ( tmp[i] >= 'A' )&&( tmp[i] <= 'Z' ) ) t=32; }
     if ( c == 2 ) { if ( ( tmp[i] >= 'a' )&&( tmp[i] <= 'z' ) ) t=-32; }
     tmp[i]=(char)( tmp[i] + t );
    }
  }
}

char * Translate( char * str )
{
 static char   trbuf[512];
        char   tmp[512];
        int    i,c;
        int    t;
 memset( trbuf,0,512 );
 memset( tmp,0,128 );
 for ( c=0,i=0;i < (int)strlen( str );i++ )
  {
   if ( str[i] != '$' ) { trbuf[c++]=str[i]; trbuf[c]=0; }
    else
    {
     switch ( str[++i] )
      {
       case 't': sprintf( tmp,"%02d",guiIntfStruct.Track ); strcat( trbuf,tmp ); break;
       case 'o': TranslateFilename( 0,tmp ); strcat( trbuf,tmp ); break;
       case 'f': TranslateFilename( 1,tmp ); strcat( trbuf,tmp ); break;
       case 'F': TranslateFilename( 2,tmp ); strcat( trbuf,tmp ); break;
       case '6': t=guiIntfStruct.LengthInSec; goto calclengthhhmmss;
       case '1': t=guiIntfStruct.TimeSec;
calclengthhhmmss:
            sprintf( tmp,"%02d:%02d:%02d",t/3600,t/60%60,t%60 ); strcat( trbuf,tmp );
            break;
       case '7': t=guiIntfStruct.LengthInSec; goto calclengthmmmmss;
       case '2': t=guiIntfStruct.TimeSec;
calclengthmmmmss:
            sprintf( tmp,"%04d:%02d",t/60,t%60 ); strcat( trbuf,tmp );
            break;
       case '3': sprintf( tmp,"%02d",guiIntfStruct.TimeSec / 3600 ); strcat( trbuf,tmp ); break;
       case '4': sprintf( tmp,"%02d",( ( guiIntfStruct.TimeSec / 60 ) % 60 ) ); strcat( trbuf,tmp ); break;
       case '5': sprintf( tmp,"%02d",guiIntfStruct.TimeSec % 60 ); strcat( trbuf,tmp ); break;
       case '8': sprintf( tmp,"%01d:%02d:%02d",guiIntfStruct.TimeSec / 3600,( guiIntfStruct.TimeSec / 60 ) % 60,guiIntfStruct.TimeSec % 60 ); strcat( trbuf,tmp ); break;
       case 'v': sprintf( tmp,"%3.2f%%",guiIntfStruct.Volume ); strcat( trbuf,tmp ); break;
       case 'V': sprintf( tmp,"%3.1f",guiIntfStruct.Volume ); strcat( trbuf,tmp ); break;
       case 'b': sprintf( tmp,"%3.2f%%",guiIntfStruct.Balance ); strcat( trbuf,tmp ); break;
       case 'B': sprintf( tmp,"%3.1f",guiIntfStruct.Balance ); strcat( trbuf,tmp ); break;
       case 'd': sprintf( tmp,"%d",guiIntfStruct.FrameDrop ); strcat( trbuf,tmp ); break;
       case 's': if ( guiIntfStruct.Playing == 0 ) strcat( trbuf,"s" ); break;
       case 'l': if ( guiIntfStruct.Playing == 1 ) strcat( trbuf,"p" ); break;
       case 'e': if ( guiIntfStruct.Playing == 2 ) strcat( trbuf,"e" ); break;
       case 'a':
            switch ( guiIntfStruct.AudioType )
             {
              case 0: strcat( trbuf,"n" ); break;
              case 1: strcat( trbuf,"m" ); break;
              case 2: strcat( trbuf,"t" ); break;
             }
            break;
       case 'T':
           switch ( guiIntfStruct.StreamType )
            {
             case STREAMTYPE_FILE:   strcat( trbuf,"f" ); break;
             case STREAMTYPE_VCD:    strcat( trbuf,"v" ); break;
             case STREAMTYPE_STREAM: strcat( trbuf,"u" ); break;
#ifdef USE_DVDREAD
             case STREAMTYPE_DVD:    strcat( trbuf,"d" ); break;
#endif
             default:                strcat( trbuf," " ); break;
            }
           break;
       case '$': strcat( trbuf,"$" ); break;
       default: continue;
      }
     c=strlen( trbuf );
    }
  }
 return trbuf;
}

inline void PutImage( txSample * bf,int x,int y,int max,int ofs )
{
 int i=0,ix,iy;
 unsigned long * buf = NULL;
 unsigned long * drw = NULL;
 unsigned long   tmp;

 if ( ( !bf )||( bf->Image == NULL ) ) return;

 i=( bf->Width * ( bf->Height / max ) ) * ofs;
 buf=(unsigned long *)mplDrawBuffer;
 drw=(unsigned long *)bf->Image;

 for ( iy=y;iy < (int)(y+bf->Height / max);iy++ )
  for ( ix=x;ix < (int)(x+bf->Width);ix++ )
   {
    tmp=drw[i++];
    if ( tmp != 0x00ff00ff )
     buf[ iy*appMPlayer.main.Bitmap.Width+ix ]=tmp;
   }
}

void mplMainDraw( wsParamDisplay )
{
 wItem    * item;
 txSample * image = NULL;
 int        i;

 if ( appMPlayer.mainWindow.Visible == wsWindowNotVisible ||
      !mainVisible ) return;
//      !appMPlayer.mainWindow.Mapped ) return;

 btnModify( evSetMoviePosition,guiIntfStruct.Position );
 btnModify( evSetVolume,guiIntfStruct.Volume );

 if ( mplMainRender )
  {
   memcpy( mplDrawBuffer,appMPlayer.main.Bitmap.Image,appMPlayer.main.Bitmap.ImageSize );
   for( i=0;i < appMPlayer.NumberOfItems + 1;i++ )
    {
     item=&appMPlayer.Items[i];
     switch( item->type )
      {
       case itButton:
            PutImage( &item->Bitmap,item->x,item->y,3,item->pressed );
            break;
       case itPotmeter:
            PutImage( &item->Bitmap,item->x,item->y,item->phases,item->phases * ( item->value / 100.0f ) );
            break;
       case itHPotmeter:
            PutImage( &item->Bitmap,item->x,item->y,item->phases,item->phases * ( item->value / 100.0f ) );
            PutImage( &item->Mask,item->x + (int)( ( item->width - item->psx ) * item->value / 100.0f ),item->y,3,item->pressed );
            break;
       case itSLabel:
            image=fntRender( item->fontid,0,item->width,"%s",item->label );
            goto drawrenderedtext;
       case itDLabel:
            image=fntRender( item->fontid,mplTimer%item->width,item->width,"%s",Translate( item->label ) );
////            image=fntRender( item->fontid,( mplRedrawTimer / 10 )%item->width,item->width,"%s",Translate( item->label ) );
drawrenderedtext:
            PutImage( image,item->x,item->y,1,0 );
            if ( image )
             {
              if ( image->Image ) free( image->Image );
              free( image );
             }
            break;
      }
    }
   wsConvert( &appMPlayer.mainWindow,mplDrawBuffer,appMPlayer.main.Bitmap.ImageSize );
   mplMainRender=0;
  }
 wsPutImage( &appMPlayer.mainWindow );
// XFlush( wsDisplay );
}

#define IZE(x) printf("@@@ " x " @@@\n");

extern void exit_player(char* how);
extern int audio_id;
extern int dvdsub_id;
extern char * dvd_device;

void mplEventHandling( int msg,float param )
{
 int j;

 switch( msg )
  {
// --- user events
   case evExit:
        exit_player( "Exit" );
        break;

#ifdef USE_DVDREAD
   case evPlayDVD:
        guiIntfStruct.DVD.current_title=1;
        guiIntfStruct.DVD.current_chapter=1;
        guiIntfStruct.DVD.current_angle=1;
play_dvd_2:
        guiIntfStruct.StreamType=STREAMTYPE_DVD;
#endif
   case evPlay:
   case evPlaySwitchToPause:
        mplMainAutoPlay=0;
        if ( ( msg == evPlaySwitchToPause )&&( guiIntfStruct.Playing == 1 ) ) goto NoPause;

        switch ( guiIntfStruct.StreamType )
         {
          case STREAMTYPE_STREAM:
          case STREAMTYPE_VCD:
          case STREAMTYPE_FILE:
               dvd_title=0;
               break;
#ifdef USE_DVDREAD
          case STREAMTYPE_DVD:
	       if ( !dvd_device ) 
	        {
	         dvd_device=DEFAULT_DVD_DEVICE;
                 guiSetFilename( guiIntfStruct.Filename,dvd_device );
		} 
	       if ( guiIntfStruct.Playing != 2 )
	        {
	         dvd_title=guiIntfStruct.DVD.current_title;
	         dvd_angle=guiIntfStruct.DVD.current_angle;
                 dvd_chapter=guiIntfStruct.DVD.current_chapter;
                 guiIntfStruct.DVDChanged=1;
		} 
               break;
#endif
         }
        mplPlay();
        mplMainRender=1;
        break;
#ifdef USE_DVDREAD
   case evSetDVDSubtitle:
        dvdsub_id=(int)param;
        goto play_dvd_2;
        break;
   case evSetDVDAudio:
        audio_id=(int)param;
        goto play_dvd_2;
        break;
   case evSetDVDChapter:
        guiIntfStruct.DVD.current_chapter=(int)param;
        goto play_dvd_2;
        break;
   case evSetDVDTitle:
        guiIntfStruct.DVD.current_title=(int)param;
	guiIntfStruct.DVD.current_chapter=1;
	guiIntfStruct.DVD.current_angle=1;
        goto play_dvd_2;
        break;
#endif

   case evPause:
   case evPauseSwitchToPlay:
NoPause:
        mplPause();
        mplMainRender=1;
        break;

   case evStop:
        mplStop();
        mplMainRender=1;
        break;

   case evLoadPlay:
        mplMainAutoPlay=1;
   case evLoad:
        mplMainRender=1;
        gtkShow( evLoad,NULL );
        break;
   case evLoadSubtitle:
        mplMainRender=1;
        gtkShow( evLoadSubtitle,NULL );
        break;
   case evPrev:
        mplMainRender=1;
        mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"[mw.h] previous stream ...\n" );
        break;
   case evNext:
        mplMainRender=1;
        mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"[mw.h] next stream ...\n" );
        break;

   case evPlayList:
        IZE("evPlayList");
        mplMainRender=1;
        gtkShow( evPlayList,NULL );
#warning disabled old gtk code
#if 0
        if ( gtkVisiblePlayList )
         {
          btnModify( evPlayList,btnReleased );
          gtkShMem->vs.window=evPlayList;
          gtkSendMessage( evHideWindow );
          gtkVisiblePlayList=0;
         }
         else
          {
           gtkSendMessage( evPlayList );
           btnModify( evPlayList,btnPressed );
           gtkVisiblePlayList=1;
          }
#endif
        break;

   case evSkinBrowser: gtkShow( evSkinBrowser,skinName ); break;
   case evAbout:       gtkShow( evAbout,NULL ); break;
   case evPreferences: gtkShow( evPreferences,NULL ); break;

   case evForward1min:      mplRelSeek( 60 );  break;
   case evBackward1min:     mplRelSeek( -60 ); break;
   case evForward10sec:     mplRelSeek( 10 );  break;
   case evBackward10sec:    mplRelSeek( -10 ); break;
   case evSetMoviePosition: mplAbsSeek( param ); break;

   case evIncVolume:  vo_x11_putkey( wsGrayMul ); break;
   case evDecVolume:  vo_x11_putkey( wsGrayDiv ); break;
   case evMute:       guiIntfStruct.Mute=1; break;
   case evSetVolume:
   case evSetBalance: guiIntfStruct.VolumeChanged=1; break;


   case evIconify:
        switch ( (int)param )
         {
          case 0: wsIconify( appMPlayer.mainWindow ); break;
          case 1: wsIconify( appMPlayer.subWindow ); break;
         }
        break;
   case evNormalSize:
        if ( guiIntfStruct.Playing )
         {
          appMPlayer.subWindow.isFullScreen=True;
          appMPlayer.subWindow.OldX=( wsMaxX - guiIntfStruct.MovieWidth ) / 2;
          appMPlayer.subWindow.OldY=( wsMaxY - guiIntfStruct.MovieHeight ) / 2;
          appMPlayer.subWindow.OldWidth=guiIntfStruct.MovieWidth; appMPlayer.subWindow.OldHeight=guiIntfStruct.MovieHeight;
          wsFullScreen( &appMPlayer.subWindow );
	  vo_fs=0;
         }
        break;
   case evDoubleSize:
        if ( guiIntfStruct.Playing )
         {
          appMPlayer.subWindow.isFullScreen=True;
          appMPlayer.subWindow.OldX=( wsMaxX - guiIntfStruct.MovieWidth * 2 ) / 2;
          appMPlayer.subWindow.OldY=( wsMaxY - guiIntfStruct.MovieHeight * 2 ) / 2;
          appMPlayer.subWindow.OldWidth=guiIntfStruct.MovieWidth * 2; appMPlayer.subWindow.OldHeight=guiIntfStruct.MovieHeight * 2;
          wsFullScreen( &appMPlayer.subWindow );
	  vo_fs=0;
         }
        break;
   case evFullScreen:
        for ( j=0;j<appMPlayer.NumberOfItems + 1;j++ )
         {
          if ( appMPlayer.Items[j].msg == evFullScreen )
           {
            appMPlayer.Items[j].tmp=!appMPlayer.Items[j].tmp;
            appMPlayer.Items[j].pressed=appMPlayer.Items[j].tmp;
           }
         }
        mplFullScreen();
        mplMainRender=1;
        break;

// --- timer events
   case evRedraw:
        mplMainRender=1;
        wsPostRedisplay( &appMPlayer.mainWindow );
        XFlush( wsDisplay );
        mplRedrawTimer=mplRedrawTimerConst;
        break;
// --- system events
   case evNone:
        mp_msg( MSGT_GPLAYER,MSGL_STATUS,"[mw] event none received.\n" );
        break;
   default:
        mp_msg( MSGT_GPLAYER,MSGL_STATUS,"[mw] unknown event received ( %d,%.2f ).\n",msg,param );
        break;
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
   case wsPRMouseButton:
          mplShowMenu( RX,RY );
          itemtype=itPRMButton;
          break;
   case wsRRMouseButton:
          mplHideMenu( RX,RY );
          break;

   case wsPLMouseButton:
          sx=X; sy=Y; boxMoved=1; itemtype=itPLMButton; // if move the main window
          SelectedItem=currentselected;
          if ( SelectedItem == -1 ) break; // yeees, i'm move the fucking window
          boxMoved=0; mplMainRender=1; // No, not move the window, i'm pressed one button
          item=&appMPlayer.Items[SelectedItem];
          itemtype=item->type;
          item->pressed=btnPressed;
          switch( item->type )
           {
            case itButton:
                 if ( ( SelectedItem > -1 ) &&
                    ( ( ( appMPlayer.Items[SelectedItem].msg == evPlaySwitchToPause && item->msg == evPauseSwitchToPlay ) ) ||
                      ( ( appMPlayer.Items[SelectedItem].msg == evPauseSwitchToPlay && item->msg == evPlaySwitchToPause ) ) ) )
                  { appMPlayer.Items[SelectedItem].pressed=btnDisabled; }
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
                 switch ( item->msg )
                  {
                   case evSetVolume:
                        guiIntfStruct.VolumeChanged=1;
                        guiIntfStruct.Volume=item->value;
                        break;
                  }
                 value=item->value;
                 break;
           }
          mplEventHandling( item->msg,value );
          mplMainRender=1;
          itemtype=0;
          break;

   case wsPMMouseButton:
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
            switch ( item->msg )
             {
              case evSetVolume:
                   guiIntfStruct.VolumeChanged=1;
                   guiIntfStruct.Volume=item->value;
                   break;
             }
            mplEventHandling( item->msg,item->value );
            mplMainRender=1;
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
            case itHPotmeter:
                 item->value=(float)( X - item->x ) / item->width * 100.0f;
potihandled:
                 if ( item->value > 100.0f ) item->value=100.0f;
                 if ( item->value < 0.0f ) item->value=0.0f;
                 switch ( item->msg )
                  {
                   case evSetVolume:
                        guiIntfStruct.VolumeChanged=1;
                        guiIntfStruct.Volume=item->value;
                        break;
                  }
                 mplEventHandling( item->msg,item->value );
                 mplMainRender=1; wsPostRedisplay( &appMPlayer.mainWindow );
                 break;
           }
          break;
  }
 if ( Button != wsMoveMouse ) wsPostRedisplay( &appMPlayer.mainWindow );
}

int keyPressed = 0;

void mplMainKeyHandle( int State,int Type,int Key )
{
 int msg = evNone;

 if ( Type != wsKeyPressed ) return;
 switch ( Key )
  {
   case wsEnter:     msg=evPlay; break;
#ifndef HAVE_NEW_INPUT
   case '.':
   case '>':         msg=evNext; break;
   case ',':
   case '<':         msg=evPrev; break;

   case wsEscape:    msg=evExit; break;

   case wsSpace:     msg=evPause; break;
   case wsa:
   case wsA:         msg=evAbout; break;
   case wsb:
   case wsB:         msg=evSkinBrowser; break;
   case wse:
   case wsE:         msg=evEqualeaser; break;
   case wsf:
   case wsF:         msg=evFullScreen; break;
   case wsl:
   case wsL:         msg=evLoad; break;
   case wsu:
   case wsU:         msg=evLoadSubtitle; break;
   case wsm:
   case wsM:         msg=evMute; break;
   case wss:
   case wsS:         msg=evStop; break;
   case wsp:
   case wsP:         msg=evPlayList; break;
#endif

   case wsXF86LowerVolume:  msg=evDecVolume; break;
   case wsXF86RaiseVolume:  msg=evIncVolume; break;
   case wsXF86Mute:         msg=evMute; break;
   case wsXF86Play:         msg=evPlaySwitchToPause; break;
   case wsXF86Stop:         msg=evStop; break;
   case wsXF86Prev:         msg=evPrev; break;
   case wsXF86Next:         msg=evNext; break;
   case wsXF86Media:        msg=evLoad; break;

   default:          vo_x11_putkey( Key ); return;
  }
 if ( msg != evNone ) mplEventHandling( msg,0 );
}
