
// main window

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../app.h"
#include "../skin/font.h"
#include "../skin/skin.h"
#include "../wm/ws.h"

#include "../../config.h"
#include "../../help_mp.h"
#include "../../libvo/x11_common.h"

#include "../../libmpdemux/stream.h"
#include "../../mixer.h"
#include "../../libvo/sub.h"
#include "../../mplayer.h"

#include "../../libmpdemux/demuxer.h"
#include "../../libmpdemux/stheader.h"
#include "../../codec-cfg.h"


#include "play.h"
#include "widgets.h"

extern unsigned int GetTimerMS( void );

inline void TranslateFilename( int c,char * tmp )
{
 int i;
 switch ( guiIntfStruct.StreamType )
  {
   case STREAMTYPE_STREAM:
        strcpy( tmp,guiIntfStruct.Filename );
        break;
   case STREAMTYPE_FILE:
          if ( ( guiIntfStruct.Filename )&&( guiIntfStruct.Filename[0] ) )
           {
	    if ( strrchr( guiIntfStruct.Filename,'/' ) ) strcpy( tmp,strrchr( guiIntfStruct.Filename,'/' ) + 1 );
	     else strcpy( tmp,guiIntfStruct.Filename );
            if ( tmp[strlen( tmp ) - 4] == '.' ) tmp[strlen( tmp ) - 4]=0;
            if ( tmp[strlen( tmp ) - 5] == '.' ) tmp[strlen( tmp ) - 5]=0;
           } else strcpy( tmp,MSGTR_NoFileLoaded );
          break;
#ifdef USE_DVDREAD
   case STREAMTYPE_DVD:
          if ( guiIntfStruct.DVD.current_chapter ) sprintf( tmp,MSGTR_Chapter,guiIntfStruct.DVD.current_chapter );
            else strcat( tmp,MSGTR_NoChapter );
          break;
#endif
#ifdef HAVE_VCD
   case STREAMTYPE_VCD:
        sprintf( tmp,MSGTR_VCDTrack,guiIntfStruct.Track );
	break;
#endif
   default: strcpy( tmp,MSGTR_NoMediaOpened );
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
       case 'x': sprintf( tmp,"%d",guiIntfStruct.MovieWidth ); strcat( trbuf,tmp ); break;
       case 'y': sprintf( tmp,"%d",guiIntfStruct.MovieHeight ); strcat( trbuf,tmp ); break;
       case 'C': sprintf( tmp,"%s", guiIntfStruct.sh_video? ((sh_video_t *)guiIntfStruct.sh_video)->codec->name : "");
                 strcat( trbuf,tmp ); break;
       case 's': if ( guiIntfStruct.Playing == 0 ) strcat( trbuf,"s" ); break;
       case 'l': if ( guiIntfStruct.Playing == 1 ) strcat( trbuf,"p" ); break;
       case 'e': if ( guiIntfStruct.Playing == 2 ) strcat( trbuf,"e" ); break;
       case 'a':
            if ( muted ) { strcat( trbuf,"n" ); break; }
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
#ifdef HAVE_VCD
             case STREAMTYPE_VCD:    strcat( trbuf,"v" ); break;
#endif
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

static char * image_buffer = NULL;
static int    image_width = 0;

void PutImage( txSample * bf,int x,int y,int max,int ofs )
{
 int i=0,ix,iy;
 uint32_t * buf = NULL;
 uint32_t * drw = NULL;
 register uint32_t tmp;
 register uint32_t yc;  

 if ( ( !bf )||( bf->Image == NULL ) ) return;

 i=( bf->Width * ( bf->Height / max ) ) * ofs;
 buf=(uint32_t *)image_buffer;
 drw=(uint32_t *)bf->Image;

#if 1
 for ( iy=y;iy < (int)(y+bf->Height / max);iy++ )
  for ( ix=x;ix < (int)(x+bf->Width);ix++ )
   {
    tmp=drw[i++]; 
    if ( tmp != 0x00ff00ff ) buf[iy * image_width + ix]=tmp;
   }
#else
 yc=y * image_width; 
 for ( iy=y;iy < (int)(y+bf->Height / max);iy++ )
  {
   for ( ix=x;ix < (int)(x+bf->Width);ix++ )
    {
     tmp=drw[i++]; 
     if ( tmp != 0x00ff00ff ) buf[yc + ix]=tmp;
    }
   yc+=image_width;
  }
#endif
}

void Render( wsTWindow * window,wItem * Items,int nrItems,char * db,int size )
{
 wItem    * item;
 txSample * image = NULL;
 int        i, type;

 image_buffer=db;
 image_width=window->Width;

 for( i=0;i < nrItems + 1;i++ )
  {
   item=&Items[i];
   switch( item->type )
    {
     case itButton:
          PutImage( &item->Bitmap,item->x,item->y,3,item->pressed );
          break;
     case itPotmeter:
          PutImage( &item->Bitmap,item->x,item->y,item->phases,( item->phases - 1 ) * ( item->value / 100.0f ) );
          break;
     case itHPotmeter:
          PutImage( &item->Bitmap,item->x,item->y,item->phases,item->phases * ( item->value / 100.0f ) );
          PutImage( &item->Mask,item->x + (int)( ( item->width - item->psx ) * item->value / 100.0f ),item->y,3,item->pressed );
          break;
     case itVPotmeter:
          PutImage( &item->Bitmap,
	    item->x,item->y,
	    item->phases,
	    item->phases * ( item->value / 100.0f ) );
          PutImage( &item->Mask,
	    item->x,item->y + (int)( ( item->height - item->psy ) * item->value / 100.0f ),
	    3,item->pressed );
          break;
     case itSLabel:
          image=fntRender( item,0,"%s",item->label );
          if ( image ) PutImage( image,item->x,item->y,1,0 );
     case itDLabel:
          {
           char * t = Translate( item->label );
           int    l = fntTextWidth( item->fontid,t );
           image=fntRender( item,(GetTimerMS() / 20)%(l?l:item->width),"%s",t );
	  }
          if ( image ) PutImage( image,item->x,item->y,1,0 );
          break;
    }
  }
 wsConvert( window,db,size );
}
