/*****************************************************************************
 *
 *  - XviD 1.0 decoder module for mplayer/mencoder -
 *
 *  Copyright(C) 2003 Marco Belli <elcabesa@inwind.it>
 *               2003 Edouard Gomez <ed.gomez@free.fr>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 *****************************************************************************/

/*****************************************************************************
 * Includes
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "mp_msg.h"

#ifdef HAVE_XVID4

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

m_option_t xvid_dec_opts[] = {
	{ "dr2", &do_dr2, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{ "nodr2", &do_dr2, CONF_TYPE_FLAG, 0, 1, 0, NULL},
	{ "filmeffect", &filmeffect, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{ "deblock-luma", &lumadeblock, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{ "deblock-chroma", &chromadeblock, CONF_TYPE_FLAG, 0, 0, 1, NULL},
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
} priv_t;

/*****************************************************************************
 * Video decoder API function definitions
 ****************************************************************************/

/*============================================================================
 * control - to set/get/query special features/parameters
 *==========================================================================*/

static int control(sh_video_t *sh,int cmd,void* arg,...)
{
	return(CONTROL_UNKNOWN);
}

/*============================================================================
 * init - initialize the codec
 *==========================================================================*/

static int init(sh_video_t *sh)
{
	xvid_gbl_init_t xvid_ini;
	xvid_dec_create_t dec_p;
	priv_t* p;
	int cs;

	memset(&xvid_ini, 0, sizeof(xvid_gbl_init_t));
	xvid_ini.version = XVID_VERSION;
	memset(&dec_p, 0, sizeof(xvid_dec_create_t));
	dec_p.version = XVID_VERSION;

	if(!mpcodecs_config_vo(sh, sh->disp_w, sh->disp_h, IMGFMT_YV12))
		return(0);

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
		return(0);
	}

	if(xvid_global(NULL, XVID_GBL_INIT, &xvid_ini, NULL))
		return(0);

	dec_p.width = sh->disp_w;
	dec_p.height =  sh->disp_h;

	if(xvid_decore(0, XVID_DEC_CREATE, &dec_p, NULL)<0) {
		mp_msg(MSGT_DECVIDEO, MSGL_ERR, "XviD init failed\n");
		return(0);
	}

	p = (priv_t*)malloc(sizeof(priv_t));
	p->cs = cs;
	p->hdl = dec_p.handle;
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

	return(1);
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
	priv_t* p = sh->context;

	mp_image_t* mpi = mpcodecs_get_image(sh, p->img_type,
					     MP_IMGFLAG_ACCEPT_STRIDE,
					     sh->disp_w,sh->disp_h);

	if(!data || !mpi || len <= 0)
		return(NULL);

	memset(&dec,0,sizeof(xvid_dec_frame_t));
	dec.version = XVID_VERSION;

	dec.bitstream = data;
	dec.length = len;

	dec.general |= XVID_LOWDELAY 
	        | (filmeffect ? XVID_FILMEFFECT : 0 )
	        | (lumadeblock ? XVID_DEBLOCKY : 0 )
	        | (chromadeblock ? XVID_DEBLOCKUV : 0 );

	dec.output.csp = p->cs;   

	if(p->cs != XVID_CSP_INTERNAL) {
		dec.output.plane[0] = mpi->planes[0];
		dec.output.plane[1] = mpi->planes[1];
		dec.output.plane[2] = mpi->planes[2];

		dec.output.stride[0] = mpi->stride[0]; 
		dec.output.stride[1] = mpi->stride[1]; 
		dec.output.stride[2] = mpi->stride[2];
	}

	if(xvid_decore(p->hdl, XVID_DEC_DECODE, &dec, NULL) < 0) {
		mp_msg(MSGT_DECVIDEO, MSGL_ERR, "Decoding error\n");
		return(NULL);
	}

	if(p->cs == XVID_CSP_INTERNAL) {
		mpi->planes[0] = dec.output.plane[0];
		mpi->planes[1] = dec.output.plane[1];
		mpi->planes[2] = dec.output.plane[2];

		mpi->stride[0] = dec.output.stride[0];
		mpi->stride[1] = dec.output.stride[1];
		mpi->stride[2] = dec.output.stride[2];
	}

	return(mpi);
}

/*****************************************************************************
 * Module structure definition
 ****************************************************************************/

static vd_info_t info = 
{
	"XviD 1.0 decoder",
	"xvid",
	"Marco Belli <elcabesa@inwind.it>, Edouard Gomez <ed.gomez@free.fr>",
	"Marco Belli <elcabesa@inwind.it>, Edouard Gomez <ed.gomez@free.fr>",
	"No Comment"
};

LIBVD_EXTERN(xvid)

#endif  /* HAVE_XVID4 */

/* Please do not change that tag comment.
 * arch-tag: b7d654a5-76ea-4768-9713-2c791567fe7d mplayer xvid decoder module */
