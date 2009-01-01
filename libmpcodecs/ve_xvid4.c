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
#include <string.h>
#include <errno.h>
#include <math.h>
#include <limits.h>
#include <time.h>

#include "config.h"
#include "mp_msg.h"

#include "codec-cfg.h"
#include "stream/stream.h"
#include "libmpdemux/demuxer.h"
#include "libmpdemux/stheader.h"

#include "stream/stream.h"
#include "libmpdemux/muxer.h"

#include "img_format.h"
#include "mp_image.h"
#include "vf.h"

#include <xvid.h>
#include <stdio.h>
#include <stdarg.h>
#include <limits.h>
#include <assert.h>

#include "m_option.h"
#include "libavutil/avutil.h"

#define FINE (!0)
#define BAD (!FINE)

#define MAX_ZONES   64

// Profile flag definitions
#define PROFILE_ADAPTQUANT 0x00000001
#define PROFILE_BVOP       0x00000002
#define PROFILE_MPEGQUANT  0x00000004
#define PROFILE_INTERLACE  0x00000008
#define PROFILE_QPEL       0x00000010
#define PROFILE_GMC        0x00000020
#define PROFILE_4MV        0x00000040
#define PROFILE_DXN        0x00000080

// Reduce code duplication in profiles[] array
#define PROFILE_S   (PROFILE_4MV)
#define PROFILE_AS  (PROFILE_4MV|PROFILE_ADAPTQUANT|PROFILE_BVOP|PROFILE_MPEGQUANT|PROFILE_INTERLACE|PROFILE_QPEL|PROFILE_GMC)

typedef const struct
{
	const char *name;            ///< profile name
	int id;                      ///< mpeg-4 profile id; iso/iec 14496-2:2001 table G-1
	int width;                   ///< profile width restriction
	int height;                  ///< profile height restriction
	int fps;                     ///< profile frame rate restriction
	int max_objects;             ///< ??????
	int total_vmv_buffer_sz;     ///< macroblock memory; when BVOPS=false, vmv = 2*vcv; when BVOPS=true,  vmv = 3*vcv
	int max_vmv_buffer_sz;       ///< max macroblocks per vop
	int vcv_decoder_rate;        ///< macroblocks decoded per second
	int max_acpred_mbs;          ///< percentage
	int max_vbv_size;            ///< max vbv size (bits) 16368 bits
	int max_video_packet_length; ///< bits
	int max_bitrate;             ///< bits per second
	int vbv_peakrate;            ///< max bits over anyone second period; 0=don't care
	int dxn_max_bframes;         ///< dxn: max consecutive bframes
	unsigned int flags;          ///< flags for allowed options/dxn note the definitions for PROFILE_S and PROFILE_AS
} profile_t;

// Code taken from XviD VfW source for profile support

/* default vbv_occupancy is (64/170)*vbv_buffer_size */

static const profile_t profiles[] =
{
	/*   name               p@l    w    h    fps  obj Tvmv  vmv     vcv  ac%   vbv        pkt     bps    vbv_peak dbf  flags */
	/* unrestricted profile (default) */
	{ "unrestricted",  0x00,    0,   0,  0,  0,    0,    0,      0, 100,   0*16368,    -1,       0,        0, -1, 0xffffffff & ~PROFILE_DXN },

	{ "sp0",           0x08,  176, 144, 15,  1,  198,   99,   1485, 100,  10*16368,  2048,   64000,        0, -1, PROFILE_S },
	/* simple@l0: max f_code=1, intra_dc_vlc_threshold=0 */
	/* if ac preidition is used, adaptive quantization must not be used */
	/* <=qcif must be used */
	{ "sp1",           0x01,  176, 144, 15,  4,  198,   99,   1485, 100,  10*16368,  2048,   64000,        0, -1, PROFILE_S|PROFILE_ADAPTQUANT },
	{ "sp2",           0x02,  352, 288, 15,  4,  792,  396,   5940, 100,  40*16368,  4096,  128000,        0, -1, PROFILE_S|PROFILE_ADAPTQUANT },
	{ "sp3",           0x03,  352, 288, 15,  4,  792,  396,  11880, 100,  40*16368,  8192,  384000,        0, -1, PROFILE_S|PROFILE_ADAPTQUANT },

	{ "asp0",          0xf0,  176, 144, 30,  1,  297,   99,   2970, 100,  10*16368,  2048,  128000,        0, -1, PROFILE_AS },
	{ "asp1",          0xf1,  176, 144, 30,  4,  297,   99,   2970, 100,  10*16368,  2048,  128000,        0, -1, PROFILE_AS },
	{ "asp2",          0xf2,  352, 288, 15,  4, 1188,  396,   5940, 100,  40*16368,  4096,  384000,        0, -1, PROFILE_AS },
	{ "asp3",          0xf3,  352, 288, 30,  4, 1188,  396,  11880, 100,  40*16368,  4096,  768000,        0, -1, PROFILE_AS },
	/*  ISMA Profile 1, (ASP) @ L3b (CIF, 1.5 Mb/s) CIF(352x288), 30fps, 1.5Mbps max ??? */
	{ "asp4",          0xf4,  352, 576, 30,  4, 2376,  792,  23760,  50,  80*16368,  8192, 3000000,        0, -1, PROFILE_AS },
	{ "asp5",          0xf5,  720, 576, 30,  4, 4860, 1620,  48600,  25, 112*16368, 16384, 8000000,        0, -1, PROFILE_AS },

	//	information provided by DivXNetworks, USA.
	//  "DivX Certified Profile Compatibility v1.1", February 2005
	{ "dxnhandheld",   0x00,  176, 144, 15,  1,  198,   99,   1485, 100,   32*8192,    -1,  537600,   800000,  0, PROFILE_ADAPTQUANT|PROFILE_DXN },
	{ "dxnportntsc",   0x00,  352, 240, 30,  1,  990,  330,  36000, 100,  384*8192,    -1, 4854000,  8000000,  1, PROFILE_4MV|PROFILE_ADAPTQUANT|PROFILE_BVOP|PROFILE_DXN },
	{ "dxnportpal",    0x00,  352, 288, 25,  1, 1188,  396,  36000, 100,  384*8192,    -1, 4854000,  8000000,  1, PROFILE_4MV|PROFILE_ADAPTQUANT|PROFILE_BVOP|PROFILE_DXN },
	{ "dxnhtntsc",     0x00,  720, 480, 30,  1, 4050, 1350,  40500, 100,  384*8192,    -1, 4854000,  8000000,  1, PROFILE_4MV|PROFILE_ADAPTQUANT|PROFILE_BVOP|PROFILE_INTERLACE|PROFILE_DXN },
	{ "dxnhtpal",      0x00,  720, 576, 25,  1, 4860, 1620,  40500, 100,  384*8192,    -1, 4854000,  8000000,  1, PROFILE_4MV|PROFILE_ADAPTQUANT|PROFILE_BVOP|PROFILE_INTERLACE|PROFILE_DXN },
	{ "dxnhdtv",       0x00, 1280, 720, 30,  1,10800, 3600, 108000, 100,  768*8192,    -1, 9708400, 16000000,  2, PROFILE_4MV|PROFILE_ADAPTQUANT|PROFILE_BVOP|PROFILE_INTERLACE|PROFILE_DXN },

	{ NULL,            0x00,    0,   0,  0,  0,    0,    0,      0,   0,         0,     0,       0,        0,  0, 0x00000000 },
};

/**
 * \brief return the pointer to a chosen profile
 * \param str the profile name
 * \return pointer of the appropriate profiles array entry or NULL for a mistyped profile name
 */
static const profile_t *profileFromName(const char *str)
{
	profile_t *cur = profiles;
	while (cur->name && strcasecmp(cur->name, str)) cur++;
	if(!cur->name) return NULL;
	return cur;
}

/*****************************************************************************
 * Configuration options
 ****************************************************************************/

extern char* passtmpfile;

static int xvidenc_bitrate = 0;
static int xvidenc_pass = 0;
static float xvidenc_quantizer = 0;

static int xvidenc_packed = 0;
static int xvidenc_closed_gop = 1;
static int xvidenc_interlaced = 0;
static int xvidenc_quarterpel = 0;
static int xvidenc_gmc = 0;
static int xvidenc_trellis = 1;
static int xvidenc_cartoon = 0;
static int xvidenc_hqacpred = 1;
static int xvidenc_chromame = 1;
static int xvidenc_chroma_opt = 0;
static int xvidenc_vhq = 1;
static int xvidenc_bvhq = 1;
static int xvidenc_motion = 6;
static int xvidenc_turbo = 0;
static int xvidenc_stats = 0;
static int xvidenc_max_key_interval = 0; /* Let xvidcore set a 10s interval by default */
static int xvidenc_frame_drop_ratio = 0;
static int xvidenc_greyscale = 0;
static int xvidenc_luminance_masking = 0;
static int xvidenc_debug = 0;
static int xvidenc_psnr = 0;

static int xvidenc_max_bframes = 2;
static int xvidenc_num_threads = 0;
static int xvidenc_bquant_ratio = 150;
static int xvidenc_bquant_offset = 100;
static int xvidenc_bframe_threshold = 0;

static int xvidenc_min_quant[3] = {2, 2, 2};
static int xvidenc_max_quant[3] = {31, 31, 31};
static char *xvidenc_intra_matrix_file = NULL;
static char *xvidenc_inter_matrix_file = NULL;
static char *xvidenc_quant_method = NULL;

static int xvidenc_cbr_reaction_delay_factor = 0;
static int xvidenc_cbr_averaging_period = 0;
static int xvidenc_cbr_buffer = 0;

static int xvidenc_vbr_keyframe_boost = 0;
static int xvidenc_vbr_overflow_control_strength = 5;
static int xvidenc_vbr_curve_compression_high = 0;
static int xvidenc_vbr_curve_compression_low = 0;
static int xvidenc_vbr_max_overflow_improvement = 5;
static int xvidenc_vbr_max_overflow_degradation = 5;
static int xvidenc_vbr_kfreduction = 0;
static int xvidenc_vbr_kfthreshold = 0;
static int xvidenc_vbr_container_frame_overhead = 24; /* mencoder uses AVI container */

// commandline profile option string - default to unrestricted
static char *xvidenc_profile = "unrestricted";

static char *xvidenc_par = NULL;
static int xvidenc_par_width = 0;
static int xvidenc_par_height = 0;
static float xvidenc_dar_aspect = 0.0f;
static int xvidenc_autoaspect = 0;

static char *xvidenc_zones = NULL; // zones string

m_option_t xvidencopts_conf[] =
{
	/* Standard things mencoder should be able to treat directly */
	{"bitrate", &xvidenc_bitrate, CONF_TYPE_INT, 0, 0, 0, NULL},
	{"pass", &xvidenc_pass, CONF_TYPE_INT, CONF_RANGE, 1, 2, NULL},
	{"fixed_quant", &xvidenc_quantizer, CONF_TYPE_FLOAT, CONF_RANGE, 1, 31, NULL},

	/* Features */
	{"quant_type", &xvidenc_quant_method, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"me_quality", &xvidenc_motion, CONF_TYPE_INT, CONF_RANGE, 0, 6, NULL},
	{"chroma_me", &xvidenc_chromame, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"nochroma_me", &xvidenc_chromame, CONF_TYPE_FLAG, 0, 1, 0, NULL},
	{"chroma_opt", &xvidenc_chroma_opt, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"nochroma_opt", &xvidenc_chroma_opt, CONF_TYPE_FLAG, 0, 1, 0, NULL},
	{"vhq", &xvidenc_vhq, CONF_TYPE_INT, CONF_RANGE, 0, 4, NULL},
	{"bvhq", &xvidenc_bvhq, CONF_TYPE_INT, CONF_RANGE, 0, 1, NULL},
	{"max_bframes", &xvidenc_max_bframes, CONF_TYPE_INT, CONF_RANGE, 0, 20, NULL},
	{"threads", &xvidenc_num_threads, CONF_TYPE_INT, 0, 0, 0, NULL},
	{"bquant_ratio", &xvidenc_bquant_ratio, CONF_TYPE_INT, CONF_RANGE, 0, 200, NULL},
	{"bquant_offset", &xvidenc_bquant_offset, CONF_TYPE_INT, CONF_RANGE, 0, 200, NULL},
	{"bf_threshold", &xvidenc_bframe_threshold, CONF_TYPE_INT, CONF_RANGE, -255, 255, NULL},
	{"qpel", &xvidenc_quarterpel, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"noqpel", &xvidenc_quarterpel, CONF_TYPE_FLAG, 0, 1, 0, NULL},
	{"gmc", &xvidenc_gmc, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"nogmc", &xvidenc_gmc, CONF_TYPE_FLAG, 0, 1, 0, NULL},
	{"trellis", &xvidenc_trellis, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"notrellis", &xvidenc_trellis, CONF_TYPE_FLAG, 0, 1, 0, NULL},
	{"packed", &xvidenc_packed, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"nopacked", &xvidenc_packed, CONF_TYPE_FLAG, 0, 1, 0, NULL},
	{"closed_gop", &xvidenc_closed_gop, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"noclosed_gop", &xvidenc_closed_gop, CONF_TYPE_FLAG, 0, 1, 0, NULL},
	{"interlacing", &xvidenc_interlaced, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"nointerlacing", &xvidenc_interlaced, CONF_TYPE_FLAG, 0, 1, 0, NULL},
	{"cartoon", &xvidenc_cartoon, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"nocartoon", &xvidenc_cartoon, CONF_TYPE_FLAG, 0, 1, 0, NULL},
	{"hq_ac", &xvidenc_hqacpred, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"nohq_ac", &xvidenc_hqacpred, CONF_TYPE_FLAG, 0, 1, 0, NULL},
	{"frame_drop_ratio", &xvidenc_frame_drop_ratio, CONF_TYPE_INT, CONF_RANGE, 0, 100, NULL},
	{"max_key_interval", &xvidenc_max_key_interval, CONF_TYPE_INT, CONF_MIN, 0, 0, NULL},
	{"greyscale", &xvidenc_greyscale, CONF_TYPE_FLAG, 0, 0, 1, NULL}, /* kept for backward compatibility */
	{"grayscale", &xvidenc_greyscale, CONF_TYPE_FLAG, 0, 0, 1, NULL},		
	{"nogreyscale", &xvidenc_greyscale, CONF_TYPE_FLAG, 0, 1, 0, NULL},
	{"lumi_mask", &xvidenc_luminance_masking, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"nolumi_mask", &xvidenc_luminance_masking, CONF_TYPE_FLAG, 0, 1, 0, NULL},
	{"turbo", &xvidenc_turbo, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"debug", &xvidenc_debug, CONF_TYPE_INT , 0 ,0,-1,NULL},
	{"stats", &xvidenc_stats, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"psnr",  &xvidenc_psnr , CONF_TYPE_FLAG, 0, 0, 1, NULL},


	/* section [quantizer] */
	{"min_iquant", &xvidenc_min_quant[0], CONF_TYPE_INT, CONF_RANGE, 1, 31, NULL},
	{"max_iquant", &xvidenc_max_quant[0], CONF_TYPE_INT, CONF_RANGE, 1, 31, NULL},
	{"min_pquant", &xvidenc_min_quant[1], CONF_TYPE_INT, CONF_RANGE, 1, 31, NULL},
	{"max_pquant", &xvidenc_max_quant[1], CONF_TYPE_INT, CONF_RANGE, 1, 31, NULL},
	{"min_bquant", &xvidenc_min_quant[2], CONF_TYPE_INT, CONF_RANGE, 1, 31, NULL},
	{"max_bquant", &xvidenc_max_quant[2], CONF_TYPE_INT, CONF_RANGE, 1, 31, NULL},
	{"quant_intra_matrix", &xvidenc_intra_matrix_file, CONF_TYPE_STRING, 0, 0, 100, NULL},
	{"quant_inter_matrix", &xvidenc_inter_matrix_file, CONF_TYPE_STRING, 0, 0, 100, NULL},

	/* section [cbr] */
	{"rc_reaction_delay_factor", &xvidenc_cbr_reaction_delay_factor, CONF_TYPE_INT, CONF_RANGE, 0, 100, NULL},
	{"rc_averaging_period", &xvidenc_cbr_averaging_period, CONF_TYPE_INT, CONF_MIN, 0, 0, NULL},
	{"rc_buffer", &xvidenc_cbr_buffer, CONF_TYPE_INT, CONF_MIN, 0, 0, NULL},

	/* section [vbr] */
	{"keyframe_boost", &xvidenc_vbr_keyframe_boost, CONF_TYPE_INT, CONF_RANGE, 0, 100, NULL},
	{"curve_compression_high", &xvidenc_vbr_curve_compression_high, CONF_TYPE_INT, CONF_RANGE, 0, 100, NULL},
	{"curve_compression_low", &xvidenc_vbr_curve_compression_low, CONF_TYPE_INT, CONF_RANGE, 0, 100, NULL},
	{"overflow_control_strength", &xvidenc_vbr_overflow_control_strength, CONF_TYPE_INT, CONF_RANGE, 0, 100, NULL},
	{"max_overflow_improvement", &xvidenc_vbr_max_overflow_improvement, CONF_TYPE_INT, CONF_RANGE, 0, 100, NULL},
	{"max_overflow_degradation", &xvidenc_vbr_max_overflow_degradation, CONF_TYPE_INT, CONF_RANGE, 0, 100, NULL},
	{"kfreduction", &xvidenc_vbr_kfreduction, CONF_TYPE_INT, CONF_RANGE, 0, 100, NULL},
	{"kfthreshold", &xvidenc_vbr_kfthreshold, CONF_TYPE_INT, CONF_MIN, 0, 0, NULL},
	{"container_frame_overhead", &xvidenc_vbr_container_frame_overhead, CONF_TYPE_INT, CONF_MIN, 0, 0, NULL},

	/* Section Aspect Ratio */
	{"par", &xvidenc_par, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"par_width", &xvidenc_par_width, CONF_TYPE_INT, CONF_RANGE, 0, 255, NULL},
	{"par_height", &xvidenc_par_height, CONF_TYPE_INT, CONF_RANGE, 0, 255, NULL},
	{"aspect", &xvidenc_dar_aspect, CONF_TYPE_FLOAT, CONF_RANGE, 0.1, 9.99, NULL},
	{"autoaspect", &xvidenc_autoaspect, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"noautoaspect", &xvidenc_autoaspect, CONF_TYPE_FLAG, 0, 1, 0, NULL},

	/* Section Zones */
	{"zones", &xvidenc_zones, CONF_TYPE_STRING, 0, 0, 0, NULL},

	/* section profiles */
	{"profile", &xvidenc_profile, CONF_TYPE_STRING, 0, 0, 0, NULL},

	/* End of the config array */
	{NULL, 0, 0, 0, 0, 0, NULL}
};

/*****************************************************************************
 * Module private data
 ****************************************************************************/

typedef struct xvid_mplayer_module_t
{
	/* Instance related global vars */
	void *instance;
	xvid_gbl_init_t      init;
	xvid_enc_create_t    create;
	xvid_enc_frame_t     frame;
	xvid_plugin_single_t onepass;
	xvid_plugin_2pass1_t pass1;
	xvid_plugin_2pass2_t pass2;

	/* This data must survive local block scope, so here it is */
	xvid_enc_plugin_t    plugins[7];
	xvid_enc_zone_t      zones[MAX_ZONES];

	/* MPEG4 stream buffer */
	muxer_stream_t *mux;

	/* Stats accumulators */
	int frames;
	long long sse_y;
	long long sse_u;
	long long sse_v;

	/* Min & Max PSNR */
	int min_sse_y;
	int min_sse_u;
	int min_sse_v;
	int min_framenum;
	int max_sse_y;
	int max_sse_u;
	int max_sse_v;
	int max_framenum;
	
	int pixels;
	
	/* DAR/PAR and all that thingies */
	int d_width;
	int d_height;
	FILE *fvstats;
} xvid_mplayer_module_t;

static int dispatch_settings(xvid_mplayer_module_t *mod);
static int set_create_struct(xvid_mplayer_module_t *mod);
static int set_frame_struct(xvid_mplayer_module_t *mod, mp_image_t *mpi);
static void update_stats(xvid_mplayer_module_t *mod, xvid_enc_stats_t *stats);
static void print_stats(xvid_mplayer_module_t *mod);
static void flush_internal_buffers(xvid_mplayer_module_t *mod);
static const char *par_string(int parcode);
static const char *errorstring(int err);

/*****************************************************************************
 * Video Filter API function definitions
 ****************************************************************************/

/*============================================================================
 * config
 *==========================================================================*/

static int
config(struct vf_instance_s* vf,
       int width, int height, int d_width, int d_height,
       unsigned int flags, unsigned int outfmt)
{
	int err;
	xvid_mplayer_module_t *mod = (xvid_mplayer_module_t *)vf->priv;
	
	/* Complete the muxer initialization */
	mod->mux->bih->biWidth = width;
	mod->mux->bih->biHeight = height;
	mod->mux->bih->biSizeImage = 
		mod->mux->bih->biWidth * mod->mux->bih->biHeight * 3 / 2;
	mod->mux->aspect = (float)d_width/d_height;

	/* Message the FourCC type */
	mp_msg(MSGT_MENCODER, MSGL_INFO,
	       "videocodec: XviD (%dx%d fourcc=%x [%.4s])\n",
	       width, height, mod->mux->bih->biCompression,
	       (char *)&mod->mux->bih->biCompression);

	/* Total number of pixels per frame required for PSNR */
	mod->pixels = mod->mux->bih->biWidth*mod->mux->bih->biHeight;

	/*--------------------------------------------------------------------
	 * Dispatch all module settings to XviD structures
	 *------------------------------------------------------------------*/

	mod->d_width = d_width;
	mod->d_height = d_height;

	if(dispatch_settings(mod) == BAD)
		return BAD;

	/*--------------------------------------------------------------------
	 * Set remaining information in the xvid_enc_create_t structure
	 *------------------------------------------------------------------*/

	if(set_create_struct(mod) == BAD)
		return BAD;

	/*--------------------------------------------------------------------
	 * Encoder instance creation
	 *------------------------------------------------------------------*/

	err = xvid_encore(NULL, XVID_ENC_CREATE, &mod->create, NULL);

	if(err<0) {
		mp_msg(MSGT_MENCODER, MSGL_ERR,
		       "xvid: xvidcore returned a '%s' error\n", errorstring(err));
		return BAD;
	}
	
	/* Store the encoder instance into the private data */
	mod->instance = mod->create.handle;

	mod->mux->decoder_delay = mod->create.max_bframes ? 1 : 0;

	return FINE;
}

/*============================================================================
 * uninit
 *==========================================================================*/

static void
uninit(struct vf_instance_s* vf)
{

	xvid_mplayer_module_t *mod = (xvid_mplayer_module_t *)vf->priv;

	/* Destroy xvid instance */
	xvid_encore(mod->instance, XVID_ENC_DESTROY, NULL, NULL);

	/* Display stats (if any) */
	print_stats(mod);

	/* Close PSNR file if ever opened */
	if (mod->fvstats) {
		fclose(mod->fvstats);
		mod->fvstats = NULL;
	}

        /* Free allocated memory */
	if(mod->frame.quant_intra_matrix)
	    free(mod->frame.quant_intra_matrix);

	if(mod->frame.quant_inter_matrix)
	    free(mod->frame.quant_inter_matrix);

	if(mod->mux->bih)
	    free(mod->mux->bih);

	free(vf->priv);
	vf->priv=NULL;

	return;
}

/*============================================================================
 * control
 *==========================================================================*/

static int
control(struct vf_instance_s* vf, int request, void* data)
{
xvid_mplayer_module_t *mod = (xvid_mplayer_module_t *)vf->priv;

	switch(request){
	    case  VFCTRL_FLUSH_FRAMES:
	    if(mod)/*paranoid*/
                flush_internal_buffers(mod);
	    break;
	}
	return CONTROL_UNKNOWN;
}

/*============================================================================
 * query_format
 *==========================================================================*/

static int
query_format(struct vf_instance_s* vf, unsigned int fmt)
{
	switch(fmt){
	case IMGFMT_YV12:
	case IMGFMT_IYUV:
	case IMGFMT_I420:
		return VFCAP_CSP_SUPPORTED | VFCAP_CSP_SUPPORTED_BY_HW;
	case IMGFMT_YUY2:
	case IMGFMT_UYVY:
		return VFCAP_CSP_SUPPORTED;
	}
	return BAD;
}

/*============================================================================
 * put_image
 *==========================================================================*/

static int
put_image(struct vf_instance_s* vf, mp_image_t *mpi, double pts)
{
	int size;
	xvid_enc_stats_t stats; 
	xvid_mplayer_module_t *mod = (xvid_mplayer_module_t *)vf->priv;

	/* Prepare the stats */
	memset(&stats,0,sizeof( xvid_enc_stats_t));
	stats.version = XVID_VERSION;

	/* -------------------------------------------------------------------
	 * Set remaining information in the xvid_enc_frame_t structure
	 * NB: all the other struct members were initialized by
	 *     dispatch_settings
	 * -----------------------------------------------------------------*/

	if(set_frame_struct(mod, mpi) == BAD)
		return BAD;

	/* -------------------------------------------------------------------
	 * Encode the frame
	 * ---------------------------------------------------------------- */

	size = xvid_encore(mod->instance, XVID_ENC_ENCODE, &mod->frame, &stats);

	/* Analyse the returned value */
	if(size<0) {
		mp_msg(MSGT_MENCODER, MSGL_ERR,
		       "xvid: xvidcore returned a '%s' error\n", errorstring(size));
		return BAD;
	}

	/* If size is == 0, we're done with that frame */
	if(size == 0) {
		++mod->mux->encoder_delay;
		return FINE;
	}

	/* xvidcore returns stats about encoded frame in an asynchronous way
	 * accumulate these stats */
	update_stats(mod, &stats);

	/* xvidcore outputed bitstream -- mux it */
	muxer_write_chunk(mod->mux,
			  size,
			  (mod->frame.out_flags & XVID_KEYFRAME)?0x10:0, MP_NOPTS_VALUE, MP_NOPTS_VALUE);

	return FINE;
}

/*============================================================================
 * vf_open
 *==========================================================================*/

static int
vf_open(vf_instance_t *vf, char* args)
{
	xvid_mplayer_module_t *mod;
	xvid_gbl_init_t xvid_gbl_init;
	xvid_gbl_info_t xvid_gbl_info;

	/* Setting libmpcodec module API pointers */
	vf->config       = config;
	vf->default_caps = VFCAP_CONSTANT;
	vf->control      = control;
	vf->uninit       = uninit;
	vf->query_format = query_format;
	vf->put_image    = put_image;

	/* Allocate the private part of the codec module */
	vf->priv = malloc(sizeof(xvid_mplayer_module_t));
	mod = (xvid_mplayer_module_t*)vf->priv;

	if(mod == NULL) {
		mp_msg(MSGT_MENCODER,MSGL_ERR,
		       "xvid: memory allocation failure (private data)\n");
		return BAD;
	}

	/* Initialize the module to zeros */
	memset(mod, 0, sizeof(xvid_mplayer_module_t));
	mod->min_sse_y = mod->min_sse_u = mod->min_sse_v = INT_MAX;
	mod->max_sse_y = mod->max_sse_u = mod->max_sse_v = INT_MIN;

	/* Bind the Muxer */
	mod->mux = (muxer_stream_t*)args;

	/* Initialize muxer BITMAP header */
	mod->mux->bih = calloc(1, sizeof(BITMAPINFOHEADER));

	if(mod->mux->bih  == NULL) {
		mp_msg(MSGT_MENCODER,MSGL_ERR,
		       "xvid: memory allocation failure (BITMAP header)\n");
		return BAD;
	}

	mod->mux->bih->biSize = sizeof(BITMAPINFOHEADER);
	mod->mux->bih->biWidth = 0;
	mod->mux->bih->biHeight = 0;
	mod->mux->bih->biPlanes = 1;
	mod->mux->bih->biBitCount = 12;
	mod->mux->bih->biCompression = mmioFOURCC('X','V','I','D');

	/* Retrieve information about the host XviD library */
	memset(&xvid_gbl_info, 0, sizeof(xvid_gbl_info_t));
	xvid_gbl_info.version = XVID_VERSION;

	if (xvid_global(NULL, XVID_GBL_INFO, &xvid_gbl_info, NULL) < 0) {
		mp_msg(MSGT_MENCODER,MSGL_WARN, "xvid: could not get information about the library\n");
	} else {
		mp_msg(MSGT_MENCODER,MSGL_INFO, "xvid: using library version %d.%d.%d (build %s)\n",
		       XVID_VERSION_MAJOR(xvid_gbl_info.actual_version),
		       XVID_VERSION_MINOR(xvid_gbl_info.actual_version),
		       XVID_VERSION_PATCH(xvid_gbl_info.actual_version),
		       xvid_gbl_info.build);
	}
		
	/* Initialize the xvid_gbl_init structure */
	memset(&xvid_gbl_init, 0, sizeof(xvid_gbl_init_t));
	xvid_gbl_init.version = XVID_VERSION;
	xvid_gbl_init.debug = xvidenc_debug;

	/* Initialize the xvidcore library */
	if (xvid_global(NULL, XVID_GBL_INIT, &xvid_gbl_init, NULL) < 0) {
		mp_msg(MSGT_MENCODER,MSGL_ERR, "xvid: initialisation failure\n");
		return BAD;
	}

	return FINE;
}

/*****************************************************************************
 * Helper functions
 ****************************************************************************/

static void *read_matrix(unsigned char *filename);

static int dispatch_settings(xvid_mplayer_module_t *mod)
{
	xvid_enc_create_t *create     = &mod->create;
	xvid_enc_frame_t  *frame      = &mod->frame;
	xvid_plugin_single_t *onepass = &mod->onepass;
	xvid_plugin_2pass2_t *pass2   = &mod->pass2;
	AVRational ar;

	const int motion_presets[7] =
		{
			0,
			0,
			0,
			0,
			XVID_ME_HALFPELREFINE16,
			XVID_ME_HALFPELREFINE16 | XVID_ME_ADVANCEDDIAMOND16,
			XVID_ME_HALFPELREFINE16 | XVID_ME_EXTSEARCH16 |
			XVID_ME_HALFPELREFINE8  | XVID_ME_USESQUARES16
		};

	//profile is unrestricted as default
	const profile_t *selected_profile =  profileFromName("unrestricted");
	if(xvidenc_profile)
		selected_profile = profileFromName(xvidenc_profile);
	if(!selected_profile)
	{
		mp_msg(MSGT_MENCODER,MSGL_ERR,
			"xvid:[ERROR] \"%s\" is an invalid profile name\n", xvidenc_profile);
		return BAD;
	}
	
	/* -------------------------------------------------------------------
	 * Dispatch all settings having an impact on the "create" structure
	 * This includes plugins as they are passed to encore through the
	 * create structure
	 * -----------------------------------------------------------------*/

	/* -------------------------------------------------------------------
	 * The create structure
	 * ---------------------------------------------------------------- */

	create->global = 0;

        if(xvidenc_psnr)
	    xvidenc_stats = 1;

	if(xvidenc_stats)
		create->global |= XVID_GLOBAL_EXTRASTATS_ENABLE;

	create->num_zones = 0;
	create->zones = NULL;
	create->num_plugins = 0;
	create->plugins = NULL;
	create->num_threads = xvidenc_num_threads;

	if( (selected_profile->flags & PROFILE_BVOP) &&
		/* dxn: prevent bframes usage if interlacing is selected */
		!((selected_profile->flags & PROFILE_DXN) && xvidenc_interlaced) )
	{
	create->max_bframes = xvidenc_max_bframes;
	create->bquant_ratio = xvidenc_bquant_ratio;
	create->bquant_offset = xvidenc_bquant_offset;
		if(xvidenc_packed)
			create->global |= XVID_GLOBAL_PACKED;
		if(xvidenc_closed_gop)
			create->global |= XVID_GLOBAL_CLOSED_GOP;

		/* dxn: restrict max bframes, require closed gop
			and require packed b-frames */
		if(selected_profile->flags & PROFILE_DXN)
		{
			if(create->max_bframes > selected_profile->dxn_max_bframes)
				create->max_bframes = selected_profile->dxn_max_bframes;
			create->global |= XVID_GLOBAL_CLOSED_GOP;
			create->global |= XVID_GLOBAL_PACKED;
		}
	}
	else
		create->max_bframes = 0;

#if XVID_API >= XVID_MAKE_API(4,1)
	/* dxn: always write divx5 userdata */
	if(selected_profile->flags & PROFILE_DXN)
		create->global |= XVID_GLOBAL_DIVX5_USERDATA;
#endif
	
	create->max_key_interval = xvidenc_max_key_interval;
	create->frame_drop_ratio = xvidenc_frame_drop_ratio;
	create->min_quant[0] = xvidenc_min_quant[0];
	create->min_quant[1] = xvidenc_min_quant[1];
	create->min_quant[2] = xvidenc_min_quant[2];
	create->max_quant[0] = xvidenc_max_quant[0];
	create->max_quant[1] = xvidenc_max_quant[1];
	create->max_quant[2] = xvidenc_max_quant[2];


	/* -------------------------------------------------------------------
	 * The single pass plugin
	 * ---------------------------------------------------------------- */

	if (xvidenc_bitrate > 16000) onepass->bitrate = xvidenc_bitrate;
	else onepass->bitrate = xvidenc_bitrate*1000;
	onepass->reaction_delay_factor = xvidenc_cbr_reaction_delay_factor;
	onepass->averaging_period = xvidenc_cbr_averaging_period;
	onepass->buffer = xvidenc_cbr_buffer;

	/* -------------------------------------------------------------------
	 * The pass2 plugin
	 * ---------------------------------------------------------------- */

	pass2->keyframe_boost = xvidenc_vbr_keyframe_boost;
	pass2->overflow_control_strength = xvidenc_vbr_overflow_control_strength;
	pass2->curve_compression_high = xvidenc_vbr_curve_compression_high;
	pass2->curve_compression_low = xvidenc_vbr_curve_compression_low;
	pass2->max_overflow_improvement = xvidenc_vbr_max_overflow_improvement;
	pass2->max_overflow_degradation = xvidenc_vbr_max_overflow_degradation;
	pass2->kfreduction = xvidenc_vbr_kfreduction;
	pass2->kfthreshold = xvidenc_vbr_kfthreshold;
	pass2->container_frame_overhead = xvidenc_vbr_container_frame_overhead;

	/* VBV */

#if XVID_API >= XVID_MAKE_API(4,1)
	pass2->vbv_size = selected_profile->max_vbv_size;
	pass2->vbv_initial = (selected_profile->max_vbv_size*3)>>2; /* 75% */
	pass2->vbv_maxrate = selected_profile->max_bitrate;
	pass2->vbv_peakrate = selected_profile->vbv_peakrate*3;
#endif
// XXX: xvidcore currently provides a "peak bits over 3 seconds" constraint.
// according to the latest dxn literature, a 1 second constraint is now used

	create->profile = selected_profile->id;

	/* -------------------------------------------------------------------
	 * The frame structure
	 * ---------------------------------------------------------------- */
	frame->vol_flags = 0;
	frame->vop_flags = 0;
	frame->motion    = 0;

	frame->vop_flags |= XVID_VOP_HALFPEL;
	frame->motion    |= motion_presets[xvidenc_motion];

	if(xvidenc_stats)
		frame->vol_flags |= XVID_VOL_EXTRASTATS;

	if(xvidenc_greyscale)
		frame->vop_flags |= XVID_VOP_GREYSCALE;

	if(xvidenc_cartoon) {
		frame->vop_flags |= XVID_VOP_CARTOON;
		frame->motion |= XVID_ME_DETECT_STATIC_MOTION;
	}

	// MPEG quantisation is only supported in ASP and unrestricted profiles
	if((selected_profile->flags & PROFILE_MPEGQUANT) &&
		(xvidenc_quant_method != NULL) &&
		!strcasecmp(xvidenc_quant_method, "mpeg"))
	{
		frame->vol_flags |= XVID_VOL_MPEGQUANT;
	if(xvidenc_intra_matrix_file != NULL) {
		frame->quant_intra_matrix = (unsigned char*)read_matrix(xvidenc_intra_matrix_file);
		if(frame->quant_intra_matrix != NULL) {
			mp_msg(MSGT_MENCODER, MSGL_INFO, "xvid: Loaded Intra matrix (switching to mpeg quantization type)\n");
			if(xvidenc_quant_method) free(xvidenc_quant_method);
			xvidenc_quant_method = strdup("mpeg");
		}
	}
	if(xvidenc_inter_matrix_file != NULL) {
		frame->quant_inter_matrix = read_matrix(xvidenc_inter_matrix_file);
		if(frame->quant_inter_matrix) {
			mp_msg(MSGT_MENCODER, MSGL_INFO, "\nxvid: Loaded Inter matrix (switching to mpeg quantization type)\n");
			if(xvidenc_quant_method) free(xvidenc_quant_method);
			xvidenc_quant_method = strdup("mpeg");
		}
	}
	}
	if(xvidenc_quarterpel && (selected_profile->flags & PROFILE_QPEL)) {
		frame->vol_flags |= XVID_VOL_QUARTERPEL;
		frame->motion    |= XVID_ME_QUARTERPELREFINE16;
		frame->motion    |= XVID_ME_QUARTERPELREFINE8;
	}
	if(xvidenc_gmc && (selected_profile->flags & PROFILE_GMC)) {
		frame->vol_flags |= XVID_VOL_GMC;
		frame->motion    |= XVID_ME_GME_REFINE;
	}
	if(xvidenc_interlaced && (selected_profile->flags & PROFILE_INTERLACE)) {
		frame->vol_flags |= XVID_VOL_INTERLACING;
	}
	if(xvidenc_trellis) {
		frame->vop_flags |= XVID_VOP_TRELLISQUANT;
	}
	if(xvidenc_hqacpred) {
		frame->vop_flags |= XVID_VOP_HQACPRED;
	}
	if(xvidenc_chroma_opt) {
		frame->vop_flags |= XVID_VOP_CHROMAOPT;
	}
	if((xvidenc_motion > 4) && (selected_profile->flags & PROFILE_4MV)) {
		frame->vop_flags |= XVID_VOP_INTER4V;
	}
	if(xvidenc_chromame) {
		frame->motion |= XVID_ME_CHROMA_PVOP;
		frame->motion |= XVID_ME_CHROMA_BVOP;
	}
	if(xvidenc_vhq >= 1) {
		frame->vop_flags |= XVID_VOP_MODEDECISION_RD;
	}
	if(xvidenc_vhq >= 2) {
		frame->motion |= XVID_ME_HALFPELREFINE16_RD;
		frame->motion |= XVID_ME_QUARTERPELREFINE16_RD;
	}
	if(xvidenc_vhq >= 3) {
		frame->motion |= XVID_ME_HALFPELREFINE8_RD;
		frame->motion |= XVID_ME_QUARTERPELREFINE8_RD;
		frame->motion |= XVID_ME_CHECKPREDICTION_RD;
	}
	if(xvidenc_vhq >= 4) {
		frame->motion |= XVID_ME_EXTSEARCH_RD;
	}
	if(xvidenc_bvhq >= 1) {
#if XVID_API >= XVID_MAKE_API(4,1)
		frame->vop_flags |= XVID_VOP_RD_BVOP;
#endif
	}
	if(xvidenc_turbo) {
		frame->motion |= XVID_ME_FASTREFINE16;
		frame->motion |= XVID_ME_FASTREFINE8;
		frame->motion |= XVID_ME_SKIP_DELTASEARCH;
		frame->motion |= XVID_ME_FAST_MODEINTERPOLATE;
		frame->motion |= XVID_ME_BFRAME_EARLYSTOP;
	}

	/* motion level == 0 means no motion search which is equivalent to
	 * intra coding only */
	if(xvidenc_motion == 0) {
		frame->type = XVID_TYPE_IVOP;
	} else {
		frame->type = XVID_TYPE_AUTO;
	}

	frame->bframe_threshold = xvidenc_bframe_threshold;

	/* PAR related initialization */
	frame->par = XVID_PAR_11_VGA; /* Default */

	if( !(selected_profile->flags & PROFILE_DXN) )
	{
	if(xvidenc_dar_aspect > 0) 
	    ar = av_d2q(xvidenc_dar_aspect * mod->mux->bih->biHeight / mod->mux->bih->biWidth, 255);
	else if(xvidenc_autoaspect)
	    ar = av_d2q((float)mod->d_width / mod->d_height * mod->mux->bih->biHeight / mod->mux->bih->biWidth, 255);
	else ar.num = ar.den = 0;
	
	if(ar.den != 0) {
		if(ar.num == 12 && ar.den == 11)
		    frame->par = XVID_PAR_43_PAL;
		else if(ar.num == 10 && ar.den == 11)
		    frame->par = XVID_PAR_43_NTSC;
		else if(ar.num == 16 && ar.den == 11)
		    frame->par = XVID_PAR_169_PAL;
		else if(ar.num == 40 && ar.den == 33)
		    frame->par = XVID_PAR_169_NTSC;
		else
		{    
		    frame->par = XVID_PAR_EXT;
		    frame->par_width = ar.num;
		    frame->par_height= ar.den;
		}
			
	} else if(xvidenc_par != NULL) {
		if(strcasecmp(xvidenc_par, "pal43") == 0)
			frame->par = XVID_PAR_43_PAL;
		else if(strcasecmp(xvidenc_par, "pal169") == 0)
			frame->par = XVID_PAR_169_PAL;
		else if(strcasecmp(xvidenc_par, "ntsc43") == 0)
			frame->par = XVID_PAR_43_NTSC;
		else if(strcasecmp(xvidenc_par, "ntsc169") == 0)
			frame->par = XVID_PAR_169_NTSC;
		else if(strcasecmp(xvidenc_par, "ext") == 0)
			frame->par = XVID_PAR_EXT;

	if(frame->par == XVID_PAR_EXT) {
		if(xvidenc_par_width)
			frame->par_width = xvidenc_par_width;
		else
			frame->par_width = 1;

		if(xvidenc_par_height)
			frame->par_height = xvidenc_par_height;
		else
			frame->par_height = 1;
	}
	}

	/* Display par information */
	mp_msg(MSGT_MENCODER, MSGL_INFO, "xvid: par=%d/%d (%s), displayed=%dx%d, sampled=%dx%d\n", 
			ar.num, ar.den, par_string(frame->par),
			mod->d_width, mod->d_height, mod->mux->bih->biWidth, mod->mux->bih->biHeight);
	}
	else
		mp_msg(MSGT_MENCODER, MSGL_INFO,
			"xvid: par=0/0 (vga11) forced by choosing a DXN profile\n");
	return FINE;
}

static int set_create_struct(xvid_mplayer_module_t *mod)
{
	int pass;
	int doZones = 0;
	xvid_enc_create_t *create    = &mod->create;

	// profile is unrestricted as default
	profile_t *selected_profile =  profileFromName("unrestricted");
	if(xvidenc_profile)
		selected_profile = profileFromName(xvidenc_profile);
	if(!selected_profile)
		return BAD;

	/* Most of the structure is initialized by dispatch settings, only a
	 * few things are missing  */
	create->version = XVID_VERSION;

	/* Width and Height */
	create->width  = mod->mux->bih->biWidth;
	create->height = mod->mux->bih->biHeight;

	/* Check resolution of video to be coded is within profile width/height
	   restrictions */
	if( ((selected_profile->width != 0) &&
		(mod->mux->bih->biWidth > selected_profile->width)) ||
		((selected_profile->height != 0) &&
		(mod->mux->bih->biHeight > selected_profile->height)) )
	{
		mp_msg(MSGT_MENCODER,MSGL_ERR,
			"xvid:[ERROR] resolution must be <= %dx%d for the chosen profile\n",
			selected_profile->width, selected_profile->height);
		return BAD;
	}

	/* FPS */
	create->fincr = mod->mux->h.dwScale;
	create->fbase = mod->mux->h.dwRate;

	// Check frame rate is within profile restrictions
	if( ((float)mod->mux->h.dwRate/(float)mod->mux->h.dwScale > (float)selected_profile->fps) &&
		(selected_profile->fps != 0))
	{
		mp_msg(MSGT_MENCODER,MSGL_ERR,
			"xvid:[ERROR] frame rate must be <= %d for the chosen profile\n",
			selected_profile->fps);
		return BAD;
	}

	/* Encodings zones */
	memset(mod->zones, 0, sizeof(mod->zones));
	create->zones     = mod->zones;
	create->num_zones = 0;

	/* Plugins */
	memset(mod->plugins, 0, sizeof(mod->plugins));
	create->plugins     = mod->plugins;
	create->num_plugins = 0;

	/* -------------------------------------------------------------------
	 * Initialize and bind the right rate controller plugin
	 * ---------------------------------------------------------------- */

	/* First we try to sort out configuration conflicts */
	if(xvidenc_quantizer != 0 && (xvidenc_bitrate || xvidenc_pass)) {
		mp_msg(MSGT_MENCODER, MSGL_ERR,
		       "xvid: you can't mix Fixed Quantizer Rate Control"
		       " with other Rate Control mechanisms\n");
		return BAD;
	}

	if(xvidenc_bitrate != 0 && xvidenc_pass == 1) {
		mp_msg(MSGT_MENCODER, MSGL_WARN,
		       "xvid: bitrate setting is ignored during first pass\n");
	}

	/* Sort out which sort of pass we are supposed to do
	 * pass == 1<<0 CBR
	 * pass == 1<<1 Two pass first pass
	 * pass == 1<<2 Two pass second pass
	 * pass == 1<<3 Constant quantizer
	 */
#define MODE_CBR    (1<<0)
#define MODE_2PASS1 (1<<1)
#define MODE_2PASS2 (1<<2)
#define MODE_QUANT  (1<<3)

	pass = 0;

	if(xvidenc_bitrate != 0 && xvidenc_pass == 0)
		pass |= MODE_CBR;

	if(xvidenc_pass == 1)
		pass |= MODE_2PASS1;

	if(xvidenc_bitrate != 0 && xvidenc_pass == 2)
		pass |= MODE_2PASS2;

	if(xvidenc_quantizer != 0  && xvidenc_pass == 0)
		pass |= MODE_QUANT;

	/* We must be in at least one RC mode */
	if(pass == 0) {
		mp_msg(MSGT_MENCODER, MSGL_ERR,
		       "xvid: you must specify one or a valid combination of "
		       "'bitrate', 'pass', 'fixed_quant' settings\n");
		return BAD;
	}

	/* Sanity checking */
	if(pass != MODE_CBR    && pass != MODE_QUANT &&
	   pass != MODE_2PASS1 && pass != MODE_2PASS2) {
		mp_msg(MSGT_MENCODER, MSGL_ERR,
		       "xvid: this code should not be reached - fill a bug "
		       "report\n");
		return BAD;
	}

	/* This is a single pass encoding: either a CBR pass or a constant
	 * quantizer pass */
	if(pass == MODE_CBR  || pass == MODE_QUANT) {
		xvid_plugin_single_t *onepass = &mod->onepass;

		/* There is not much left to initialize after dispatch settings */
		onepass->version = XVID_VERSION;
		if (xvidenc_bitrate > 16000) onepass->bitrate = xvidenc_bitrate;
		else onepass->bitrate = xvidenc_bitrate*1000;

		/* Quantizer mode uses the same plugin, we have only to define
		 * a constant quantizer zone beginning at frame 0 */
		if(pass == MODE_QUANT) {
                        AVRational squant;
			squant = av_d2q(xvidenc_quantizer,128);

			create->zones[create->num_zones].mode      = XVID_ZONE_QUANT;
			create->zones[create->num_zones].frame     = 0;
			create->zones[create->num_zones].increment = squant.num;
			create->zones[create->num_zones].base      = squant.den;
			create->num_zones++;

			mp_msg(MSGT_MENCODER, MSGL_INFO,
			       "xvid: Fixed Quant Rate Control -- quantizer=%d/%d=%2.2f\n",
			       squant.num,
			       squant.den,
			       (float)(squant.num)/(float)(squant.den));
			
		} else {
			mp_msg(MSGT_MENCODER, MSGL_INFO,
			       "xvid: CBR Rate Control -- bitrate=%dkbit/s\n",
			       xvidenc_bitrate>16000?xvidenc_bitrate/1000:xvidenc_bitrate);
			doZones = 1;
		}

		create->plugins[create->num_plugins].func  = xvid_plugin_single;
		create->plugins[create->num_plugins].param = onepass;
		create->num_plugins++;
	}

	/* This is the first pass of a Two pass process */
	if(pass == MODE_2PASS1) {
		xvid_plugin_2pass1_t *pass1 = &mod->pass1;

		/* There is not much to initialize for this plugin */
		pass1->version  = XVID_VERSION;
		pass1->filename = passtmpfile;

		create->plugins[create->num_plugins].func  = xvid_plugin_2pass1;
		create->plugins[create->num_plugins].param = pass1;
		create->num_plugins++;

		mp_msg(MSGT_MENCODER, MSGL_INFO,
		       "xvid: 2Pass Rate Control -- 1st pass\n");
	}

	/* This is the second pass of a Two pass process */
	if(pass == MODE_2PASS2) {
		xvid_plugin_2pass2_t *pass2 = &mod->pass2;

		/* There is not much left to initialize after dispatch settings */
		pass2->version  = XVID_VERSION;
		pass2->filename =  passtmpfile;

		/* Positive bitrate values are bitrates as usual but if the
		 * value is negative it is considered as being a total size
		 * to reach (in kilobytes) */
		if(xvidenc_bitrate > 0) {
			if(xvidenc_bitrate > 16000) pass2->bitrate = xvidenc_bitrate;
			else pass2->bitrate = xvidenc_bitrate*1000;
			mp_msg(MSGT_MENCODER, MSGL_INFO,
			       "xvid: 2Pass Rate Control -- 2nd pass -- bitrate=%dkbit/s\n",
			       xvidenc_bitrate>16000?xvidenc_bitrate/1000:xvidenc_bitrate);
		} else {
			pass2->bitrate  = xvidenc_bitrate;
			mp_msg(MSGT_MENCODER, MSGL_INFO,
			       "xvid: 2Pass Rate Control -- 2nd pass -- total size=%dkB\n",
			       -xvidenc_bitrate);
		}

		create->plugins[create->num_plugins].func  = xvid_plugin_2pass2;
		create->plugins[create->num_plugins].param = pass2;
		create->num_plugins++;
		doZones = 1;
	}

	if(xvidenc_luminance_masking && (selected_profile->flags & PROFILE_ADAPTQUANT)) {
		create->plugins[create->num_plugins].func = xvid_plugin_lumimasking;
		create->plugins[create->num_plugins].param = NULL;
		create->num_plugins++;
	}

	// parse zones
	if (xvidenc_zones != NULL && doZones > 0) // do not apply zones in CQ, and first pass mode (xvid vfw doesn't allow them in those modes either)
	{
		char *p;
		int i;
		p = xvidenc_zones;
		create->num_zones = 0; // set the number of zones back to zero, this overwrites the zone defined for CQ - desired because each zone has to be specified on the commandline even in cq mode
		for(i = 0; p; i++)
		{
        		int start;
        		int q;
			double value;
			char mode;
        		int e = sscanf(p, "%d,%c,%lf", &start, &mode, &value); // start,mode(q = constant quant, w = weight),value
        		if(e != 3)
			{
	    			mp_msg(MSGT_MENCODER,MSGL_ERR, "error parsing zones\n");
            		return BAD;
        		}
			q = (int)(value * 100);
			if (mode == 'q')
			{
				if (q < 200 || q > 3100) // make sure that quantizer is in allowable range
				{
					mp_msg(MSGT_MENCODER, MSGL_ERR, "zone quantizer must be between 2 and 31\n");
					return BAD;
				}
				else
				{
					create->zones[create->num_zones].mode      = XVID_ZONE_QUANT;
				}
			}
			if (mode == 'w')
			{
				if (q < 1 || q > 200)
				{
					mp_msg(MSGT_MENCODER, MSGL_ERR, "zone weight must be between 1 and 200\n");
					return BAD;
				}
				else
				{
					create->zones[create->num_zones].mode      = XVID_ZONE_WEIGHT;
				}
			}
			create->zones[create->num_zones].frame     = start;
			create->zones[create->num_zones].increment = q;
			create->zones[create->num_zones].base      = 100; // increment is 100 times the actual value
			create->num_zones++;
			if (create->num_zones > MAX_ZONES) // show warning if we have too many zones
			{
				mp_msg(MSGT_MENCODER, MSGL_ERR, "too many zones, zones will be ignored\n");
			}
        		p = strchr(p, '/');
        		if(p) p++;
    		}
	}
	return FINE;
}

static int set_frame_struct(xvid_mplayer_module_t *mod, mp_image_t *mpi)
{
	xvid_enc_frame_t *frame = &mod->frame;

	/* Most of the initialization is done during dispatch_settings */
	frame->version = XVID_VERSION;

	/* Bind output buffer */
	frame->bitstream = mod->mux->buffer;
	frame->length    = -1;

	/* Frame format */
	switch(mpi->imgfmt) {
	case IMGFMT_YV12:
	case IMGFMT_IYUV:
	case IMGFMT_I420:
		frame->input.csp = XVID_CSP_USER;
		break;
	case IMGFMT_YUY2:
		frame->input.csp = XVID_CSP_YUY2;
		break;
	case IMGFMT_UYVY:
		frame->input.csp = XVID_CSP_UYVY;
		break;
	default:
		mp_msg(MSGT_MENCODER, MSGL_ERR,
		       "xvid: unsupported picture format (%s)!\n",
		       vo_format_name(mpi->imgfmt));
		return BAD;
	}

	/* Bind source frame */
	frame->input.plane[0]  = mpi->planes[0];
	frame->input.plane[1]  = mpi->planes[1];
	frame->input.plane[2]  = mpi->planes[2];
	frame->input.stride[0] = mpi->stride[0];
	frame->input.stride[1] = mpi->stride[1];
	frame->input.stride[2] = mpi->stride[2];

	/* Force the right quantizer -- It is internally managed by RC
	 * plugins */
	frame->quant = 0;

	return FINE;
}

static void
flush_internal_buffers(xvid_mplayer_module_t *mod)		
{
	int size;
	xvid_enc_frame_t *frame = &mod->frame;

	if (mod->instance == NULL)
	    return; /* encoder not initialized */

	/* Init a fake frame to force flushing */
	frame->version = XVID_VERSION;
	frame->bitstream = mod->mux->buffer;
	frame->length    = -1;
	frame->input.csp = XVID_CSP_NULL;
	frame->input.plane[0] = NULL;
	frame->input.plane[1] = NULL;
	frame->input.plane[2] = NULL;
	frame->input.stride[0] = 0;
	frame->input.stride[1] = 0;
	frame->input.stride[2] = 0;
	frame->quant = 0;

	/* Flush encoder buffers caused by bframes usage */
	do {
		xvid_enc_stats_t stats;
		memset(&stats, 0, sizeof(xvid_enc_stats_t));
		stats.version = XVID_VERSION;

		/* Encode internal buffer */
		size = xvid_encore(mod->instance, XVID_ENC_ENCODE, &mod->frame, &stats);

		if (size>0) {
			/* Update stats */
			update_stats(mod, &stats);

			/* xvidcore outputed bitstream -- mux it */
			muxer_write_chunk(mod->mux, size,
					(mod->frame.out_flags & XVID_KEYFRAME)?0x10:0, MP_NOPTS_VALUE, MP_NOPTS_VALUE);
		}
	} while (size>0);
}

#define SSE2PSNR(sse, nbpixels) \
		((!(sse)) ? 99.99f : 48.131f - 10*(double)log10((double)(sse)/(double)((nbpixels))))
static void
update_stats(xvid_mplayer_module_t *mod, xvid_enc_stats_t *stats)
{
	if(xvidenc_stats && stats->type > 0) {
		mod->sse_y += stats->sse_y;
		mod->sse_u += stats->sse_u;
		mod->sse_v += stats->sse_v;

		if(mod->min_sse_y > stats->sse_y) {
			mod->min_sse_y = stats->sse_y;
			mod->min_sse_u = stats->sse_u;
			mod->min_sse_v = stats->sse_v;
			mod->min_framenum = mod->frames;
		}

		if(mod->max_sse_y < stats->sse_y) {
			mod->max_sse_y = stats->sse_y;
			mod->max_sse_u = stats->sse_u;
			mod->max_sse_v = stats->sse_v;
			mod->max_framenum = mod->frames;
		}
		
		if (xvidenc_psnr) {
			if (!mod->fvstats) {
				char filename[20];
				time_t today2;
				struct tm *today;
				today2 = time (NULL);
				today = localtime (&today2);
				sprintf (filename, "psnr_%02d%02d%02d.log", today->tm_hour, today->tm_min, today->tm_sec);
				mod->fvstats = fopen (filename,"w");
				if (!mod->fvstats) {
					perror ("fopen");
					/* Disable PSNR file output so we don't get here again */
					xvidenc_psnr = 0;
				}
			}
			fprintf (mod->fvstats, "%6d, %2d, %6d, %2.2f, %2.2f, %2.2f, %2.2f %c\n",
					mod->frames,
					stats->quant,
					stats->length,
					SSE2PSNR (stats->sse_y, mod->pixels),
					SSE2PSNR (stats->sse_u, mod->pixels / 4),
					SSE2PSNR (stats->sse_v, mod->pixels / 4),
					SSE2PSNR (stats->sse_y + stats->sse_u + stats->sse_v,(double)mod->pixels * 1.5),
					stats->type==1?'I':stats->type==2?'P':stats->type==3?'B':stats->type?'S':'?'
				);
		}
		mod->frames++;
	}
}

static void
print_stats(xvid_mplayer_module_t *mod)
{
	if (mod->frames) {
		mod->sse_y /= mod->frames;
		mod->sse_u /= mod->frames;
		mod->sse_v /= mod->frames;

		mp_msg(MSGT_MENCODER, MSGL_INFO,
				"The value 99.99dB is a special value and represents "
				"the upper range limit\n");
		mp_msg(MSGT_MENCODER, MSGL_INFO,
				"xvid:     Min PSNR Y:%.2f, Cb:%.2f, Cr:%.2f, All:%.2f in frame %d\n",
				SSE2PSNR(mod->max_sse_y, mod->pixels),
				SSE2PSNR(mod->max_sse_u, mod->pixels/4),
				SSE2PSNR(mod->max_sse_v, mod->pixels/4),
				SSE2PSNR(mod->max_sse_y + mod->max_sse_u + mod->max_sse_v, mod->pixels*1.5),
				mod->max_framenum);
		mp_msg(MSGT_MENCODER, MSGL_INFO,
				"xvid: Average PSNR Y:%.2f, Cb:%.2f, Cr:%.2f, All:%.2f for %d frames\n",
				SSE2PSNR(mod->sse_y, mod->pixels),
				SSE2PSNR(mod->sse_u, mod->pixels/4),
				SSE2PSNR(mod->sse_v, mod->pixels/4),
				SSE2PSNR(mod->sse_y + mod->sse_u + mod->sse_v, mod->pixels*1.5),
				mod->frames);
		mp_msg(MSGT_MENCODER, MSGL_INFO,
				"xvid:     Max PSNR Y:%.2f, Cb:%.2f, Cr:%.2f, All:%.2f in frame %d\n",
				SSE2PSNR(mod->min_sse_y, mod->pixels),
				SSE2PSNR(mod->min_sse_u, mod->pixels/4),
				SSE2PSNR(mod->min_sse_v, mod->pixels/4),
				SSE2PSNR(mod->min_sse_y + mod->min_sse_u + mod->min_sse_v, mod->pixels*1.5),
				mod->min_framenum);
	}
}
#undef SSE2PSNR

static void *read_matrix(unsigned char *filename)
{
	int i;
	unsigned char *matrix;
	FILE *input;
	
	/* Allocate matrix space */
	if((matrix = malloc(64*sizeof(unsigned char))) == NULL)
	   return NULL;

	/* Open the matrix file */
	if((input = fopen(filename, "rb")) == NULL) {
		mp_msg(MSGT_MENCODER, MSGL_ERR,
			"xvid: Error opening the matrix file %s\n",
			filename);
		free(matrix);
		return NULL;
	}

	/* Read the matrix */
	for(i=0; i<64; i++) {

		int value;

		/* If fscanf fails then get out of the loop */
		if(fscanf(input, "%d", &value) != 1) {
			mp_msg(MSGT_MENCODER, MSGL_ERR,
				"xvid: Error reading the matrix file %s\n",
				filename);
			free(matrix);
			fclose(input);
			return NULL;
		}

		/* Clamp the value to safe range */
		value     = (value<  1)?1  :value;
		value     = (value>255)?255:value;
		matrix[i] = value;
	}

	/* Fills the rest with 1 */
	while(i<64) matrix[i++] = 1;

	/* We're done */
	fclose(input);

	return matrix;
	
}


static const char *
par_string(int parcode)
{
	const char *par_string;
	switch (parcode) {
	case XVID_PAR_11_VGA:
		par_string = "vga11";
		break;
	case XVID_PAR_43_PAL:
		par_string = "pal43";
		break;
	case XVID_PAR_43_NTSC:
		par_string = "ntsc43";
		break;
	case XVID_PAR_169_PAL:
		par_string = "pal169";
		break;
	case XVID_PAR_169_NTSC:
		par_string = "ntsc69";
		break;
	case XVID_PAR_EXT:
		par_string = "ext";
		break;
	default:
		par_string = "unknown";
		break;
	}
	return par_string;
}

static const char *errorstring(int err)
{
	const char *error;
	switch(err) {
	case XVID_ERR_FAIL:
		error = "General fault";
		break;
	case XVID_ERR_MEMORY:
		error =  "Memory allocation error";
		break;
	case XVID_ERR_FORMAT:
		error =  "File format error";
		break;
	case XVID_ERR_VERSION:
		error =  "Structure version not supported";
		break;
	case XVID_ERR_END:
		error =  "End of stream reached";
		break;
	default:
		error = "Unknown";
	}

	return error;
}

/*****************************************************************************
 * Module structure definition
 ****************************************************************************/

vf_info_t ve_info_xvid = {
	"XviD 1.0 encoder",
	"xvid",
	"Marco Belli <elcabesa@inwind.it>, Edouard Gomez <ed.gomez@free.fr>",
	"No comment",
	vf_open
};


/* Please do not change that tag comment.
 * arch-tag: 42ccc257-0548-4a3e-9617-2876c4e8ac88 mplayer xvid encoder module */
