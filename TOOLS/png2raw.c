#define DEBUG

#include <stdlib.h>

//#include "png.h"
#include <png.h>

typedef struct _txSample
{
 unsigned int  Width;
 unsigned int  Height;
 unsigned int  BPP;
 unsigned long ImageSize;
 char *        Image;
} txSample;

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

 if ( pinfo == NULL ) return 1;

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

int pngLoadRaw( const char *filename,pngRawInfo *pinfo )
{
 int result;
 FILE *fp=fopen( filename,"rb" );

 if ( fp == NULL ) return 0;
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
    fprintf( stderr,"[png] file read error ( %s ).\n",fname );
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
    fprintf( stderr,"[png]  Not enough memory for image buffer.\n" );
   #endif
   return 2;
  }
 memcpy( bf->Image,raw.Data,bf->ImageSize );
 free( raw.Data );
 #ifdef DEBUG
  fprintf( stderr,"[png] filename: %s.\n",fname );
  fprintf( stderr,"[png]  size: %dx%d bits: %d\n",bf->Width,bf->Height,bf->BPP );
  fprintf( stderr,"[png]  imagesize: %lu\n",bf->ImageSize );
  fprintf( stderr,"Palette: %s\n",raw.Palette?"yes":"no");
 #endif
 return 0;
}

static char fname[256];

static unsigned char rawhead[32]={'m','h','w','a','n','h',0,4,
                                   0,0,0,0,1,0,0,0,
                                   0,0,0,0,0,0,0,0,
                                   0,0,0,0,0,0,0,0};
static unsigned char rawpal[3*256];

int main(int argc,char* argv[]){
  txSample ize;
  FILE *f;
  int i;
  for(i=0;i<256;i++) rawpal[i*3]=rawpal[i*3+1]=rawpal[i*3+2]=i;

if(argc<2) {printf("Usage: png2raw file1 [file2...]\n");exit(1);}
while(argc>1){
  ++argv;--argc;
  printf("Converting %s...\n",argv[0]);
  if(pngRead(argv[0],&ize)) continue;
  if(ize.BPP!=8){ printf("Invalid BPP: %d\n",ize.BPP);continue;}
  snprintf(fname,256,"%s.raw",argv[0]);
  f=fopen(fname,"wb");
  rawhead[8]=ize.Width>>8;
  rawhead[9]=ize.Width&255;
  rawhead[10]=ize.Height>>8;
  rawhead[11]=ize.Height&255;
  fwrite(rawhead,32,1,f);
  fwrite(rawpal,3*256,1,f);
  fwrite(ize.Image,ize.ImageSize,1,f);
  fclose(f);
  
}



}



