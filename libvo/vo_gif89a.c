/*
   MPlayer video driver for animated gif output
  
   (C) 2002
   
   Written by Joey Parrish <joey@nicewarrior.org>
   Based on vo_directfb2.c

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
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
#include "video_out.h"
#include "video_out_internal.h"
#include "../postproc/rgb2rgb.h"

#define MPLAYER_VERSION 0.90
#define VO_GIF_REVISION 4

static vo_info_t info = {
	"animated GIF output",
	"gif89a",
	"Joey Parrish joey@nicewarrior.org",
	""
};

LIBVO_EXTERN(gif89a)

extern int verbose;
extern int vo_config_count;


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
// pointer for whole frame rendering
static uint8_t *frame_data = NULL;
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

static uint32_t preinit(const char *arg)
{
	float fps;
	if (verbose) printf("GIF89a: Preinit entered\n");
		
	target_fps = 0;
	fps = 0;
	
	if (arg) {
		char *temp = NULL;
		if (sscanf(arg, "%f", &fps)) {
			if (fps < 0) fps = 0;
			// find the next argument
			temp = strchr(arg, ':');
			if (temp != NULL) temp++;
		} else {
			// find the first argument
			temp = (char *)arg;
		}

		if (temp != NULL) {
			if (*temp != '\0') {
				gif_filename = strdup(temp);
			}
		}
	}

	if (fps > vo_fps)
		fps = vo_fps; // i will not duplicate frames.
	
	if (fps <= 0) {
		target_fps = default_fps;
		if (verbose)
			printf("GIF89a: default, %.2f fps\n", target_fps);
	} else {
		target_fps = fps;
		if (verbose)
			printf("GIF89a: output fps forced to %.2f\n", target_fps);
	}
	
	ideal_delay = 100 / target_fps; // in centiseconds
	frame_cycle = vo_fps / target_fps;
	// we make one output frame every (frame_cycle) frames, on average.
	
	if (gif_filename == NULL) {
		gif_filename = strdup(DEFAULT_FILE);
		if (verbose)
			printf("GIF89a: default, file \"%s\"\n", gif_filename);
	} else {
		if (verbose)
			printf("GIF89a: file forced to \"%s\"\n", gif_filename);
	}
	
	if (verbose)
		printf("GIF89a: Preinit OK\n");
	return 0;
}

static uint32_t config(uint32_t s_width, uint32_t s_height, uint32_t d_width,
		uint32_t d_height, uint32_t fullscreen, char *title,
		uint32_t format)
{
#ifdef HAVE_GIF_4
	// these are control blocks for the gif looping extension.
	char LB1[] = "NETSCAPE2.0";
	char LB2[] = { 1, 0, 0 };
#endif

	if (verbose) {
		printf("GIF89a: Config entered [%ix%i]\n",s_width,s_height);
		printf("GIF89a: With requested format: %s\n",vo_format_name(format));
	}
	
	// save these for later.
	img_width = s_width;
	img_height = s_height;

	// multiple configs without uninit are not allowed.
	// this is because config opens a new gif file.
	if (vo_config_count > 0) {
		if (verbose)
			printf("GIF89a: Reconfigure attempted.\n");
		return 0;
	}
	// reconfigure need not be a fatal error, so return 0.
	// multiple configs without uninit will result in two
	// movies concatenated in one gif file.  the output
	// gif will have the dimensions of the first movie.
	
	switch (format) {
		case IMGFMT_RGB24: break;     
		case IMGFMT_YV12:
			yuv2rgb_init(24, MODE_BGR);
			slice_data = malloc(img_width * img_height * 3);
			if (slice_data == NULL) {
				printf("GIF89a: malloc failed.\n");
				return 1;
			}
			break;
		default:
			printf("GIF89a: Error - given unsupported colorspace.\n");
			return 1;     
	}

	// the EGifSetGifVersion line causes segfaults in certain
	// earlier versions of libungif.  i don't know exactly which,
	// but certainly in all those before v4.  if you have problems,
	// you need to upgrade your gif library.
#ifdef HAVE_GIF_4
	EGifSetGifVersion("89a");
#else
	printf("GIF89a: Your version of libungif needs to be upgraded.\n");
	printf("GIF89a: Some functionality has been disabled.\n");
#endif
	
	new_gif = EGifOpenFileName(gif_filename, 0);
	if (new_gif == NULL) {
		printf("GIF89a: error opening file \"%s\" for output.\n", gif_filename);
		return 1;
	}

	reduce_data = malloc(img_width * img_height);
	if (reduce_data == NULL) {
		printf("GIF89a: malloc failed.\n");
		return 1;
	}

	reduce_cmap = MakeMapObject(256, NULL);
	if (reduce_cmap == NULL) {
		printf("GIF89a: malloc failed.\n");
		return 1;
	}
	
	// initialize the delay and framedrop variables.
	ideal_time = 0;
	real_time = 0;
	cycle_pos = 0;
	frame_adj = 0;

	// set the initial width and height info.
	EGifPutScreenDesc(new_gif, s_width, s_height, 256, 0, reduce_cmap);
#ifdef HAVE_GIF_4
	// version 3 of libungif does not support multiple control blocks.
	// looping requires multiple control blocks.
	// therefore, looping is only enabled for v4 and up.
	EGifPutExtensionFirst(new_gif, 0xFF, 11, LB1);
	EGifPutExtensionLast(new_gif, 0, 3, LB2);
#endif

	if (verbose)
		printf("GIF89a: Config finished.\n");
	return 0;
}

// we do not draw osd.
void draw_osd() {}

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
	uint8_t *img_data;

	cycle_pos++;
	if (cycle_pos < frame_cycle - frame_adj)
		return; // we are skipping this frame

	// slice_data is used for per slice rendering,
	// and frame_data is used for per frame rendering.
	// i seperated these two because slice_data is
	// ram i allocate, and frame_data is not.
	// using one pointer for both can lead to
	// either segfault (freeing ram that i'm not supposed
	// to) or memory leaks (not freeing any ram at all)
	if (slice_data != NULL) img_data = slice_data;
	else img_data = frame_data;
	
	// quantize the image
	ret = gif_reduce(img_width, img_height, img_data, reduce_data, reduce_cmap->Colors);
	if (ret == GIF_ERROR) {
		printf("GIF89a: Quantize failed.\n");
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

static uint32_t draw_frame(uint8_t *src[])
{
	frame_data = src[0];
	return 0;
}

static uint32_t draw_slice(uint8_t *src[], int stride[], int w, int h, int x, int y)
{
	uint8_t *dst;
	dst = slice_data + (img_width * y + x) * 3;
	yuv2rgb(dst, src[0], src[1], src[2], w, h, img_width * 3, stride[0], stride[1]);
	return 0;
}

static uint32_t query_format(uint32_t format)
{
	switch (format) {
		case IMGFMT_YV12:
			return VFCAP_CSP_SUPPORTED | VFCAP_TIMER | VFCAP_ACCEPT_STRIDE;
		case IMGFMT_RGB24:
			return VFCAP_CSP_SUPPORTED | VFCAP_CSP_SUPPORTED_BY_HW | VFCAP_TIMER;
	}
	return 0;
}

static uint32_t control(uint32_t request, void *data, ...)
{
	if (request == VOCTRL_QUERY_FORMAT) {
		return query_format(*((uint32_t*)data));
	}
	return VO_NOTIMPL;
}

static void uninit(void)
{
	if (verbose)
		printf("GIF89a: Uninit entered\n");
	
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
	frame_data = NULL;
	slice_data = NULL;
	reduce_data = NULL;
	reduce_cmap = NULL;
}

