
#include <stdlib.h>

#include "./png.h"
#include "../../error.h"
#include <png.h>

typedef struct
{
 unsigned int    Width;
 unsigned int    Height;
 unsigned int    Depth;
 unsigned int    Alpha;

 unsigned int    Components;
 unsigned char * Data;
 unsigned char * Palette;
} pngRawInfo;

int pngLoadRawF( FILE *fp,pngRawInfo *pinfo )
{
 unsigned char   header[8];
 png_structp     png;
 png_infop       info;
 png_infop       endinfo;
 png_bytep       data;
 png_bytep     * row_p;
 png_uint_32     width,height;
 int             depth,color;
 png_uint_32     i;

 if ( !pinfo ) return 1;

 fread( header,1,8,fp );
 if ( !png_check_sig( header,8 ) ) return 1;

 png=png_create_read_struct( PNG_LIBPNG_VER_STRING,NULL,NULL,NULL );
 info=png_create_info_struct( png );
 endinfo=png_create_info_struct( png );

 png_init_io( png,fp );
 png_set_sig_bytes( png,8 );
 png_read_info( png,info );
 png_get_IHDR( png,info,&width,&height,&depth,&color,NULL,NULL,NULL );

 pinfo->Width=width;
 pinfo->Height=height;
 pinfo->Depth=depth;

 data=( png_bytep ) malloc( png_get_rowbytes( png,info )*height );
 row_p=( png_bytep * ) malloc( sizeof( png_bytep )*height );
 for ( i=0; i < height; i++ ) row_p[i]=&data[png_get_rowbytes( png,info )*i];

 png_read_image( png,row_p );
 free( row_p );

 if ( color == PNG_COLOR_TYPE_PALETTE )
  {
   int cols;
   png_get_PLTE( png,info,( png_colorp * ) &pinfo->Palette,&cols );
  }
  else pinfo->Palette=NULL;

 if ( color&PNG_COLOR_MASK_ALPHA )
  {
   if ( color&PNG_COLOR_MASK_PALETTE || color == PNG_COLOR_TYPE_GRAY_ALPHA ) pinfo->Components=2;
     else pinfo->Components=4;
   pinfo->Alpha=8;
  }
  else
   {
    if ( color&PNG_COLOR_MASK_PALETTE || color == PNG_COLOR_TYPE_GRAY ) pinfo->Components=1;
      else pinfo->Components=3;
    pinfo->Alpha=0;
   }
 pinfo->Data=data;

 png_read_end( png,endinfo );
 png_destroy_read_struct( &png,&info,&endinfo );

 return 0;
}

int pngLoadRaw( const char * filename,pngRawInfo * pinfo )
{
 int result;
 FILE *fp=fopen( filename,"rb" );

 if ( !fp ) return 1;
 result=pngLoadRawF( fp,pinfo );
 if ( fclose( fp ) != 0 )
  {
   if ( result )
    {
     free( pinfo->Data );
     free( pinfo->Palette );
    }
   return 1;
  }
 return 0;
}

int pngRead( unsigned char * fname,txSample * bf )
{
 pngRawInfo raw;

 if ( pngLoadRaw( fname,&raw ) )
  {
   #ifdef DEBUG
    dbprintf( 4,"[png] file read error ( %s ).\n",fname );
   #endif
   return 1;
  }
 bf->Width=raw.Width;
 bf->Height=raw.Height;
 bf->BPP=( raw.Depth * raw.Components ) + raw.Alpha;
 bf->ImageSize=bf->Width * bf->Height * ( bf->BPP / 8 );
 if ( ( bf->Image=malloc( bf->ImageSize ) ) == NULL )
  {
   #ifdef DEBUG
    dbprintf( 4,"[png]  Not enough memory for image buffer.\n" );
   #endif
   return 2;
  }
 memcpy( bf->Image,raw.Data,bf->ImageSize );
 free( raw.Data );
 #ifdef DEBUG
  dbprintf( 4,"[png] filename: %s.\n",fname );
  dbprintf( 4,"[png]  size: %dx%d bits: %d\n",bf->Width,bf->Height,bf->BPP );
  dbprintf( 4,"[png]  imagesize: %lu\n",bf->ImageSize );
 #endif
 return 0;
}
