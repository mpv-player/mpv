/* 
 * vo_dxr3.c - DXR3/H+ video out
 *
 * Copyright (C) 2002 David Holm <dholm@iname.com>
 *
 */

/* ChangeLog added 2002-01-10
 * 2002-01-17:
 *  Testrelease of new sync engine (using previously undocumented feature of em8300).
 *
 * 2002-01-15:
 *  Preliminary subpic support with -vc mpegpes and dvd's
 *  Device interfaces tries the new naming scheme by default (even though most users probably still use the old one)
 *
 * 2002-01-10:
 *  I rehauled the entire codebase. I have now changed to
 *  Kernighan & Ritchie codingstyle, please mail me if you 
 *  find any inconcistencies.
 */
 
#include <linux/em8300.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <time.h>

#include "config.h"
#include "fastmemcpy.h"

#include "video_out.h"
#include "video_out_internal.h"
#include "../postproc/rgb2rgb.h"

#ifdef USE_MP1E
#include "../libmp1e/libmp1e.h"
#endif

#ifdef HAVE_MMX
#include "mmx.h"
#endif

LIBVO_EXTERN (dxr3)

#ifdef USE_MP1E
/* libmp1e specific stuff */
static rte_context *mp1e_context = NULL;
static rte_codec *mp1e_codec = NULL;
static rte_buffer mp1e_buffer;

/* Color buffer data used with libmp1e */
static unsigned char *picture_data[3];
static int picture_linesize[3];
#endif

/* Resolutions and positions */
static int v_width, v_height;
static int s_width, s_height;
static int s_pos_x, s_pos_y;
static int d_pos_x, d_pos_y;
static int osd_w, osd_h;

static int img_format = 0;

/* File descriptors */
static int fd_control = -1;
static int fd_video = -1;
static int fd_spu = -1;

/* Static variable used in ioctl's */
static int ioval = 0;
static int pts = 0;

static vo_info_t vo_info = 
{
	"DXR3/H+ video out",
	"dxr3",
	"David Holm <dholm@iname.com>",
	""
};

#ifdef USE_MP1E
void write_dxr3(rte_context *context, void *data, size_t size, void *user_data)
{
	size_t data_left = size;
	/* Set the timestamp of the next video packet */
	if (ioctl(fd_video, EM8300_IOCTL_VIDEO_SETPTS, &pts) < 0) {
		printf("VO: [dxr3] Unable to set pts\n");
	}
	
	/* Force data into the buffer */
	while (data_left) {
		data_left -= write(fd_video, (void*) data + (size - data_left), data_left);
	}
}
#endif

static uint32_t init(uint32_t scr_width, uint32_t scr_height, uint32_t width, uint32_t height, uint32_t fullscreen, char *title, uint32_t format)
{
	int tmp1, tmp2;
	em8300_register_t reg;
	char devname[80];
	
	/* Open the control interface */
	if (vo_subdevice) {
		sprintf(devname, "/dev/em8300-%s", vo_subdevice);
	} else {
		/* Try new naming scheme by default */
		sprintf(devname, "/dev/em8300-0");
	}
	fd_control = open(devname, O_WRONLY);
	if (fd_control < 1) {
		/* Fall back to old naming scheme */
		printf("VO: [dxr3] Error opening %s for writing, trying /dev/em8300 instead\n", devname);
		sprintf(devname, "/dev/em8300");
		fd_control = open(devname, O_WRONLY);
		if (fd_control < 1) {
			printf("VO: [dxr3] Error opening /dev/em8300 for writing as well!\nBailing\n");
			return -1;
		}
	}

	/* Open the video interface */
	if (vo_subdevice) {
		sprintf(devname, "/dev/em8300_mv-%s", vo_subdevice);
	} else {
		/* Try new naming scheme by default */
		sprintf(devname, "/dev/em8300_mv-0");
	}
	fd_video = open(devname, O_WRONLY);
	if (fd_video < 0) {
		/* Fall back to old naming scheme */
		printf("VO: [dxr3] Error opening %s for writing, trying /dev/em8300_mv instead\n", devname);
		sprintf(devname, "/dev/em8300_mv");
		fd_video = open(devname, O_WRONLY);
		if (fd_video < 0) {
			printf("VO: [dxr3] Error opening /dev/em8300_mv for writing as well!\nBailing\n");
			uninit();
			return -1;
		}
	} else {
		printf("VO: [dxr3] Opened %s\n", devname);
	}
	
	/* Open the subpicture interface */
	if (vo_subdevice) {
		sprintf(devname, "/dev/em8300_sp-%s", vo_subdevice);
	} else {
		/* Try new naming scheme by default */
		sprintf(devname, "/dev/em8300_sp-0");
	}
	fd_spu = open(devname, O_WRONLY);
	if (fd_spu < 0) {
		/* Fall back to old naming scheme */
		printf("VO: [dxr3] Error opening %s for writing, trying /dev/em8300_sp instead\n", devname);
		sprintf(devname, "/dev/em8300_sp");
		fd_spu = open(devname, O_WRONLY);
		if (fd_spu < 0) {
			printf("VO: [dxr3] Error opening /dev/em8300_sp for writing as well!\nBailing\n");
			uninit();
			return -1;
		}
	}
    
	/* This activates the subpicture processor, you can safely disable this and still send */
	/* broken subpics to the em8300, if it's enabled and you send broken subpics you will end */
	/* up in a lockup */
	ioval = EM8300_SPUMODE_ON;
	if (ioctl(fd_control, EM8300_IOCTL_SET_SPUMODE, &ioval) < 0) {
		printf("VO: [dxr3] Unable to set subpicture mode!\n");
		uninit();
		return -1;
	}

	/* Set the playmode to play (just in case another app has set it to something else) */
	ioval = EM8300_PLAYMODE_PLAY;
	if (ioctl(fd_control, EM8300_IOCTL_SET_PLAYMODE, &ioval) < 0) {
		printf("VO: [dxr3] Unable to set playmode!\n");
	}
	
	/* Start em8300 prebuffering and sync engine */
	reg.microcode_register = 1;
	reg.reg = 0;
	reg.val = MVCOMMAND_SYNC;
	ioctl(fd_control, EM8300_IOCTL_WRITEREG, &reg);

	/* Store some variables statically that we need later in another scope */
	img_format = format;
	v_width = scr_width;
	v_height = scr_height;

	/* libmp1e requires a width and height that is x|16 */
	s_width = (v_width + 15) / 16;
	s_width *= 16;
	s_height = (v_height + 15) / 16;
	s_height *= 16;
    
	/* Try to figure out whether to use widescreen output or not */
	/* Anamorphic widescreen modes makes this a pain in the ass */
	tmp1 = abs(height - ((width / 4) * 3));
	tmp2 = abs(height - (int) (width / 2.35));
	if (tmp1 < tmp2) {
		ioval = EM8300_ASPECTRATIO_4_3;
		printf("VO: [dxr3] Setting aspect ratio to 4:3\n");
	} else {
		ioval = EM8300_ASPECTRATIO_16_9;
		printf("VO: [dxr3] Setting aspect ratio to 16:9\n");
	}
	ioctl(fd_control, EM8300_IOCTL_SET_ASPECTRATIO, &ioval);
    
	if (format == IMGFMT_YV12 || format == IMGFMT_YUY2 || format == IMGFMT_BGR24) {
#ifdef USE_MP1E
		int size;
		enum rte_frame_rate frame_rate;
		enum rte_pixformat pixel_format;

		/* Here follows initialization of libmp1e specific stuff */
		if (!rte_init()) {
			printf("VO: [dxr3] Unable to initialize MP1E!\n");
			uninit();
			return -1;
		}
	
		mp1e_context = rte_context_new(s_width, s_height, NULL);
		rte_set_verbosity(mp1e_context, 0);
	
		printf("VO: [dxr3] %dx%d => %dx%d\n", v_width, v_height, s_width, s_height);

		if (!mp1e_context) {
			printf( "VO: [dxr3] Unable to create context!\n" );
			uninit();
			return -1;
		}
	
		/* I wonder if we could benefit from using mpeg2 em8300-wise (hardware processing) */
		if (!rte_set_format(mp1e_context, "mpeg1")) {
			printf("VO: [dxr3] Unable to set format\n");
			uninit();
			return -1;
		}

		rte_set_mode(mp1e_context, RTE_VIDEO);
		mp1e_codec = rte_codec_set(mp1e_context, RTE_STREAM_VIDEO, 0, "mpeg1-video");

		if (vo_fps < 24.0) {
			frame_rate = RTE_RATE_1;
		} else if (vo_fps < 25.0) {
			frame_rate = RTE_RATE_2;
		} else if (vo_fps < 29.97) {
			frame_rate = RTE_RATE_3;
		} else if (vo_fps < 30.0) {
			frame_rate = RTE_RATE_4;
		} else if (vo_fps < 50.0) {
			frame_rate = RTE_RATE_5;
		} else if (vo_fps < 59.97) {
			frame_rate = RTE_RATE_6;
		} else if (vo_fps < 60.0) {
			frame_rate = RTE_RATE_7;
		} else if (vo_fps > 60.0) {
			frame_rate = RTE_RATE_8;
		} else {
			frame_rate = RTE_RATE_NORATE;
		}

		if (format == IMGFMT_YUY2) {
			pixel_format = RTE_YUYV;
		} else {
			pixel_format = RTE_YUV420;
		}
		if (!rte_set_video_parameters(mp1e_context, pixel_format, mp1e_context->width, mp1e_context->height, frame_rate, 3e6, "I")) {
			printf("VO: [dxr3] Unable to set mp1e context!\n");
			rte_context_destroy(mp1e_context);
			mp1e_context = 0;
			uninit();
			return -1;
		}
	
		rte_set_input(mp1e_context, RTE_VIDEO, RTE_PUSH, TRUE, NULL, NULL, NULL);
		rte_set_output(mp1e_context, (void*) write_dxr3, NULL, NULL);
	
		if (!rte_init_context(mp1e_context)) {
			printf("VO: [dxr3] Unable to init mp1e context!\n");
			uninit();
			return -1;
		}

		/* This stuff calculations the relative position of video and osd on screen */
		/* Old stuff taken from the dvb driver, should be removed when introducing spuenc */
		osd_w = s_width;
		d_pos_x = (s_width - v_width) / 2;
		if (d_pos_x < 0) {
			s_pos_x = -d_pos_x;
			d_pos_x = 0;
			osd_w = s_width;
		} else {
			s_pos_x = 0;
		}
		osd_h = s_height;
		d_pos_y = (s_height - v_height) / 2;
		if (d_pos_y < 0) {
			s_pos_y = -d_pos_y;
			d_pos_y = 0;
			osd_h = s_height;
		} else {
			s_pos_y = 0;
		}
                
		size = s_width * s_height;

		if (format == IMGFMT_YUY2) {
			/* YUY2 Needs no conversion, so no need for a pixel buffer */
			picture_data[0] = NULL;
			picture_linesize[0] = s_width * 2;
		} else {
			/* Create a pixel buffer and set up pointers for color components */
			picture_data[0] = malloc((size * 3)/2);
			picture_data[1] = picture_data[0] + size;
			picture_data[2] = picture_data[1] + size / 4;
			picture_linesize[0] = s_width;
			picture_linesize[1] = s_width / 2;
			picture_linesize[2] = s_width / 2;
		}

		if (!rte_start_encoding(mp1e_context)) {
			printf("VO: [dxr3] Unable to start mp1e encoding!\n");
			uninit();
			return -1;
		}

		if (format == IMGFMT_BGR24) {
			yuv2rgb_init(24, MODE_BGR);
		}
		return 0;
#endif
		return -1;
	} else if (format == IMGFMT_MPEGPES) {
		printf("VO: [dxr3] Format: MPEG-PES (no conversion needed)\n");
		return 0;
	}

	printf("VO: [dxr3] Format: Unsupported\n");
	uninit();
	return -1;
}

static const vo_info_t* get_info(void)
{
	return &vo_info;
}

static void draw_alpha(int x0, int y0, int w, int h, unsigned char* src, unsigned char *srca, int srcstride)
{
#ifdef USE_MP1E
	/* This function draws the osd and subtitles etc. It will change to use spuenc soon */
	switch (img_format) {
	case IMGFMT_BGR24:
	case IMGFMT_YV12:
		vo_draw_alpha_yv12(w, h, src, srca, srcstride,
			picture_data[0] + (x0 + d_pos_x) + (y0 + d_pos_y) * picture_linesize[0], picture_linesize[0]);
		break;
	case IMGFMT_YUY2:
		vo_draw_alpha_yuy2(w, h, src, srca, srcstride,
			picture_data[0] + (x0 + d_pos_x) * 2 + (y0 + d_pos_y) * picture_linesize[0], picture_linesize[0]);
		break;
	}
#endif
}

static void draw_osd(void)
{
	if (img_format != IMGFMT_MPEGPES) {
		vo_draw_text(osd_w, osd_h, draw_alpha);
	}
}

static uint32_t draw_frame(uint8_t * src[])
{
	if (img_format == IMGFMT_MPEGPES) {
		vo_mpegpes_t *p = (vo_mpegpes_t *) src[0];
		size_t data_left = p->size;

		if (p->id == 0x20) {
			/* Set subpic timestamp */
			if (ioctl(fd_spu, EM8300_IOCTL_SPU_SETPTS, &pts) < 0) {
				printf("VO: [dxr3] Unable to set pts\n");
			}

			/* Force subpic data into buffer */
			while (data_left) {
				data_left -= write(fd_spu, (void*) (p->data + p->size-data_left), data_left);
			}
		} else {
			/* Set frame timestamp */
			if (ioctl(fd_video, EM8300_IOCTL_VIDEO_SETPTS, &pts) < 0) {
				printf("VO: [dxr3] Unable to set pts\n");
			}

			/* Force video data into buffer */
			while (data_left) {
				data_left -= write(fd_video, (void*) (p->data + p->size-data_left), data_left);
			}
		}
		return 0;
	}
#ifdef USE_MP1E
	else if (img_format == IMGFMT_YUY2) {
		picture_data[0] = src[0];
		return 0;
	} else if (img_format == IMGFMT_BGR24) {
		/* BGR24 needs to be converted to YUV420 before libmp1e will touch it */
		int x, y, w = v_width, h = v_height;
		unsigned char *s, *dY, *dU, *dV;
	
		if ((d_pos_x + w) > picture_linesize[0]) {
			w = picture_linesize[0] - d_pos_x;
		}
		if ((d_pos_y + h) > s_height) {
			h = s_height - d_pos_y;
		}

		s = src[0] + s_pos_y * (w * 3);

		dY = picture_data[0] + d_pos_y * picture_linesize[0];
		dU = picture_data[1] + (d_pos_y / 2) * picture_linesize[1];
		dV = picture_data[2] + (d_pos_y / 2) * picture_linesize[2];
	
		rgb24toyv12(s, dY, dU, dV, w, h, picture_linesize[0], picture_linesize[1], v_width * 3);
	
		mp1e_buffer.data = picture_data[0];
		mp1e_buffer.time = pts / 90000.0;
		mp1e_buffer.user_data = NULL;
		vo_draw_text(osd_w, osd_h, draw_alpha);
		rte_push_video_buffer(mp1e_context, &mp1e_buffer);
		return 0;
	}
#endif
	return -1;
}

static void flip_page(void)
{
	static int prev_pts = 0;
	/* Flush the device if a seek occured */
	if (prev_pts > pts) {
		printf("Seek\n");
		ioval = EM8300_SUBDEVICE_VIDEO;
		ioctl(fd_control, EM8300_IOCTL_FLUSH, &ioval);
		pts += 90000.0 / vo_fps;
		ioctl(fd_control, EM8300_IOCTL_SCR_SET, &pts);
		if (ioctl(fd_video, EM8300_IOCTL_VIDEO_SETPTS, &pts) < 0) {
			printf("VO: [dxr3] Unable to set pts\n");
		}
	}
	prev_pts = pts;
#ifdef USE_MP1E
	if (img_format == IMGFMT_YV12) {
		mp1e_buffer.data = picture_data[0];
		mp1e_buffer.time = pts / 90000.0;
		mp1e_buffer.user_data = NULL;
		rte_push_video_buffer(mp1e_context, &mp1e_buffer);
	} else if (img_format == IMGFMT_YUY2) {
		mp1e_buffer.data = picture_data[0];
		mp1e_buffer.time = pts / 90000.0;
		mp1e_buffer.user_data = NULL;
		rte_push_video_buffer(mp1e_context, &mp1e_buffer);
	}
#endif
	pts += 90000.0 / vo_fps;
}

static uint32_t draw_slice(uint8_t *srcimg[], int stride[], int w, int h, int x0, int y0)
{
#ifdef USE_MP1E
	if (img_format == IMGFMT_YV12) {
		int y;
		unsigned char *s, *s1;
		unsigned char *d, *d1;

		x0 += d_pos_x;
		y0 += d_pos_y;

		if ((x0 + w) > picture_linesize[0]) {
			w = picture_linesize[0] - x0;
		}
		if ((y0 + h) > s_height) {
			h = s_height - y0;
		}

		s = srcimg[0] + s_pos_x + s_pos_y * stride[0];
		d = picture_data[0] + x0 + y0 * picture_linesize[0];
		for(y = 0; y < h; y++) {
			memcpy(d, s, w);
			s += stride[0];
			d += picture_linesize[0];
		}

		w /= 2;
		h /= 2;
		x0 /= 2;
		y0 /= 2;
	
		s = srcimg[1] + s_pos_x + (s_pos_y * stride[1]);
		d = picture_data[1] + x0 + (y0 * picture_linesize[1]);
		s1 = srcimg[2] + s_pos_x + (s_pos_y * stride[2]);
		d1 = picture_data[2] + x0 + (y0 * picture_linesize[2]);
		for(y = 0; y < h; y++) {
			memcpy(d, s, w);
			memcpy(d1, s1, w);
			s += stride[1];
			s1 += stride[2];
			d += picture_linesize[1];
			d1 += picture_linesize[2];
		}
		return 0;
	}
#endif
	return -1;
}

static uint32_t query_format(uint32_t format)
{
	uint32_t flag = 0;
	
	if (format == IMGFMT_MPEGPES) {
		/* Hardware accelerated | Hardware supports subpics | Hardware handles syncing */
		flag = 0x2 | 0x8 | 0x100;
#ifdef USE_MP1E
	} else if (format == IMGFMT_YV12) {
		/* Conversion needed | OSD Supported | Hardware handles syncing */
		flag = 0x1 | 0x4 | 0x100;
	} else if (format == IMGFMT_YUY2) {
		/* Conversion needed | OSD Supported | Hardware handles syncing */
		flag = 0x1 | 0x4 | 0x100;
	} else if (format == IMGFMT_BGR24) {
		/* Conversion needed | OSD Supported | Hardware handles syncing */
		flag = 0x1 | 0x4 | 0x100;
	} else {
		printf("VO: [dxr3] Format unsupported, mail dholm@iname.com\n");
#else
	} else {
		printf("VO: [dxr3] You have disabled libmp1e support, you won't be able to play this format!\n");
#endif
	}
	return flag;
}

static void uninit(void)
{
	printf("VO: [dxr3] Uninitializing\n");
#ifdef USE_MP1E
	if (picture_data[0]) {
		free(picture_data[0]);
	}
#endif
	if (fd_video) {
		close(fd_video);
	}
	if (fd_spu) {
		close(fd_spu);
	}
	if (fd_control) {
		close(fd_control);
	}
}

static void check_events(void)
{
}
