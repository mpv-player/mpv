
#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "mp_msg.h"

#include <jconfig.h>
#include <jmorecfg.h>
#include <jpeglib.h>
#include <jpegint.h>
#include <jerror.h>

#include <setjmp.h>

#include "bswap.h"
#include "postproc/rgb2rgb.h"
#include "libvo/fastmemcpy.h"

#include "vd_internal.h"

static vd_info_t info = {
	"JPEG Images decoder",
	"ijpg",
	VFM_IJPG,
	"Pontscho",
	"based on vd_mpng.c",
	"uses Indipended JPEG Group's jpeglib"
};

LIBVD_EXTERN(ijpg)

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

typedef struct
{
 struct          jpeg_source_mgr pub;
 unsigned char * inbuf;
 int             bufsize;
} my_source_mgr;

typedef my_source_mgr * my_src_ptr;

METHODDEF(void) init_source (j_decompress_ptr cinfo)
{
}

METHODDEF(boolean) fill_input_buffer (j_decompress_ptr cinfo)
{
 my_src_ptr src = (my_src_ptr) cinfo->src;                                                         
 size_t nbytes;                                                                                    
 src->pub.next_input_byte = src->inbuf;
 src->pub.bytes_in_buffer = src->bufsize;
 return TRUE;
}
                                                                                                        
METHODDEF(void) skip_input_data (j_decompress_ptr cinfo, long num_bytes)                           
{                                                                                                  
 my_src_ptr src = (my_src_ptr) cinfo->src;                                                        

 if (num_bytes > 0)
  {
   while (num_bytes > (long) src->pub.bytes_in_buffer)
    {
     num_bytes -= (long) src->pub.bytes_in_buffer;
     (void) fill_input_buffer(cinfo);
    }
   src->pub.next_input_byte += (size_t) num_bytes;
   src->pub.bytes_in_buffer -= (size_t) num_bytes;
  }
}

METHODDEF(void) term_source (j_decompress_ptr cinfo) { }                                           
						   
GLOBAL(void) jpeg_buf_src ( j_decompress_ptr cinfo, char * inbuf,int bufsize )                     
{
 my_src_ptr src;
 if (cinfo->src == NULL) cinfo->src=malloc( sizeof( my_source_mgr ) );
 src = (my_src_ptr) cinfo->src;
 src->pub.init_source = init_source;
 src->pub.fill_input_buffer = fill_input_buffer;
 src->pub.skip_input_data = skip_input_data;
 src->pub.resync_to_restart = jpeg_resync_to_restart;
 src->pub.term_source = term_source;
 src->inbuf = inbuf;
 src->bufsize=bufsize;
 src->pub.bytes_in_buffer = 0;
 src->pub.next_input_byte = NULL;
}

struct my_error_mgr
{
 struct jpeg_error_mgr pub;
 jmp_buf setjmp_buffer;
};

typedef struct my_error_mgr * my_error_ptr;

METHODDEF(void) my_error_exit (j_common_ptr cinfo)
{
 my_error_ptr myerr=(my_error_ptr) cinfo->err;
 (*cinfo->err->output_message) (cinfo);
 longjmp(myerr->setjmp_buffer, 1);
}

static struct     jpeg_decompress_struct cinfo;
static struct     my_error_mgr jerr;
static int        row_stride;
static int 	  count;
				  
// decode a frame
static mp_image_t* decode(sh_video_t *sh,void* data,int len,int flags){
 mp_image_t * mpi = NULL;
 int	      width,height,depth,i,j;

 if ( len <= 0 ) return NULL; // skipped frame

 cinfo.err=jpeg_std_error( &jerr.pub );
 jerr.pub.error_exit=my_error_exit;
 if( setjmp( jerr.setjmp_buffer ) )
  {
   mp_msg( MSGT_DECVIDEO,MSGL_ERR,"[ijpg] setjmp error ...\n" );
   return NULL;
  }
  
 jpeg_create_decompress( &cinfo );
 jpeg_buf_src( &cinfo,data,len );
 jpeg_read_header( &cinfo,TRUE );
 width=cinfo.image_width;
 height=cinfo.image_height;
 jpeg_start_decompress( &cinfo );
 depth=cinfo.output_components * 8;

 switch( depth ) {
   case 8:  out_fmt=IMGFMT_BGR8;  break;
   case 24: out_fmt=IMGFMT_BGR24; break;
   default: mp_msg( MSGT_DECVIDEO,MSGL_ERR,"Sorry, unsupported JPEG colorspace: %d.\n",depth ); return NULL;
 }

 if ( last_w!=width || last_h!=height || last_c!=out_fmt )
  {
   last_w=width; last_h=height; last_c=out_fmt;
   if ( !out_fmt ) return NULL;
   mpcodecs_config_vo( sh,width,height,out_fmt );
  }

 mpi=mpcodecs_get_image( sh,MP_IMGTYPE_TEMP,MP_IMGFLAG_ACCEPT_STRIDE,width,height );
 if ( !mpi ) return NULL;

 row_stride=cinfo.output_width * cinfo.output_components;

 for ( i=0;i < height;i++ )
  {
   char * row = mpi->planes[0] + mpi->stride[0] * i;
   jpeg_read_scanlines( &cinfo,&row,1 );
#warning workaround for rgb2bgr
   if ( depth == 24 )
    for ( j=0;j < width * 3;j+=3 )
     {
      char c;
      c=row[j];
      row[j]=row[j+2];
      row[j+2]=c;
     }
  }
  
 jpeg_finish_decompress(&cinfo);                                                                   
 jpeg_destroy_decompress(&cinfo);                                                                  
	    
 return mpi;
}
