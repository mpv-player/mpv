/*
 * vo_dxr3.c - DXR3/H+ video out
 *
 * Copyright (C) 2002 David Holm <dholm@iname.com>
 *
 */

/* ChangeLog added 2002-01-10
 * 2002-07-18:
 *  Disabled spuenc support, this is still not stable enough =(
 *
 * 2002-07-05:
 *  Removed lavc and fame encoder to be compatible with new libvo style.
 *  Added graphic equalizer support.
 *
 * 2002-04-15:
 *  The spuenc code isn't 100% stable yet, therefore I'm disabling
 *  it due to the upcoming stable release.
 *
 * 2002-04-03:
 *  Carl George added spuenc support
 *
 * 2002-03-26:
 *  XorA added an option parser and support for selecting encoder
 *  codec. We thank him again.
 *
 * 2002-03-25:
 *  A couple of bugfixes by XorA
 *
 * 2002-03-23:
 *  Thanks to Marcel Hild <hild@b4mad.net> the jitter-bug experienced
 *  with some videos have been fixed, many thanks goes to him.
 *
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
#include "cpudetect.h"
#include "spuenc.h"

#define SPU_SUPPORT

static vo_info_t vo_info = 
{
	"DXR3/H+ video out",
	"dxr3",
	"David Holm <dholm@iname.com>",
	""
};
LIBVO_EXTERN (dxr3)

/* Resolutions and positions */
static int v_width, v_height;
static int s_width, s_height;
static int osd_w, osd_h;
static int noprebuf = 0;
static int img_format = 0;

/* File descriptors */
static int fd_control = -1;
static int fd_video = -1;
static int fd_spu = -1;
static char fdv_name[80];
static char fds_name[80];

#ifdef SPU_SUPPORT
/* on screen display/subpics */
static char *osdpicbuf = NULL;
static int osdpicbuf_w;
static int osdpicbuf_h;
static int disposd = 0;
static encodedata *spued;
#endif

/* Static variable used in ioctl's */
static int ioval = 0;

static int get_video_eq(vidix_video_eq_t *info);
static int set_video_eq(vidix_video_eq_t *info);

uint32_t control(uint32_t request, void *data, ...)
{
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
			close(fd_spu);
			fd_spu = open(fds_name, O_WRONLY);
			fsync(fd_video);
			fsync(fd_spu);
		}
		return VO_TRUE;
	case VOCTRL_QUERY_FORMAT:
	    {
		uint32_t flag = 0;

		if (*((uint32_t*)data) != IMGFMT_MPEGPES)
		    return 0;

		flag = VFCAP_CSP_SUPPORTED | VFCAP_CSP_SUPPORTED_BY_HW | VFCAP_SPU;
		if (!noprebuf)
		    flag |= VFCAP_TIMER;
		return flag;
	    }
	case VOCTRL_SET_EQUALIZER:
	    {
		va_list ap;
		int value;
		em8300_bcs_t bcs;
		
		va_start(ap, data);
		value = va_arg(ap, int);
		va_end(ap);

		if (ioctl(fd_control, EM8300_IOCTL_GETBCS, &bcs) < 0)
		    return VO_FALSE;
		if (!strcasecmp(data, "brightness"))
		    bcs.brightness = value;
		else if (!strcasecmp(data, "contrast"))
		    bcs.contrast = value;
		else if (!strcasecmp(data, "saturation"))
		    bcs.saturation = value;
		
		if (ioctl(fd_control, EM8300_IOCTL_SETBCS, &bcs) < 0)
		    return VO_FALSE;
		return VO_TRUE;
	    }
	case VOCTRL_GET_EQUALIZER:
	    {
		va_list ap;
		int *value;
		em8300_bcs_t bcs;
		
		va_start(ap, data);
		value = va_arg(ap, int*);
		va_end(ap);

		if (ioctl(fd_control, EM8300_IOCTL_GETBCS, &bcs) < 0)
		    return VO_FALSE;
		
		if (!strcasecmp(data, "brightness"))
		    *value = bcs.brightness;
		else if (!strcasecmp(data, "contrast"))
		    *value = bcs.contrast;
		else if (!strcasecmp(data, "saturation"))
		    *value = bcs.saturation;
		return VO_TRUE;
	    }
	}
	return VO_NOTIMPL;
}

static uint32_t config(uint32_t width, uint32_t height, uint32_t d_width, uint32_t d_height, uint32_t fullscreen, char *title, uint32_t format, const vo_tune_info_t *info)
{
	int tmp1, tmp2, size;
	em8300_register_t reg;
	extern float monitor_aspect;

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
#ifdef MVCOMMAND_SYNC
	reg.microcode_register = 1;
	reg.reg = 0;
	reg.val = MVCOMMAND_SYNC;
	ioctl(fd_control, EM8300_IOCTL_WRITEREG, &reg);
#endif

#ifdef EM8300_IOCTL_FLUSH	
	/* Clean buffer by syncing it */
	ioval = EM8300_SUBDEVICE_VIDEO;
	ioctl(fd_control, EM8300_IOCTL_FLUSH, &ioval);
	ioval = EM8300_SUBDEVICE_AUDIO;
	ioctl(fd_control, EM8300_IOCTL_FLUSH, &ioval);
#endif

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

	/* Set monitor_aspect to avoid jitter */
	monitor_aspect = (float) width / (float) height;
	
	/* libavcodec requires a width and height that is x|16 */
	aspect_save_orig(width, height);
	aspect_save_prescale(d_width, d_height);
#ifdef EM8300_IOCTL_GET_VIDEOMODE
	ioctl(fd_control, EM8300_IOCTL_GET_VIDEOMODE, &ioval);
	if (ioval == EM8300_VIDEOMODE_NTSC) {
		printf("VO: [dxr3] Setting up for NTSC.\n");
		aspect_save_screenres(352, 240);
	} else {
		printf("VO: [dxr3] Setting up for PAL/SECAM.\n");
		aspect_save_screenres(352, 288);
	}
#endif
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

#ifdef SPU_SUPPORT
	osdpicbuf = malloc(s_width * s_height);
	if (osdpicbuf == NULL) {
		printf("vo_dxr3: out of mem\n");
		return -1;
	}
	spued = (encodedata *) malloc(sizeof(encodedata));
	if (spued == NULL) {
		printf("vo_dxr3:out of mem\n");
		return -1;
	}
	osd_w = s_width;
	osd_h = s_height;
	osdpicbuf_w = s_width;
	osdpicbuf_h = s_height;
#endif

	return 0;
}

static const vo_info_t* get_info(void)
{
	return &vo_info;
}

static void draw_alpha(int x, int y, int w, int h, unsigned char* src, unsigned char *srca, int srcstride)
{
#ifdef SPU_SUPPORT
	int lx, ly, bx;
	unsigned char *buf = &osdpicbuf[(y * osdpicbuf_w) + x];
	for (ly = 0; ly < h - 1; ly++) {
		for(lx = 0; lx < w; lx++) {
			if ((srca[(ly * srcstride) + lx] != 0) && (src[(ly * srcstride) + lx] != 0)) {
				if(src[(ly * srcstride) + lx] >= 128) {
					buf[ly * osdpicbuf_w + lx] = 3;
				}
			}
		}
	}
	pixbuf_encode_rle(x, y, osdpicbuf_w, osdpicbuf_h - 1, osdpicbuf, osdpicbuf_w, spued);
#endif
}

static void draw_osd(void)
{
#ifdef SPU_SUPPORT
	if ((disposd % 15) == 0) {
		vo_draw_text(osd_w, osd_h, draw_alpha);
		memset(osdpicbuf, 0, s_width * s_height);

		/* could stand some check here to see if the subpic hasn't changed
		 * as if it hasn't and we re-send it it will "blink" as the last one
		 * is turned off, and the new one (same one) is turned on
		 */
/*		Subpics are not stable yet =(
		expect lockups if you enable		
		if (!noprebuf) {
			ioctl(fd_spu, EM8300_IOCTL_SPU_SETPTS, &vo_pts);
		}
		write(fd_spu, spued->data, spued->count);*/
	}
	disposd++;
#endif
}


static uint32_t draw_frame(uint8_t * src[])
{
	char *pData;
	int pSize;
	vo_mpegpes_t *p = (vo_mpegpes_t *) src[0];
		
#ifdef SPU_SUPPORT
	if (p->id == 0x20) {
		if (!noprebuf) {
			ioctl(fd_spu, EM8300_IOCTL_SPU_SETPTS, &vo_pts);
		}
		write(fd_spu, p->data, p->size);
	} else
#endif
		write(fd_video, p->data, p->size);
	return 0;
}

static void flip_page(void)
{
	int size;
	if (!noprebuf) {
		ioctl(fd_video, EM8300_IOCTL_VIDEO_SETPTS, &vo_pts);
	}
}

static uint32_t draw_slice(uint8_t *srcimg[], int stride[], int w, int h, int x0, int y0)
{
	return -1;
}

static void uninit(void)
{
	printf("VO: [dxr3] Uninitializing\n");
	if (fd_video) {
		close(fd_video);
	}
	if (fd_spu) {
		close(fd_spu);
	}
	if (fd_control) {
		close(fd_control);
	}
#ifdef SPU_SUPPORT
	if(osdpicbuf) {
		free(osdpicbuf);
	}
	if(spued) {
		free(spued);
	}
#endif
}

static void check_events(void)
{
}

static uint32_t preinit(const char *arg)
{
	char devname[80];
	int fdflags = O_WRONLY;
	CpuCaps cpucaps;

	GetCpuCaps(&cpucaps);
	/* Open the control interface */
	if (arg && !strncmp("noprebuf", arg, 8)) {
		printf("VO: [dxr3] Disabling prebuffering.\n");
		noprebuf = 1;
		fdflags |= O_NONBLOCK;
		arg = strchr(arg, ':');
		if (arg) {
			arg++;
		}
	}

	if (cpucaps.has3DNowExt) {
		printf("VO: [dxr3] Fast AMD special disabling prebuffering.\n");
		noprebuf = 1;
		fdflags |= O_NONBLOCK;
	}

	if (arg && arg[0]) {
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
	fdflags |= O_NONBLOCK;
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
	strcpy(fds_name, devname);
	
	return 0;
}
