
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "tga.h"
#include "../../error.h"

int tgaRead( char * filename,txSample * bf )
{
 FILE          * BMP;
 unsigned long   i;
 char            tmp[255];
 unsigned char * line;
 int             linesize;
 char          * comment;
 tgaHeadert      tgaHeader;

 strcpy( tmp,filename );
 if ( !strstr( tmp,".tga" ) ) strcat( tmp,".tga" );
 if ( (BMP=fopen( tmp,"rb" )) == NULL )
  {
   #ifdef DEBUG
    dbprintf( 4,"[tga] File not found ( %s ).\n",tmp );
   #endif
   return 1;
  }
 if ( (i=fread( &tgaHeader,sizeof( tgaHeader ),1,BMP )) != 1 )
  {
   #ifdef DEBUG
    dbprintf( 4,"[tga] Header read error ( %s ).\n",tmp );
   #endif
   return 2;
  }
 if ( tgaHeader.depth < 24 )
  {
   #ifdef DEBUG
    dbprintf( 4,"[tga] Sorry, this loader not supported 16 bit or less ...\n" );
   #endif
   return 3;
  }
 bf->Width=tgaHeader.sx;
 bf->Height=tgaHeader.sy;
 bf->BPP=tgaHeader.depth;
 bf->ImageSize=bf->Width * bf->Height * ( bf->BPP / 8 );

 if ( ( bf->Image=malloc( bf->ImageSize ) ) == NULL )
  {
   #ifdef DEBUG
    dbprintf( 4,"[tga]  Not enough memory for image buffer.\n" );
   #endif
   return 4;
  }

 comment=NULL;
 if ( tgaHeader.tmp[0] != 0 )
  {
   if ( ( comment=malloc( tgaHeader.tmp[0] + 1 ) ) == NULL )
    {
     #ifdef DEBUG
      dbprintf( 4,"[tga] Not enough memory for comment string.\n" );
     #endif
     return 5;
    }
   memset( comment,0,tgaHeader.tmp[0] + 1 );
   if ( fread( comment,tgaHeader.tmp[0],1,BMP ) != 1 )
    {
     #ifdef DEBUG
      dbprintf( 4,"[tga] Comment read error.\n" );
     #endif
   return 6;
    }
  }

 #ifdef DEBUG
  dbprintf( 4,"[tga] filename ( read ): %s\n",tmp );
  dbprintf( 4,"[tga]  size: %dx%d bits: %d\n",bf->Width,bf->Height,bf->BPP );
  dbprintf( 4,"[tga]  imagesize: %lu\n",bf->ImageSize );
  if ( comment ) dbprintf( 4,"[tga]  comment: %s\n",comment );
 #endif

 if ( comment ) free( comment );

 if ( fread( bf->Image,bf->ImageSize,1,BMP ) != 1 )
  {
   #ifdef DEBUG
    dbprintf( 4,"[tga] Image read error.\n" );
   #endif
   return 7;
  }

 fclose( BMP );

 if ( tgaHeader.ctmp == 0 )
  {
   linesize=bf->Width * ( bf->BPP / 8 );
   if ( (line=malloc( linesize )) == NULL )
    {
     #ifdef DEBUG
      dbprintf( 4,"[tga] Not enough memory for flipping.\n" );
     #endif
     return 8;
    }

   for ( i=0;i < bf->Height / 2;i++ )
    {
     memcpy( line,&bf->Image[ i * linesize ],linesize );
     memcpy( &bf->Image[ i * linesize ],&bf->Image[ ( bf->Height - i - 1 ) * linesize ],linesize );
     memcpy( &bf->Image[ ( bf->Height - i - 1 ) * linesize ],line,linesize );
    }
   free( line );
  }

 return 0;
}

char comment[] = "fresh!mindworkz's TGA Filter. v0.1";

void tgaWriteTexture( char * filename,txSample * bf )
{
 FILE          * BMP;
 int             i;
 unsigned char * line;
 int             linesize;
 tgaHeadert      tgaHeader;
 char            tmp[255];

 strcpy( tmp,filename );
 if ( !strstr( tmp,".tga" ) ) strcat( tmp,".tga" );
 if ( ( BMP=fopen( tmp,"wb+" ) ) == NULL )
  {
   dbprintf( 0,"[tga] File not open ( %s ).\n",tmp );
   exit( 0 );
  }
 memset( &tgaHeader,0,sizeof( tgaHeader ) );
 tgaHeader.sx=bf->Width;
 tgaHeader.sy=bf->Height;
 tgaHeader.depth=bf->BPP;
 tgaHeader.ctmp=0;
 tgaHeader.tmp[0]=strlen( comment );
 if ( bf->BPP != 8 ) tgaHeader.tmp[2]=2;
  else tgaHeader.tmp[2]=3;

 #ifdef DEBUG
  dbprintf( 4,"\n[tga] filename ( write ): %s\n",tmp );
  dbprintf( 4,"[tga]  size: %dx%d\n",bf->Width,bf->Height );
  dbprintf( 4,"[tga]  bits: %d\n",bf->BPP );
  dbprintf( 4,"[tga]  imagesize: %lu\n",bf->ImageSize );
  dbprintf( 4,"[tga]  comment: %s\n",comment );
  dbprintf( 4,"\n" );
 #endif

 if ( tgaHeader.ctmp == 0 )
  {
   linesize=bf->Width * ( bf->BPP / 8 );
   if ( (line=malloc( linesize )) == NULL )
    {
     dbprintf( 0,"[tga] Not enough memory for flipping.\n" );
     exit( 0 );
    }

   for ( i=0;i < bf->Height / 2;i++ )
    {
     memcpy( line,&bf->Image[ i * linesize ],linesize );
     memcpy( &bf->Image[ i * linesize ],&bf->Image[ ( bf->Height - i - 1 ) * linesize ],linesize );
     memcpy( &bf->Image[ ( bf->Height - i - 1 ) * linesize ],line,linesize );
    }
   free( line );
  }

 fwrite( &tgaHeader,sizeof( tgaHeader ),1,BMP );
 fwrite( comment,strlen( comment ),1,BMP );
 fwrite( bf->Image,bf->ImageSize,1,BMP );

 fclose( BMP );
}

void tgaWriteBuffer( char * fname,unsigned char * Buffer,int sx,int sy,int BPP )
{
 txSample tmp;

 memset( &tmp,0,sizeof( tmp ) );
 tmp.Width=sx;
 tmp.Height=sy;
 tmp.BPP=BPP;
 tmp.ImageSize=sx * sy * ( BPP / 8 );
 tmp.Image=Buffer;
 tgaWriteTexture( fname,&tmp );
}