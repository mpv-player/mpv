
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <inttypes.h>

#include "../app.h"
#include "skin.h"
#include "font.h"
#include "cut.h"
#include "../../mp_msg.h"

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

 strcpy( Fonts[id]->name,name );
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

 strcpy( tmp,path ); strcat( tmp,fname ); strcat( tmp,".fnt" );
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
   ptmp=strdelspacesbeforecommand( tmp );
   if ( !tmp[0] ) continue;
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

typedef struct
{
 int  pos;
 char c;
} iChar;

txSample * fntRender( wItem * item,int px,char * fmt,... )
{
#if 0
 txSample  * tmp = NULL;
 va_list     ap;
 char        p[512];
 iChar       pos[512];
 int 	     i, dx = 0, s, tw;
 uint32_t  * ibuf;
 uint32_t  * obuf;

 va_start( ap,fmt );
 vsnprintf( p,512,fmt,ap );
 va_end( ap );

 if ( ( !item )||
      ( !Fonts[item->fontid] )||
      ( !p[0] )||
      ( !fntTextWidth( item->fontid,p ) ) ) return NULL;

 tw=fntTextWidth( item->fontid,p );

 if ( item->Bitmap.Image == NULL ) 
  {
   item->Bitmap.Height=item->height=fntTextHeight( item->fontid,p );
   item->Bitmap.Width=item->width;
   item->Bitmap.ImageSize=item->height * item->width * 4;
   item->Bitmap.BPP=32;
   item->Bitmap.Image=malloc( item->Bitmap.ImageSize );
  }

 obuf=(uint32_t *)item->Bitmap.Image;
 ibuf=(uint32_t *)Fonts[item->fontid]->Bitmap.Image;

 for ( i=0;i < item->Bitmap.ImageSize / 4;i++ ) obuf[i]=0xff00ff;
 
 if ( tw < item->width ) 
  {
   switch ( item->align )
    {
     default:
     case fntAlignLeft:   dx=0; break;
     case fntAlignCenter: dx=( item->width - fntTextWidth( item->fontid,p ) ) / 2; break;
     case fntAlignRight:  dx=item->width - fntTextWidth( item->fontid,p ); break;
    }
    
  } else dx+=px;
/*
 for ( i=0;i < (int)strlen( p );i++ )
  {
   int c = (int)p[i];
   int fw = Fonts[item->fontid]->Fnt[c].sx;
   int fh = Fonts[item->fontid]->Fnt[c].sy;
   int fx = Fonts[item->fontid]->Fnt[c].x;
   int fy = Fonts[item->fontid]->Fnt[c].y;

   if ( fw != -1 )
    {
     // font rendernig
     int x,y;
     for ( y=0;y < fh;y++ )
      {
       if ( dx >= 0 ) 
        for ( x=0; x < fw;x++ )
         {
          if ( dx + x >= item->width ) goto fnt_exit; 
          obuf[y * item->width + x + dx]=ibuf[ ( fy + y ) * Fonts[item->fontid]->Bitmap.Width + fx + x ];
         }
      }
     dx+=fw;
    } else dx+=4;
  }

fnt_exit:
*/

if ( !strncmp( p,"lofasz",6 ) )
{
 int i,j, c = 0;
 char t[512];
 memset( t,0,512 );
// printf( "!!!! " );
 for ( i=0; i < (int)strlen( p );i++ )
  {
   int c = (int)p[i];
   int fw = Fonts[item->fontid]->Fnt[c].sx;
   pos[i].pos=dx;
   pos[i].c=p[i];
   if ( pos[i].pos > item->width ) pos[i].pos-=item->width;
//   printf( "%d; ",pos[i] );
   dx+=fw;
  }
 for ( i=0;i < (int)strlen( p );i++ ) 
  for ( j=strlen( p );j > i;j-- )
   if ( pos[j].pos < pos[i].pos )
   {
    iChar tmp;
    memcpy( &tmp,&pos[i],sizeof( iChar ) );
    memcpy( &pos[i],&pos[j],sizeof( iChar ) );
    memcpy( &pos[j],&tmp,sizeof( iChar ) );
   }
//
 for ( i=0;i < (int)strlen( p );i++ )
  t[c++]=pos[i].c;
//  if ( pos[i].pos > 0 && pos[i].pos < item->width ) t[c++]=pos[i].c;
 printf( "!!! %s\n",t );
}

 return &item->Bitmap;
 
#else
 txSample 	 tmp2;
 txSample      * tmp = NULL;
 va_list         ap;
 char            p[512];
 uint32_t * ibuf;
 uint32_t * obuf;
 int             i,x,y;
 int             oy = 0, ox = 0, dx = 0, s = 0;
 int		 id=item->fontid;
 int		 sx=item->width;
 int		 a=item->align;

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
   unsigned int c = (unsigned char)p[i];
   int cx,cy;
   
   if ( Fonts[id]->Fnt[c].sx == -1 ) c=32;
   
   cx=Fonts[id]->Fnt[c].x;
   cy=Fonts[id]->Fnt[c].y;

   for ( oy=0,y=cy;y < cy + Fonts[id]->Fnt[c].sy; y++,oy++ )
     for ( ox=0,x=cx;x < cx + Fonts[id]->Fnt[c].sx; x++,ox++ )
        obuf[ oy * tmp->Width + dx + ox ]=ibuf[ y * Fonts[id]->Bitmap.Width + x ];

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
#endif

 return tmp;
}
