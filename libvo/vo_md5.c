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

LIBVO_EXTERN (md5)

static vo_info_t vo_info = 
{
	"MD5 sum",
	"md5",
	"walken",
	""
};

extern vo_functions_t video_out_pgm;

static FILE * md5_file;
static int framenum = -2;

static uint32_t
init(uint32_t width, uint32_t height, uint32_t d_width, uint32_t d_height, uint32_t fullscreen, char *title, uint32_t format)
{
    md5_file = fopen ("md5", "w");
    return video_out_pgm.init (width, height, d_width,d_height,fullscreen, title, format);
}

static const vo_info_t*
get_info(void)
{
    return &vo_info;
}

static void flip_page (void)
{
}

//static uint32_t draw_slice(uint8_t * src[], uint32_t slice_num)
static uint32_t draw_slice(uint8_t *image[], int stride[], int w,int h,int x,int y)
{
    return 0;
}

extern uint32_t output_pgm_frame (char * fname, uint8_t * src[]);

static uint32_t draw_frame(uint8_t * src[])
{
    char buf[100];
    char buf2[100];
    FILE * f;
    int i;

    if (++framenum < 0)
	return 0;

    sprintf (buf, "%d.pgm", framenum);
    output_pgm_frame (buf, src);

    sprintf (buf2, "md5sum %s", buf);
    f = popen (buf2, "r");
    i = fread (buf2, 1, sizeof(buf2), f);
    pclose (f);
    fwrite (buf2, 1, i, md5_file);

    remove (buf);

    return 0;
}

static uint32_t
query_format(uint32_t format)
{
//    switch(format){
//    case IMGFMT_YV12:
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


