#define DISP

/* 
 * vo_png.c, Portable Network Graphics Renderer for Mplayer
 *
 * Copyright 2001 by Felix Buenemann <atmosfear@users.sourceforge.net>
 *
 * Uses libpng (which uses zlib), so see according licenses.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <png.h>
//#include "/usr/include/png.h"


#include "config.h"
#include "video_out.h"
#include "video_out_internal.h"

#include "../postproc/rgb2rgb.h"

LIBVO_EXTERN (png)

static vo_info_t vo_info = 
{
	"PNG file",
	"png",
	"Felix Buenemann <atmosfear@users.sourceforge.net>",
	""
};

#define RGB 0
#define BGR 1

extern int verbose;
int z_compression = Z_NO_COMPRESSION;
static int image_width;
static int image_height;
static int image_format;
static uint8_t *image_data=NULL;

static int bpp = 24;
static int cspace = RGB;
static int framenum = 0;

struct pngdata {
	FILE * fp;
	png_structp png_ptr;
	png_infop info_ptr;
	enum {OK,ERROR} status;  
};
	
static void draw_alpha(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride){
    vo_draw_alpha_rgb24(w, h, src, srca, stride, image_data + 3 * (y0 * image_width + x0), 3 * image_width);
}

static uint32_t
config(uint32_t width, uint32_t height, uint32_t d_width, uint32_t d_height, uint32_t fullscreen, char *title, uint32_t format,const vo_tune_info_t *info)
{
    image_height = height;
    image_width = width;
    image_format = format;
    //printf("Verbose level is %i\n", verbose);
    
    switch(format) {
	case IMGFMT_BGR24:
	     bpp = 24;
	     cspace = BGR;
	break;     
	case IMGFMT_RGB24:
	     bpp = 24;
	     cspace = RGB;
	break;     
	case IMGFMT_IYUV:
	case IMGFMT_I420:
	case IMGFMT_YV12:
	     bpp = 24;
	     cspace = BGR;
	     yuv2rgb_init(bpp,MODE_RGB);
	     image_data = malloc(image_width*image_height*3);
	break;
	default:
	     return 1;     
    }		
    
    if((z_compression >= 0) && (z_compression <= 9)) {
	    if(z_compression == 0) {
		    printf("PNG Warning: compression level set to 0, compression disabled!\n");
		    printf("PNG Info: Use the -z <n> switch to set compression level from 0 to 9.\n");
		    printf("PNG Info: (0 = no compression, 1 = fastest, lowest - 9 best, slowest compression)\n");
	    }	    
    }
    else {	    	    
	    printf("PNG Warning: compression level out of range setting to 1!\n");
	    printf("PNG Info: Use the -z <n> switch to set compression level from 0 to 9.\n");
	    printf("PNG Info: (0 = no compression, 1 = fastest, lowest - 9 best, slowest compression)\n");
	    z_compression = Z_BEST_SPEED;
    }
    
    if(verbose)	printf("PNG Compression level %i\n", z_compression);   
	  	
    return 0;
}

static const vo_info_t*
get_info(void)
{
    return &vo_info;
}


struct pngdata create_png (char * fname)
{
    struct pngdata png;
    
    /*png_structp png_ptr = png_create_write_struct
       (PNG_LIBPNG_VER_STRING, (png_voidp)user_error_ptr,
        user_error_fn, user_warning_fn);*/
    //png_byte *row_pointers[image_height];
    png.png_ptr = png_create_write_struct
       (PNG_LIBPNG_VER_STRING, NULL,
        NULL, NULL);
    png.info_ptr = png_create_info_struct(png.png_ptr);
   
    if (!png.png_ptr) {
       if(verbose > 1) printf("PNG Failed to init png pointer\n");
       png.status = ERROR;
       return png;
    }   
    
    if (!png.info_ptr) {
       if(verbose > 1) printf("PNG Failed to init png infopointer\n");
       png_destroy_write_struct(&png.png_ptr,
         (png_infopp)NULL);
       png.status = ERROR;
       return png;
    }
    
    if (setjmp(png.png_ptr->jmpbuf)) {
	if(verbose > 1) printf("PNG Internal error!\n");    
        png_destroy_write_struct(&png.png_ptr, &png.info_ptr);
        fclose(png.fp);
        png.status = ERROR;
        return png;
    }
    
    png.fp = fopen (fname, "wb");
    if (png.fp == NULL) {
	printf("\nPNG Error opening %s for writing!\n", strerror(errno));
       	png.status = ERROR;
       	return png;
    }	    
    
    if(verbose > 1) printf("PNG Init IO\n");
    png_init_io(png.png_ptr, png.fp);

    /* set the zlib compression level */
    png_set_compression_level(png.png_ptr, z_compression);
		    
    
    /*png_set_IHDR(png_ptr, info_ptr, width, height,
       bit_depth, color_type, interlace_type,
       compression_type, filter_type)*/
    png_set_IHDR(png.png_ptr, png.info_ptr, image_width, image_height,
       8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
       PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    
    if(verbose > 1) printf("PNG Write Info\n");
    png_write_info(png.png_ptr, png.info_ptr);
    
    if(cspace) {
    	if(verbose > 1) printf("PNG Set BGR Conversion\n");
    	png_set_bgr(png.png_ptr);
    }	

    png.status = OK;
    return png;
    
}    
       
static uint8_t destroy_png(struct pngdata png) {
	    
    if(verbose > 1) printf("PNG Write End\n");
    png_write_end(png.png_ptr, png.info_ptr);

    if(verbose > 1) printf("PNG Destroy Write Struct\n");
    png_destroy_write_struct(&png.png_ptr, &png.info_ptr);
    
    fclose (png.fp);

    return 0;
}

static uint32_t draw_frame(uint8_t * src[])
{
    char buf[100];
    int k, bppmul = bpp/8;
    struct pngdata png;
    png_byte *row_pointers[image_height];
    
    snprintf (buf, 100, "%08d.png", ++framenum);

    png = create_png(buf);

    if(png.status){
	    printf("PNG Error in create_png\n");
	    return 1;
    }	     

    if(verbose > 1) printf("PNG Creating Row Pointers\n");
    for ( k = 0; k < image_height; k++ ) row_pointers[k] = &src[0][image_width*k*bppmul];
    
    //png_write_flush(png.png_ptr);
    //png_set_flush(png.png_ptr, nrows);

    if(verbose > 1) printf("PNG Writing Image Data\n");
    png_write_image(png.png_ptr, row_pointers);

    return destroy_png(png);

}

static void draw_osd(void)
{
    if(image_data) vo_draw_text(image_width, image_height, draw_alpha);
}

static void flip_page (void)
{
    char buf[100];
    int k, bppmul = bpp/8;
    struct pngdata png;
    png_byte *row_pointers[image_height];
  
  if((image_format == IMGFMT_YV12) || (image_format == IMGFMT_IYUV) || (image_format == IMGFMT_I420)) {

    snprintf (buf, 100, "%08d.png", ++framenum);

    png = create_png(buf);

    if(png.status){
	    printf("PNG Error in create_png\n");
    }	     

    if(verbose > 1) printf("PNG Creating Row Pointers\n");
    for ( k = 0; k < image_height; k++ ) row_pointers[k] = &image_data[image_width*k*bppmul];
    
    //png_write_flush(png.png_ptr);
    //png_set_flush(png.png_ptr, nrows);

    if(verbose > 1) printf("PNG Writing Image Data\n");
    png_write_image(png.png_ptr, row_pointers);

    destroy_png(png);
  }
}

static uint32_t draw_slice( uint8_t *src[],int stride[],int w,int h,int x,int y )
{
  uint8_t *dst = image_data + (image_width * y + x) * (bpp/8);
  yuv2rgb(dst,src[0],src[1],src[2],w,h,image_width*(bpp/8),stride[0],stride[1]);
  return 0;
}

static uint32_t
query_format(uint32_t format)
{
    switch(format){
    case IMGFMT_IYUV:
    case IMGFMT_I420:
    case IMGFMT_YV12:
	return VFCAP_CSP_SUPPORTED|VFCAP_OSD;
    case IMGFMT_RGB|24:
    case IMGFMT_BGR|24:
        return VFCAP_CSP_SUPPORTED|VFCAP_CSP_SUPPORTED_BY_HW;
    }
    return 0;
}

static void
uninit(void)
{
	if(image_data){ free(image_data);image_data=NULL;}
}


static void check_events(void)
{
}

static uint32_t preinit(const char *arg)
{
    if(arg) 
    {
	printf("PNG Unknown subdevice: %s\n",arg);
	return ENOSYS;
    }
    return 0;
}

static uint32_t control(uint32_t request, void *data, ...)
{
  switch (request) {
  case VOCTRL_QUERY_FORMAT:
    return query_format(*((uint32_t*)data));
  }
  return VO_NOTIMPL;
}
