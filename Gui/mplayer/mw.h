
// main window

#include "../../libmpdemux/stream.h"

unsigned char * mplDrawBuffer = NULL;
int             mplMainRender = 1;

int             mplMainAutoPlay = 0;
int		mplMiddleMenu = 0;

int             mainVisible = 1;

int             boxMoved = 0;
int             sx = 0,sy = 0;
int             i,pot = 0;

inline void TranslateFilename( int c,char * tmp )
{
 int i;
 switch ( mplShMem->StreamType )
  {
   case STREAMTYPE_FILE:
          if ( gtkShMem->fs.filename[0] )
           {
            strcpy( tmp,gtkShMem->fs.filename );
            if ( tmp[strlen( tmp ) - 4] == '.' ) tmp[strlen( tmp ) - 4]=0;
            if ( tmp[strlen( tmp ) - 5] == '.' ) tmp[strlen( tmp ) - 5]=0;
           } else strcpy( tmp,"no file loaded" );
          break;
#ifdef USE_DVDREAD		     
   case STREAMTYPE_DVD:
          if ( mplShMem->DVD.current_chapter ) sprintf( tmp,"chapter %d",mplShMem->DVD.current_chapter );
            else strcat( tmp,"no chapter" );
          break;
#endif
   default: strcpy( tmp,"no media opened" );
  }
 if ( c )
  {
   for ( i=0;i < strlen( tmp );i++ )
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
        char   tmp[128];
        int    i,c;
        int    t;
 memset( trbuf,0,512 );
 memset( tmp,0,128 );
 for ( c=0,i=0;i < strlen( str );i++ )
  {
   if ( str[i] != '$' ) { trbuf[c++]=str[i]; trbuf[c]=0; }
    else
    {
     switch ( str[++i] )
      {
       case 't': sprintf( tmp,"%02d",mplShMem->Track ); strcat( trbuf,tmp ); break;
       case 'o': TranslateFilename( 0,tmp ); strcat( trbuf,tmp ); break;
       case 'f': TranslateFilename( 1,tmp ); strcat( trbuf,tmp ); break;
       case 'F': TranslateFilename( 2,tmp ); strcat( trbuf,tmp ); break;
       case '6': t=mplShMem->LengthInSec; goto calclengthhhmmss;
       case '1': t=mplShMem->TimeSec;
calclengthhhmmss:
            sprintf( tmp,"%02d:%02d:%02d",t/3600,t/60%60,t%60 ); strcat( trbuf,tmp );
            break;
       case '7': t=mplShMem->LengthInSec; goto calclengthmmmmss;
       case '2': t=mplShMem->TimeSec;
calclengthmmmmss:
            sprintf( tmp,"%04d:%02d",t/60,t%60 ); strcat( trbuf,tmp );
            break;
       case '3': sprintf( tmp,"%02d",mplShMem->TimeSec / 3600 ); strcat( trbuf,tmp ); break;
       case '4': sprintf( tmp,"%02d",( ( mplShMem->TimeSec / 60 ) % 60 ) ); strcat( trbuf,tmp ); break;
       case '5': sprintf( tmp,"%02d",mplShMem->TimeSec % 60 ); strcat( trbuf,tmp ); break;
       case '8': sprintf( tmp,"%01d:%02d:%02d",mplShMem->TimeSec / 3600,( mplShMem->TimeSec / 60 ) % 60,mplShMem->TimeSec % 60 ); strcat( trbuf,tmp ); break;
       case 'v': sprintf( tmp,"%3.2f%%",mplShMem->Volume ); strcat( trbuf,tmp ); break;
       case 'V': sprintf( tmp,"%3.1f",mplShMem->Volume ); strcat( trbuf,tmp ); break;
       case 'b': sprintf( tmp,"%3.2f%%",mplShMem->Balance ); strcat( trbuf,tmp ); break;
       case 'B': sprintf( tmp,"%3.1f",mplShMem->Balance ); strcat( trbuf,tmp ); break;
       case 'd': sprintf( tmp,"%d",mplShMem->FrameDrop ); strcat( trbuf,tmp ); break;
       case 's': if ( mplShMem->Playing == 0 ) strcat( trbuf,"s" ); break;
       case 'l': if ( mplShMem->Playing == 1 ) strcat( trbuf,"p" ); break;
       case 'e': if ( mplShMem->Playing == 2 ) strcat( trbuf,"e" ); break;
       case 'a':
            switch ( mplShMem->AudioType )
             {
              case 0: strcat( trbuf,"n" ); break;
              case 1: strcat( trbuf,"m" ); break;
              case 2: strcat( trbuf,"t" ); break;
             }
            break;
       case 'T':
           switch ( mplShMem->StreamType )
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

 for ( iy=y;iy < y+bf->Height / max;iy++ )
  for ( ix=x;ix < x+bf->Width;ix++ )
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

 btnModify( evSetMoviePosition,mplShMem->Position );
 btnModify( evSetVolume,mplShMem->Volume );

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
//            image=fntRender( item->fontid,( mplRedrawTimer / 10 )%item->width,item->width,"%s",Translate( item->label ) );
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

void mplMsgHandle( int msg,float param )
{
 int j;

 switch( msg )
  {
// --- user events
   case evExit:
        wsDoExit();  // sets wsTrue=False;
        exit_player( "Exit" );
        break;

#ifdef USE_DVDREAD
   case evPlayDVD:
        dvd_title=1;
        dvd_chapter=1;
        dvd_angle=1;
play_dvd_2:
        mplShMem->StreamType=STREAMTYPE_DVD;
#endif
   case evPlay:
   case evPlaySwitchToPause:
        btnModify( evPlaySwitchToPause,btnDisabled );
        btnModify( evPauseSwitchToPlay,btnReleased );
        if ( ( msg == evPlaySwitchToPause )&( mplShMem->Playing == 1 ) ) goto NoPause;
        mplMainRender=1;

        switch ( mplShMem->StreamType )
         {
          case STREAMTYPE_STREAM: 
          case STREAMTYPE_VCD:
          case STREAMTYPE_FILE:   
	       dvd_title=0;
	       break;
#ifdef USE_DVDREAD
          case STREAMTYPE_DVD:    
	       strcpy( mplShMem->Filename,"/dev/dvd" );
	       break;
#endif
         }
        mplPlay();
        break;
   case evSetDVDSubtitle:
#ifdef USE_DVDREAD
        dvdsub_id=(int)param;
	dvd_title=mplShMem->DVD.current_title;
	dvd_angle=mplShMem->DVD.current_angle;
        dvd_chapter=mplShMem->DVD.current_chapter;
        mplShMem->DVDChanged=1;
	goto play_dvd_2;
#endif
        break;
   case evSetDVDAudio:
#ifdef USE_DVDREAD
        audio_id=(int)param;
	dvd_title=mplShMem->DVD.current_title;
	dvd_angle=mplShMem->DVD.current_angle;
        dvd_chapter=mplShMem->DVD.current_chapter;
        mplShMem->DVDChanged=1;
	goto play_dvd_2;
#endif
        break;
   case evSetDVDChapter:
#ifdef USE_DVDREAD
	dvd_title=mplShMem->DVD.current_title;
	dvd_angle=mplShMem->DVD.current_angle;
        dvd_chapter=(int)param;
        mplShMem->DVDChanged=1;
	goto play_dvd_2;
#endif
        break;
   case evSetDVDTitle:
#ifdef USE_DVDREAD
        dvd_title=(int)param;
	dvd_chapter=1;
	dvd_angle=1;
        mplShMem->DVDChanged=1;
	goto play_dvd_2;
#endif
        break;

   case evPause:
   case evPauseSwitchToPlay:
//        btnModify( evPlaySwitchToPause,btnReleased );
//        btnModify( evPauseSwitchToPlay,btnDisabled );
NoPause:
        mplMainRender=1;
        mplPause();
        break;

   case evStop:
        IZE("evStop");
        btnModify( evPlaySwitchToPause,btnReleased );
        btnModify( evPauseSwitchToPlay,btnDisabled );
        mplMainRender=1;
        mplStop();
        break;

   case evLoadPlay:
        mplMainAutoPlay=1;
   case evLoad:
        mplMainRender=1;
        gtkSendMessage( evLoad );
        break;
   case evLoadSubtitle:
        mplMainRender=1;
        gtkSendMessage( evLoadSubtitle );
        break;
   case evPrev:
        IZE("evPrev");
        mplMainRender=1;
        #ifdef DEBUG
         dbprintf( 1,"[mw.h] previous stream ...\n" );
        #endif
        break;
   case evNext:
        IZE("evNext");
        mplMainRender=1;
        #ifdef DEBUG
         dbprintf( 1,"[mw.h] next stream ...\n" );
        #endif
        break;

   case evPlayList:
        IZE("evPlayList");
        mplMainRender=1;
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
        break;

   case evSkinBrowser: gtkSendMessage( evSkinBrowser ); break;
   case evAbout:       gtkSendMessage( evAbout ); break;
   case evPreferences: gtkSendMessage( evPreferences ); break;

   case evForward1min:      mplRelSeek( 60 );  break;
   case evBackward1min:     mplRelSeek( -60 ); break;
   case evForward10sec:     mplRelSeek( 10 );  break;
   case evBackward10sec:    mplRelSeek( -10 ); break;
   case evSetMoviePosition: mplAbsSeek( param ); break;

   case evIncVolume:  vo_x11_putkey( wsGrayMul ); break;
   case evDecVolume:  vo_x11_putkey( wsGrayDiv ); break;
   case evMute:       mplShMem->Mute=1; break;
   case evSetVolume:
   case evSetBalance: mplShMem->VolumeChanged=1; break;


   case evIconify:
        switch ( (int)param )
         {
          case 0: wsIconify( appMPlayer.mainWindow ); break;
          case 1: wsIconify( appMPlayer.subWindow ); break;
         }
        break;
   case evNormalSize:
        if ( mplShMem->Playing )
         {
          appMPlayer.subWindow.isFullScreen=True;
          appMPlayer.subWindow.OldX=( wsMaxX - moviewidth ) / 2;
          appMPlayer.subWindow.OldY=( wsMaxY - movieheight ) / 2;
          appMPlayer.subWindow.OldWidth=moviewidth; appMPlayer.subWindow.OldHeight=movieheight;
          wsFullScreen( &appMPlayer.subWindow );
          mplResize( appMPlayer.subWindow.X,appMPlayer.subWindow.Y,moviewidth,movieheight );
         }
        break;
   case evDoubleSize:
        if ( mplShMem->Playing )
         {
          appMPlayer.subWindow.isFullScreen=True;
          appMPlayer.subWindow.OldX=( wsMaxX - moviewidth * 2 ) / 2;
          appMPlayer.subWindow.OldY=( wsMaxY - movieheight * 2 ) / 2;
          appMPlayer.subWindow.OldWidth=moviewidth * 2; appMPlayer.subWindow.OldHeight=movieheight * 2;
          wsFullScreen( &appMPlayer.subWindow );
          mplResize( appMPlayer.subWindow.X,appMPlayer.subWindow.Y,moviewidth,movieheight );
         }
        break;
   case evFullScreen:
        IZE("evFullS");
        for ( j=0;j<appMPlayer.NumberOfItems + 1;j++ )
         {
          if ( appMPlayer.Items[j].msg == evFullScreen )
           {
            appMPlayer.Items[j].tmp=!appMPlayer.Items[j].tmp;
            appMPlayer.Items[j].pressed=appMPlayer.Items[j].tmp;
           }
         }
        mplMainRender=1;
        mplFullScreen();
        break;

// --- timer events
   case evHideMouseCursor:
        wsVisibleMouse( &appMPlayer.subWindow,wsHideMouseCursor );
        break;
   case evRedraw:
        mplMainRender=1;
        wsPostRedisplay( &appMPlayer.mainWindow );
//        if ( !mplShMem->Playing )
//      wsPostRedisplay( &appMPlayer.subWindow );
        XFlush( wsDisplay );
        mplRedrawTimer=mplRedrawTimerConst;
        break;
   case evGeneralTimer:
        if ( mplMainAutoPlay )
         {
          mplMainRender=1;
          mplMainAutoPlay=0;
          mplPlay();
         }
	if ( mplMiddleMenu )
	 {
	  mplMiddleMenu=0;
	  mplMsgHandle( gtkShMem->popupmenu,gtkShMem->popupmenuparam );
	 }
        break;
// --- system events
   case evNone:
        dbprintf( 1,"[mw] event none received.\n" );
        break;
   default:
        dbprintf( 1,"[mw] unknown event received ( %d,%.2f ).\n",msg,param );
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

 wsVisibleMouse( &appMPlayer.subWindow,wsShowMouseCursor );

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
          item->used=1;
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
                 item->used=0;
                 btnModify( item->msg,(float)( X - item->x ) / item->width * 100.0f );
                 switch ( item->msg )
                  {
                   case evSetVolume:
                        mplShMem->VolumeChanged=1;
                        mplShMem->Volume=item->value;
                        break;
                  }
                 value=item->value;
                 break;
           }
          mplMsgHandle( item->msg,value );
          mplMainRender=1;
          itemtype=0;
          break;
	  
   case wsPMMouseButton:
#ifdef USE_DVDREAD
	memcpy( &gtkShMem->DVD,&mplShMem->DVD,sizeof( mplDVDStruct ) );
#endif
        gtkSendMessage( evShowPopUpMenu );
	break;	  

// --- rolled mouse ... de szar :)))
   case wsP5MouseButton: value=-2.5f; goto rollerhandled;
   case wsP4MouseButton: value= 2.5f;
rollerhandled:
          item=&appMPlayer.Items[currentselected];
          if ( ( item->type == itHPotmeter )||( item->type == itVPotmeter )||( item->type == itPotmeter ) )
           {
            item->used=0;
            item->value+=value;
            btnModify( item->msg,item->value );
            switch ( item->msg )
             {
              case evSetVolume:
                   mplShMem->VolumeChanged=1;
                   mplShMem->Volume=item->value;
                   break;
             }
            mplMsgHandle( item->msg,item->value );
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
                        mplShMem->VolumeChanged=1;
                        mplShMem->Volume=item->value;
                        break;
                  }
                 mplMsgHandle( item->msg,item->value );
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
 switch ( Key )
  {
   case '.':
   case '>':         msg=evNext; break;
   case ',':
   case '<':         msg=evPrev; break;

   case wsEscape:    msg=evExit; break;

   case wsEnter:     msg=evPlay; break;
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
   default:          vo_x11_putkey( Key ); return;
  }
 if ( ( msg != evNone )&&( Type == wsKeyPressed ) )
  {
   mplMsgHandle( msg,0 );
//   mplMainRender=1;
//   wsPostRedisplay( &appMPlayer.mainWindow );
  }
}
