/* 
 * vo_jpeg.c, JPEG Renderer for Mplayer
 *
 * Copyright 2002 by Pontscho (pontscho@makacs.poliod.hu)
 * 25/04/2003: Spring cleanup -- alex
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <jpeglib.h>

#include "config.h"
#include "video_out.h"
#include "video_out_internal.h"

static vo_info_t info=
{
	"JPEG file",
	"jpeg",
	"Zoltan Ponekker (pontscho@makacs.poliod.hu)",
	""
};

LIBVO_EXTERN (jpeg)

static int image_width;
static int image_height;

int jpeg_baseline = 1;
int jpeg_progressive_mode = 0;
int jpeg_optimize = 100;
int jpeg_smooth = 0;
int jpeg_quality = 75;
char * jpeg_outdir = ".";

static int framenum=0;

static uint32_t config(uint32_t width, uint32_t height, uint32_t d_width, uint32_t d_height, uint32_t fullscreen, char *title, uint32_t format)
{
     image_height=height;
     image_width=width;
    
 return 0;
}

static uint32_t jpeg_write( uint8_t * name,uint8_t * buffer )
{
 FILE * o;
 struct jpeg_compress_struct cinfo;
 struct jpeg_error_mgr jerr;
 JSAMPROW row_pointer[1];
 int row_stride;

 if ( !buffer ) return 1; 
 if ( (o=fopen( name,"wb" )) == NULL ) return 1;
 
 cinfo.err=jpeg_std_error(&jerr);
 jpeg_create_compress(&cinfo);
 jpeg_stdio_dest( &cinfo,o );

 cinfo.image_width=image_width;
 cinfo.image_height=image_height;
 cinfo.input_components=3;
 cinfo.in_color_space=JCS_RGB;
 
 jpeg_set_defaults( &cinfo );
 jpeg_set_quality( &cinfo,jpeg_quality,jpeg_baseline );
 cinfo.optimize_coding=jpeg_optimize;
 cinfo.smoothing_factor=jpeg_smooth;

 if ( jpeg_progressive_mode ) jpeg_simple_progression( &cinfo );                                  
 jpeg_start_compress( &cinfo,TRUE );

 row_stride = image_width * 3;
 while ( cinfo.next_scanline < cinfo.image_height )                                                 
  {                                                                                               
   row_pointer[0]=&buffer[ cinfo.next_scanline * row_stride ];
   (void)jpeg_write_scanlines( &cinfo,row_pointer,1 );
  }                                                                                               

 jpeg_finish_compress( &cinfo );
 fclose( o );
 jpeg_destroy_compress( &cinfo );
 
 return 0;
}

static uint32_t draw_frame(uint8_t * src[])
{
 char buf[256];
 uint8_t *dst= src[0];
    
 snprintf (buf, 256, "%s/%08d.jpg", jpeg_outdir, ++framenum);

 return jpeg_write( buf,src[0] );
}

static void draw_osd(void)
{
}

static void flip_page (void)
{
}

static uint32_t draw_slice( uint8_t *src[],int stride[],int w,int h,int x,int y )
{
  return 0;
}

static uint32_t query_format(uint32_t format)
{
    if (format == IMGFMT_RGB24)
	    return VFCAP_CSP_SUPPORTED|VFCAP_CSP_SUPPORTED_BY_HW;

    return 0;
}

static void uninit(void)
{
}

static void check_events(void)
{
}

static uint32_t preinit(const char *arg)
{
 return 0;
}

static uint32_t control(uint32_t request, void *data, ...)
{
 switch (request) 
  {
   case VOCTRL_QUERY_FORMAT:
        return query_format(*((uint32_t*)data));
  }
 return VO_NOTIMPL;
}
