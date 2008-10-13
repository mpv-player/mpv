/*
 * Copyright (C) 2005 Rik Snel <rsnel@cube.dyndns.org>
 * - based on vd_mpegpes.c by A'rpi (C) 2002-2003
 * - guess_mjpeg_type code stolen from lav_io.c (C) 2000 Rainer Johanni
 *   <Rainer@Johanni.de> from the mjpegtools package
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

#include "config.h"
#include "mp_msg.h"
#include "vfcap.h"

/* some convenient #define's, is this portable enough? */
#define VERBOSE(...) mp_msg(MSGT_DECVIDEO, MSGL_V, "vd_zrmjpeg: " __VA_ARGS__)
#define ERROR(...) mp_msg(MSGT_DECVIDEO, MSGL_ERR, "vd_zrmjpeg: " __VA_ARGS__)
#define WARNING(...) mp_msg(MSGT_DECVIDEO, MSGL_WARN, \
		"vd_zrmjpeg: " __VA_ARGS__)

#include "vd_internal.h"

static vd_info_t info = 
{
	"Zoran MJPEG Video passthrough",
	"zrmjpeg",
	"Rik Snel <snel@phys.uu.nl>",
	"Rik Snel <snel@phys.uu.nl>",
	"for hw decoders (DC10(+)/buz/lml33)"
};

LIBVD_EXTERN(zrmjpeg)

#include "libvo/video_out.h"

typedef struct {
	int vo_initialized;
	unsigned int preferred_csp;
} vd_zrmjpeg_ctx_t;

static int query_format(sh_video_t *sh, unsigned int format) {
	vd_zrmjpeg_ctx_t *ctx = sh->context;
	if (format == ctx->preferred_csp) return VFCAP_CSP_SUPPORTED;
	return CONTROL_FALSE;
}
	
// to set/get/query special features/parameters
static int control(sh_video_t *sh, int cmd, void* arg, ...) {
	switch (cmd) {
		case VDCTRL_QUERY_FORMAT:
			return query_format(sh, *((unsigned int*)arg));
	}
	return CONTROL_UNKNOWN;
}

// init driver
static int init(sh_video_t *sh) {
	vd_zrmjpeg_ctx_t *ctx;

	VERBOSE("init called\n");
	ctx = malloc(sizeof(*ctx));
	if (!ctx) return 0;
	memset(ctx, 0, sizeof(*ctx));
	sh->context = ctx;

	/* defer init of vo until the first frame is known */
	return 1; 
#if 0
	return mpcodecs_config_vo(sh, sh->disp_w, sh->disp_h, IMGFMT_ZRMJPEGIT); 
#endif
}

// uninit driver
static void uninit(sh_video_t *sh) {
	free(sh->context);
}

/* parts directly stolen from scan_jpg() and lav_open_input_file */
static int get_int2(unsigned char *buf) {
	return buf[0]*256 + buf[1];
}

#define M_SOF0  0xC0
#define M_SOF1  0xC1
#define M_DHT   0xC4
#define M_SOI   0xD8		/* Start Of Image (beginning of datastream) */
#define M_EOI   0xD9		/* End Of Image (end of datastream) */
#define M_SOS   0xDA		/* Start Of Scan (begins compressed data) */
#define M_DQT   0xDB
#define M_APP0  0xE0
#define M_APP1  0xE1
/* returns 0 in case of failure */
static unsigned int guess_mjpeg_type(unsigned char *data, unsigned int size,
		int d_height) {
	unsigned int p;
	int marker, length, height, i, hf[3], vf[3];
	unsigned int app0 = 0, header = 0;

	/* The initial marker must be SIO */
	if (size < 2) {
		ERROR("JPEG data too short (%d bytes)\n", size);
		return 0;
	}

	if (data[0] != 0xFF || data[1] != M_SOI) {
		ERROR("JPEG data must start with FFD8, but doesn't\n");
		return 0;
	}
	
	p = 2; /* pointer within jpeg data */

	while (p < size) {
		/* search 0xFF */
		while(data[p] != 0xFF) {
			p++;
			if (p >= size) return 0;
		}
		
		/* get marker code, skip duplicate FF's */
		while(data[p] == 0xFF) {
			p++;
			if (p >= size) return 0;
		}

		marker = data[p++];

		/* marker may have an associated length */
		if (p <= size - 2) length = get_int2(data+p);
		else length = 0; 

		switch (marker) {
			case M_SOF0:
			case M_SOF1:
				header = p-2;
				VERBOSE("found offset of header %u\n",
						header);
				break;
			case M_SOS:
				size = 0;
				continue;
			case M_APP0:
				app0 = p-2;
				VERBOSE("found offset of APP0 %u\n",
						app0);
				break;
		}

		/* these markers shouldn't have parameters,
		 * i.e. we don't need to skip anaything */
		if (marker == 0 || marker == 1 || 
				(marker >= 0xd0 && marker < 0xd8))
			continue; 
		
		if  (p + length <= size) p += length;
		else {
			ERROR("input JPEG too short, data missing\n");
			return 0;
		}
	}

	if (!header) {
		ERROR("JPEG header (with resolution and sampling factors) not found\n");
		return 0;
	}

	if (data[header + 9] != 3) {
		ERROR("JPEG has wrong number of components\n");
		return 0;
	}

	/* get the horizontal and vertical sample factors */
	for (i = 0; i < 3; i++) {
		hf[i] = data[header + 10 + 3*i + 1]>>4;
		vf[i] = data[header + 10 + 3*i + 1]&0x0F;
	}

	if (hf[0] != 2 || hf[1] != 1 || hf[2] != 1 ||
			vf[0] != 1 || vf[1] != 1 || vf[2] != 1) {
		ERROR("JPEG has wrong internal image format\n");
	} else VERBOSE("JPEG has colorspace YUV422 with minimal sampling factors (good)\n");

	height = get_int2(data + header + 5);
	if (height == d_height) {
		VERBOSE("data is non interlaced\n");
		return IMGFMT_ZRMJPEGNI;
	}

	if (2*height != d_height) {
		ERROR("something very inconsistent happened\n");
		return 0;
	}


	if (app0 && get_int2(data + app0 + 2) >= 5 &&
			strncasecmp((char*)(data + app0 + 4), "AVI1", 4) == 0) {
		if (data[app0+8] == 1) {
			VERBOSE("data is interlaced, APP0: top-first (1)\n");
			return IMGFMT_ZRMJPEGIT;
		} else {
			VERBOSE("data is interlaced, APP0: bottom-first (%d)\n",
					data[app0+8]);
			return IMGFMT_ZRMJPEGIB;
		}
	} else {
		VERBOSE("data is interlaced, no (valid) APP0 marker, "
				"guessing top-first\n");
		return IMGFMT_ZRMJPEGIT;
	}

	
	return 0;
}	

// decode a frame
static mp_image_t* decode(sh_video_t *sh, void* data, int len, int flags) {
	mp_image_t* mpi;
	vd_zrmjpeg_ctx_t *ctx = sh->context;

	if (!ctx->vo_initialized) {
		ctx->preferred_csp = guess_mjpeg_type(data, len, sh->disp_h);
		if (ctx->preferred_csp == 0) return NULL;
		mpcodecs_config_vo(sh, sh->disp_w, sh->disp_h, 
				ctx->preferred_csp);
		ctx->vo_initialized = 1;
	}

	mpi = mpcodecs_get_image(sh, MP_IMGTYPE_EXPORT, 0, 
			sh->disp_w, sh->disp_h);
	/* abuse of mpi */
    	mpi->planes[0]=(uint8_t*)data;
	mpi->planes[1]=(uint8_t*)len;
    	return mpi;
}
