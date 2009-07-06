/*
 * - XviD 1.x decoder module for mplayer/mencoder -
 *
 * Copyright(C) 2003      Marco Belli <elcabesa@inwind.it>
 *              2003-2004 Edouard Gomez <ed.gomez@free.fr>
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

/*****************************************************************************
 * Includes
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "mp_msg.h"

#include "vd_internal.h"
#include "m_option.h"

#include <xvid.h>

/*****************************************************************************
 * Configuration options
 ****************************************************************************/

static int do_dr2 = 1;
static int filmeffect = 0;
static int lumadeblock = 0;
static int chromadeblock = 0;
static int lumadering = 0;
static int chromadering = 0;

const m_option_t xvid_dec_opts[] = {
	{ "dr2", &do_dr2, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{ "nodr2", &do_dr2, CONF_TYPE_FLAG, 0, 1, 0, NULL},
	{ "filmeffect", &filmeffect, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{ "deblock-luma", &lumadeblock, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{ "deblock-chroma", &chromadeblock, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{ "dering-luma", &lumadering, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{ "dering-chroma", &chromadering, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{NULL, NULL, 0, 0, 0, 0, NULL}
};

/*****************************************************************************
 * Module private data
 ****************************************************************************/

typedef struct {
	int cs;
	unsigned char img_type;
	void* hdl;
	mp_image_t* mpi;
	int vo_initialized;
} priv_t;

/*****************************************************************************
 * Module function helpers
 ****************************************************************************/

static float stats2aspect(xvid_dec_stats_t *stats);

/*****************************************************************************
 * Video decoder API function definitions
 ****************************************************************************/

/*============================================================================
 * control - to set/get/query special features/parameters
 *==========================================================================*/

static int control(sh_video_t *sh,int cmd,void* arg,...)
{
	return CONTROL_UNKNOWN;
}

/*============================================================================
 * init - initialize the codec
 *==========================================================================*/

static int init(sh_video_t *sh)
{
	xvid_gbl_info_t xvid_gbl_info;
	xvid_gbl_init_t xvid_ini;
	xvid_dec_create_t dec_p;
	priv_t* p;
	int cs;

	memset(&xvid_gbl_info, 0, sizeof(xvid_gbl_info_t));
	xvid_gbl_info.version = XVID_VERSION;

	memset(&xvid_ini, 0, sizeof(xvid_gbl_init_t));
	xvid_ini.version = XVID_VERSION;

	memset(&dec_p, 0, sizeof(xvid_dec_create_t));
	dec_p.version = XVID_VERSION;


	switch(sh->codec->outfmt[sh->outfmtidx]){
	case IMGFMT_YV12:
		/* We will use our own buffers, this speeds decoding avoiding
		 * frame memcpy's overhead */
		cs = (do_dr2)?XVID_CSP_INTERNAL:XVID_CSP_USER;
		break;
	case IMGFMT_YUY2:
		cs = XVID_CSP_YUY2;
		break;
	case IMGFMT_UYVY:
		cs = XVID_CSP_UYVY;
		break;
	case IMGFMT_I420:
	case IMGFMT_IYUV:
		/* We will use our own buffers, this speeds decoding avoiding
		 * frame memcpy's overhead */
		cs = (do_dr2)?XVID_CSP_INTERNAL:XVID_CSP_USER;
		break;
	case IMGFMT_BGR15:
		cs = XVID_CSP_RGB555;
		break;
	case IMGFMT_BGR16:
		cs = XVID_CSP_RGB565;
		break;
	case IMGFMT_BGR32:
		cs = XVID_CSP_BGRA;
		break;
	case IMGFMT_YVYU:
		cs = XVID_CSP_YVYU;
		break;
	default:
		mp_msg(MSGT_DECVIDEO, MSGL_ERR, "Unsupported out_fmt: 0x%X\n",
		       sh->codec->outfmt[sh->outfmtidx]);
		return 0;
	}

	/* Gather some information about the host library */
	if(xvid_global(NULL, XVID_GBL_INFO, &xvid_gbl_info, NULL) < 0) {
		mp_msg(MSGT_MENCODER,MSGL_INFO, "xvid: could not get information about the library\n");
	} else {
		mp_msg(MSGT_MENCODER,MSGL_INFO, "xvid: using library version %d.%d.%d (build %s)\n",
		       XVID_VERSION_MAJOR(xvid_gbl_info.actual_version),
		       XVID_VERSION_MINOR(xvid_gbl_info.actual_version),
		       XVID_VERSION_PATCH(xvid_gbl_info.actual_version),
		       xvid_gbl_info.build);
	}

	/* Initialize the xvidcore library */
	if(xvid_global(NULL, XVID_GBL_INIT, &xvid_ini, NULL))
		return 0;

	/* We use 0 width and height so xvidcore will resize its buffers
	 * if required. That allows this vd plugin to do resize on first
	 * VOL encountered (don't trust containers' width and height) */
	dec_p.width = 0;
	dec_p.height =  0;

	/* Get a decoder instance */
	if(xvid_decore(0, XVID_DEC_CREATE, &dec_p, NULL)<0) {
		mp_msg(MSGT_DECVIDEO, MSGL_ERR, "XviD init failed\n");
		return 0;
	}

	p = malloc(sizeof(priv_t));
	p->cs = cs;
	p->hdl = dec_p.handle;
	p->vo_initialized = 0;
	sh->context = p;

	switch(cs) {
	case XVID_CSP_INTERNAL:
		p->img_type = MP_IMGTYPE_EXPORT;
		break;
	case XVID_CSP_USER:
		p->img_type = MP_IMGTYPE_STATIC;
		break;
	default:
		p->img_type = MP_IMGTYPE_TEMP;
		break;
	}

	return 1;
}

/*============================================================================
 * uninit - close the codec
 *==========================================================================*/

static void uninit(sh_video_t *sh){
	priv_t* p = sh->context;
	if(!p)
		return;
	xvid_decore(p->hdl,XVID_DEC_DESTROY, NULL, NULL);
	free(p);
}

/*============================================================================
 * decode - decode a frame from stream
 *==========================================================================*/

static mp_image_t* decode(sh_video_t *sh, void* data, int len, int flags)
{
	xvid_dec_frame_t dec;
	xvid_dec_stats_t stats;
	mp_image_t* mpi = NULL;

	priv_t* p = sh->context;


	if(!data || len <= 0)
		return NULL;

	memset(&dec,0,sizeof(xvid_dec_frame_t));
	memset(&stats, 0, sizeof(xvid_dec_stats_t));
	dec.version = XVID_VERSION;
	stats.version = XVID_VERSION;

	dec.bitstream = data;
	dec.length = len;

	dec.general |= XVID_LOWDELAY
	/* XXX: if lowdelay is unset, and xvidcore internal buffers are
	 *      used => crash. MUST FIX */
	        | (filmeffect ? XVID_FILMEFFECT : 0 )
	        | (lumadeblock ? XVID_DEBLOCKY : 0 )
	        | (chromadeblock ? XVID_DEBLOCKUV : 0 );
#if XVID_API >= XVID_MAKE_API(4,1)
	dec.general |= (lumadering ? XVID_DEBLOCKY|XVID_DERINGY : 0 );
	dec.general |= (chromadering ? XVID_DEBLOCKUV|XVID_DERINGUV : 0 );
#endif
	dec.output.csp = p->cs;

	/* Decoding loop because xvidcore may return VOL information for
	 * on the fly buffer resizing. In that case we must decode VOL,
	 * init VO, then decode the frame */
	do {
		int consumed;

		/* If we don't know frame size yet, don't even try to request
		 * a buffer, we must loop until we find a VOL, so VO plugin
		 * is initialized and we can obviously output something */
		if (p->vo_initialized) {
			mpi = mpcodecs_get_image(sh, p->img_type,
					MP_IMGFLAG_ACCEPT_STRIDE,
					sh->disp_w, sh->disp_h);

			if(p->cs != XVID_CSP_INTERNAL) {
				dec.output.plane[0] = mpi->planes[0];
				dec.output.plane[1] = mpi->planes[1];
				dec.output.plane[2] = mpi->planes[2];

				dec.output.stride[0] = mpi->stride[0];
				dec.output.stride[1] = mpi->stride[1];
				dec.output.stride[2] = mpi->stride[2];
			}
		}

		/* Decode data */
		consumed = xvid_decore(p->hdl, XVID_DEC_DECODE, &dec, &stats);
		if (consumed < 0) {
			mp_msg(MSGT_DECVIDEO, MSGL_ERR, "Decoding error\n");
			return NULL;
		}

		/* Found a VOL information stats, if VO plugin is not initialized
		 * yet then do it now */
		if (stats.type == XVID_TYPE_VOL && !p->vo_initialized) {
			sh->aspect = stats2aspect(&stats);
			if(!mpcodecs_config_vo(sh, stats.data.vol.width, stats.data.vol.height, IMGFMT_YV12))
				return NULL;

			/* Don't take this path twice */
			p->vo_initialized = !p->vo_initialized;
		}

		/* Don't forget to update buffer position and buffer length */
		dec.bitstream += consumed;
		dec.length -= consumed;
	} while ((stats.type == XVID_TYPE_VOL || stats.type == XVID_TYPE_NOTHING) && dec.length > 0);

	/* There are two ways to get out of the decoding loop:
	 *  - a frame has been returned
	 *  - no more data in buffer and no frames returned */

	/* If mpi is NULL, it proves nothing has been returned by the decoder
	 * so don't try to display internal buffers. */
	if (mpi != NULL && p->cs == XVID_CSP_INTERNAL) {
		mpi->planes[0] = dec.output.plane[0];
		mpi->planes[1] = dec.output.plane[1];
		mpi->planes[2] = dec.output.plane[2];

		mpi->stride[0] = dec.output.stride[0];
		mpi->stride[1] = dec.output.stride[1];
		mpi->stride[2] = dec.output.stride[2];
	}

	/* If we got out the decoding loop because the buffer was empty and there was nothing
	 * to output yet, then just return NULL */
	return (stats.type == XVID_TYPE_NOTHING) ? NULL : mpi;
}

/*****************************************************************************
 * Helper functions
 ****************************************************************************/

/* Returns DAR value according to VOL's informations contained in stats
 * param */
static float stats2aspect(xvid_dec_stats_t *stats)
{
	if (stats->type == XVID_TYPE_VOL) {
		float wpar;
		float hpar;
		float dar;

		/* MPEG4 strem stores PAR (Pixel Aspect Ratio), mplayer uses
		 * DAR (Display Aspect Ratio)
		 *
		 * Both are related thanks to the equation:
		 *            width
		 *      DAR = ----- x PAR
		 *            height
		 *
		 * As MPEG4 is so well designed (*cough*), VOL header carries
		 * both informations together -- lucky eh ? */

		switch (stats->data.vol.par) {
		case XVID_PAR_11_VGA: /* 1:1 vga (square), default if supplied PAR is not a valid value */
			wpar = hpar = 1.0f;
			break;
		case XVID_PAR_43_PAL: /* 4:3 pal (12:11 625-line) */
			wpar = 12;
			hpar = 11;
			break;
		case XVID_PAR_43_NTSC: /* 4:3 ntsc (10:11 525-line) */
			wpar = 10;
			hpar = 11;
			break;
		case XVID_PAR_169_PAL: /* 16:9 pal (16:11 625-line) */
			wpar = 16;
			hpar = 11;
			break;
		case XVID_PAR_169_NTSC: /* 16:9 ntsc (40:33 525-line) */
			wpar = 40;
			hpar = 33;
			break;
		case XVID_PAR_EXT: /* extended par; use par_width, par_height */
			wpar = stats->data.vol.par_width;
			hpar = stats->data.vol.par_height;
			break;
		default:
			wpar = hpar = 1.0f;
			break;
		}

		dar  = ((float)stats->data.vol.width*wpar);
		dar /= ((float)stats->data.vol.height*hpar);

		return dar;
	}

	return 0.0f;
}

/*****************************************************************************
 * Module structure definition
 ****************************************************************************/

static const vd_info_t info =
{
	"XviD 1.0 decoder",
	"xvid",
	"Marco Belli <elcabesa@inwind.it>, Edouard Gomez <ed.gomez@free.fr>",
	"Marco Belli <elcabesa@inwind.it>, Edouard Gomez <ed.gomez@free.fr>",
	"No Comment"
};

LIBVD_EXTERN(xvid)

/* Please do not change that tag comment.
 * arch-tag: b7d654a5-76ea-4768-9713-2c791567fe7d mplayer xvid decoder module */
