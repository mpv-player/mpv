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

#include "config.h"
#include "video_out.h"
#include "video_out_internal.h"

static vo_info_t info = 
{
	"PNG file",
	"png",
	"Felix Buenemann <atmosfear@users.sourceforge.net>",
	""
};

LIBVO_EXTERN (png)

extern int verbose;
int z_compression = Z_NO_COMPRESSION;
static int framenum = 0;

struct pngdata {
	FILE * fp;
	png_structp png_ptr;
	png_infop info_ptr;
	enum {OK,ERROR} status;  
};

static uint32_t
config(uint32_t width, uint32_t height, uint32_t d_width, uint32_t d_height, uint32_t fullscreen, char *title, uint32_t format)
{
    
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


struct pngdata create_png (char * fname, int image_width, int image_height, int swapped)
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
    
    if(swapped) {
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

static uint32_t draw_image(mp_image_t* mpi){
    char buf[100];
    int k;
    struct pngdata png;
    png_byte *row_pointers[mpi->h];

    // if -dr or -slices then do nothing:
    if(mpi->flags&(MP_IMGFLAG_DIRECT|MP_IMGFLAG_DRAW_CALLBACK)) return VO_TRUE;
    
    snprintf (buf, 100, "%08d.png", ++framenum);

    png = create_png(buf, mpi->w, mpi->h, mpi->flags&MP_IMGFLAG_SWAPPED);

    if(png.status){
	    printf("PNG Error in create_png\n");
	    return 1;
    }	     

    if(verbose > 1) printf("PNG Creating Row Pointers\n");
    for ( k = 0; k < mpi->h; k++ )
	row_pointers[k] = mpi->planes[0]+mpi->stride[0]*k;

    //png_write_flush(png.png_ptr);
    //png_set_flush(png.png_ptr, nrows);

    if(verbose > 1) printf("PNG Writing Image Data\n");
    png_write_image(png.png_ptr, row_pointers);

    destroy_png(png);

    return VO_TRUE;
}

static void draw_osd(void){}

static void flip_page (void){}

static uint32_t draw_frame(uint8_t * src[])
{
    return -1;
}

static uint32_t draw_slice( uint8_t *src[],int stride[],int w,int h,int x,int y )
{
    return -1;
}

static uint32_t
query_format(uint32_t format)
{
    switch(format){
    case IMGFMT_RGB|24:
    case IMGFMT_BGR|24:
        return VFCAP_CSP_SUPPORTED|VFCAP_CSP_SUPPORTED_BY_HW|VFCAP_ACCEPT_STRIDE;
    }
    return 0;
}

static void uninit(void){}

static void check_events(void){}

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
  case VOCTRL_DRAW_IMAGE:
    return draw_image(data);
  case VOCTRL_QUERY_FORMAT:
    return query_format(*((uint32_t*)data));
  }
  return VO_NOTIMPL;
}
