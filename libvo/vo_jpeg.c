#define DISP

/* 
 * vo_jpeg.c, JPEG Renderer for Mplayer
 *
 * Copyright 2001 by Pontscho (pontscho@makacs.poliod.hu)
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

#include "../postproc/swscale.h"
#include "../postproc/rgb2rgb.h"

LIBVO_EXTERN (jpeg)

static vo_info_t vo_info=
{
	"JPEG file",
	"jpeg",
	"Zoltan Ponekker (pontscho@makacs.poliod.hu)",
	""
};

#define RGB 0
#define BGR 1

extern int verbose;
static int image_width;
static int image_height;
static int image_format;
static uint8_t *image_data=NULL;
static unsigned int scale_srcW=0, scale_srcH=0;

int jpeg_baseline = 1;
int jpeg_progressive_mode = 0;
int jpeg_optimize = 100;
int jpeg_smooth = 0;
int jpeg_quality = 75;
char * jpeg_outdir;

#define bpp 24

static int cspace=RGB;
static int framenum=0;

static void draw_alpha(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride)
{
 vo_draw_alpha_rgb24(w, h, src, srca, stride, image_data + 3 * (y0 * image_width + x0), 3 * image_width);
}

static uint32_t config(uint32_t width, uint32_t height, uint32_t d_width, uint32_t d_height, uint32_t fullscreen, char *title, uint32_t format,const vo_tune_info_t *info)
{
 if ( fullscreen&0x04 && ( width != d_width || height != d_height )&&( ( format == IMGFMT_YV12 ) ) )
  {
   // software scaling 
   image_width=(d_width + 7) & ~7;
   image_height=d_height;
   scale_srcW=width;
   scale_srcH=height;
   SwScale_Init();
  }
   else 
    {
     image_height=height;
     image_width=width;
    }
    
 image_format=format;
 switch(format) 
  {
   case IMGFMT_BGR32:
        cspace=BGR;
        image_data=malloc( image_width * image_height * 3 );
        break;
   case IMGFMT_BGR24:
        cspace=BGR;
        image_data=malloc( image_width * image_height * 3 );
        break;     
   case IMGFMT_RGB24:
        cspace=RGB;
	break;     
   case IMGFMT_IYUV:
   case IMGFMT_I420:
   case IMGFMT_YV12:
        cspace=BGR;
        yuv2rgb_init( bpp,MODE_BGR );
        image_data=malloc( image_width * image_height * 3 );
	break;
   default:
        return 1;     
  }		
    
 return 0;
}

static const vo_info_t*
get_info(void)
{
    return &vo_info;
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

 jpeg_set_quality( &cinfo,jpeg_quality,jpeg_baseline );

 cinfo.image_width=image_width;
 cinfo.image_height=image_height;
 cinfo.input_components=bpp / 8;
 cinfo.in_color_space=JCS_RGB;
 cinfo.optimize_coding=jpeg_optimize;
 cinfo.smoothing_factor=jpeg_smooth;
 
 jpeg_set_defaults( &cinfo );
 if ( jpeg_progressive_mode ) jpeg_simple_progression( &cinfo );                                  
 jpeg_start_compress( &cinfo,TRUE );

 row_stride = image_width * ( bpp / 8 );
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
    
 snprintf (buf, 256, "%s/%08d.jpg", jpeg_outdir, ++framenum);

 if ( image_format == IMGFMT_BGR32 )
  {
   rgb32to24( src[0],image_data,image_width * image_height * 4 );
   rgb24tobgr24( image_data,image_data,image_width * image_height * 3 );
   src[0]=image_data;
  }
 if ( image_format == IMGFMT_BGR24 )
  {
   rgb24tobgr24( src[0],image_data,image_width * image_height * 3 );
   src[0]=image_data;
  }
 return jpeg_write( buf,src[0] );
}

static void draw_osd(void)
{
 vo_draw_text(image_width, image_height, draw_alpha);
}

static void flip_page (void)
{
 char buf[256];
  
 if((image_format == IMGFMT_YV12) || (image_format == IMGFMT_IYUV) || (image_format == IMGFMT_I420))
  {
   snprintf (buf, 256, "%s/%08d.jpg", jpeg_outdir, ++framenum);
   jpeg_write( buf,image_data );
  }
}

static uint32_t draw_slice( uint8_t *src[],int stride[],int w,int h,int x,int y )
{
  // hack: swap planes for I420 ;) -- alex 
 if ((image_format == IMGFMT_IYUV) || (image_format == IMGFMT_I420))
  {
   uint8_t *src_i420[3];
    
   src_i420[0]=src[0];
   src_i420[1]=src[2];
   src_i420[2]=src[1];
   src=src_i420;
  }

 if (scale_srcW) 
  {
   uint8_t *dst[3]={image_data, NULL, NULL};
   SwScale_YV12slice(src,stride,y,h,
		      dst, image_width*((bpp+7)/8), bpp,
 		      scale_srcW, scale_srcH, image_width, image_height);
   }
    else 
     {
      uint8_t *dst=image_data + (image_width * y + x) * (bpp/8);
      yuv2rgb(dst,src[0],src[1],src[2],w,h,image_width*(bpp/8),stride[0],stride[1]);
     }
  return 0;
}

static uint32_t query_format(uint32_t format)
{
 switch( format )
  {
   case IMGFMT_IYUV:
   case IMGFMT_I420:
   case IMGFMT_YV12:
   case IMGFMT_RGB|24:
   case IMGFMT_BGR|24:
   case IMGFMT_BGR|32:
        return 1;
  }
 return 0;
}

static void uninit(void)
{
 if ( image_data ) 
  { 
   free( image_data );
   image_data=NULL;
  }
}

static void check_events(void)
{
}

static uint32_t preinit(const char *arg)
{
 if(arg) 
  {
   printf("JPEG Unknown subdevice: %s\n",arg);
   return ENOSYS;
  }
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
