/* 
 * vo_zr2.c - playback on zoran cards 
 * Based on vo_zr.c,v 1.27
 * Copyright (C) Rik Snel 2001-2003, License GNU GPL v2
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
#include "videodev_mjpeg.h"

#include "config.h"

#include "video_out.h"
#include "video_out_internal.h"
#include "mp_msg.h"
#include "fastmemcpy.h"

static vo_info_t info = {
	"Zoran ZR360[56]7/ZR36060 Driver (DC10(+)/buz/lml33/MatroxRR)",
	"zr2",
	"Rik Snel <snel@phys.uu.nl>",
	""
};

LIBVO_EXTERN(zr2)

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

static char *guess_device(char *suggestion) {
	struct stat vstat;
	int res;
	char *devs[] = {
		"/dev/video",
		"/dev/video0",
		"/dev/v4l/video0",
		"/dev/v4l0",
		"/dev/v4l",
		NULL
	};
	char **dev = devs;

	if (suggestion) {
		if (!*suggestion) {
			ERROR("error: specified device name is empty string\n");
			return NULL;
		}

		res = stat(suggestion, &vstat);
		if (res == 0 && S_ISCHR(vstat.st_mode)) {
			VERBOSE("using device %s\n", suggestion);
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

static uint32_t query_format(uint32_t format) {
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
	memcpy(p->buf + p->zrq.size*p->frame, mpi->planes[0], size);
			
	return VO_TRUE;
}

static char *normstring(int norm) {
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

static uint32_t preinit(const char *arg) {
	char *dev = NULL, *ptr = NULL, *tmp;
	vo_zr2_priv_t *p = &priv;
	int last = 0, norm = VIDEO_MODE_AUTO;

	VERBOSE("preinit() called\n");
	memset(p, 0, sizeof(*p)); /* set defaults */
	p->vdes = -1;

	if (arg) {
		/* save subdevice string */
		p->subdevice = strdup(arg);
		if (!p->subdevice) {
			ERROR("out of memory, this is bad\n");
			uninit();
			return 1;
		}

		tmp = ptr = p->subdevice;
		do {
			while (*tmp != ':' && *tmp) tmp++;
			if (*tmp == ':') *tmp++ = '\0';
			else last = 1;
			DBG2("processing subdevice option \"%s\"\n", ptr);
			if (!strncmp("dev=", ptr, 4)) {
				dev = ptr + 4;
				VERBOSE("user specified device \"%s\"\n", dev);
			} else if (!strcasecmp("PAL", ptr)) {
				norm = VIDEO_MODE_PAL;
				VERBOSE("user specified video norm PAL\n");
			} else if (!strcasecmp("SECAM", ptr)) {
				norm = VIDEO_MODE_SECAM;
				VERBOSE("user specified video norm SECAM\n");
			} else if (!strcasecmp("NTSC", ptr)) {
				norm = VIDEO_MODE_NTSC;
				VERBOSE("user specified video norm NTSC\n");
			} else if (!strcmp("prebuf", ptr)) {
				WARNING("prebuffering is not yet supported\n");
			} else {
				WARNING("ignoring unknown subdevice option " 
						"\"%s\", or missing argument\n",
						ptr);
			}
			ptr = tmp;
		} while (!last);
	}

	dev = guess_device(dev);
	if (!dev) {
		uninit();
		return 1;
	}

	p->vdes = open(dev, O_RDWR);
	if (p->vdes < 0) {
		ERROR("error opening %s: %s\n", dev, strerror(errno));
		uninit();
		return 1;
	}
	
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

static uint32_t config(uint32_t width, uint32_t height, uint32_t d_width, 
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

static uint32_t control(uint32_t request, void *data, ...) {
	switch (request) {
  		case VOCTRL_QUERY_FORMAT:
			return query_format(*((uint32_t*)data));
		case VOCTRL_DRAW_IMAGE:
			return draw_image(data);
  	}
	return VO_NOTIMPL;
}

static uint32_t draw_frame(uint8_t *src[]) {
	return 0;
}

static uint32_t draw_slice(uint8_t *image[], int stride[],
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
