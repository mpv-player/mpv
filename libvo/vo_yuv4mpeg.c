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
 * 2002/06/19 Klaus Stengel <Klaus.Stengel@asamnet.de>
 *            - added support for interlaced output
 *              Activate by using '-vo yuv4mpeg:interlaced'
 *              or '-vo yuv4mpeg:interlaced_bf' if your source has
 *              bottom fields first
 *            - added some additional checks to catch problems
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

static uint8_t *rgb_buffer = NULL;
static uint8_t *rgb_line_buffer = NULL;

static int using_format = 0;
static FILE *yuv_out;
static int write_bytes;

#define Y4M_ILACE_NONE         'p'  /* non-interlaced, progressive frame */
#define Y4M_ILACE_TOP_FIRST    't'  /* interlaced, top-field first       */
#define Y4M_ILACE_BOTTOM_FIRST 'b'  /* interlaced, bottom-field first    */

/* Set progressive mode as default */
static int config_interlace = Y4M_ILACE_NONE;
#define Y4M_IS_INTERLACED (config_interlace != Y4M_ILACE_NONE)

static uint32_t config(uint32_t width, uint32_t height, uint32_t d_width, 
       uint32_t d_height, uint32_t fullscreen, char *title, 
       uint32_t format)
{
	image_height = height;
	image_width = width;
	using_format = format;

	if (Y4M_IS_INTERLACED)
	{
		if (height % 4)
		{
			perror("yuv4mpeg: Interlaced mode requires image height to be divisable by 4");
			return -1;
		}
		
		rgb_line_buffer = malloc(image_width * 3);
		if (!rgb_line_buffer)
		{
			perror("yuv4mpeg: Unable to allocate line buffer for interlaced mode");
			return -1;
		}
		
		if (using_format == IMGFMT_YV12)
			printf("yuv4mpeg: WARNING: Input not RGB; Can't seperate chrominance by fields!\n");
	}
				
	if (width % 2)
	{
		perror("yuv4mpeg: Image width must be divisable by 2");
		return -1;
	}	
	
	if(using_format != IMGFMT_YV12)
	{
		rgb_buffer = malloc(image_width * image_height * 3);
		if (!rgb_buffer)
		{
			perror("yuv4mpeg: Not enough memory to allocate RGB framebuffer");
			return -1;
		}
	}

	write_bytes = image_width * image_height * 3 / 2;
	image = malloc(write_bytes);

	yuv_out = fopen("stream.yuv", "wb");
	if (!yuv_out || image == 0) 
	{
		perror("yuv4mpeg: Can't get memory or file handle to write stream.yuv");
		return -1;
	}
	image_y = image;
	image_u = image_y + image_width * image_height;
	image_v = image_u + image_width * image_height / 4;
	
	// This isn't right.  
	// But it should work as long as the file isn't interlaced
	// or otherwise unusual (the "Ip A0:0" part).

	/* At least the interlacing is ok now */
	fprintf(yuv_out, "YUV4MPEG2 W%d H%d F%ld:%ld I%c A0:0\n", 
			image_width, image_height, (long)(vo_fps * 1000000.0), 
			(long)1000000, config_interlace);

	fflush(yuv_out);
	return 0;
}

static const vo_info_t* get_info(void)
{
    return &vo_info;
}

/* Only use when h divisable by 2! */
static void swap_fields(uint8_t *ptr, const int h, const int stride)
{
	int i;
	
	for (i=0; i<h; i +=2)
	{
		memcpy(rgb_line_buffer     , ptr + stride *  i   , stride);
		memcpy(ptr + stride *  i   , ptr + stride * (i+1), stride);
		memcpy(ptr + stride * (i+1), rgb_line_buffer     , stride);
	}
}

static void draw_alpha(int x0, int y0, int w, int h, unsigned char *src,
                       unsigned char *srca, int stride) {
	switch (using_format)
	{
    	case IMGFMT_YV12:
	    	vo_draw_alpha_yv12(w, h, src, srca, stride, 
				       image + y0 * image_width + x0, image_width);
			break;
		
		case IMGFMT_BGR|24:
		case IMGFMT_RGB|24:
			if (config_interlace != Y4M_ILACE_BOTTOM_FIRST)
				vo_draw_alpha_rgb24(w, h, src, srca, stride,
						rgb_buffer + (y0 * image_width + x0) * 3, image_width * 3);
			else
			{
				swap_fields (rgb_buffer, image_height, image_width * 3);

				vo_draw_alpha_rgb24(w, h, src, srca, stride,
						rgb_buffer + (y0 * image_width  + x0) * 3, image_width * 3);
				
				swap_fields (rgb_buffer, image_height, image_width * 3);
			}
			break;
	}
}

static void draw_osd(void)
{
    vo_draw_text(image_width, image_height, draw_alpha);
}

static void deinterleave_fields(uint8_t *ptr, const int stride,
							  const int img_height)
{
	unsigned int i, j, k_start = 1, modv = img_height - 1;
	unsigned char *line_state = malloc(modv);

	for (i=0; i<modv; i++)
		line_state[i] = 0;

	line_state[0] = 1;
	
	while(k_start < modv)
	{
		i = j = k_start;
		memcpy(rgb_line_buffer, ptr + stride * i, stride);

		while (!line_state[j])
		{
			line_state[j] = 1;
			i = j;
			j = j * 2 % modv;
			memcpy(ptr + stride * i, ptr + stride * j, stride);
		}
		memcpy(ptr + stride * i, rgb_line_buffer, stride);
		
		while(k_start < modv && line_state[k_start])
			k_start++;
	}
	free(line_state);
}

static void vo_y4m_write(const void *ptr, const size_t num_bytes)
{
	if (fwrite(ptr, 1, num_bytes, yuv_out) != num_bytes)
		perror("yuv4mpeg: Error writing image to output!");
}

static void flip_page (void)
{
	uint8_t *upper_y, *upper_u, *upper_v, *rgb_buffer_lower;
	int rgb_stride, uv_stride, field_height;
	unsigned int i, low_ofs;
	
	fprintf(yuv_out, "FRAME\n");
	
	if (using_format != IMGFMT_YV12)
	{
		rgb_stride = image_width * 3;
		uv_stride = image_width / 2;
		
		if (Y4M_IS_INTERLACED)
		{
			field_height = image_height / 2;		

			upper_y = image;
			upper_u = upper_y + image_width * field_height;
			upper_v = upper_u + image_width * field_height / 4;
			low_ofs = image_width * field_height * 3 / 2;
			rgb_buffer_lower = rgb_buffer + rgb_stride * field_height;
		
			deinterleave_fields(rgb_buffer, rgb_stride, image_height);

			rgb24toyv12(rgb_buffer, upper_y, upper_u, upper_v,
						 image_width, field_height,
						 image_width, uv_stride, rgb_stride);
			rgb24toyv12(rgb_buffer_lower,  upper_y + low_ofs,
						 upper_u + low_ofs, upper_v + low_ofs,
						 image_width, field_height,
						 image_width, uv_stride, rgb_stride);
		
			/* Write Y plane */
			for(i = 0; i < field_height; i++)
			{
				vo_y4m_write(upper_y + image_width * i,           image_width);
				vo_y4m_write(upper_y + image_width * i + low_ofs, image_width);
			}

			/* Write U and V plane */
			for(i = 0; i < field_height / 2; i++)
			{
				vo_y4m_write(upper_u + uv_stride * i,           uv_stride);
				vo_y4m_write(upper_u + uv_stride * i + low_ofs, uv_stride);
			}
			for(i = 0; i < field_height / 2; i++)
			{
				vo_y4m_write(upper_v + uv_stride * i,           uv_stride);
				vo_y4m_write(upper_v + uv_stride * i + low_ofs, uv_stride);
			}
			return; /* Image written; We have to stop here */
		}

		rgb24toyv12(rgb_buffer, image_y, image_u, image_v,
					image_width, image_height,
					image_width, uv_stride, rgb_stride);
	}

	/* Write progressive frame */
	vo_y4m_write(image, write_bytes);
}

static uint32_t draw_slice(uint8_t *srcimg[], int stride[], int w,int h,int x,int y)
{
	int i;
	uint8_t *dst, *src = srcimg[0];
	
	switch (using_format)
	{
		case IMGFMT_YV12:
		
		// copy Y:
		dst = image_y + image_width * y + x;
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
		break;
		
		case IMGFMT_BGR24:
		case IMGFMT_RGB24:
			dst = rgb_buffer + (image_width * y + x) * 3;
			for (i = 0; i < h; i++)
			{
				memcpy(dst, src, w * 3);
				src += stride[0];
				dst += image_width * 3;
			}
			break;
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
		case IMGFMT_RGB|24:
			memcpy(rgb_buffer, src[0], image_width * image_height * 3);
			break;
	}
    return 0;
}

static uint32_t query_format(uint32_t format)
{
    
	if (Y4M_IS_INTERLACED)
    {
		/* When processing interlaced material we want to get the raw RGB
         * data and do the YV12 conversion ourselves to have the chrominance
         * information sampled correct. */
		
		switch(format)
		{
			case IMGFMT_YV12:
				return VFCAP_CSP_SUPPORTED|VFCAP_OSD;
			case IMGFMT_BGR|24:
			case IMGFMT_RGB|24:
				return VFCAP_CSP_SUPPORTED|VFCAP_CSP_SUPPORTED_BY_HW|VFCAP_OSD;
		}
	}
	else
	{

		switch(format)
		{
			case IMGFMT_YV12:
				return VFCAP_CSP_SUPPORTED|VFCAP_CSP_SUPPORTED_BY_HW|VFCAP_OSD;
    		case IMGFMT_BGR|24:
    		case IMGFMT_RGB|24:
        		return VFCAP_CSP_SUPPORTED|VFCAP_OSD;
    	}	
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
	
	if(rgb_buffer)
		free(rgb_buffer);
	rgb_buffer = NULL;

	if(rgb_line_buffer)
		free(rgb_line_buffer);
	rgb_line_buffer = NULL;
}


static void check_events(void)
{
}


static uint32_t preinit(const char *arg)
{
    int arg_unrecognized = 0;

    if(arg) 
    {
        /* configure output mode */
	if (strcmp(arg, "interlaced"))
            arg_unrecognized++;
	else
	    config_interlace = Y4M_ILACE_TOP_FIRST;

        if (strcmp(arg, "interlaced_bf"))
            arg_unrecognized++;
        else
            config_interlace = Y4M_ILACE_BOTTOM_FIRST;

        /* If both tests failed the argument is invalid */
        if (arg_unrecognized == 2)
        {
	        printf("vo_yuv4mpeg: Unknown subdevice: %s\n", arg);
			return ENOSYS;
		}
    }

    /* Inform user which output mode is used */
    switch (config_interlace)
    {
        case Y4M_ILACE_TOP_FIRST:
            printf("vo_yuv4mpeg: Interlaced output mode, top-field first\n");
            break;
        case Y4M_ILACE_BOTTOM_FIRST:
            printf("vo_yuv4mpeg: Interlaced output mode, bottom-field first\n");
            break;
        default:
            printf("vo_yuv4mpeg: Using (default) progressive frame mode\n");
            break;
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
