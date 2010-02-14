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

#include "gui/app.h"
#include "gui/skin/font.h"
#include "gui/skin/skin.h"
#include "gui/wm/ws.h"

#include "config.h"
#include "help_mp.h"
#include "libvo/x11_common.h"

#include "stream/stream.h"
#include "mixer.h"
#include "libvo/sub.h"

#include "libmpdemux/demuxer.h"
#include "libmpdemux/stheader.h"
#include "codec-cfg.h"
#include "access_mpcontext.h"
#include "libavutil/avstring.h"

#include "gui_common.h"
#include "play.h"
#include "widgets.h"

unsigned int GetTimerMS( void );

static inline void TranslateFilename( int c,char * tmp,size_t tmplen )
{
 int i;
 char * p;

 switch ( guiIntfStruct.StreamType )
  {
   case STREAMTYPE_STREAM:
        av_strlcpy(tmp, guiIntfStruct.Filename, tmplen);
        break;
   case STREAMTYPE_FILE:
          if ( ( guiIntfStruct.Filename )&&( guiIntfStruct.Filename[0] ) )
           {
            if ( (p = strrchr(guiIntfStruct.Filename, '/')) )
              av_strlcpy(tmp, p + 1, tmplen);
            else
              av_strlcpy(tmp, guiIntfStruct.Filename, tmplen);
            if ( tmp[strlen( tmp ) - 4] == '.' ) tmp[strlen( tmp ) - 4]=0;
            if ( tmp[strlen( tmp ) - 5] == '.' ) tmp[strlen( tmp ) - 5]=0;
           } else av_strlcpy( tmp,MSGTR_NoFileLoaded,tmplen );
          break;
#ifdef CONFIG_DVDREAD
   case STREAMTYPE_DVD:
          if ( guiIntfStruct.DVD.current_chapter ) snprintf(tmp,tmplen,MSGTR_Chapter,guiIntfStruct.DVD.current_chapter );
            else av_strlcat( tmp,MSGTR_NoChapter,tmplen );
          break;
#endif
#ifdef CONFIG_VCD
   case STREAMTYPE_VCD:
        snprintf( tmp,tmplen,MSGTR_VCDTrack,guiIntfStruct.Track );
	break;
#endif
   default: av_strlcpy( tmp,MSGTR_NoMediaOpened,tmplen );
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

/* Unsafe!  Pass only null-terminated strings as (char *)str. */
char * Translate( char * str )
{
 mixer_t *mixer = mpctx_get_mixer(guiIntfStruct.mpcontext);
 static char   trbuf[512];
        char   tmp[512];
        int    i,c;
        int    t;
        int    strsize = 0;
 memset( trbuf,0,512 );
 memset( tmp,0,128 );
 strsize = strlen(str);
 for ( c=0,i=0;i < strsize;i++ )
  {
   if ( str[i] != '$' ) { trbuf[c++]=str[i]; trbuf[c]=0; }
    else
    {
     switch ( str[++i] )
      {
       case 't': snprintf( tmp,sizeof( tmp ),"%02d",guiIntfStruct.Track );
		 av_strlcat( trbuf,tmp,sizeof( trbuf ) ); break;
       case 'o': TranslateFilename( 0,tmp,sizeof( tmp ) );
		 av_strlcat( trbuf,tmp,sizeof( trbuf ) ); break;
       case 'f': TranslateFilename( 1,tmp,sizeof( tmp ) );
		 av_strlcat( trbuf,tmp,sizeof( trbuf ) ); break;
       case 'F': TranslateFilename( 2,tmp,sizeof( tmp ) );
		 av_strlcat( trbuf,tmp,sizeof( trbuf ) ); break;
       case '6': t=guiIntfStruct.LengthInSec; goto calclengthhhmmss;
       case '1': t=guiIntfStruct.TimeSec;
calclengthhhmmss:
            snprintf( tmp,sizeof( tmp ),"%02d:%02d:%02d",t/3600,t/60%60,t%60 );
            av_strlcat( trbuf,tmp,sizeof( trbuf ) );
            break;
       case '7': t=guiIntfStruct.LengthInSec; goto calclengthmmmmss;
       case '2': t=guiIntfStruct.TimeSec;
calclengthmmmmss:
            snprintf( tmp,sizeof( tmp ),"%04d:%02d",t/60,t%60 );
            av_strlcat( trbuf,tmp,sizeof( trbuf ) );
            break;
       case '3': snprintf( tmp,sizeof( tmp ),"%02d",guiIntfStruct.TimeSec / 3600 );
		 av_strlcat( trbuf,tmp,sizeof( trbuf ) ); break;
       case '4': snprintf( tmp,sizeof( tmp ),"%02d",( ( guiIntfStruct.TimeSec / 60 ) % 60 ) );
		 av_strlcat( trbuf,tmp,sizeof( trbuf ) ); break;
       case '5': snprintf( tmp,sizeof( tmp ),"%02d",guiIntfStruct.TimeSec % 60 );
		 av_strlcat( trbuf,tmp,sizeof( trbuf ) ); break;
       case '8': snprintf( tmp,sizeof( tmp ),"%01d:%02d:%02d",guiIntfStruct.TimeSec / 3600,( guiIntfStruct.TimeSec / 60 ) % 60,guiIntfStruct.TimeSec % 60 ); av_strlcat( trbuf,tmp,sizeof( trbuf ) ); break;
       case 'v': snprintf( tmp,sizeof( tmp ),"%3.2f%%",guiIntfStruct.Volume );
		 av_strlcat( trbuf,tmp,sizeof( trbuf ) ); break;
       case 'V': snprintf( tmp,sizeof( tmp ),"%3.1f",guiIntfStruct.Volume );
		 av_strlcat( trbuf,tmp,sizeof( trbuf ) ); break;
       case 'b': snprintf( tmp,sizeof( tmp ),"%3.2f%%",guiIntfStruct.Balance );
		 av_strlcat( trbuf,tmp,sizeof( trbuf ) ); break;
       case 'B': snprintf( tmp,sizeof( tmp ),"%3.1f",guiIntfStruct.Balance );
		 av_strlcat( trbuf,tmp,sizeof( trbuf ) ); break;
       case 'd': snprintf( tmp,sizeof( tmp ),"%d",guiIntfStruct.FrameDrop );
		 av_strlcat( trbuf,tmp,sizeof( trbuf ) ); break;
       case 'x': snprintf( tmp,sizeof( tmp ),"%d",guiIntfStruct.MovieWidth );
		 av_strlcat( trbuf,tmp,sizeof( trbuf ) ); break;
       case 'y': snprintf( tmp,sizeof( tmp ),"%d",guiIntfStruct.MovieHeight );
		 av_strlcat( trbuf,tmp,sizeof( trbuf ) ); break;
       case 'C': snprintf( tmp,sizeof( tmp ),"%s", guiIntfStruct.sh_video? ((sh_video_t *)guiIntfStruct.sh_video)->codec->name : "");
                 av_strlcat( trbuf,tmp,sizeof( trbuf ) ); break;
       case 's': if ( guiIntfStruct.Playing == 0 ) av_strlcat( trbuf,"s",sizeof( trbuf ) ); break;
       case 'l': if ( guiIntfStruct.Playing == 1 ) av_strlcat( trbuf,"p",sizeof( trbuf ) ); break;
       case 'e': if ( guiIntfStruct.Playing == 2 ) av_strlcat( trbuf,"e",sizeof( trbuf ) ); break;
       case 'a':
            if ( mixer->muted ) { av_strlcat( trbuf,"n",sizeof( trbuf ) ); break; }
            switch ( guiIntfStruct.AudioType )
             {
              case 0: av_strlcat( trbuf,"n",sizeof( trbuf ) ); break;
              case 1: av_strlcat( trbuf,"m",sizeof( trbuf ) ); break;
              case 2: av_strlcat( trbuf,"t",sizeof( trbuf ) ); break;
             }
            break;
       case 'T':
           switch ( guiIntfStruct.StreamType )
            {
             case STREAMTYPE_FILE:   av_strlcat( trbuf,"f",sizeof( trbuf ) ); break;
#ifdef CONFIG_VCD
             case STREAMTYPE_VCD:    av_strlcat( trbuf,"v",sizeof( trbuf ) ); break;
#endif
             case STREAMTYPE_STREAM: av_strlcat( trbuf,"u",sizeof( trbuf ) ); break;
#ifdef CONFIG_DVDREAD
             case STREAMTYPE_DVD:    av_strlcat( trbuf,"d",sizeof( trbuf ) ); break;
#endif
             default:                av_strlcat( trbuf," ",sizeof( trbuf ) ); break;
            }
           break;
       case '$': av_strlcat( trbuf,"$",sizeof( trbuf ) ); break;
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
 /* register uint32_t yc; */

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

void SimplePotmeterPutImage( txSample * bf,int x,int y,float frac )
{
 int i=0,w,r,ix,iy;
 uint32_t * buf = NULL;
 uint32_t * drw = NULL;
 register uint32_t tmp;

 if ( ( !bf )||( bf->Image == NULL ) ) return;

 buf=(uint32_t *)image_buffer;
 drw=(uint32_t *)bf->Image;
 w=bf->Width*frac;
 r=bf->Width-w;
 for ( iy=y;iy < (int)(y+bf->Height);iy++ )
 {
  for ( ix=x;ix < (int)(x+w);ix++ )
   {
    tmp=drw[i++];
    if ( tmp != 0x00ff00ff ) buf[iy * image_width + ix]=tmp;
   }
  i+=r;
 }
}

void Render( wsTWindow * window,wItem * Items,int nrItems,char * db,int size )
{
 wItem    * item;
 txSample * image = NULL;
 int        i;

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
          if (item->phases == 1)SimplePotmeterPutImage( &item->Bitmap,item->x,item->y, item->value / 100.0f );
          else PutImage( &item->Bitmap,item->x,item->y,item->phases,( item->phases - 1 ) * ( item->value / 100.0f ) );
          break;
     case itHPotmeter:
          if (item->phases == 1)SimplePotmeterPutImage( &item->Bitmap,item->x,item->y, item->value / 100.0f );
          else PutImage( &item->Bitmap,item->x,item->y,item->phases,( item->phases - 1 ) * ( item->value / 100.0f ) );
          PutImage( &item->Mask,item->x + (int)( ( item->width - item->psx ) * item->value / 100.0f ),item->y,3,item->pressed );
          break;
     case itVPotmeter:
          PutImage( &item->Bitmap,
	    item->x,item->y,
	    item->phases,
	    item->phases * ( 1. - item->value / 100.0f ) );
          PutImage( &item->Mask,
	    item->x,item->y + (int)( ( item->height - item->psy ) * ( 1. - item->value / 100.0f ) ),
	    3,item->pressed );
          break;
     case itSLabel:
          image=fntRender( item,0,"%s",item->label );
          if ( image ) PutImage( image,item->x,item->y,1,0 );
     case itDLabel:
          {
           char * t = Translate( item->label );
           int    l = fntTextWidth( item->fontid,t );
           l=(l?l:item->width);
           image=fntRender( item,l-(GetTimerMS() / 20)%l,"%s",t );
	  }
          if ( image ) PutImage( image,item->x,item->y,1,0 );
          break;
    }
  }
 wsConvert( window,db,size );
}
