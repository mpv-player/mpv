/*
 * MPlayer video driver for animated GIF output
 *
 * copyright (C) 2002 Joey Parrish <joey@nicewarrior.org>
 * based on vo_directfb2.c
 *
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/* Notes:
 * when setting output framerate, frames will be ignored as needed
 * to achieve the desired rate.  no frames will be duplicated.
 *
 * output framerate can be specified as a float
 * value now, instead of just an int.
 *
 * adjustments will be made to both the frame drop cycle and the
 * delay per frame to achieve the desired output framerate.
 *
 * time values are in centiseconds, because that's
 * what the gif spec uses for it's delay values.
 * 
 * preinit looks for arguments in one of the following formats (in this order):
 * fps:filename  -- sets the framerate (float) and output file
 * fps           -- sets the framerate (float), default file out.gif
 * filename      -- defaults to 5 fps, sets output file
 * (none)        -- defaults to 5 fps, output file out.gif
 *
 * trying to put the filename before the framerate will result in the
 * entire argument being interpretted as the filename.
 */

#include <gif_lib.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "subopt-helper.h"
#include "video_out.h"
#include "video_out_internal.h"
#include "mp_msg.h"

#define MPLAYER_VERSION 0.90
#define VO_GIF_REVISION 6

static const vo_info_t info = {
	"animated GIF output",
	"gif89a",
	"Joey Parrish joey@nicewarrior.org",
	""
};

const LIBVO_EXTERN(gif89a)


// how many frames per second we are aiming for during output.
static float target_fps;
// default value for output fps.
static const float default_fps = 5.00;
// the ideal gif delay per frame.
static float ideal_delay;
// the ideal time thus far.
static float ideal_time;
// actual time thus far.
static int real_time;
// nominal framedrop cycle length in frames
static float frame_cycle;
// position in the framedrop cycle
static int cycle_pos;
// adjustment of the framedrop cycle
static float frame_adj;

// the output width and height
static uint32_t img_width;
static uint32_t img_height;
// image data for slice rendering
static uint8_t *slice_data = NULL;
// reduced image data for flip_page
static uint8_t *reduce_data = NULL;
// reduced color map for flip_page
static ColorMapObject *reduce_cmap = NULL;

// a pointer to the gif structure
static GifFileType *new_gif = NULL;
// a string to contain the filename of the output gif
static char *gif_filename = NULL;
// the default output filename
#define DEFAULT_FILE "out.gif"

static opt_t subopts[] = {
  {"output",       OPT_ARG_MSTRZ, &gif_filename, NULL, 0},
  {"fps",          OPT_ARG_FLOAT, &target_fps,   NULL, 0},
  {NULL, 0, NULL, NULL, 0}
};

static int preinit(const char *arg)
{
	target_fps = 0;

	if (subopt_parse(arg, subopts) != 0) {
		mp_msg(MSGT_VO, MSGL_FATAL,
			"\n-vo gif89a command line help:\n"
			"Example: mplayer -vo gif89a:output=file.gif:fps=4.9\n"
			"\nOptions:\n"
			"  output=<filename>\n"
			"    Specify the output file.  The default is out.gif.\n"
			"  fps=<rate>\n"
			"    Specify the target framerate.  The default is 5.0.\n"
			"\n");
		return -1;
	}

	if (target_fps > vo_fps)
		target_fps = vo_fps; // i will not duplicate frames.

	if (target_fps <= 0) {
		target_fps = default_fps;
		mp_msg(MSGT_VO, MSGL_V, "GIF89a: default, %.2f fps\n", target_fps);
	} else {
		mp_msg(MSGT_VO, MSGL_V, "GIF89a: output fps forced to %.2f\n", target_fps);
	}
	
	ideal_delay = 100 / target_fps; // in centiseconds
	frame_cycle = vo_fps / target_fps;
	// we make one output frame every (frame_cycle) frames, on average.
	
	if (gif_filename == NULL) {
		gif_filename = strdup(DEFAULT_FILE);
		mp_msg(MSGT_VO, MSGL_V, "GIF89a: default, file \"%s\"\n", gif_filename);
	} else {
		mp_msg(MSGT_VO, MSGL_V, "GIF89a: file forced to \"%s\"\n", gif_filename);
	}
	
	mp_msg(MSGT_VO, MSGL_DBG2, "GIF89a: Preinit OK\n");
	return 0;
}

static int config(uint32_t s_width, uint32_t s_height, uint32_t d_width,
		uint32_t d_height, uint32_t flags, char *title,
		uint32_t format)
{
#ifdef CONFIG_GIF_4
	// these are control blocks for the gif looping extension.
	char LB1[] = "NETSCAPE2.0";
	char LB2[] = { 1, 0, 0 };
#endif

	mp_msg(MSGT_VO, MSGL_DBG2, "GIF89a: Config entered [%dx%d]\n", s_width,s_height);
	mp_msg(MSGT_VO, MSGL_DBG2, "GIF89a: With requested format: %s\n", vo_format_name(format));
	
	// save these for later.
	img_width = s_width;
	img_height = s_height;

	// multiple configs without uninit are not allowed.
	// this is because config opens a new gif file.
	if (vo_config_count > 0) {
		mp_msg(MSGT_VO, MSGL_V, "GIF89a: Reconfigure attempted.\n");
		return 0;
	}
	// reconfigure need not be a fatal error, so return 0.
	// multiple configs without uninit will result in two
	// movies concatenated in one gif file.  the output
	// gif will have the dimensions of the first movie.
	
	if (format != IMGFMT_RGB24) {
		mp_msg(MSGT_VO, MSGL_ERR, "GIF89a: Error - given unsupported colorspace.\n");
		return 1;
	}
	
	// the EGifSetGifVersion line causes segfaults in certain
	// earlier versions of libungif.  i don't know exactly which,
	// but certainly in all those before v4.  if you have problems,
	// you need to upgrade your gif library.
#ifdef CONFIG_GIF_4
	EGifSetGifVersion("89a");
#else
	mp_msg(MSGT_VO, MSGL_ERR, "GIF89a: Your version of libungif needs to be upgraded.\n");
	mp_msg(MSGT_VO, MSGL_ERR, "GIF89a: Some functionality has been disabled.\n");
#endif
	
	new_gif = EGifOpenFileName(gif_filename, 0);
	if (new_gif == NULL) {
		mp_msg(MSGT_VO, MSGL_ERR, "GIF89a: error opening file \"%s\" for output.\n", gif_filename);
		return 1;
	}

	slice_data = malloc(img_width * img_height * 3);
	if (slice_data == NULL) {
		mp_msg(MSGT_VO, MSGL_ERR, "GIF89a: malloc failed.\n");
		return 1;
	}

	reduce_data = malloc(img_width * img_height);
	if (reduce_data == NULL) {
		free(slice_data); slice_data = NULL;
		mp_msg(MSGT_VO, MSGL_ERR, "GIF89a: malloc failed.\n");
		return 1;
	}

	reduce_cmap = MakeMapObject(256, NULL);
	if (reduce_cmap == NULL) {
		free(slice_data); slice_data = NULL;
		free(reduce_data); reduce_data = NULL;
		mp_msg(MSGT_VO, MSGL_ERR, "GIF89a: malloc failed.\n");
		return 1;
	}
	
	// initialize the delay and framedrop variables.
	ideal_time = 0;
	real_time = 0;
	cycle_pos = 0;
	frame_adj = 0;

	// set the initial width and height info.
	EGifPutScreenDesc(new_gif, s_width, s_height, 256, 0, reduce_cmap);
#ifdef CONFIG_GIF_4
	// version 3 of libungif does not support multiple control blocks.
	// looping requires multiple control blocks.
	// therefore, looping is only enabled for v4 and up.
	EGifPutExtensionFirst(new_gif, 0xFF, 11, LB1);
	EGifPutExtensionLast(new_gif, 0, 3, LB2);
#endif

	mp_msg(MSGT_VO, MSGL_DBG2, "GIF89a: Config finished.\n");
	return 0;
}

// we do not draw osd.
void draw_osd(void) {}

// we do not handle events.
static void check_events(void) {}

static int gif_reduce(int width, int height, uint8_t *src, uint8_t *dst, GifColorType *colors)
{
	unsigned char Ra[width * height];
	unsigned char Ga[width * height];
	unsigned char Ba[width * height];
	unsigned char *R, *G, *B;
	int size = 256;
	int i;

	R = Ra; G = Ga; B = Ba;
	for (i = 0; i < width * height; i++)
	{
		*R++ = *src++;
		*G++ = *src++;
		*B++ = *src++;
	}
	
	R = Ra; G = Ga; B = Ba;
	return QuantizeBuffer(width, height, &size, R, G, B, dst, colors);
}

static void flip_page(void)
{
	char CB[4]; // control block
	int delay = 0;
	int ret;

	cycle_pos++;
	if (cycle_pos < frame_cycle - frame_adj)
		return; // we are skipping this frame

	// quantize the image
	ret = gif_reduce(img_width, img_height, slice_data, reduce_data, reduce_cmap->Colors);
	if (ret == GIF_ERROR) {
		mp_msg(MSGT_VO, MSGL_ERR, "GIF89a: Quantize failed.\n");
		return;
	}

	// calculate frame delays and frame skipping
	ideal_time += ideal_delay;
	delay = (int)(ideal_time - real_time);
	real_time += delay;
	frame_adj += cycle_pos;
	frame_adj -= frame_cycle;
	cycle_pos = 0;
	
	// set up the delay control block
	CB[0] = (char)(delay >> 8);
	CB[1] = (char)(delay & 0xff);
	CB[2] = 0;
	CB[3] = 0;

	// put the control block with delay info
	EGifPutExtension(new_gif, 0xF9, 0x04, CB);
	// put the image description
	EGifPutImageDesc(new_gif, 0, 0, img_width, img_height, 0, reduce_cmap);
	// put the image itself
	EGifPutLine(new_gif, reduce_data, img_width * img_height);
}

static int draw_frame(uint8_t *src[])
{
	return 1;
}

static int draw_slice(uint8_t *src[], int stride[], int w, int h, int x, int y)
{
	uint8_t *dst, *frm;
	int i;
	dst = slice_data + (img_width * y + x) * 3;
	frm = src[0];
	for (i = 0; i < h; i++) {
		memcpy(dst, frm, w * 3);
		dst += (img_width * 3);
		frm += stride[0];
	}
	return 0;
}

static int query_format(uint32_t format)
{
	if (format == IMGFMT_RGB24)
		return VFCAP_CSP_SUPPORTED | VFCAP_CSP_SUPPORTED_BY_HW | VFCAP_TIMER | VFCAP_ACCEPT_STRIDE;
	return 0;
}

static int control(uint32_t request, void *data, ...)
{
	if (request == VOCTRL_QUERY_FORMAT) {
		return query_format(*((uint32_t*)data));
	}
	if (request == VOCTRL_DUPLICATE_FRAME) {
		flip_page();
		return VO_TRUE;
	}
	return VO_NOTIMPL;
}

static void uninit(void)
{
	mp_msg(MSGT_VO, MSGL_DBG2, "GIF89a: Uninit entered\n");
	
	if (new_gif != NULL) {
		char temp[256];
		// comment the gif and close it
		snprintf(temp, 256, "MPlayer gif output v%2.2f-%d (c) %s\r\n",
			MPLAYER_VERSION, VO_GIF_REVISION,
			"joey@nicewarrior.org");
		EGifPutComment(new_gif, temp);
		EGifCloseFile(new_gif); // also frees gif storage space.
	}
	
	// free our allocated ram
	if (gif_filename != NULL) free(gif_filename);
	if (slice_data != NULL) free(slice_data);
	if (reduce_data != NULL) free(reduce_data);
	if (reduce_cmap != NULL) FreeMapObject(reduce_cmap);
	
	// set the pointers back to null.
	new_gif = NULL;
	gif_filename = NULL;
	slice_data = NULL;
	reduce_data = NULL;
	reduce_cmap = NULL;
}

