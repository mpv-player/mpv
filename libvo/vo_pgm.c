/* 
 * video_out_pgm.c, pgm interface
 *
 *
 * Copyright (C) 1996, MPEG Software Simulation Group. All Rights Reserved. 
 *
 * Hacked into mpeg2dec by
 * 
 * Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
 *
 * 15 & 16 bpp support added by Franck Sicard <Franck.Sicard@solsoft.fr>
 *
 * Xv image suuport by Gerd Knorr <kraxel@goldbach.in-berlin.de>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "config.h"
#include "video_out.h"
#include "video_out_internal.h"

LIBVO_EXTERN (pgm)

static vo_info_t vo_info = 
{
	"PGM file",
	"pgm",
	"walken",
	""
};

static int image_width;
static int image_height;
static char header[1024];
static int framenum = 0;

static uint8_t *image=NULL;

char vo_pgm_filename[24];

static uint32_t
config(uint32_t width, uint32_t height, uint32_t d_width, uint32_t d_height, uint32_t fullscreen, char *title, uint32_t format)
{
    image_height = height;
    image_width = width;
    image=malloc(width*height*3/2);

    snprintf (header, 1024, "P5\n\n%d %d\n255\n", width, height*3/2);

    return 0;
}

static const vo_info_t*
get_info(void)
{
    return &vo_info;
}

static void draw_osd(void)
{
}

static void flip_page (void)
{
    FILE * f;

    snprintf (vo_pgm_filename, 24, "%08d.pgm", framenum++);

    f = fopen (vo_pgm_filename, "wb");  if (f == NULL) return;
    fwrite (header, strlen (header), 1, f);
    fwrite (image, image_width, image_height*3/2, f);
    fclose (f);

    return;
}

static uint32_t draw_slice(uint8_t *srcimg[], int stride[], int w,int h,int x,int y)
{
    int i;
    // copy Y:
    uint8_t *dst=image+image_width*y+x;
    uint8_t *src=srcimg[0];
    for(i=0;i<h;i++){
        memcpy(dst,src,w);
        src+=stride[0];
        dst+=image_width;
    }
{
    // copy U+V:
    uint8_t *src1=srcimg[1];
    uint8_t *src2=srcimg[2];
    uint8_t *dst=image+image_width*image_height+image_width*(y/2)+(x/2);
    for(i=0;i<h/2;i++){
        memcpy(dst,src1,w/2);
        memcpy(dst+image_width/2,src2,w/2);
        src1+=stride[1];
        src2+=stride[2];
        dst+=image_width;
    }

}
    
    return 0;
}


static uint32_t draw_frame(uint8_t * src[])
{
    return 0;
}

static uint32_t
query_format(uint32_t format)
{
    if(format==IMGFMT_YV12) return 1;
//    switch(format){
//    case IMGFMT_YV12:
//    case IMGFMT_RGB|24:
//    case IMGFMT_BGR|24:
//        return 1;
//    }
    return 0;
}

static void
uninit(void)
{
    if(image){ free(image);image=NULL;}
}


static void check_events(void)
{
}

static uint32_t preinit(const char *arg)
{
    if(arg) 
    {
	printf("vo_pgm: Unknown subdevice: %s\n",arg);
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
