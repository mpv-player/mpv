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

LIBVO_EXTERN (md5)

static vo_info_t vo_info = 
{
	"MD5 sum",
	"md5",
	"walken",
	""
};

extern vo_functions_t video_out_pgm;
extern char vo_pgm_filename[24];

static FILE * md5_file;

static uint32_t
config(uint32_t width, uint32_t height, uint32_t d_width, uint32_t d_height, uint32_t fullscreen, char *title, uint32_t format)
{
    md5_file = fopen ("md5", "w");
    return video_out_pgm.config (width, height, d_width,d_height,fullscreen, title, format);
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
    char buf2[100];
    FILE * f;
    int i;

    video_out_pgm.flip_page();

    snprintf (buf2, 100, "md5sum %s", vo_pgm_filename);
    f = popen (buf2, "r");
    i = fread (buf2, 1, sizeof(buf2), f);
    pclose (f);
    fwrite (buf2, 1, i, md5_file);

    remove (vo_pgm_filename);
    
}

//static uint32_t draw_slice(uint8_t * src[], uint32_t slice_num)
static uint32_t draw_slice(uint8_t *image[], int stride[], int w,int h,int x,int y)
{
    return video_out_pgm.draw_slice(image,stride,w,h,x,y);
}

//extern uint32_t output_pgm_frame (char * fname, uint8_t * src[]);

static uint32_t draw_frame(uint8_t * src[])
{
    return 0;
}

static uint32_t
query_format(uint32_t format)
{
    return video_out_pgm.control(VOCTRL_QUERY_FORMAT, &format);
}


static void
uninit(void)
{
    video_out_pgm.uninit();
    fclose(md5_file);
}


static void check_events(void)
{
}

static uint32_t preinit(const char *arg)
{
    if(arg) 
    {
	printf("vo_md5: Unknown subdevice: %s\n",arg);
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
