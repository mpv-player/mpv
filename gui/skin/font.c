
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <inttypes.h>

#include "app.h"
#include "skin.h"
#include "font.h"
#include "cut.h"
#include "../mp_msg.h"

int items;

bmpFont * Fonts[26] = { NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL };

int fntAddNewFont( char * name )
{
 int id;
 int i;

 for( id=0;id<26;id++ )
   if ( !Fonts[id] ) break;

 if ( id == 25 ) return -2;

 if ( ( Fonts[id]=calloc( 1,sizeof( bmpFont ) ) ) == NULL ) return -1;

 strlcpy( Fonts[id]->name,name,128 ); // FIXME: as defined in font.h
 for ( i=0;i<256;i++ ) 
   Fonts[id]->Fnt[i].x=Fonts[id]->Fnt[i].y=Fonts[id]->Fnt[i].sx=Fonts[id]->Fnt[i].sy=-1;

 return id;
}

void fntFreeFont( void )
{
 int i;
 for( i=0;i < 25;i++ )
  {
   if ( Fonts[i] )
    {
     if ( Fonts[i]->Bitmap.Image ) free( Fonts[i]->Bitmap.Image );
     free( Fonts[i] );
     Fonts[i]=NULL;
    }
  }
}

int fntRead( char * path,char * fname )
{
 FILE * f;
 unsigned char   tmp[512];
 unsigned char * ptmp;
 unsigned char   command[32];
 unsigned char   param[256];
 int             c,linenumber = 0;
 int             id = fntAddNewFont( fname );
 
 if ( id < 0 ) return id;

 strlcpy( tmp,path,sizeof( tmp ) );
 strlcat( tmp,fname,sizeof( tmp ) ); strlcat( tmp,".fnt",sizeof( tmp ) );
 if ( ( f=fopen( tmp,"rt" ) ) == NULL ) 
   { free( Fonts[id] ); return -3; }
   
 while ( !feof( f ) )
  {
   fgets( tmp,255,f ); linenumber++;

   c=tmp[ strlen( tmp ) - 1 ]; if ( ( c == '\n' )||( c == '\r' ) ) tmp[ strlen( tmp ) - 1 ]=0;
   c=tmp[ strlen( tmp ) - 1 ]; if ( ( c == '\n' )||( c == '\r' ) ) tmp[ strlen( tmp ) - 1 ]=0;
   for ( c=0;c < (int)strlen( tmp );c++ )
     if ( tmp[c] == ';' ) { tmp[c]=0; break; }
   if ( !tmp[0] ) continue;
   ptmp=trimleft( tmp );
   if ( !tmp[0] ) continue;
   ptmp=strswap( ptmp,'\t',' ' );
   ptmp=trim( ptmp );
   cutItem( ptmp,command,'=',0 ); cutItem( ptmp,param,'=',1 );
   if ( command[0] == '"' )
    {
     int i;
     cutItem( command,command,'"',1 );
     i=(int)command[0];
     cutItem( param,tmp,',',0 ); Fonts[id]->Fnt[i].x=atoi( tmp );
     cutItem( param,tmp,',',1 ); Fonts[id]->Fnt[i].y=atoi( tmp );
     cutItem( param,tmp,',',2 ); Fonts[id]->Fnt[i].sx=atoi( tmp );
     cutItem( param,tmp,',',3 ); Fonts[id]->Fnt[i].sy=atoi( tmp );
     mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"[font]  char: '%s' params: %d,%d %dx%d\n",command,Fonts[id]->Fnt[i].x,Fonts[id]->Fnt[i].y,Fonts[id]->Fnt[i].sx,Fonts[id]->Fnt[i].sy );
    }
    else
     {
      if ( !strcmp( command,"image" ) )
       {
        strlcpy( tmp,path,sizeof( tmp )  ); strlcat( tmp,param,sizeof( tmp ) );
        mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"[font] font imagefile: %s\n",tmp );
        if ( skinBPRead( tmp,&Fonts[id]->Bitmap ) ) return -4;
       }
     }
   }

 return 0;
}

int fntFindID( char * name )
{
 int i;
 for ( i=0;i < 25;i++ )
   if ( Fonts[i] )
     if ( !strcmp( name,Fonts[i]->name ) ) return i;
 return -1;
}

int fntTextWidth( int id,char * str )
{
 int size = 0;
 int i;

 if ( ( !Fonts[id] )||( !str[0] ) ) return 0;

 for ( i=0;i < (int)strlen( str );i++ )
  {
   unsigned char c = (unsigned char)str[i];
   if ( Fonts[id]->Fnt[c].sx == -1 ) c = ' ';
   size+= Fonts[id]->Fnt[ c ].sx;
  }
 return size;
}

int fntTextHeight( int id,char * str )
{
 int max = 0,i;

 if ( ( !Fonts[id] )||( !str[0] ) ) return 0;

 for ( i=0;i < (int)strlen( str );i++ )
  {
   int h;
   unsigned char c = (unsigned char)str[i];
   if ( Fonts[id]->Fnt[c].sx == -1 ) c = ' ';
   h = Fonts[id]->Fnt[c].sy;
   if ( h > max ) max=h;
  }
 return max;
}

txSample * fntRender( wItem * item,int px,const char * fmt,... )
{
 va_list         ap;
 unsigned char   p[512];
 unsigned int    c;
 int 	         i, dx = 0, tw, fbw, iw, id, ofs;
 int 		 x,y,fh,fw,fyc,yc;
 uint32_t      * ibuf;
 uint32_t      * obuf;

 va_start( ap,fmt );
 vsnprintf( p,512,fmt,ap );
 va_end( ap );

 iw=item->width;
 id=item->fontid;

 if ( ( !item )||
      ( !Fonts[id] )||
      ( !p[0] )||
      ( !fntTextWidth( id,p ) ) ) return NULL;

 tw=fntTextWidth( id,p );
 fbw=Fonts[id]->Bitmap.Width;

 if ( item->Bitmap.Image == NULL ) 
  {
   item->Bitmap.Height=item->height=fntTextHeight( id,p );
   item->Bitmap.Width=item->width=iw;
   item->Bitmap.ImageSize=item->height * iw * 4;
   if ( !item->Bitmap.ImageSize ) return NULL;
   item->Bitmap.BPP=32;
   item->Bitmap.Image=malloc( item->Bitmap.ImageSize );
  }

 obuf=(uint32_t *)item->Bitmap.Image;
 ibuf=(uint32_t *)Fonts[id]->Bitmap.Image;

 for ( i=0;i < item->Bitmap.ImageSize / 4;i++ ) obuf[i]=0xff00ff;
 
 if ( tw <= iw ) 
  {
   switch ( item->align )
    {
     default:
     case fntAlignLeft:   dx=0; break;
     case fntAlignCenter: dx=( iw - fntTextWidth( id,p ) ) / 2; break;
     case fntAlignRight:  dx=iw - fntTextWidth( id,p ); break;
    }
    
  } else dx+=px;

 ofs=dx;
 
 for ( i=0;i < (int)strlen( p );i++ )
  {
   c=(unsigned int)p[i];
   fw=Fonts[id]->Fnt[c].sx;
   
   if ( fw == -1 ) { c=32; fw=Fonts[id]->Fnt[c].sx; }
   
   fh=Fonts[id]->Fnt[c].sy;
   fyc=Fonts[id]->Fnt[c].y * fbw + Fonts[id]->Fnt[c].x;
   yc=dx;
     
   if ( dx >= 0 ) 
    for ( y=0;y < fh;y++ )
     {
      for ( x=0; x < fw;x++ )
       if ( dx + x >= 0 && dx + x < iw ) obuf[yc + x]=ibuf[ fyc + x ];
      fyc+=fbw;
      yc+=iw;
     }
   dx+=fw;
  }

 if ( ofs > 0 && tw > item->width )
  {
   dx=ofs;
   for ( i=(int)strlen( p );i > 0;i-- )
    {
     c=(unsigned int)p[i];
     fw=Fonts[id]->Fnt[c].sx;
  
     if ( fw == -1 ) { c=32; fw=Fonts[id]->Fnt[c].sx; }

     fh=Fonts[id]->Fnt[c].sy;
     fyc=Fonts[id]->Fnt[c].y * fbw + Fonts[id]->Fnt[c].x;

     dx-=fw; yc=dx;
     if ( dx >= 0 ) 
      for ( y=0;y < fh;y++ )
       {
        for ( x=fw - 1;x >= 0;x-- )
         if ( dx + x >= 0 && dx + x < iw ) obuf[yc + x]=ibuf[fyc + x];
        fyc+=fbw;
	yc+=iw;
       }
    }
  }

 return &item->Bitmap;
}
