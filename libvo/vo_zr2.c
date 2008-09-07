/*
 * playback on Zoran cards, based on vo_zr.c
 *
 * copyright (C) 2001-2005 Rik Snel
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

/* $Id$ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/videodev.h>
#include "config.h"
#include "videodev_mjpeg.h"
#include "video_out.h"
#include "video_out_internal.h"
#include "mp_msg.h"
#include "subopt-helper.h"
#include "fastmemcpy.h"

static const vo_info_t info = {
	"Zoran ZR360[56]7/ZR36060 Driver (DC10(+)/buz/lml33/MatroxRR)",
	"zr2",
	"Rik Snel <rsnel@cube.dyndns.org>",
	""
};

const LIBVO_EXTERN(zr2)

typedef struct {
	/* options */
	char *subdevice;

	/* information for (and about) the zoran card */

	unsigned char *buf; 		 /* the JPEGs will be placed here */
	struct mjpeg_requestbuffers zrq; /* info about this buffer */

	int vdes;			 /* file descriptor of card */
	int playing;                     /* 0 or 1 */
	int frame, sync, queue; 	 /* buffer management */
	struct mjpeg_sync zs;		 /* state information */
	struct mjpeg_params zp;
	struct video_capability vc;	 /* max resolution and so on */
} vo_zr2_priv_t;

static vo_zr2_priv_t priv;

#define ZR2_MJPEG_NBUFFERS	2
#define ZR2_MJPEG_SIZE		1024*256	

/* some convenient #define's, is this portable enough? */
#define DBG2(...) mp_msg(MSGT_VO, MSGL_DBG2, "vo_zr2: " __VA_ARGS__)
#define VERBOSE(...) mp_msg(MSGT_VO, MSGL_V, "vo_zr2: " __VA_ARGS__)
#define ERROR(...) mp_msg(MSGT_VO, MSGL_ERR, "vo_zr2: " __VA_ARGS__)
#define WARNING(...) mp_msg(MSGT_VO, MSGL_WARN, "vo_zr2: " __VA_ARGS__)

static void stop_playing(vo_zr2_priv_t *p) {
	if (p->playing) {
		p->frame = -1;
		if (ioctl(p->vdes, MJPIOC_QBUF_PLAY, &p->frame) < 0)  
			ERROR("error stopping playback\n");
		p->playing = 0;
		p->sync = 0;
		p->queue = 0;
		p->frame = 0;
	}
}

static const char *guess_device(const char *suggestion, int inform) {
	struct stat vstat;
	int res;
	static const char * const devs[] = {
		"/dev/video",
		"/dev/video0",
		"/dev/v4l/video0",
		"/dev/v4l0",
		"/dev/v4l",
		NULL
	};
	const char * const *dev = devs;

	if (suggestion) {
		if (!*suggestion) {
			ERROR("error: specified device name is empty string\n");
			return NULL;
		}

		res = stat(suggestion, &vstat);
		if (res == 0 && S_ISCHR(vstat.st_mode)) {
			if (inform) VERBOSE("using device %s\n", suggestion);
			return suggestion;
		} else {
			if (res != 0) ERROR("%s does not exist\n", suggestion);
			else ERROR("%s is no character device\n", suggestion);
			/* don't try to be smarter than the user, just exit */
			return NULL;
		}
	}

	while (*(++dev) != NULL) {
		if (stat(*dev, &vstat) == 0 && S_ISCHR(vstat.st_mode)) {
			VERBOSE("guessed video device %s\n", *dev);
			return *dev;
		}
		dev++;
	}

	ERROR("unable to find video device\n");

	return NULL;
}

static int query_format(uint32_t format) {
	if (format==IMGFMT_ZRMJPEGNI ||
			format==IMGFMT_ZRMJPEGIT ||
			format==IMGFMT_ZRMJPEGIB)
		return VFCAP_CSP_SUPPORTED|VFCAP_CSP_SUPPORTED_BY_HW;
	return 0;
}

static uint32_t draw_image(mp_image_t *mpi) {
	vo_zr2_priv_t *p = &priv;
	int size = (int)mpi->planes[1];
	if (size > (int)p->zrq.size) {
		ERROR("incoming JPEG image (size=%d) doesn't fit in buffer\n",
				size);
		return VO_FALSE;
	}

	/* looking for free buffer */
	if (p->queue - p->sync < (int)p->zrq.count) p->frame = p->queue;
	else {
		if (ioctl(p->vdes, MJPIOC_SYNC, &p->zs) < 0) {
			ERROR("error waiting for buffer to become free\n");
			return VO_FALSE;
		}
		p->frame = p->zs.frame;
		p->sync++;
	}

	/* copy the jpeg image to the buffer which we acquired */
	fast_memcpy(p->buf + p->zrq.size*p->frame, mpi->planes[0], size);
			
	return VO_TRUE;
}

static const char *normstring(int norm) {
	switch (norm) {
		case VIDEO_MODE_PAL:
			return "PAL";
		case VIDEO_MODE_NTSC:
			return "NTSC";
		case VIDEO_MODE_SECAM:
			return "SECAM";
		case VIDEO_MODE_AUTO:
			return "auto";
	}
	return "undefined";
}

static int get_norm(const char *n) {
	if (!strcmp(n, "PAL")) return VIDEO_MODE_PAL;
	if (!strcmp(n, "NTSC")) return VIDEO_MODE_NTSC;
	if (!strcmp(n, "SECAM")) return VIDEO_MODE_SECAM;
	if (!strcmp(n, "auto")) return VIDEO_MODE_AUTO;
	return -1; /* invalid */
}

static int nc(const char **norm) {
	if (get_norm(*norm) == -1) {
		ERROR("norm \"%s\" is not supported, choose from PAL, NTSC, SECAM and auto\n", *norm);
		return 0;
	} else return 1;
}

static int pbc(int *prebuf) {
	if (*prebuf) WARNING("prebuffering is not yet supported\n");
	return 1;
}

static int preinit(const char *arg) {
	vo_zr2_priv_t *p = &priv;
	const char *dev = NULL;
	char *dev_arg = NULL, *norm_arg = NULL;
	int norm = VIDEO_MODE_AUTO, prebuf = 0;
	opt_t subopts[] = { /* don't want warnings with -Wall... */
		{ "dev",    OPT_ARG_MSTRZ, &dev_arg,   NULL, 	        0 },
		{ "prebuf", OPT_ARG_BOOL,  &prebuf,    (opt_test_f)pbc, 0 },
		{ "norm",   OPT_ARG_MSTRZ, &norm_arg,  (opt_test_f)nc,  0 },
		{ NULL,     0, 		   NULL,       NULL, 	        0 }
	};

	VERBOSE("preinit() called with arg: %s\n", arg);
	memset(p, 0, sizeof(*p)); /* set defaults */
	p->vdes = -1;

	if (subopt_parse(arg, subopts)) {
		mp_msg(MSGT_VO, MSGL_FATAL,
				"Allowed suboptions for -vo zr2 are:\n"
				"-  dev=DEVICE               (default: %s)\n"
				"-  norm=PAL|NTSC|SECAM|auto (default: auto)\n"
				"-  prebuf/noprebuf          (default:"
				" noprebuf)\n"
				"\n"
				"Example: mplayer -vo zr2:dev=/dev/video1:"
				"norm=PAL movie.avi\n\n"
				, guess_device(NULL, 0));
		free(norm_arg);
		free(dev_arg);
		return -1;
	}
				
	/* interpret the strings we got from subopt_parse */
	if (norm_arg) {
		norm = get_norm(norm_arg);
		free(norm_arg);
	}

	if (dev_arg) dev = dev_arg;

	dev = guess_device(dev, 1);
	if (!dev) {
		free(dev_arg);
		uninit();
		return 1;
	}

	p->vdes = open(dev, O_RDWR);
	if (p->vdes < 0) {
		ERROR("error opening %s: %s\n", dev, strerror(errno));
		free(dev_arg);
		uninit();
		return 1;
	}
	
	free(dev_arg);

	/* check if we really are dealing with a zoran card */
	if (ioctl(p->vdes, MJPIOC_G_PARAMS, &p->zp) < 0) {
		ERROR("%s probably is not a DC10(+)/buz/lml33\n", dev);
		uninit();
		return 1;
	}

	VERBOSE("kernel driver version %d.%d, current norm is %s\n", 
			p->zp.major_version, p->zp.minor_version,
			normstring(p->zp.norm));

	/* changing the norm in the zoran_params and MJPIOC_S_PARAMS
	 * does nothing the last time I tried, so bail out if the norm 
	 * is not correct */
	if (norm != VIDEO_MODE_AUTO &&  p->zp.norm != norm) {
		ERROR("mplayer currently can't change the video norm, "
				"change it with (eg.) XawTV and retry.\n");
		uninit();
		return 1;
	}
		
	/* gather useful information */
	if (ioctl(p->vdes, VIDIOCGCAP, &p->vc) < 0) {
		ERROR("error getting video capabilities from %s\n", dev);
		uninit();
		return 1;
	}
	
	VERBOSE("card reports maxwidth=%d, maxheight=%d\n", 
			p->vc.maxwidth, p->vc.maxheight);

	/* according to the mjpegtools source, some cards return a bogus 
	 * vc.maxwidth, correct it here. If a new zoran card appears with a 
	 * maxwidth different 640, 720 or 768 this code may lead to problems */
	if (p->vc.maxwidth != 640 && p->vc.maxwidth != 768) {
		VERBOSE("card probably reported bogus width (%d), "
				"changing to 720\n", p->vc.maxwidth);
		p->vc.maxwidth = 720;
	}

	p->zrq.count = ZR2_MJPEG_NBUFFERS;
	p->zrq.size = ZR2_MJPEG_SIZE;

	if (ioctl(p->vdes, MJPIOC_REQBUFS, &p->zrq)) {
		ERROR("error requesting %d buffers of size %d\n", 
				ZR2_MJPEG_NBUFFERS, ZR2_MJPEG_NBUFFERS);
		uninit();
		return 1;
	}
	
	VERBOSE("got %ld buffers of size %ld (wanted %d buffers of size %d)\n",
			p->zrq.count, p->zrq.size, ZR2_MJPEG_NBUFFERS,
			ZR2_MJPEG_SIZE);
	
	p->buf = (unsigned char*)mmap(0, p->zrq.count*p->zrq.size,
			PROT_READ|PROT_WRITE, MAP_SHARED, p->vdes, 0);

	if (p->buf == MAP_FAILED) {
		ERROR("error mapping requested buffers: %s", strerror(errno));
		uninit();
		return 1;
	}

    	return 0;
}

static int config(uint32_t width, uint32_t height, uint32_t d_width, 
	uint32_t d_height, uint32_t flags, char *title, uint32_t format) {
	int fields = 1, top_first = 1, err = 0;
	int stretchx = 1, stretchy = 1;
	struct mjpeg_params zptmp;
	vo_zr2_priv_t *p = &priv;
	VERBOSE("config() called\n");

	/* paranoia check */
	if (!query_format(format)) {
		ERROR("called with wrong format, should be impossible\n");
		return 1;
	}

	if ((int)height > p->vc.maxheight) {
		ERROR("input height %d is too large, maxheight=%d\n",
				height, p->vc.maxheight);
		err = 1;
	}

	if (format != IMGFMT_ZRMJPEGNI) {
		fields = 2;
		if (format == IMGFMT_ZRMJPEGIB)
			top_first = 0;
	} else if ((int)height > p->vc.maxheight/2) {
		ERROR("input is too high (%d) for non-interlaced playback"
				"max=%d\n", height, p->vc.maxheight);
		err = 1;
	}

	if (width%16 != 0) {
		ERROR("input width=%d, must be multiple of 16\n", width);
		err = 1;
	}
	
	if (height%(fields*8) != 0) {
		ERROR("input height=%d, must be multiple of %d\n", 
				height, 2*fields);
		err = 1;
	}

	/* we assume sample_aspect = 1 */
	if (fields == 1) {
		if (2*d_width <= (uint32_t)p->vc.maxwidth) {
			VERBOSE("stretching x direction to preserve aspect\n");
			d_width *= 2;
		} else VERBOSE("unable to preserve aspect, screen width "
					"too small\n");
	}

	if (d_width == width) stretchx = 1;
	else if (d_width == 2*width) stretchx = 2;
#if 0 /* do minimal stretching for now */
	else if (d_width == 4*width) stretchx = 4;
	else WARNING("d_width must be {1,2,4}*width, using defaults\n");

	if (d_height == height) stretchy = 1;
	else if (d_height == 2*height) stretchy = 2;
	else if (d_height == 4*height) stretchy = 4;
	else WARNING("d_height must be {1,2,4}*height, using defaults\n");
#endif

	if (stretchx*width > (uint32_t)p->vc.maxwidth) {
		ERROR("movie to be played is too wide, width=%d>maxwidth=%d\n",
				width*stretchx, p->vc.maxwidth);
		err = 1;
	}

	if (stretchy*height > (uint32_t)p->vc.maxheight) {
		ERROR("movie to be played is too heigh, height=%d>maxheight"
				"=%d\n", height*stretchy, p->vc.maxheight);
		err = 1;
	}

	if (err == 1) return 1;

	/* some video files (eg. concatenated MPEG files), make MPlayer
	 * call config() during playback while no parameters have changed.
	 * We make configuration changes to a temporary params structure,
	 * compare it with the old params structure and only apply the new
	 * config if it is different from the old one. */
	memcpy(&zptmp, &p->zp, sizeof(zptmp));

	/* translate the configuration to zoran understandable format */
	zptmp.decimation = 0;
	zptmp.HorDcm = stretchx;
	zptmp.VerDcm = stretchy;
	zptmp.TmpDcm = 1;
	zptmp.field_per_buff = fields;
	zptmp.odd_even = top_first;

	/* center the image on screen */
	zptmp.img_x = (p->vc.maxwidth - width*stretchx)/2;
	zptmp.img_y = (p->vc.maxheight - height*stretchy*(3-fields))/4;

	zptmp.img_width = stretchx*width;
	zptmp.img_height = stretchy*height/fields;

	VERBOSE("tv: %dx%d, out: %dx%d+%d+%d, in: %ux%u %s%s%s\n",
			p->vc.maxwidth, p->vc.maxheight,
			zptmp.img_width, 2*zptmp.img_height,
			zptmp.img_x, 2*zptmp.img_y,
			width, height, (fields == 1) ? "non-interlaced" : "",
			(fields == 2 && top_first == 1) 
			?  "interlaced top first" : "",
			(fields == 2 && top_first == 0) 
			? "interlaced bottom first" : "");

	if (memcmp(&zptmp, &p->zp, sizeof(zptmp))) {
		/* config differs, we must update */
		memcpy(&p->zp, &zptmp, sizeof(zptmp));
		stop_playing(p);
		if (ioctl(p->vdes, MJPIOC_S_PARAMS, &p->zp) < 0) {
			ERROR("error writing display params to card\n");
			return 1;
		}
		VERBOSE("successfully written display parameters to card\n");
	} else VERBOSE("config didn't change, no need to write it to card\n");

	return 0;
}

static int control(uint32_t request, void *data, ...) {
	switch (request) {
  		case VOCTRL_QUERY_FORMAT:
			return query_format(*((uint32_t*)data));
		case VOCTRL_DRAW_IMAGE:
			return draw_image(data);
  	}
	return VO_NOTIMPL;
}

static int draw_frame(uint8_t *src[]) {
	return 0;
}

static int draw_slice(uint8_t *image[], int stride[],
		int w, int h, int x, int y) {
 	return 0;
}

static void draw_osd(void) {
}

static void flip_page(void) {
	vo_zr2_priv_t *p = &priv;
	/* queueing the buffer for playback */
	/* queueing the first buffer automatically starts playback */
	if (p->playing == 0) p->playing = 1;
	if (ioctl(p->vdes, MJPIOC_QBUF_PLAY, &p->frame) < 0) 
		ERROR("error queueing buffer for playback\n");
	else p->queue++;
}

static void check_events(void) {
}

static void uninit(void) {
	vo_zr2_priv_t *p = &priv;
	VERBOSE("uninit() called (may be called from preinit() on error)\n");

	stop_playing(p);

	if (p->buf && munmap(p->buf, p->zrq.size*p->zrq.count)) 
		ERROR("error munmapping buffer: %s\n", strerror(errno));

	if (p->vdes >= 0) close(p->vdes);
	free(p->subdevice);
}
