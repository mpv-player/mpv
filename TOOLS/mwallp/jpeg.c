// based on vd_ijpg.c by Pontscho

#include <stdio.h>
#include <stdlib.h>

#include <jpeglib.h>
#define UINT16 IJPG_UINT16
#define INT16 IJPG_INT16

#include <setjmp.h>

#include <inttypes.h>

#include "img_format.h"

#include "swscale.h"
#include "rgb2rgb.h"

static SwsContext *swsContext=NULL;

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

// decode a frame
int decode_jpeg(void* data,int len,char* dbuffer,int dwidth,int dheight, int dstride,int dbpp){
 int	      width,height,depth,i;
 int        row_stride;
 unsigned char*	img=NULL;
 int in_fmt=0;

 if ( len <= 0 ) return 0; // skipped frame

 cinfo.err=jpeg_std_error( &jerr.pub );
 jerr.pub.error_exit=my_error_exit;
 if( setjmp( jerr.setjmp_buffer ) )
  {
   return 0;
  }
  
 jpeg_create_decompress( &cinfo );
 jpeg_buf_src( &cinfo,data,len );
 jpeg_read_header( &cinfo,TRUE );
 width=cinfo.image_width;
 height=cinfo.image_height;
 jpeg_start_decompress( &cinfo );
 depth=cinfo.output_components * 8;

 switch( depth ) {
   case 8: in_fmt=IMGFMT_Y8;break;
   case 24: in_fmt=IMGFMT_RGB|24;break;
   default: return 0;
 }

 row_stride=cinfo.output_width * cinfo.output_components;
 img=malloc(row_stride * (height+1));

 for ( i=0;i < height;i++ ){
//    char* row=dbuffer+i*dstride;
    char* row=img+row_stride*i;
    jpeg_read_scanlines( &cinfo,(JSAMPLE**)&row,1 );
 }
 
 jpeg_finish_decompress(&cinfo);
 jpeg_destroy_decompress(&cinfo);

 swsContext= getSwsContextFromCmdLine(width,height, in_fmt, 
    				      dwidth,dheight, IMGFMT_BGR|dbpp);

 swsContext->swScale(swsContext,&img,&row_stride,0,height,&dbuffer, &dstride);
 
 freeSwsContext(swsContext);

 free(img);

 return 1;
}
