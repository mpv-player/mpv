
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <inttypes.h>

#include "skin.h"
#include "font.h"
#include "cut.h"
#include "../../mp_msg.h"

int items;

bmpFont * Fonts[25] = { NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL };

int fntAddNewFont( char * name )
{
 int id;
 for( id=0;id<25;id++ ) if ( !Fonts[id] ) break;
 if ( ( Fonts[id]=malloc( sizeof( bmpFont ) ) ) == NULL ) return -1;
 strcpy( Fonts[id]->name,name );
 memset( Fonts[id]->Fnt,-1,256 * sizeof( fntChar ) );
 return id;
}

void fntFreeFont( void )
{
 int i;
 for( i=0;i<25;i++ )
  {
   if ( Fonts[i] )
    {
     if ( Fonts[i]->Bitmap.Image ) free( Fonts[i]->Bitmap.Image );
     free( Fonts[i] );
     Fonts[i]=NULL;
    }
  }
}

int fntRead( char * path,char * fname,int id )
{
 FILE * f;
 unsigned char   tmp[512];
 unsigned char * ptmp;
 unsigned char   command[32];
 unsigned char   param[256];
 int             c,linenumber = 0;

 strcpy( tmp,path ); strcat( tmp,fname ); strcat( tmp,".fnt" );
 if ( ( f=fopen( tmp,"rt" ) ) == NULL ) return -1;
 while ( !feof( f ) )
  {
   fgets( tmp,255,f ); linenumber++;

   c=tmp[ strlen( tmp ) - 1 ]; if ( ( c == '\n' )||( c == '\r' ) ) tmp[ strlen( tmp ) - 1 ]=0;
   c=tmp[ strlen( tmp ) - 1 ]; if ( ( c == '\n' )||( c == '\r' ) ) tmp[ strlen( tmp ) - 1 ]=0;
   for ( c=0;c < (int)strlen( tmp );c++ )
    if ( tmp[c] == ';' )
     {
      tmp[c]=0;
      break;
     }
   if ( strlen( tmp ) == 0 ) continue;
   ptmp=strdelspacesbeforecommand( tmp );
   if ( strlen( ptmp ) == 0 ) continue;
   ptmp=strswap( ptmp,'\t',' ' );
   ptmp=strdelspaces( ptmp );
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
        strcpy( tmp,path ); strcat( tmp,param );
        mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"[font] font imagefile: %s\n",tmp );
        if ( skinBPRead( tmp,&Fonts[id]->Bitmap ) ) return -2;
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
 if ( !Fonts[id] ) return 0;
 for ( i=0;i < (int)strlen( str );i++ )
   if ( Fonts[id]->Fnt[ (int)str[i] ].sx != -1 ) size+=Fonts[id]->Fnt[ (int)str[i] ].sx;
 return size;
}

int fntTextHeight( int id,char * str )
{
 int max = 0,i;
 if ( !Fonts[id] ) return 0;
 for ( i=0;i < (int)strlen( str );i++ )
   if ( Fonts[id]->Fnt[ (int)str[i] ].sy > max ) max=Fonts[id]->Fnt[ (int)str[i] ].sy;
 return max;
}

txSample * fntRender( int id,int px,int sx,char * fmt,... )
{
 txSample      * tmp = NULL;
 txSample 	 tmp2;
 char            p[512];
 va_list         ap;
 uint32_t * ibuf;
 uint32_t * obuf;
 int             i,x,y;
 int             oy = 0, ox = 0, dx = 0;

 va_start( ap,fmt );
 vsnprintf( p,512,fmt,ap );
 va_end( ap );

 if ( ( !Fonts[id] )||
      ( !strlen( p ) )||
      ( !fntTextWidth( id,p ) )||
      ( (tmp=malloc( sizeof( txSample ) )) == NULL ) ) return NULL;

 tmp->Width=fntTextWidth( id,p );
 tmp->Height=fntTextHeight( id,p );
 tmp->BPP=32;
 tmp->ImageSize=tmp->Width * tmp->Height * 4;
 if ( ( tmp->Image=malloc( tmp->ImageSize ) ) ==  NULL ) return NULL;

 obuf=(uint32_t *)tmp->Image;
 ibuf=(uint32_t *)Fonts[id]->Bitmap.Image;
 for ( i=0;i < (int)strlen( p );i++ )
  {
   char c = p[i];
   if ( Fonts[id]->Fnt[c].x == -1 ) c=32;
   for ( oy=0,y=Fonts[id]->Fnt[c].y;y < Fonts[id]->Fnt[c].y + Fonts[id]->Fnt[c].sy; y++,oy++ )
    for ( ox=0,x=Fonts[id]->Fnt[c].x;x < Fonts[id]->Fnt[c].x + Fonts[id]->Fnt[c].sx; x++,ox++ )
     {
      obuf[ oy * tmp->Width + dx + ox ]=ibuf[ y * Fonts[id]->Bitmap.Width + x ];
     }
   dx+=Fonts[id]->Fnt[c].sx;
  }

 if ( ( sx > 0 )&&( sx < tmp->Width ) )
  {
   tmp2.ImageSize=sx * tmp->Height * 4;
   if ( ( tmp2.Image=malloc( tmp2.ImageSize ) ) ==  NULL ) { free( tmp->Image ); return NULL; }

   obuf=(uint32_t *)tmp->Image;
   ibuf=(uint32_t *)tmp2.Image;
   oy=0;

   for ( y=0;y < tmp->Height;y++ )
    {
     ox=px;
     dx=y * tmp->Width;
     for ( x=0;x < sx;x++ )
      {
       ibuf[oy++]=obuf[dx + ox++];
       if ( ox >= tmp->Width ) ox=0;
      }
    }

   free( tmp->Image ); tmp->Width=sx; tmp->ImageSize=tmp2.ImageSize; tmp->Image=tmp2.Image;
  }

 return tmp;
}
