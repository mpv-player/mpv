
#ifndef __MY_BMP
#define __MY_BMP

/*
    0.1  : BMP type.
    2.5  : File size.
    6.7  : Res.
    8.9  : Res.
   10.13 : Offset of bitmap.
   14.17 : Header size.
   18.21 : X size.
   22.25 : Y size.
   26.27 : Number of planes.
   28.29 : Number of bits per pixel.
   30.33 : Compression flag.
   34.37 : Image data size in bytes.
   38.41 : Res
   42.45 : Res
   46.49 : Res
   50.53 : Res
*/

#include <stdlib.h>
#include <stdio.h>

#include "bmp.h"
#include "../bitmap.h"
#include "../../error.h"

int bmpRead( unsigned char * fname,txSample * bF )
{
 unsigned char   bmpHeader[54];
 FILE          * BMP;
 unsigned long   i;
 unsigned char * line;
 int             linesize;


 if ( (BMP=fopen( fname,"rt" )) == NULL )
  {
#ifdef DEBUG
   dbprintf( 4,"[bmp] File not found ( %s ).\n",fname );
#endif
   return 1;
  }
 if ( (i=fread( bmpHeader,54,1,BMP )) != 1 )
  {
#ifdef DEBUG
   dbprintf( 4,"[bmp] Header read error ( %s ).\n",fname );
#endif
   return 2;
  }
// memcpy( &bF->Size,&bmpHeader[2],4 );
 memcpy( &bF->Width,&bmpHeader[18],4 );
 memcpy( &bF->Height,&bmpHeader[22],4 );
 memcpy( &bF->BPP,&bmpHeader[28],2 );
// memcpy( &bF->ImageSize,&bmpHeader[34],4 );
 bF->ImageSize=( bF->Width * bF->Height ) * ( bF->BPP / 8 );

 if ( bF->BPP < 24 )
  {
   #ifdef DEBUG
    dbprintf( 4,"[bmp] Sorry, this loader not supported 16 bit or less ...\n" );
   #endif
   return 3;
  }

#ifdef DEBUG
 dbprintf( 4,"[bmp] filename: %s\n",fname );
 dbprintf( 4,"[bmp]  size: %dx%d bits: %d\n",bF->Width,bF->Height,bF->BPP );
 dbprintf( 4,"[bmp]  imagesize: %lu\n",bF->ImageSize );
#endif

 if ( ( bF->Image=malloc( bF->ImageSize ) ) == NULL )
  {
   #ifdef DEBUG
    dbprintf( 4,"[bmp]  Not enough memory for image buffer.\n" );
   #endif
   return 4;
  }

 if ( (i=fread( bF->Image,bF->ImageSize,1,BMP )) != 1 )
   {
   #ifdef DEBUG
    dbprintf( 4,"[bmp]  Image read error.\n" );
   #endif
   return 5;
  }

 fclose( BMP );

 linesize=bF->Width * ( bF->BPP / 8 );
 if ( (line=malloc( linesize )) == NULL )
  {
   #ifdef DEBUG
    dbprintf( 4,"[bmp] Not enough memory for flipping.\n" );
   #endif
   return 6;
  }

 for ( i=0;i < bF->Height / 2;i++ )
  {
   memcpy( line,&bF->Image[ i * linesize ],linesize );
   memcpy( &bF->Image[ i * linesize ],&bF->Image[ ( bF->Height - i - 1 ) * linesize ],linesize );
   memcpy( &bF->Image[ ( bF->Height - i - 1 ) * linesize ],line,linesize );
  }
 free( line );

 return 0;
}

#endif