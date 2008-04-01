
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "mp_msg.h"

#include <jpeglib.h>
#define UINT16 IJPG_UINT16
#define INT16 IJPG_INT16

#include <setjmp.h>

#include "mpbswap.h"

#include "vd_internal.h"

static vd_info_t info = {
	"JPEG Images decoder",
	"ijpg",
	"Pontscho",
	"based on vd_mpng.c",
	"uses Independent JPEG Group's jpeglib"
};

LIBVD_EXTERN(ijpg)

static int last_w=-1;
static int last_h=-1;
static int last_depth=-1;

// to set/get/query special features/parameters
static int control(sh_video_t *sh,int cmd,void* arg,...){
    if (cmd == VDCTRL_QUERY_FORMAT) {
        int format = *(int *)arg;
        if ((last_depth == 24 && format == IMGFMT_RGB24) ||
            (last_depth == 8  && format == IMGFMT_Y8   ))
            return CONTROL_TRUE;
        return CONTROL_FALSE;
    }
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

// decode a frame
static mp_image_t* decode(sh_video_t *sh,void* data,int len,int flags){
 struct jpeg_decompress_struct cinfo;
 struct my_error_mgr jerr;
 mp_image_t * mpi = NULL;
 int	      width,height,depth,i;

 if ( len <= 0 ) return NULL; // skipped frame

 memset(&cinfo, 0, sizeof(cinfo));
 memset(&jerr, 0, sizeof(jerr));
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
 sh->disp_w=width=cinfo.image_width;
 sh->disp_h=height=cinfo.image_height;
 jpeg_start_decompress( &cinfo );
 depth=cinfo.output_components * 8;

 switch( depth ) {
   case 8:
   case 24: break;
   default: mp_msg( MSGT_DECVIDEO,MSGL_ERR,"Sorry, unsupported JPEG colorspace: %d.\n",depth ); return NULL;
 }

 if ( last_w!=width || last_h!=height || last_depth != depth )
  {
   last_depth = depth;
   if(!mpcodecs_config_vo( sh,width,height, depth == 8 ? IMGFMT_Y8 : IMGFMT_RGB24 )) return NULL;
   last_w=width; last_h=height;
  }

 mpi=mpcodecs_get_image( sh,MP_IMGTYPE_TEMP,MP_IMGFLAG_ACCEPT_STRIDE,width,height );
 if ( !mpi ) return NULL;

 for ( i=0;i < height;i++ )
  {
   unsigned char * drow = mpi->planes[0] + mpi->stride[0] * i;
   jpeg_read_scanlines( &cinfo,(JSAMPLE**)&drow,1 );
  }

 jpeg_finish_decompress(&cinfo);
 jpeg_destroy_decompress(&cinfo);

 return mpi;
}
