
#include <stdlib.h>

#include "config.h"
#include "bswap.h"
#include "postproc/rgb2rgb.h"
#include "mp_msg.h"
#include "png.h"

int    pngPointer;

void pngReadFN( png_structp pngstr,png_bytep buffer,png_size_t size )
{
 char * p = pngstr->io_ptr;
 memcpy( buffer,(char *)&p[pngPointer],size );
 pngPointer+=size;
}

void decode_mpng(
  unsigned char *encoded,
  int encoded_size,
  unsigned char *decoded,
  int width,
  int height,
  int bytes_per_pixel)
{
 png_structp     png;
 png_infop       info;
 png_infop       endinfo;
 png_bytep       data;
 png_bytep     * row_p;
 png_uint_32     png_width,png_height;
 char	       * palette = NULL;
 int             depth,color;
 png_uint_32     i;
 png=png_create_read_struct( PNG_LIBPNG_VER_STRING,NULL,NULL,NULL );
 info=png_create_info_struct( png );
 endinfo=png_create_info_struct( png );

 pngPointer=8;
 png_set_read_fn( png,encoded,pngReadFN );
 png_set_sig_bytes( png,8 );
 png_read_info( png,info );
 png_get_IHDR( png,info,&png_width,&png_height,&depth,&color,NULL,NULL,NULL );

 png_set_bgr( png );

#if 0
 switch( info->color_type )
  {
   case PNG_COLOR_TYPE_GRAY_ALPHA: printf( "[png] used GrayA -> stripping alpha channel\n" ); break;
   case PNG_COLOR_TYPE_GRAY:       printf( "[png] used Gray -> rgb\n" ); break;
   case PNG_COLOR_TYPE_PALETTE:    printf( "[png] used palette -> rgb\n" ); break;
   case PNG_COLOR_TYPE_RGB_ALPHA:  printf( "[png] used RGBA -> stripping alpha channel\n" ); break;
   case PNG_COLOR_TYPE_RGB:        printf( "[png] read rgb datas.\n" ); break;
  }
#endif

 if ( info->color_type == PNG_COLOR_TYPE_RGB ) data=decoded;
  else data=(png_bytep)malloc( png_get_rowbytes( png,info ) * height );

 row_p=(png_bytep*)malloc( sizeof( png_bytep ) * png_height );
 for ( i=0; i < png_height; i++ ) row_p[i]=&data[png_get_rowbytes( png,info ) * i];
 png_read_image( png,row_p );
 free( row_p );
						     
 switch( info->color_type )
  {
   case PNG_COLOR_TYPE_GRAY_ALPHA:
          mp_msg( MSGT_DECVIDEO,MSGL_INFO,"Sorry gray scaled png with alpha channel not supported at moment.\n" );
          free( data );
	  break;
   case PNG_COLOR_TYPE_GRAY:
          palette=malloc( 1024 );
          for ( i=0;i < 256;i++ ) palette[(i*4)]=palette[(i*4)+1]=palette[(i*4)+2]=(char)i;
	  palette8torgb24( data,decoded,png_width * png_height,palette );
          free( data );
	  break;
   case PNG_COLOR_TYPE_PALETTE:
          { 
           int    cols;
	   unsigned char * pal;
           png_get_PLTE( png,info,(png_colorp*)&pal,&cols ); 
           palette=calloc( 1,1024 );
	   for ( i=0;i < cols;i++ )
	    {
	     palette[(i*4)  ]=pal[(i*3)+2];
	     palette[(i*4)+1]=pal[(i*3)+1];
	     palette[(i*4)+2]=pal[(i*3)  ];
	    }
	  }
	  palette8torgb24( data,decoded,png_width * png_height,palette );
          free( data );
	  break;
   case PNG_COLOR_TYPE_RGB_ALPHA:
          rgb32to24( data,decoded,png_width * png_height * 4 );
          free( data );
	  break;
  }

 if ( palette ) free( palette );

 png_read_end( png,endinfo );
 png_destroy_read_struct( &png,&info,&endinfo );
}
	    
