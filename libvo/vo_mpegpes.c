
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "video_out.h"
#include "video_out_internal.h"

LIBVO_EXTERN (mpegpes)

static vo_info_t vo_info = 
{
	"Mpeg-PES file",
	"mpgpes",
	"A'rpi",
	""
};

static uint32_t
init(uint32_t width, uint32_t height, uint32_t d_width, uint32_t d_height, uint32_t fullscreen, char *title, uint32_t format)
{

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
}

static uint32_t draw_slice(uint8_t *srcimg[], int stride[], int w,int h,int x,int y)
{
    return 0;
}


static uint32_t draw_frame(uint8_t * src[])
{


    return 0;
}

static uint32_t
query_format(uint32_t format)
{
    if(format==IMGFMT_MPEGPES) return 1;
    return 0;
}

static void
uninit(void)
{
}


static void check_events(void)
{
}

