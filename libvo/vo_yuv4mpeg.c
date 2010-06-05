/*
 * yuv4mpeg (mjpegtools) interface
 *
 * Thrown together by Robert Kesterson <robertk@robertk.com>
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
 *
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
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
	AVRational fps_frac = av_d2q(vo_fps, vo_fps * 1001 + 2);
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
	}

	if (width % 2)
	{
		mp_msg(MSGT_VO,MSGL_FATAL,
			MSGTR_VO_YUV4MPEG_WidthDivisibleBy2);
		return -1;
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

static void draw_alpha(int x0, int y0, int w, int h, unsigned char *src,
                       unsigned char *srca, int stride) {
	    	vo_draw_alpha_yv12(w, h, src, srca, stride,
				       image + y0 * image_width + x0, image_width);
}

static void draw_osd(void)
{
    vo_draw_text(image_width, image_height, draw_alpha);
}

static void vo_y4m_write(const void *ptr, const size_t num_bytes)
{
	if (fwrite(ptr, 1, num_bytes, yuv_out) != num_bytes)
		mp_msg(MSGT_VO,MSGL_ERR,
			MSGTR_VO_YUV4MPEG_OutFileWriteError);
}

static int write_last_frame(void)
{
    fprintf(yuv_out, "FRAME\n");

    vo_y4m_write(image, write_bytes);
    return VO_TRUE;
}

static void flip_page (void)
{
	fprintf(yuv_out, "FRAME\n");

	vo_y4m_write(image, write_bytes);
}

static int draw_slice(uint8_t *srcimg[], int stride[], int w,int h,int x,int y)
{
	int i;
	uint8_t *dst, *src = srcimg[0];

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
	return 0;
}

static int draw_frame(uint8_t * src[])
{
			// gets done in draw_slice
    return 0;
}

static int query_format(uint32_t format)
{
	if (format == IMGFMT_YV12)
		return VFCAP_CSP_SUPPORTED|VFCAP_CSP_SUPPORTED_BY_HW|VFCAP_OSD|VFCAP_ACCEPT_STRIDE;
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
  const opt_t subopts[] = {
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
