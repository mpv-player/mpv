/* 
 * vo_dxr3.c - DXR3/H+ video out
 *
 * Copyright (C) 2002 David Holm <dholm@iname.com>
 *
 */

/* ChangeLog added 2002-01-10
 * 2002-02-09:
 *  Thanks to the new control() method I have finally been able to enable the em8300 prebuffering.
 *  This should speed up playback on all systems, the vout cpu usage should rocket since I will be hogging
 *  the pci bus. Not to worry though, since frames are prebuffered it should be able to take a few blows
 *  if you start doing other stuff simultaneously.
 *
 * 2002-02-03:
 *  Removal of libmp1e, libavcodec has finally become faster (and it's code is helluva lot cleaner)
 *
 * 2002-02-02:
 *  Cleaned out some old code which might have slowed down writes
 *
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

#ifdef USE_LIBAVCODEC
#ifdef USE_LIBAVCODEC_SO
#include <libffmpeg/avcodec.h>
#else
#include "libavcodec/avcodec.h"
#endif
/* for video encoder */
static AVCodec *avc_codec = NULL;
static AVCodecContext *avc_context = NULL;
static AVPicture avc_picture;
char *picture_buf = NULL;
char *avc_outbuf = NULL;
int avc_outbuf_size = 100000;
#endif

#ifdef HAVE_MMX
#include "mmx.h"
#endif

LIBVO_EXTERN (dxr3)

/* Resolutions and positions */
static int v_width, v_height;
static int s_width, s_height;
static int s_pos_x, s_pos_y;
static int d_pos_x, d_pos_y;
static int osd_w, osd_h;
static int noprebuf = 0;
static int img_format = 0;

/* File descriptors */
static int fd_control = -1;
static int fd_video = -1;
static int fd_spu = -1;
static char fdv_name[80];

/* Static variable used in ioctl's */
static int ioval = 0;

static vo_info_t vo_info = 
{
	"DXR3/H+ video out",
	"dxr3",
	"David Holm <dholm@iname.com>",
	""
};

uint32_t control(uint32_t request, void *data, ...)
{
	uint32_t flag = 0;
	switch (request) {
	case VOCTRL_RESET:
		if (!noprebuf) {
			close(fd_video);
			fd_video = open(fdv_name, O_WRONLY);
			fsync(fd_video);
		}
		return VO_TRUE;
	case VOCTRL_QUERY_FORMAT:
		switch (*((uint32_t*)data)) {	
		case IMGFMT_MPEGPES:
			/* Hardware accelerated | Hardware supports subpics */
			flag = 0x2 | 0x8;
			break;
#ifdef USE_LIBAVCODEC
		case IMGFMT_YV12:
		case IMGFMT_YUY2:
		case IMGFMT_RGB24:
		case IMGFMT_BGR24:
			/* Conversion needed | OSD Supported */
			flag = 0x1 | 0x4;
			break;
		default:
			printf("VO: [dxr3] Format unsupported, mail dholm@iname.com\n");
#else
		default:
			printf("VO: [dxr3] You have disabled libavcodec support (Read DOCS/codecs.html)!\n");
#endif
		}
		if (noprebuf) {
			return flag;
		} else {
			return (flag | 0x100);
		}
	}
	return VO_NOTIMPL;
}

static uint32_t config(uint32_t scr_width, uint32_t scr_height, uint32_t width, uint32_t height, uint32_t fullscreen, char *title, uint32_t format,const vo_tune_info_t *info)
{
	int tmp1, tmp2;
	em8300_register_t reg;
    
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
	
	/* Clean buffer by syncing it */
	ioval = EM8300_SUBDEVICE_VIDEO;
	ioctl(fd_control, EM8300_IOCTL_FLUSH, &ioval);
	ioval = EM8300_SUBDEVICE_AUDIO;
	ioctl(fd_control, EM8300_IOCTL_FLUSH, &ioval);
	fsync(fd_video);
	ioval = 0x900;
	ioctl(fd_control, EM8300_IOCTL_SCR_SETSPEED, &ioval);
	ioval = 0;
	ioctl(fd_control, EM8300_IOCTL_SCR_SET, &ioval);

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
    
	if (format != IMGFMT_MPEGPES) {
#ifdef USE_LIBAVCODEC
		int size;
		
		avc_codec = avcodec_find_encoder(CODEC_ID_MPEG1VIDEO);
		if (!avc_codec) {
			printf("VO: [dxr3] Unable to find mpeg1video codec\n");
			uninit();
			return -1;
		}
		avc_context = malloc(sizeof(AVCodecContext));
		memset(avc_context, 0, sizeof(avc_context));
		avc_context->width = v_width;
		avc_context->height = v_height;
		avc_context->frame_rate = 30 * FRAME_RATE_BASE;
		avc_context->gop_size = 12;
		avc_context->bit_rate = 8e6;
		avc_context->flags = CODEC_FLAG_HQ | CODEC_FLAG_QSCALE;
		avc_context->quality = 2;
		avc_context->pix_fmt = PIX_FMT_YUV420P;
		if (avcodec_open(avc_context, avc_codec) < 0) {
			printf("VO: [dxr3] Unable to open codec\n");
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

		/* Create a pixel buffer and set up pointers for color components */
		memset(&avc_picture, 0, sizeof(avc_picture));
		avc_picture.linesize[0] = v_width;
		avc_picture.linesize[1] = v_width / 2;
		avc_picture.linesize[2] = v_width / 2;
		avc_outbuf = malloc(avc_outbuf_size);

		size = s_width * s_height;
		picture_buf = malloc((size * 3) / 2);
		avc_picture.data[0] = picture_buf;
		avc_picture.data[1] = avc_picture.data[0] + size;
		avc_picture.data[2] = avc_picture.data[1] + size / 4;
		if (format == IMGFMT_BGR24) {
			yuv2rgb_init(24, MODE_BGR);
		} else {
			yuv2rgb_init(24, MODE_RGB);
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

static void draw_alpha(int x, int y, int w, int h, unsigned char* src, unsigned char *srca, int srcstride)
{
#ifdef USE_LIBAVCODEC
	vo_draw_alpha_yv12(w, h, src, srca, srcstride,
		avc_picture.data[0] + (x + d_pos_x) + (y + d_pos_y) * avc_picture.linesize[0], avc_picture.linesize[0]);
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
	ioctl(fd_video, EM8300_IOCTL_VIDEO_SETPTS, &vo_pts);
	if (img_format == IMGFMT_MPEGPES) {
		vo_mpegpes_t *p = (vo_mpegpes_t *) src[0];
		
		if (p->id == 0x20) {
			ioctl(fd_spu, EM8300_IOCTL_SPU_SETPTS, &vo_pts);
			write(fd_spu, p->data, p->size);
		} else {
			write(fd_video, p->data, p->size);
		}
		return 0;
#ifdef USE_LIBAVCODEC
	} else {
		int size = s_width * s_height;
		if (img_format == IMGFMT_YUY2) {
			yuy2toyv12(src[0], avc_picture.data[0], avc_picture.data[1], avc_picture.data[2],
				v_width, v_height, avc_picture.linesize[0], avc_picture.linesize[1], v_width*2);
		} else if (img_format == IMGFMT_BGR24) {
			rgb24toyv12(src[0], avc_picture.data[0], avc_picture.data[1], avc_picture.data[2],
				v_width, v_height, avc_picture.linesize[0], avc_picture.linesize[1], v_width*3);
		}
		draw_osd();
		size = avcodec_encode_video(avc_context, avc_outbuf, avc_outbuf_size, &avc_picture);
		write(fd_video, avc_outbuf, size);
		return 0;
#endif
	}
	return -1;
}

static void flip_page(void)
{
#ifdef USE_LIBAVCODEC
	if (img_format == IMGFMT_YV12) {
		int out_size = avcodec_encode_video(avc_context, avc_outbuf, avc_outbuf_size, &avc_picture);
		ioctl(fd_video, EM8300_IOCTL_VIDEO_SETPTS, &vo_pts);
		write(fd_video, avc_outbuf, out_size);
	}
#endif
}

static uint32_t draw_slice(uint8_t *srcimg[], int stride[], int w, int h, int x0, int y0)
{
#ifdef USE_LIBAVCODEC
	if (img_format == IMGFMT_YV12) {
		int y;
		unsigned char *s, *s1;
		unsigned char *d, *d1;

		x0 += d_pos_x;
		y0 += d_pos_y;

		if ((x0 + w) > avc_picture.linesize[0]) {
			w = avc_picture.linesize[0] - x0;
		}
		if ((y0 + h) > s_height) {
			h = s_height - y0;
		}

		s = srcimg[0] + s_pos_x + s_pos_y * stride[0];
		d = avc_picture.data[0] + x0 + y0 * avc_picture.linesize[0];
		for(y = 0; y < h; y++) {
			memcpy(d, s, w);
			s += stride[0];
			d += avc_picture.linesize[0];
		}

		w /= 2;
		h /= 2;
		x0 /= 2;
		y0 /= 2;
	
		s = srcimg[1] + s_pos_x + (s_pos_y * stride[1]);
		d = avc_picture.data[1] + x0 + (y0 * avc_picture.linesize[1]);
		s1 = srcimg[2] + s_pos_x + (s_pos_y * stride[2]);
		d1 = avc_picture.data[2] + x0 + (y0 * avc_picture.linesize[2]);
		for(y = 0; y < h; y++) {
			memcpy(d, s, w);
			memcpy(d1, s1, w);
			s += stride[1];
			s1 += stride[2];
			d += avc_picture.linesize[1];
			d1 += avc_picture.linesize[2];
		}
		return 0;
	}
#endif
	return -1;
}

static void uninit(void)
{
	printf("VO: [dxr3] Uninitializing\n");
#ifdef USE_LIBAVCODEC
	if (avc_context) {
		avcodec_close(avc_context);
	}
	if (picture_buf) {
		free(picture_buf);
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

static uint32_t preinit(const char *arg)
{
	char devname[80];
	int fdflags = O_WRONLY;
	
	/* Open the control interface */
	if (vo_subdevice) {
		if (!strcmp("noprebuf", vo_subdevice)) {
			printf("VO: [dxr3] Disabling prebuffering.\n");
			noprebuf = 1;
			fdflags |= O_NONBLOCK;
		} else {
			printf("VO: [dxr3] Forcing use of device %s\n", vo_subdevice);
			sprintf(devname, "/dev/em8300-%s", vo_subdevice);
		}
	} else {
		/* Try new naming scheme by default */
		sprintf(devname, "/dev/em8300-0");
	}
	fd_control = open(devname, fdflags);
	if (fd_control < 1) {
		/* Fall back to old naming scheme */
		printf("VO: [dxr3] Error opening %s for writing, trying /dev/em8300 instead\n", devname);
		sprintf(devname, "/dev/em8300");
		fd_control = open(devname, fdflags);
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
	fd_video = open(devname, fdflags);
	if (fd_video < 0) {
		/* Fall back to old naming scheme */
		printf("VO: [dxr3] Error opening %s for writing, trying /dev/em8300_mv instead\n", devname);
		sprintf(devname, "/dev/em8300_mv");
		fd_video = open(devname, fdflags);
		if (fd_video < 0) {
			printf("VO: [dxr3] Error opening /dev/em8300_mv for writing as well!\nBailing\n");
			uninit();
			return -1;
		}
	} else {
		printf("VO: [dxr3] Opened %s\n", devname);
	}
	strcpy(fdv_name, devname);
	
	/* Open the subpicture interface */
	if (vo_subdevice) {
		sprintf(devname, "/dev/em8300_sp-%s", vo_subdevice);
	} else {
		/* Try new naming scheme by default */
		sprintf(devname, "/dev/em8300_sp-0");
	}
	fd_spu = open(devname, fdflags);
	if (fd_spu < 0) {
		/* Fall back to old naming scheme */
		printf("VO: [dxr3] Error opening %s for writing, trying /dev/em8300_sp instead\n", devname);
		sprintf(devname, "/dev/em8300_sp");
		fd_spu = open(devname, fdflags);
		if (fd_spu < 0) {
			printf("VO: [dxr3] Error opening /dev/em8300_sp for writing as well!\nBailing\n");
			uninit();
			return -1;
		}
	}

#ifdef USE_LIBAVCODEC
	avcodec_init();
	avcodec_register_all();
#endif
	
	return 0;
}
