#define DISP

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
static int framenum = -2;

static uint32_t
init(uint32_t width, uint32_t height, uint32_t d_width, uint32_t d_height, uint32_t fullscreen, char *title, uint32_t format)
{
    image_height = height;
    image_width = width;

    sprintf (header, "P5\n\n%d %d\n255\n", width, height*3/2);

    return 0;
}

static const vo_info_t*
get_info(void)
{
    return &vo_info;
}

static void flip_page (void)
{
}

static uint32_t draw_slice(uint8_t *image[], int stride[], int w,int h,int x,int y)
//static uint32_t draw_slice(uint8_t * src[], uint32_t slice_num)
{
    return 0;
}

uint32_t output_pgm_frame (char * fname, uint8_t * src[])
{
    FILE * f;
    int i;

    f = fopen (fname, "wb");
    if (f == NULL) return 1;
    fwrite (header, strlen (header), 1, f);
    fwrite (src[0], image_width, image_height, f);
    for (i = 0; i < image_height/2; i++) {
	fwrite (src[1]+i*image_width/2, image_width/2, 1, f);
	fwrite (src[2]+i*image_width/2, image_width/2, 1, f);
    }
    fclose (f);

    return 0;
}

static uint32_t draw_frame(uint8_t * src[])
{
    char buf[100];

    if (++framenum < 0)
	return 0;

    sprintf (buf, "%d.pgm", framenum);
    return output_pgm_frame (buf, src);
}

static uint32_t
query_format(uint32_t format)
{
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
}


static void check_events(void)
{
}



