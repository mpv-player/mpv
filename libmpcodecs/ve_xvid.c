#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../config.h"
#include "../mp_msg.h"

#ifdef HAVE_XVID

#include "codec-cfg.h"
#include "stream.h"
#include "demuxer.h"
#include "stheader.h"

#include "aviwrite.h"

#include "img_format.h"
#include "mp_image.h"
#include "vf.h"

#include <xvid.h>
#include "xvid_vbr.h"

#include "cfgparser.h"

/**********************************************************************/
/* Divx4 quality to XviD encoder motion flag presets */
static int const divx4_motion_presets[7] = {
	0,
	PMV_EARLYSTOP16,
	PMV_EARLYSTOP16 | PMV_ADVANCEDDIAMOND16,
	PMV_EARLYSTOP16 | PMV_HALFPELREFINE16,
	PMV_EARLYSTOP16 | PMV_HALFPELREFINE16 | PMV_EARLYSTOP8 | PMV_HALFPELREFINE8,
	PMV_EARLYSTOP16 | PMV_HALFPELREFINE16 | PMV_EARLYSTOP8 | PMV_HALFPELREFINE8,
	PMV_EARLYSTOP16 | PMV_HALFPELREFINE16 | PMV_EXTSEARCH16 | PMV_EARLYSTOP8 | PMV_HALFPELREFINE8
};

/* Divx4 quality to general encoder flag presets */
static int const divx4_general_presets[7] = {
	0,
	XVID_H263QUANT,
	XVID_H263QUANT,
	XVID_H263QUANT | XVID_HALFPEL,
	XVID_H263QUANT | XVID_INTER4V | XVID_HALFPEL,
	XVID_H263QUANT | XVID_INTER4V | XVID_HALFPEL,
	XVID_H263QUANT | XVID_INTER4V | XVID_HALFPEL
};

struct {
    int quality;
    int bitrate;
    int rc_reaction_delay_factor;
    int rc_averaging_period;
    int rc_buffer;
    int max_quantizer;
    int min_quantizer;
    int max_key_interval;
    enum {
	XVID_MODE_CBR = 0, XVID_MODE_2PASS_1, XVID_MODE_2PASS_2, XVID_MODE_FIXED_QUANT,
	XVID_MODE_UNSPEC = -1
    } mode;
    int debug;
    char *stats_file;;
    int keyframe_boost;
    int kfthreshold;
    int kfreduction;
    int min_key_interval;
    int fixed_quant;
} xvidenc_param = {
    sizeof(divx4_motion_presets) / sizeof(divx4_motion_presets[0]) - 1, /* quality */
    0, 0, 0, 0, 0, 0, 0,
    XVID_MODE_CBR,
    1,				/* debug */
    NULL,			/* stats_file */
    -1, -1, -1,		/* keyframe_boost, kfthreshold, kfreduction */
    -1,				/* min_key_interval */
    -1,				/* fixed_quant */
};

static struct config mode_conf[] = {
    /* cbr, vbrqual, vbrquant, 2pass-1, 2pass-2-int, 2pass-2-ext */
    { "cbr", &xvidenc_param.mode, CONF_TYPE_FLAG, 0, 0, XVID_MODE_CBR, NULL},
    { "fixedquant", &xvidenc_param.mode, CONF_TYPE_FLAG, 0, 0, XVID_MODE_FIXED_QUANT, NULL},
    { "2pass-1", &xvidenc_param.mode, CONF_TYPE_FLAG, 0, 0, XVID_MODE_2PASS_1, NULL},
    { "2pass-2", &xvidenc_param.mode, CONF_TYPE_FLAG, 0, 0, XVID_MODE_2PASS_2, NULL},
    { "help", "\nAvailable modes: \n"
      "    cbr         - Constant Bit Rate\n"
      "    2pass-1     - First pass of two pass mode\n"
      "    2pass-2     - Second pass of two pass mode\n"
      "    fixedquant  - Fixed quantizer mode\n"
      "\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
    { NULL, NULL, 0, 0, 0, 0, NULL}
};

struct config xvidencopts_conf[] = {
    { "mode", mode_conf, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},
    { "quality", &xvidenc_param.quality, CONF_TYPE_INT, CONF_RANGE, 0,
      sizeof(divx4_motion_presets) / sizeof(divx4_motion_presets[0]) - 1, NULL},
    { "br", &xvidenc_param.bitrate, CONF_TYPE_INT, 0, 0, 0, NULL},
    { "rc_reaction_delay_factor", &xvidenc_param.rc_reaction_delay_factor, CONF_TYPE_INT, 0, 0, NULL},
    { "rc_averaging_period", &xvidenc_param.rc_averaging_period, CONF_TYPE_INT, 0, 0, NULL},
    { "rc_buffer", &xvidenc_param.rc_buffer, CONF_TYPE_INT, 0, 0, NULL},
    { "max_quantizer", &xvidenc_param.max_quantizer, CONF_TYPE_INT, 0, 0, NULL},
    { "min_quantizer", &xvidenc_param.max_quantizer, CONF_TYPE_INT, 0, 0, NULL},
    { "max_key_interval", &xvidenc_param.max_key_interval, CONF_TYPE_INT, 0, 0, NULL},
    { "nodebug", &xvidenc_param.debug, CONF_TYPE_FLAG, 0, 0, 0, NULL},
    { "debug", &xvidenc_param.debug, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    { "statsfile", &xvidenc_param.stats_file, CONF_TYPE_STRING, 0, 0, 0, NULL},	/* for XVID_MODE_2PASS_1/22 */
    { "keyframe_boost", &xvidenc_param.keyframe_boost, CONF_TYPE_INT, 0, 0, 0, NULL}, /* for XVID_MODE_2PASS_2 */
    { "kfthreshold", &xvidenc_param.kfthreshold, CONF_TYPE_INT, 0, 0, 0, NULL}, /* for XVID_MODE_2PASS_2 */
    { "kfreduction", &xvidenc_param.kfreduction, CONF_TYPE_INT, 0, 0, 0, NULL}, /* for XVID_MODE_2PASS_2 */
    { "min_key_interval", &xvidenc_param.max_key_interval, CONF_TYPE_INT, 0, 0, 0, NULL}, /* for XVID_MODE_2PASS_2 */
    { "fixed_quant", &xvidenc_param.fixed_quant, CONF_TYPE_INT, CONF_RANGE, 1, 31, NULL}, /* for XVID_MODE_FIXED_QUANT */
    { NULL, NULL, 0, 0, 0, 0, NULL}
};

struct vf_priv_s {
    aviwrite_stream_t* mux;
    XVID_ENC_FRAME enc_frame;
    void* enc_handle;
    vbr_control_t vbr_state;
    FILE *hintfile;
};

static FILE *
get_hint_file(struct vf_instance_s* vf, unsigned char *mode)
{
    if (vf->priv->hintfile == NULL) {
	vf->priv->hintfile = fopen("xvid_hint_me.dat", mode);
	if (vf->priv->hintfile == NULL)
	    perror("xvid: could not open xvid_hint_me.dat");
    }
    return vf->priv->hintfile;
}

static int
config(struct vf_instance_s* vf,
       int width, int height, int d_width, int d_height,
       unsigned int flags, unsigned int outfmt)
{
    XVID_ENC_PARAM enc_param;

    vf->priv->mux->bih->biWidth = width;
    vf->priv->mux->bih->biHeight = height;
    vf->priv->mux->bih->biSizeImage = vf->priv->mux->bih->biWidth * vf->priv->mux->bih->biHeight * 3;

    memset(&enc_param, 0, sizeof(enc_param));
    enc_param.width = width;
    enc_param.height = height;
    enc_param.fincr = vf->priv->mux->h.dwScale;
    enc_param.fbase = vf->priv->mux->h.dwRate;
    enc_param.rc_bitrate = xvidenc_param.bitrate;
    enc_param.rc_reaction_delay_factor = xvidenc_param.rc_reaction_delay_factor;
    enc_param.rc_averaging_period = xvidenc_param.rc_averaging_period;
    enc_param.rc_buffer = xvidenc_param.rc_buffer;
    enc_param.max_quantizer = xvidenc_param.max_quantizer;
    enc_param.min_quantizer = xvidenc_param.min_quantizer;
    enc_param.max_key_interval = xvidenc_param.max_key_interval;
    switch (xvid_encore(NULL, XVID_ENC_CREATE, &enc_param, NULL)) {
    case XVID_ERR_FAIL:
	fprintf(stderr, "xvid: encoder creation failed\n");
	return 0;
    case XVID_ERR_MEMORY:
	fprintf(stderr, "xvid: encoder creation failed, out of memory\n");
	return 0;
    case XVID_ERR_FORMAT:
	fprintf(stderr, "xvid: encoder creation failed, bad format\n");
	return 0;
    }
    vf->priv->enc_handle = enc_param.handle;

    vf->priv->enc_frame.general = divx4_general_presets[xvidenc_param.quality];
    vf->priv->enc_frame.motion = divx4_motion_presets[xvidenc_param.quality];
    switch (outfmt) {
    case IMGFMT_YV12:
	vf->priv->enc_frame.colorspace = XVID_CSP_YV12;
	break;
    case IMGFMT_IYUV: case IMGFMT_I420:
	vf->priv->enc_frame.colorspace = XVID_CSP_I420;
	break;
    case IMGFMT_YUY2:
	vf->priv->enc_frame.colorspace = XVID_CSP_YUY2;
	break;
    case IMGFMT_UYVY:
	vf->priv->enc_frame.colorspace = XVID_CSP_UYVY;
	break;
    case IMGFMT_RGB24: case IMGFMT_BGR24:
    	vf->priv->enc_frame.colorspace = XVID_CSP_RGB24;
	break;
    default:
	mp_msg(MSGT_MENCODER,MSGL_ERR,"xvid: unsupported picture format (%s)!\n",
	       vo_format_name(outfmt));
	return 0;
    }
    vf->priv->enc_frame.quant_intra_matrix = 0;
    vf->priv->enc_frame.quant_inter_matrix = 0;

    vf->priv->vbr_state.debug = xvidenc_param.debug;
    vbrSetDefaults(&vf->priv->vbr_state);
    vf->priv->vbr_state.fps = (double)enc_param.fbase / enc_param.fincr;
    if (xvidenc_param.stats_file)
	vf->priv->vbr_state.filename = xvidenc_param.stats_file;
    if (xvidenc_param.bitrate)
	vf->priv->vbr_state.desired_bitrate = xvidenc_param.bitrate;
    if (xvidenc_param.keyframe_boost)
	vf->priv->vbr_state.keyframe_boost = xvidenc_param.keyframe_boost;
    if (xvidenc_param.kfthreshold)
	vf->priv->vbr_state.kftreshold = xvidenc_param.kfthreshold;
    if (xvidenc_param.kfreduction)
	vf->priv->vbr_state.kfreduction = xvidenc_param.kfreduction;
    if (xvidenc_param.min_key_interval)
	vf->priv->vbr_state.min_key_interval = xvidenc_param.min_key_interval;
    if (xvidenc_param.max_key_interval)
	vf->priv->vbr_state.max_key_interval = xvidenc_param.max_key_interval;
    if (xvidenc_param.fixed_quant)
	vf->priv->vbr_state.fixed_quant = xvidenc_param.fixed_quant;
    switch (xvidenc_param.mode) {
    case XVID_MODE_CBR:
	vf->priv->vbr_state.mode = VBR_MODE_1PASS;
	break;
    case XVID_MODE_FIXED_QUANT:
	vf->priv->vbr_state.mode = VBR_MODE_FIXED_QUANT;
	break;
    case XVID_MODE_2PASS_1:
	vf->priv->vbr_state.mode = VBR_MODE_2PASS_1;
	break;
    case XVID_MODE_2PASS_2:
	vf->priv->vbr_state.mode = VBR_MODE_2PASS_2;
	break;
    default:
	abort();
    }
    vbrInit(&vf->priv->vbr_state);
    return 1;
}

static void
uninit(struct vf_instance_s* vf)
{
    if (vf->priv->hintfile)
	fclose(vf->priv->hintfile);
    vbrFinish(&vf->priv->vbr_state);
}

static int
control(struct vf_instance_s* vf, int request, void* data)
{
    return CONTROL_UNKNOWN;
}

static int
query_format(struct vf_instance_s* vf, unsigned int fmt)
{
    switch(fmt){
    case IMGFMT_YV12: case IMGFMT_IYUV: case IMGFMT_I420:
	return VFCAP_CSP_SUPPORTED | VFCAP_CSP_SUPPORTED_BY_HW;
    case IMGFMT_YUY2: case IMGFMT_UYVY:
	return VFCAP_CSP_SUPPORTED;
    case IMGFMT_RGB24: case IMGFMT_BGR24:
	return VFCAP_CSP_SUPPORTED | VFCAP_FLIPPED;
    }
    return 0;
}

static int
put_image(struct vf_instance_s* vf, mp_image_t *mpi)
{
    XVID_ENC_STATS enc_stats;

    vf->priv->enc_frame.bitstream = vf->priv->mux->buffer;
    vf->priv->enc_frame.length = -1 /* vf->priv->mux->buffer_size */;
    vf->priv->enc_frame.image = mpi->planes[0];
    vf->priv->enc_frame.quant = vbrGetQuant(&vf->priv->vbr_state);
    vf->priv->enc_frame.intra = vbrGetIntra(&vf->priv->vbr_state);
#if 0
    if (xvidenc_param.mode == XVID_MODE_2PASS_1) {
	vf->priv->enc_frame.hint.hintstream = hintstream;
	vf->priv->enc_frame.hint.rawhints = 0;
	vf->priv->enc_frame.general |= XVID_HINTEDME_GET;
    }
    else if (xvidenc_param.mode == XVID_MODE_2PASS_2) {
	FILE *f = get_hint_file(vf, "r");
	vf->priv->enc_frame.general &= ~XVID_HINTEDME_SET;
	if (f) {
	    int blocksize;
	    int read;
	    read = fread(&blocksize, sizeof(blocksize), 1, f);
	    if (read == 1) {
		read = fread(hintstream, blocksize, 1, f);
		if (read == blocksize) {
		    vf->priv->enc_frame.hint.hintstream = hintstream;
		    vf->priv->enc_frame.hint.rawhints = 0;
		    vf->priv->enc_frame.general |= XVID_HINTEDME_SET;
		}
		else
		    perror("xvid: hint file read block failure");
	    }
	    else
		perror("xvid: hint file read failure");
	}
    }
#endif
    switch (xvid_encore(vf->priv->enc_handle, XVID_ENC_ENCODE, &vf->priv->enc_frame, &enc_stats)) {
    case XVID_ERR_OK:
	break;
    case XVID_ERR_MEMORY:
	mp_msg(MSGT_MENCODER, MSGL_ERR, "xvid: out of memory\n");
	break;
    case XVID_ERR_FORMAT:
	mp_msg(MSGT_MENCODER, MSGL_ERR, "xvid: bad format\n");
	break;
    default:
	mp_msg(MSGT_MENCODER, MSGL_ERR, "xvid: failure\n");
	break;
    }
    mencoder_write_chunk(vf->priv->mux, vf->priv->enc_frame.length, vf->priv->enc_frame.intra ? 0x10 : 0);
    vbrUpdate(&vf->priv->vbr_state, enc_stats.quant, vf->priv->enc_frame.intra,
	      enc_stats.hlength, vf->priv->enc_frame.length, enc_stats.kblks, enc_stats.mblks, enc_stats.ublks);
#if 1
    if (vf->priv->enc_frame.general & XVID_HINTEDME_GET) {
	FILE *f = get_hint_file(vf, "w");
	if (f) {
	    unsigned int wrote;
	    wrote = fwrite(&vf->priv->enc_frame.hint.hintlength, sizeof(vf->priv->enc_frame.hint.hintlength), 1, f);
	    if (wrote == 1) {
		wrote = fwrite(&vf->priv->enc_frame.hint.hintstream, vf->priv->enc_frame.hint.hintlength, 1, f);
		if (wrote != 1)
		    perror("xvid: hint write block failure");
	    }
	    else
		perror("xvid: hint write failure");
	}
    }
#endif
    return 1;
}

//===========================================================================//

static int
vf_open(vf_instance_t *vf, char* args)
{
    XVID_INIT_PARAM params = { 0, 0, 0};
    vf->config = config;
    vf->control = control;
    vf->uninit = uninit;
    vf->query_format = query_format;
    vf->put_image = put_image;
    vf->priv = malloc(sizeof(struct vf_priv_s));
    memset(vf->priv, 0, sizeof(struct vf_priv_s));
    vf->priv->mux = (aviwrite_stream_t*)args;

    vf->priv->mux->bih = malloc(sizeof(BITMAPINFOHEADER));
    vf->priv->mux->bih->biSize = sizeof(BITMAPINFOHEADER);
    vf->priv->mux->bih->biWidth = 0;
    vf->priv->mux->bih->biHeight = 0;
    vf->priv->mux->bih->biPlanes = 1;
    vf->priv->mux->bih->biBitCount = 24;
    vf->priv->mux->bih->biCompression = mmioFOURCC('X','V','I','D');

    if (xvid_init(NULL, 0, &params, NULL) != XVID_ERR_OK) {
	fprintf(stderr, "xvid: initialisation failure\n");
	abort();
    }
    if (params.api_version != API_VERSION) {
	fprintf(stderr, "xvid: XviD library API version mismatch\n"
		"\texpected %d.%d, got %d.%d, you should recompile MPlayer.\n",
		API_VERSION >> 16, API_VERSION & 0xff,
		params.api_version >> 16, params.api_version & 0xff);
	abort();
    }

    return 1;
}

vf_info_t ve_info_xvid = {
    "XviD encoder",
    "xvid",
    "Kim Minh Kaplan",
    "for internal use by mencoder",
    vf_open
};

//===========================================================================//
#endif
