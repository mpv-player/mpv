#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <png.h>

#include "mp_msg.h"
#include "help_mp.h"
#include "bitmap.h"

int pngRead( unsigned char * fname,txSample * bf )
{
 unsigned char   header[8];
 png_structp     png;
 png_infop       info;
 png_infop       endinfo;
 png_bytep     * row_p;
 png_bytep       palette = NULL;
 int             color;
 png_uint_32     i;
 
 FILE *fp=fopen( fname,"rb" );
 if ( !fp ) 
  {
   mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"[png] file read error ( %s )\n",fname );
   return 1;
  }

 fread( header,1,8,fp );
 if ( !png_check_sig( header,8 ) ) return 1;

 png=png_create_read_struct( PNG_LIBPNG_VER_STRING,NULL,NULL,NULL );
 info=png_create_info_struct( png );
 endinfo=png_create_info_struct( png );

 png_init_io( png,fp );
 png_set_sig_bytes( png,8 );
 png_read_info( png,info );
 png_get_IHDR( png,info,&bf->Width,&bf->Height,&bf->BPP,&color,NULL,NULL,NULL );

 row_p=malloc( sizeof( png_bytep ) * bf->Height );
 if ( !row_p )
  {
   mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"[png]  not enough memory for row buffer\n" );
   return 2;
  }
 bf->Image=(png_bytep)malloc( png_get_rowbytes( png,info ) * bf->Height );
 if ( !bf->Image )
  {
   mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"[png]  not enough memory for image buffer\n" );
   return 2;
  }
 for ( i=0; i < bf->Height; i++ ) row_p[i]=&bf->Image[png_get_rowbytes( png,info ) * i];

 png_read_image( png,row_p );
 free( row_p );

#if 0
 if ( color == PNG_COLOR_TYPE_PALETTE )
  {
   int cols;
   png_get_PLTE( png,info,(png_colorp *)&palette,&cols );
  }
#endif

 if ( color&PNG_COLOR_MASK_ALPHA )
  {
   if ( color&PNG_COLOR_MASK_PALETTE || color == PNG_COLOR_TYPE_GRAY_ALPHA ) bf->BPP*=2;
     else bf->BPP*=4;
  }
  else
   {
    if ( color&PNG_COLOR_MASK_PALETTE || color == PNG_COLOR_TYPE_GRAY ) bf->BPP*=1;
      else bf->BPP*=3;
   }

 png_read_end( png,endinfo );
 png_destroy_read_struct( &png,&info,&endinfo );

 if ( fclose( fp ) != 0 )
  {
   free( bf->Image );
   free( palette );
   return 1;
  }
 bf->ImageSize=bf->Width * bf->Height * ( bf->BPP / 8 );

 mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"[png] filename: %s.\n",fname );
 mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"[png]  size: %dx%d bits: %d\n",bf->Width,bf->Height,bf->BPP );
 mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"[png]  imagesize: %lu\n",bf->ImageSize );
 return 0;
}

int conv24to32( txSample * bf )
{
 unsigned char * tmpImage;
 int             i,c;

 if ( bf->BPP == 24 )
  {
   tmpImage=bf->Image;
   bf->ImageSize=bf->Width * bf->Height * 4;
   bf->BPP=32;
   if ( ( bf->Image=malloc( bf->ImageSize ) ) == NULL )
    {
     mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"[bitmap] not enough memory for image\n" );
     return 1;
    }
   memset( bf->Image,0,bf->ImageSize );
   for ( c=0,i=0;i < (int)(bf->Width * bf->Height * 3); )
    {
     bf->Image[c++]=tmpImage[i++];	//red
     bf->Image[c++]=tmpImage[i++];	//green
     bf->Image[c++]=tmpImage[i++]; c++;	//blue
    }
   free( tmpImage );
  }
 return 0;
}

void bgr2rgb( txSample * bf )
{
 unsigned char c;
 int           i;

 for ( i=0;i < (int)bf->ImageSize;i+=4 )
  {
   c=bf->Image[i];
   bf->Image[i]=bf->Image[i+2];
   bf->Image[i+2]=c;
  }
}

void Normalize( txSample * bf )
{
 int           i;
#ifndef WORDS_BIGENDIAN 
 for ( i=0;i < (int)bf->ImageSize;i+=4 ) bf->Image[i+3]=0;
#else
 for ( i=0;i < (int)bf->ImageSize;i+=4 ) bf->Image[i]=0; 
#endif
}

unsigned char tmp[512];

unsigned char * fExist( unsigned char * fname )
{
 FILE          * fl;
 unsigned char   ext[][6] = { ".png\0",".PNG\0" };
 int             i;

 fl=fopen( fname,"rb" );
 if ( fl != NULL )
  {
   fclose( fl );
   return fname;
  }
 for ( i=0;i<2;i++ )
  {
   snprintf( tmp,511,"%s%s",fname,ext[i] );
   fl=fopen( tmp,"rb" );
   if ( fl != NULL )
    {
     fclose( fl );
     return tmp;
    }
  }
 return NULL;
}

int bpRead( char * fname, txSample * bf )
{
 fname=fExist( fname );
 if ( fname == NULL ) return -2;
 if ( pngRead( fname,bf ) ) 
  {
   mp_dbg( MSGT_GPLAYER,MSGL_FATAL,"[bitmap] unknown file type ( %s )\n",fname );
   return -5;
  }
 if ( bf->BPP < 24 )
  {
   mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"[bitmap] Sorry, only 24 and 32 bpp bitmaps are supported.\n" );
   return -1;
  }
 if ( conv24to32( bf ) ) return -8;
#ifdef WORDS_BIGENDIAN
 swab(bf->Image, bf->Image, bf->ImageSize);
#endif
 bgr2rgb( bf );
 Normalize( bf );
 return 0;
}

void Convert32to1( txSample * in,txSample * out,int adaptivlimit )
{
 out->Width=in->Width;
 out->Height=in->Height;
 out->BPP=1;
 out->ImageSize=(out->Width * out->Height + 7) / 8;
 mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"[c32to1] imagesize: %d\n",out->ImageSize );
 out->Image=calloc( 1,out->ImageSize );
 if ( out->Image == NULL ) mp_msg( MSGT_GPLAYER,MSGL_WARN,MSGTR_NotEnoughMemoryC32To1 );
 {
  int i,b,c=0; unsigned int * buf = NULL; unsigned char tmp = 0; int nothaveshape = 1;
  buf=(unsigned int *)in->Image;
  for ( b=0,i=0;i < (int)(out->Width * out->Height);i++ )
   {
    if ( (int)buf[i] != adaptivlimit ) tmp=( tmp >> 1 )|128;
     else { tmp=tmp >> 1; buf[i]=nothaveshape=0; }
    if ( b++ == 7 ) { out->Image[c++]=tmp; tmp=b=0; }
   }
  if ( b ) out->Image[c]=tmp;
  if ( nothaveshape ) { free( out->Image ); out->Image=NULL; }
 }
}

void Convert1to32( txSample * in,txSample * out )
{
 if ( in->Image == NULL ) return;
 out->Width=in->Width;
 out->Height=in->Height;
 out->BPP=32;
 out->ImageSize=out->Width * out->Height * 4;
 out->Image=calloc( 1,out->ImageSize );
 mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"[c1to32] imagesize: %d\n",out->ImageSize );
 if ( out->Image == NULL ) mp_msg( MSGT_GPLAYER,MSGL_WARN,MSGTR_NotEnoughMemoryC1To32 );
 {
  int i,b,c=0; unsigned int * buf = NULL; unsigned char tmp = 0;
  buf=(unsigned int *)out->Image;
  for ( c=0,i=0;i < (int)(in->Width * in->Height / 8);i++ )
   {
    tmp=in->Image[i];
    for ( b=0;b<8;b++ )
     {
      buf[c]=0;
      if ( tmp&0x1 ) buf[c]=0xffffffff;
      c++; tmp=tmp>>1;
     }
   }
 }
}
