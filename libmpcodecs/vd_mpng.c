#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "mp_msg.h"

#ifdef HAVE_PNG

#include <png.h>

#include "bswap.h"
#include "postproc/rgb2rgb.h"
#include "libvo/fastmemcpy.h"

#include "vd_internal.h"

static vd_info_t info = {
	"PNG Images decoder",
	"mpng",
	VFM_MPNG,
	"A'rpi",
	".so, based on mpng.c",
	"uses libpng, 8bpp modes not supported yet"
};

LIBVD_EXTERN(mpng)

static unsigned int out_fmt=0;

static int last_w=-1;
static int last_h=-1;
static int last_c=-1;

// to set/get/query special features/parameters
static int control(sh_video_t *sh,int cmd,void* arg,...){
    return CONTROL_UNKNOWN;
}

// init driver
static int init(sh_video_t *sh){
    last_w=-1;
    return 1;
}

// uninit driver
static void uninit(sh_video_t *sh){
}

//mp_image_t* mpcodecs_get_image(sh_video_t *sh, int mp_imgtype, int mp_imgflag, int w, int h);

static int    pngPointer;
static int    pngLength;

static void pngReadFN( png_structp pngstr,png_bytep buffer,png_size_t size )
{
 char * p = pngstr->io_ptr;
 if(size>pngLength-pngPointer && pngLength>=pngPointer) size=pngLength-pngPointer;
 memcpy( buffer,(char *)&p[pngPointer],size );
 pngPointer+=size;
}

// decode a frame
static mp_image_t* decode(sh_video_t *sh,void* data,int len,int flags){
    png_structp     png;
    png_infop       info;
    png_infop       endinfo;
//    png_bytep       data;
    png_bytep     * row_p;
    png_uint_32     png_width=0,png_height=0;
    char	  * palette = NULL;
    int             depth,color;
    png_uint_32     i;
    mp_image_t* mpi;

    if(len<=0) return NULL; // skipped frame
    
 png=png_create_read_struct( PNG_LIBPNG_VER_STRING,NULL,NULL,NULL );
 info=png_create_info_struct( png );
 endinfo=png_create_info_struct( png );

 pngPointer=8;
 pngLength=len;
 png_set_read_fn( png,data,pngReadFN );
 png_set_sig_bytes( png,8 );
 png_read_info( png,info );
 png_get_IHDR( png,info,&png_width,&png_height,&depth,&color,NULL,NULL,NULL );

 png_set_bgr( png );

 switch( info->color_type ) {
   case PNG_COLOR_TYPE_GRAY_ALPHA:
      mp_msg( MSGT_DECVIDEO,MSGL_INFO,"Sorry gray scaled png with alpha channel not supported at moment.\n" );
      break;
   case PNG_COLOR_TYPE_GRAY:
   case PNG_COLOR_TYPE_PALETTE:
      out_fmt=IMGFMT_BGR8;
      break;
   case PNG_COLOR_TYPE_RGB_ALPHA:
      out_fmt=IMGFMT_BGR32;
      break;
   case PNG_COLOR_TYPE_RGB:
      out_fmt=IMGFMT_BGR24;
      break;
   default:
      mp_msg( MSGT_DECVIDEO,MSGL_INFO,"Sorry, unsupported PNG colorspace: %d.\n" ,info->color_type);
 }

 // (re)init libvo if image parameters changed (width/height/colorspace)
 if(last_w!=png_width || last_h!=png_height || last_c!=out_fmt){
    last_w=png_width; last_h=png_height; last_c=out_fmt;
    if(!out_fmt) return NULL;
    if(!mpcodecs_config_vo(sh,png_width,png_height,out_fmt)) return NULL;
 }

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

    mpi=mpcodecs_get_image(sh, MP_IMGTYPE_TEMP, MP_IMGFLAG_ACCEPT_STRIDE, 
	png_width,png_height);
    if(!mpi) return NULL;

// Let's DECODE!
 row_p=(png_bytep*)malloc( sizeof( png_bytep ) * png_height );
//png_get_rowbytes( png,info ) 
 for ( i=0; i < png_height; i++ ) row_p[i]=mpi->planes[0] + mpi->stride[0]*i;
 png_read_image( png,row_p );
 free( row_p );
 
 //png_get_PLTE( png,info,(png_colorp*)&pal,&cols );
 
 png_read_end( png,endinfo );
 png_destroy_read_struct( &png,&info,&endinfo );

    return mpi;
}

#endif
