/* 
 * vo_dxr3.c - DXR3/H+ video out
 *
 * Copyright (C) 2002 David Holm <dholm@iname.com>
 *
 */

/* ChangeLog added 2002-01-10
 * 2002-03-16:
 *  Fixed problems with fame, it gives a better picture than avcodec,
 *  but is slightly slower. Most notably the wobbling effect is gone
 *  with fame.
 *
 * 2002-03-13:
 *  Preliminary fame support added (it breaks after seeking, why?)
 *
 * 2002-02-18:
 *  Fixed sync problems when pausing video (while using prebuffering)
 *
 * 2002-02-16:
 *  Fixed bug which would case invalid output when using :noprebuf
 *  Removed equalization code, it caused problems on slow systems
 *
 * 2002-02-13:
 *  Using the swscaler instead of the old hand coded shit. (Checkout man mplayer and search for sws ;).
 *  Using aspect function to setup a proper mpeg1, no more hassling with odd resolutions or GOP-sizes,
 *  this would only create jitter on some vids!
 *  The swscaler sometimes exits with sig8 on mpegs, I don't know why yet (just use -vc mpegpes in this
 *  case, and report to me if you have any avi's etc which does this...)
 *
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
#include <sys/select.h>
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
#include "aspect.h"
#include "../postproc/rgb2rgb.h"
#include "../postproc/swscale.h"

/*#ifndef USE_LIBAVCODEC*/
#  define USE_LIBFAME
/*#else
#  undef USE_LIBFAME
#endif*/
#ifdef USE_LIBFAME
#include "../libfame/fame.h"
static unsigned char *outbuf = NULL;
static fame_parameters_t fame_params;
static fame_yuv_t fame_yuv;
static fame_context_t *fame_ctx = NULL;
static fame_object_t *fame_obj;
#elif USE_LIBAVCODEC
#ifdef USE_LIBAVCODEC_SO
#include <libffmpeg/avcodec.h>
#else
#include "libavcodec/avcodec.h"
#endif
/* for video encoder */
static AVCodec *avc_codec = NULL;
static AVCodecContext *avc_context = NULL;
static AVPicture avc_picture;
int avc_outbuf_size = 100000;
#endif

char *picture_data[] = { NULL, NULL, NULL };
int picture_linesize[] = { 0, 0, 0 };

#ifdef HAVE_MMX
#include "mmx.h"
#endif

LIBVO_EXTERN (dxr3)

/* Resolutions and positions */
static int v_width, v_height;
static int s_width, s_height;
static int osd_w, osd_h;
static int noprebuf = 0;
static int img_format = 0;
static SwsContext * sws = NULL;

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
	case VOCTRL_RESUME:
		if (!noprebuf) {
			ioval = EM8300_PLAYMODE_PLAY;
			if (ioctl(fd_control, EM8300_IOCTL_SET_PLAYMODE, &ioval) < 0) {
				printf("VO: [dxr3] Unable to set playmode!\n");
			}
		}
		return VO_TRUE;
	case VOCTRL_PAUSE:
		if (!noprebuf) {
			ioval = EM8300_PLAYMODE_PAUSED;
			if (ioctl(fd_control, EM8300_IOCTL_SET_PLAYMODE, &ioval) < 0) {
				printf("VO: [dxr3] Unable to set playmode!\n");
			}
		}
		return VO_TRUE;
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
#if defined(USE_LIBFAME) || defined(USE_LIBAVCODEC)
		case IMGFMT_YV12:
		case IMGFMT_YUY2:
		case IMGFMT_RGB24:
		case IMGFMT_BGR24:
			/* Conversion needed | OSD Supported */
			flag = 0x1 | 0x4;
			break;
#else
		default:
			printf("VO: [dxr3] You have disabled libavcodec/libfame support (Read DOCS/codecs.html)!\n");
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

static uint32_t config(uint32_t width, uint32_t height, uint32_t d_width, uint32_t d_height, uint32_t fullscreen, char *title, uint32_t format, const vo_tune_info_t *info)
{
	int tmp1, tmp2, size;
	em8300_register_t reg;
    
	/* Softzoom turned on, downscale */
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
	if (!noprebuf) {
		ioval = 0x900;
		ioctl(fd_control, EM8300_IOCTL_SCR_SETSPEED, &ioval);
		ioval = 0;
		ioctl(fd_control, EM8300_IOCTL_SCR_SET, &ioval);
	}

	/* Store some variables statically that we need later in another scope */
	img_format = format;
	v_width = width;
	v_height = height;

	/* libavcodec requires a width and height that is x|16 */
	aspect_save_orig(width, height);
	aspect_save_prescale(d_width, d_height);
	ioctl(fd_control, EM8300_IOCTL_GET_VIDEOMODE, &ioval);
	if (ioval == EM8300_VIDEOMODE_NTSC) {
		printf("VO: [dxr3] Setting up for NTSC.\n");
		aspect_save_screenres(352, 240);
	} else {
		printf("VO: [dxr3] Setting up for PAL/SECAM.\n");
		aspect_save_screenres(352, 288);
	}
	aspect(&s_width, &s_height, A_ZOOM);
	s_width -= s_width % 16;
	s_height -= s_height % 16;
	
	/* Try to figure out whether to use widescreen output or not */
	/* Anamorphic widescreen modes makes this a pain in the ass */
	tmp1 = abs(d_height - ((d_width / 4) * 3));
	tmp2 = abs(d_height - (int) (d_width / 2.35));
	if (tmp1 < tmp2) {
		ioval = EM8300_ASPECTRATIO_4_3;
		printf("VO: [dxr3] Setting aspect ratio to 4:3\n");
	} else {
		ioval = EM8300_ASPECTRATIO_16_9;
		printf("VO: [dxr3] Setting aspect ratio to 16:9\n");
	}
	ioctl(fd_control, EM8300_IOCTL_SET_ASPECTRATIO, &ioval);
	
	if (format != IMGFMT_MPEGPES) {
		size = s_width * s_height;
		picture_data[0] = malloc((size * 3) / 2);
		picture_data[1] = picture_data[0] + size;
		picture_data[2] = picture_data[1] + size / 4;
		
		picture_linesize[0] = s_width;
		picture_linesize[1] = s_width / 2;
		picture_linesize[2] = s_width / 2;
#ifdef USE_LIBFAME
		fame_ctx = fame_open();
		if (!fame_ctx) {
			printf("VO: [dxr3] Cannot open libFAME!\n");
			return -1;
		}

		fame_obj = fame_get_object(fame_ctx, "motion/none");
		fame_register(fame_ctx, "motion", fame_obj);
		
		fame_params.width = s_width;
		fame_params.height = s_height;
		fame_params.coding = "IPPPPPPP";
		fame_params.quality = 90;
		fame_params.bitrate = 0;
		fame_params.slices_per_frame = 1;
		fame_params.frames_per_sequence = (int) (vo_fps + 0.5);
		fame_params.shape_quality = 100;
		fame_params.search_range = (int) (vo_fps + 0.5);
		fame_params.verbose = 0;
		fame_params.profile = NULL;
		
		if (vo_fps < 24.0) {
		    fame_params.frame_rate_num = 24000;
		    fame_params.frame_rate_den = 1001;
		} else if (vo_fps < 25.0) {
		    fame_params.frame_rate_num = 24;
		    fame_params.frame_rate_den = 1;
		} else if (vo_fps < 29.0) {
		    fame_params.frame_rate_num = 25;
		    fame_params.frame_rate_den = 1;
		} else if (vo_fps < 30.0) {
		    fame_params.frame_rate_num = 30000;
		    fame_params.frame_rate_den = 1001;
		} else if (vo_fps < 50.0) {
		    fame_params.frame_rate_num = 30;
		    fame_params.frame_rate_den = 1;
		} else if (vo_fps < 55.0) {
		    fame_params.frame_rate_num = 50;
		    fame_params.frame_rate_den = 1;
		} else if (vo_fps < 60.0) {
		    fame_params.frame_rate_num = 60000;
		    fame_params.frame_rate_den = 1001;
		} else {
		    fame_params.frame_rate_num = 60;
		    fame_params.frame_rate_den = 1;
		}
		
		outbuf = malloc(100000);
		fame_init(fame_ctx, &fame_params, outbuf, 100000);

		fame_yuv.w = s_width;
		fame_yuv.h = s_height;
		fame_yuv.y = picture_data[0];
		fame_yuv.u = picture_data[1];
		fame_yuv.v = picture_data[2];
#elif USE_LIBAVCODEC
		avc_codec = avcodec_find_encoder(CODEC_ID_MPEG1VIDEO);
		if (!avc_codec) {
			printf("VO: [dxr3] Unable to find mpeg1video codec\n");
			uninit();
			return -1;
		}
		avc_context = malloc(sizeof(AVCodecContext));
		memset(avc_context, 0, sizeof(avc_context));
		avc_context->width = s_width;
		avc_context->height = s_height;
		ioctl(fd_control, EM8300_IOCTL_GET_VIDEOMODE, &ioval);
		if (ioval == EM8300_VIDEOMODE_NTSC) {
			avc_context->gop_size = 18;
		} else {
			avc_context->gop_size = 15;
		}
		avc_context->frame_rate = (int) vo_fps * FRAME_RATE_BASE;
		avc_context->bit_rate = 6e6;
		avc_context->flags = CODEC_FLAG_HQ | CODEC_FLAG_QSCALE;
		avc_context->quality = 2;
		avc_context->pix_fmt = PIX_FMT_YUV420P;
		if (avcodec_open(avc_context, avc_codec) < 0) {
			printf("VO: [dxr3] Unable to open codec\n");
			uninit();
			return -1;
		}
		/* Create a pixel buffer and set up pointers for color components */
		memset(&avc_picture, 0, sizeof(avc_picture));
		avc_picture.linesize[0] = picture_linesize[0];
		avc_picture.linesize[1] = picture_linesize[1];
		avc_picture.linesize[2] = picture_linesize[2];
		
		avc_picture.data[0] = picture_data[0];
		avc_picture.data[1] = picture_data[1];
		avc_picture.data[2] = picture_data[2];
#endif
		sws = getSwsContextFromCmdLine(v_width, v_height, img_format, s_width, s_height, IMGFMT_YV12);
		if (!sws) {
			printf("vo_vesa: Can't initialize SwScaler\n");
			return -1;
		}
	
		/* This stuff calculations the relative position of the osd */
		osd_w = s_width;
		osd_h = s_height;

		if (format == IMGFMT_BGR24) {
			yuv2rgb_init(24, MODE_BGR);
		} else {
			yuv2rgb_init(24, MODE_RGB);
		}
		return 0;
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
#if defined(USE_LIBFAME) || defined(USE_LIBAVCODEC)
	vo_draw_alpha_yv12(w, h, src, srca, srcstride,
		picture_data[0] + x + y * picture_linesize[0], picture_linesize[0]);
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
		
		if (p->id == 0x20) {
			if (!noprebuf) {
				ioctl(fd_spu, EM8300_IOCTL_SPU_SETPTS, &vo_pts);
			}
			write(fd_spu, p->data, p->size);
		} else {
			write(fd_video, p->data, p->size);
		}
		return 0;
	} else {
		int size, srcStride = (img_format == IMGFMT_YUY2) ? (v_width * 2) : (v_width * 3);
		sws->swScale(sws, src, &srcStride, 0, v_height, picture_data, picture_linesize);
		draw_osd();
#ifdef USE_LIBFAME
		size = fame_encode_frame(fame_ctx, &fame_yuv, NULL);
		write(fd_video, outbuf, size);
#elif USE_LIBAVCODEC
		size = avcodec_encode_video(avc_context, picture_data[0], avc_outbuf_size, &avc_picture);
		write(fd_video, picture_data[0], size);
#endif
		return 0;
	}
	return -1;
}

static void flip_page(void)
{
	if (!noprebuf) {
		ioctl(fd_video, EM8300_IOCTL_VIDEO_SETPTS, &vo_pts);
	}
	if (img_format == IMGFMT_YV12) {
#ifdef USE_LIBFAME
		int size = fame_encode_frame(fame_ctx, &fame_yuv, NULL);
		write(fd_video, outbuf, size);
#elif USE_LIBAVCODEC
		int size = avcodec_encode_video(avc_context, picture_data[0], avc_outbuf_size, &avc_picture);
		if (!noprebuf) {
			ioctl(fd_video, EM8300_IOCTL_VIDEO_SETPTS, &vo_pts);
		}
		write(fd_video, picture_data[0], size);
#endif
	}
}

static uint32_t draw_slice(uint8_t *srcimg[], int stride[], int w, int h, int x0, int y0)
{
	if (img_format == IMGFMT_YV12) {
		sws->swScale(sws, srcimg, stride, y0, h, picture_data, picture_linesize);
		return 0;
	}
	return -1;
}

static void uninit(void)
{
	printf("VO: [dxr3] Uninitializing\n");
	if (sws) {
		freeSwsContext(sws);
	}
#ifdef USE_LIBFAME
	if (fame_ctx) {
		fame_close(fame_ctx);
	}
#elif USE_LIBAVCODEC
	if (avc_context) {
		avcodec_close(avc_context);
	}
#endif
	if (picture_data[0]) {
		free(picture_data[0]);
	}
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
	if (arg && !strcmp("noprebuf", arg)) {
		printf("VO: [dxr3] Disabling prebuffering.\n");
		noprebuf = 1;
		fdflags |= O_NONBLOCK;
	}

	if (arg && !noprebuf) {
		printf("VO: [dxr3] Forcing use of device %s\n", arg);
		sprintf(devname, "/dev/em8300-%s", arg);
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
	if (arg && !noprebuf) {
		sprintf(devname, "/dev/em8300_mv-%s", arg);
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
	if (arg && !noprebuf) {
		sprintf(devname, "/dev/em8300_sp-%s", arg);
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

#if !defined(USE_LIBFAME) && defined(USE_LIBAVCODEC)
	avcodec_init();
	avcodec_register_all();
#endif
	
	return 0;
}
