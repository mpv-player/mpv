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
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "video_out.h"
#include "video_out_internal.h"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <signal.h>
#include <sys/wait.h>

#if defined (linux)
#include <linux/videodev.h>
#endif


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


/**
 *
 * height should be a multiple of 2 and width should be a multiple of 2
 * chrominance data is only taken from every secound line others are ignored 
 */
#define RGB2YUV_SHIFT 8
#define BY ((int)( 0.098*(1<<RGB2YUV_SHIFT)+0.5))
#define BV ((int)(-0.071*(1<<RGB2YUV_SHIFT)+0.5))
#define BU ((int)( 0.439*(1<<RGB2YUV_SHIFT)+0.5))
#define GY ((int)( 0.504*(1<<RGB2YUV_SHIFT)+0.5))
#define GV ((int)(-0.368*(1<<RGB2YUV_SHIFT)+0.5))
#define GU ((int)(-0.291*(1<<RGB2YUV_SHIFT)+0.5))
#define RY ((int)( 0.257*(1<<RGB2YUV_SHIFT)+0.5))
#define RV ((int)( 0.439*(1<<RGB2YUV_SHIFT)+0.5))
#define RU ((int)(-0.148*(1<<RGB2YUV_SHIFT)+0.5))

static inline void rgb24toyv12(const uint8_t *src, uint8_t *ydst, uint8_t *udst, uint8_t *vdst,
	unsigned int width, unsigned int height,
	unsigned int lumStride, unsigned int chromStride, unsigned int srcStride)
{
	int y;
	const int chromWidth= width>>1;
	for(y=0; y<height; y+=2)
	{
		int i;
		for(i=0; i<chromWidth; i++)
		{
			unsigned int b= src[6*i+0];
			unsigned int g= src[6*i+1];
			unsigned int r= src[6*i+2];

			unsigned int Y  =  ((RY*r + GY*g + BY*b)>>RGB2YUV_SHIFT) + 16;
			unsigned int V  =  ((RV*r + GV*g + BV*b)>>RGB2YUV_SHIFT) + 128;
			unsigned int U  =  ((RU*r + GU*g + BU*b)>>RGB2YUV_SHIFT) + 128;

			udst[i] 	= U;
			vdst[i] 	= V;
			ydst[2*i] 	= Y;

			b= src[6*i+3];
			g= src[6*i+4];
			r= src[6*i+5];

			Y  =  ((RY*r + GY*g + BY*b)>>RGB2YUV_SHIFT) + 16;
			ydst[2*i+1] 	= Y;
		}
		ydst += lumStride;
		src  += srcStride;

		for(i=0; i<chromWidth; i++)
		{
			unsigned int b= src[6*i+0];
			unsigned int g= src[6*i+1];
			unsigned int r= src[6*i+2];

			unsigned int Y  =  ((RY*r + GY*g + BY*b)>>RGB2YUV_SHIFT) + 16;

			ydst[2*i] 	= Y;

			b= src[6*i+3];
			g= src[6*i+4];
			r= src[6*i+5];

			Y  =  ((RY*r + GY*g + BY*b)>>RGB2YUV_SHIFT) + 16;
			ydst[2*i+1] 	= Y;
		}
		udst += chromStride;
		vdst += chromStride;
		ydst += lumStride;
		src  += srcStride;
	}
}

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

static void draw_osd(void)
{
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
			uint8_t *src1 = srcimg[1];
			uint8_t *src2 = srcimg[2];
			uint8_t *dstu = image_u + image_width * (y / 2) + (x / 2);
			uint8_t *dstv = image_v + image_width * (y / 2) + (x / 2);
			for (i = 0; i < h / 2; i++)
			{
				memcpy(dstu, src1 , w / 2);
				memcpy(dstv, src2, w / 2);
				src1 += stride[1];
				src2 += stride[2];
				dstu += image_width / 2;
				dstv += image_width / 2;
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
//				RGB2YUV(image_width, image_height, src[0], image_y, image_u, image_v, 1);
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
