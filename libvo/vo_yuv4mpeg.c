/* 
 * vo_yuv4mpeg.c, yuv4mpeg (mjpegtools) interface
 *
 * Thrown together by
 * Robert Kesterson <robertk@robertk.com>
 * Based on the pgm output plugin, the rgb2rgb postproc filter, divxdec,
 * and probably others.
 *
 * This is undoubtedly incomplete, inaccurate, or just plain wrong. :-)
 *
 *
 * 2002/04/17 Juergen Hammelmann <juergen.hammelmann@gmx.de>
 *            - added support for output of subtitles
 *              best, if you give option '-osdlevel 0' to mplayer for 
 *              no watching the seek+timer
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#include "config.h"
#include "video_out.h"
#include "video_out_internal.h"

#include "sub.h"

#include "fastmemcpy.h"
#include "../postproc/rgb2rgb.h"

LIBVO_EXTERN (yuv4mpeg)

static vo_info_t vo_info = 
{
	"yuv4mpeg output for mjpegtools (to \"stream.yuv\")",
	"yuv4mpeg",
	"Robert Kesterson <robertk@robertk.com>",
	""
};

static int image_width;
static int image_height;

static uint8_t *image = NULL;
static uint8_t *image_y = NULL;
static uint8_t *image_u = NULL;
static uint8_t *image_v = NULL;

static int using_format = 0;
static FILE *yuv_out;
int write_bytes;

static uint32_t config(uint32_t width, uint32_t height, uint32_t d_width, 
       uint32_t d_height, uint32_t fullscreen, char *title, 
       uint32_t format, const vo_tune_info_t *tuneinfo)
{
    image_height = height;
    image_width = width;
	write_bytes = image_width * image_height * 3 / 2;
	using_format = format;
    image = malloc(write_bytes);

	yuv_out = fopen("stream.yuv", "wb");
	if (!yuv_out || image == NULL) 
	{
		perror("Can't get memory or file handle to stream.yuv");
		return -1;
	}
	image_y = image;
	image_u = image_y + image_width * image_height;
	image_v = image_u + (image_width * image_height) / 4;
	
	// This isn't right.  
	// But it should work as long as the file isn't interlaced
	// or otherwise unusual (the "Ip A0:0" part).
	fprintf(yuv_out, "YUV4MPEG2 W%d H%d F%ld:%ld Ip A0:0\n", 
					image_width, image_height, (long)(vo_fps * 1000000.0), 1000000);

	fflush(yuv_out);
	return 0;
}

static const vo_info_t* get_info(void)
{
    return &vo_info;
}

static void draw_alpha(int x0, int y0, int w, int h, unsigned char *src,
                       unsigned char *srca, int stride) {
    if(using_format == IMGFMT_YV12)
	{
	    vo_draw_alpha_yv12(w, h, src, srca, stride, 
			       image+(y0*image_width+x0), image_width);
	}
}

static void draw_osd(void)
{
    vo_draw_text(image_width, image_height, draw_alpha);
}

static void flip_page (void)
{
	fprintf(yuv_out, "FRAME\n");
	if(fwrite(image, 1, write_bytes, yuv_out) != write_bytes)
		perror("Error writing image to output!");
    return;
}

static uint32_t draw_slice(uint8_t *srcimg[], int stride[], int w,int h,int x,int y)
{
	if(using_format == IMGFMT_YV12)
	{
		int i;
		// copy Y:
		uint8_t *dst = image_y + image_width * y + x;
		uint8_t *src = srcimg[0];
		for (i = 0; i < h; i++)
		{
			memcpy(dst, src, w);
			src += stride[0];
			dst += image_width;
		}
		{
			// copy U + V:
			int imgstride = image_width >> 1;
			uint8_t *src1 = srcimg[1];
			uint8_t *src2 = srcimg[2];
			uint8_t *dstu = image_u + imgstride * (y >> 1) + (x >> 1);
			uint8_t *dstv = image_v + imgstride * (y >> 1) + (x >> 1);
			for (i = 0; i < h / 2; i++)
			{
				memcpy(dstu, src1 , w >> 1);
				memcpy(dstv, src2, w >> 1);
				src1 += stride[1];
				src2 += stride[2];
				dstu += imgstride;
				dstv += imgstride;
			}
		}
	}
	return 0;
}


static uint32_t draw_frame(uint8_t * src[])
{
	switch(using_format)
	{
		case IMGFMT_YV12:
			// gets done in draw_slice
			break;
		case IMGFMT_BGR|24:
			{
#ifdef GUESS_THIS_ISNT_NEEDED
				int c;
				uint8_t temp;
				//switch BGR to RGB
				for(c = 0; c < image_width * image_height; c++)
				{
					temp = src[0][c * 3];
					src[0][c * 3] = src[0][c * 3 + 2];
					src[0][c * 3 + 2] = temp;
				}
#endif
			}
			// intentional fall-through
		case IMGFMT_RGB|24:
			{
				rgb24toyv12(src[0], image_y, image_u, image_v, 
							image_width, image_height, 
							image_width, image_width / 2, image_width * 3);
			}
			break;
	}
    return 0;
}

static uint32_t query_format(uint32_t format)
{
    switch(format){
    case IMGFMT_YV12:
    case IMGFMT_BGR|24:
    case IMGFMT_RGB|24:
        return 1;
    }
    return 0;
}

static void uninit(void)
{
    if(image)
		free(image);
	image = NULL;
	if(yuv_out)
		fclose(yuv_out);
	yuv_out = NULL;
}


static void check_events(void)
{
}


static uint32_t preinit(const char *arg)
{
    if(arg) 
    {
	printf("vo_yuv4mpeg: Unknown subdevice: %s\n",arg);
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
