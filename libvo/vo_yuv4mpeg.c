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
#include <limits.h>

#include "config.h"
#include "subopt-helper.h"
#include "video_out.h"
#include "video_out_internal.h"

#include "mp_msg.h"
#include "help_mp.h"

#include "sub.h"

#include "fastmemcpy.h"
#include "libswscale/swscale.h"
#include "libswscale/rgb2rgb.h"
#include "libmpcodecs/vf_scale.h"
#include "libavutil/rational.h"

static const vo_info_t info = 
{
	"yuv4mpeg output for mjpegtools",
	"yuv4mpeg",
	"Robert Kesterson <robertk@robertk.com>",
	""
};

const LIBVO_EXTERN (yuv4mpeg)

static int image_width = 0;
static int image_height = 0;
static float image_fps = 0;

static uint8_t *image = NULL;
static uint8_t *image_y = NULL;
static uint8_t *image_u = NULL;
static uint8_t *image_v = NULL;

static uint8_t *rgb_buffer = NULL;
static uint8_t *rgb_line_buffer = NULL;

static char *yuv_filename = NULL;

static int using_format = 0;
static FILE *yuv_out;
static int write_bytes;

#define Y4M_ILACE_NONE         'p'  /* non-interlaced, progressive frame */
#define Y4M_ILACE_TOP_FIRST    't'  /* interlaced, top-field first       */
#define Y4M_ILACE_BOTTOM_FIRST 'b'  /* interlaced, bottom-field first    */

/* Set progressive mode as default */
static int config_interlace = Y4M_ILACE_NONE;
#define Y4M_IS_INTERLACED (config_interlace != Y4M_ILACE_NONE)

static int config(uint32_t width, uint32_t height, uint32_t d_width, 
       uint32_t d_height, uint32_t flags, char *title, 
       uint32_t format)
{
	AVRational pixelaspect = av_div_q((AVRational){d_width, d_height},
	                                  (AVRational){width, height});
	AVRational fps_frac = av_d2q(vo_fps, INT_MAX);
	if (image_width == width && image_height == height &&
	     image_fps == vo_fps && vo_config_count)
	  return 0;
	if (vo_config_count) {
	  mp_msg(MSGT_VO, MSGL_WARN,
	    "Video formats differ (w:%i=>%i, h:%i=>%i, fps:%f=>%f), "
	    "restarting output.\n",
	    image_width, width, image_height, height, image_fps, vo_fps);
	  uninit();
	}
	image_height = height;
	image_width = width;
	image_fps = vo_fps;
	using_format = format;

	if (Y4M_IS_INTERLACED)
	{
		if (height % 4)
		{
			mp_msg(MSGT_VO,MSGL_FATAL,
				MSGTR_VO_YUV4MPEG_InterlacedHeightDivisibleBy4);
			return -1;
		}
		
		rgb_line_buffer = malloc(image_width * 3);
		if (!rgb_line_buffer)
		{
			mp_msg(MSGT_VO,MSGL_FATAL,
				MSGTR_VO_YUV4MPEG_InterlacedLineBufAllocFail);
			return -1;
		}
		
		if (using_format == IMGFMT_YV12)
			mp_msg(MSGT_VO,MSGL_WARN,
				MSGTR_VO_YUV4MPEG_InterlacedInputNotRGB);
	}
				
	if (width % 2)
	{
		mp_msg(MSGT_VO,MSGL_FATAL,
			MSGTR_VO_YUV4MPEG_WidthDivisibleBy2);
		return -1;
	}	
	
	if(using_format != IMGFMT_YV12)
	{
		sws_rgb2rgb_init(get_sws_cpuflags());
		rgb_buffer = malloc(image_width * image_height * 3);
		if (!rgb_buffer)
		{
			mp_msg(MSGT_VO,MSGL_FATAL,
				MSGTR_VO_YUV4MPEG_NoMemRGBFrameBuf);
			return -1;
		}
	}

	write_bytes = image_width * image_height * 3 / 2;
	image = malloc(write_bytes);

	yuv_out = fopen(yuv_filename, "wb");
	if (!yuv_out || image == 0) 
	{
		mp_msg(MSGT_VO,MSGL_FATAL,
			MSGTR_VO_YUV4MPEG_OutFileOpenError,
			yuv_filename);
		return -1;
	}
	image_y = image;
	image_u = image_y + image_width * image_height;
	image_v = image_u + image_width * image_height / 4;
	
	fprintf(yuv_out, "YUV4MPEG2 W%d H%d F%d:%d I%c A%d:%d\n", 
			image_width, image_height, fps_frac.num, fps_frac.den,
			config_interlace,
			pixelaspect.num, pixelaspect.den);

	fflush(yuv_out);
	return 0;
}

/* Only use when h divisable by 2! */
static void swap_fields(uint8_t *ptr, const int h, const int stride)
{
	int i;
	
	for (i=0; i<h; i +=2)
	{
		fast_memcpy(rgb_line_buffer     , ptr + stride *  i   , stride);
		fast_memcpy(ptr + stride *  i   , ptr + stride * (i+1), stride);
		fast_memcpy(ptr + stride * (i+1), rgb_line_buffer     , stride);
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
		fast_memcpy(rgb_line_buffer, ptr + stride * i, stride);

		while (!line_state[j])
		{
			line_state[j] = 1;
			i = j;
			j = j * 2 % modv;
			fast_memcpy(ptr + stride * i, ptr + stride * j, stride);
		}
		fast_memcpy(ptr + stride * i, rgb_line_buffer, stride);
		
		while(k_start < modv && line_state[k_start])
			k_start++;
	}
	free(line_state);
}

static void vo_y4m_write(const void *ptr, const size_t num_bytes)
{
	if (fwrite(ptr, 1, num_bytes, yuv_out) != num_bytes)
		mp_msg(MSGT_VO,MSGL_ERR,
			MSGTR_VO_YUV4MPEG_OutFileWriteError);
}

static int write_last_frame(void)
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
	    return VO_TRUE; /* Image written; We have to stop here */
	}
    }
    /* Write progressive frame */
    vo_y4m_write(image, write_bytes);
    return VO_TRUE;
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

static int draw_slice(uint8_t *srcimg[], int stride[], int w,int h,int x,int y)
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
			fast_memcpy(dst, src, w);
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
				fast_memcpy(dstu, src1 , w >> 1);
				fast_memcpy(dstv, src2, w >> 1);
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
				fast_memcpy(dst, src, w * 3);
				src += stride[0];
				dst += image_width * 3;
			}
			break;
	}
	return 0;
}

static int draw_frame(uint8_t * src[])
{
	switch(using_format)
	{
		case IMGFMT_YV12:
			// gets done in draw_slice
			break;

		case IMGFMT_BGR24:
		case IMGFMT_RGB24:
			fast_memcpy(rgb_buffer, src[0], image_width * image_height * 3);
			break;
	}
    return 0;
}

static int query_format(uint32_t format)
{
    
	if (Y4M_IS_INTERLACED)
    {
		/* When processing interlaced material we want to get the raw RGB
         * data and do the YV12 conversion ourselves to have the chrominance
         * information sampled correct. */
		
		switch(format)
		{
			case IMGFMT_YV12:
				return VFCAP_CSP_SUPPORTED|VFCAP_OSD|VFCAP_ACCEPT_STRIDE;
			case IMGFMT_BGR|24:
			case IMGFMT_RGB|24:
				return VFCAP_CSP_SUPPORTED|VFCAP_CSP_SUPPORTED_BY_HW|VFCAP_OSD|VFCAP_ACCEPT_STRIDE;
		}
	}
	else
	{

		switch(format)
		{
			case IMGFMT_YV12:
				return VFCAP_CSP_SUPPORTED|VFCAP_CSP_SUPPORTED_BY_HW|VFCAP_OSD|VFCAP_ACCEPT_STRIDE;
    		case IMGFMT_BGR|24:
    		case IMGFMT_RGB|24:
        		return VFCAP_CSP_SUPPORTED|VFCAP_OSD|VFCAP_ACCEPT_STRIDE;
    	}	
	}
	return 0;
}

// WARNING: config(...) also uses this
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

	if (yuv_filename)
		free(yuv_filename);
	yuv_filename = NULL;
	image_width = 0;
	image_height = 0;
	image_fps = 0;
}


static void check_events(void)
{
}

static int preinit(const char *arg)
{
  int il, il_bf;
  opt_t subopts[] = {
    {"interlaced",    OPT_ARG_BOOL, &il,    NULL},
    {"interlaced_bf", OPT_ARG_BOOL, &il_bf, NULL},
    {"file",          OPT_ARG_MSTRZ,  &yuv_filename,  NULL},
    {NULL}
  };

  il = 0;
  il_bf = 0;
  yuv_filename = strdup("stream.yuv");
  if (subopt_parse(arg, subopts) != 0) {
    mp_msg(MSGT_VO, MSGL_FATAL, MSGTR_VO_YUV4MPEG_UnknownSubDev, arg); 
    return -1;
  }

  config_interlace = Y4M_ILACE_NONE;
  if (il)
    config_interlace = Y4M_ILACE_TOP_FIRST;
  if (il_bf)
    config_interlace = Y4M_ILACE_BOTTOM_FIRST;

    /* Inform user which output mode is used */
    switch (config_interlace)
    {
        case Y4M_ILACE_TOP_FIRST:
	    mp_msg(MSGT_VO,MSGL_STATUS,
	    	    MSGTR_VO_YUV4MPEG_InterlacedTFFMode);
            break;
        case Y4M_ILACE_BOTTOM_FIRST:
	    mp_msg(MSGT_VO,MSGL_STATUS,
	    	    MSGTR_VO_YUV4MPEG_InterlacedBFFMode);
            break;
        default:
	    mp_msg(MSGT_VO,MSGL_STATUS,
	    	    MSGTR_VO_YUV4MPEG_ProgressiveMode);
            break;
    }
    return 0;
}

static int control(uint32_t request, void *data, ...)
{
  switch (request) {
  case VOCTRL_QUERY_FORMAT:
    return query_format(*((uint32_t*)data));
  case VOCTRL_DUPLICATE_FRAME:
    return write_last_frame();
  }
  return VO_NOTIMPL;
}
