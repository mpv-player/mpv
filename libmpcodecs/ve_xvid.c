#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

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
	PMV_QUICKSTOP16,
	PMV_EARLYSTOP16,
	PMV_EARLYSTOP16 | PMV_EARLYSTOP8,
        PMV_EARLYSTOP16 | PMV_HALFPELREFINE16 | PMV_EARLYSTOP8 | PMV_HALFPELDIAMOND8,
        PMV_EARLYSTOP16 | PMV_HALFPELREFINE16 | PMV_EARLYSTOP8 | PMV_HALFPELDIAMOND8 | PMV_ADVANCEDDIAMOND16,
	PMV_EARLYSTOP16 | PMV_HALFPELREFINE16 | PMV_EXTSEARCH16 | PMV_EARLYSTOP8 | PMV_HALFPELREFINE8 | 
	PMV_HALFPELDIAMOND8 | PMV_USESQUARES16

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

extern char* passtmpfile;
extern void mencoder_write_chunk(aviwrite_stream_t *s,int len,unsigned int flags);

static int xvidenc_pass = 0;
static int xvidenc_quality = sizeof(divx4_motion_presets) / sizeof(divx4_motion_presets[0]) - 1; /* best quality */
static int xvidenc_bitrate = -1;
static int xvidenc_rc_reaction_delay_factor = -1;
static int xvidenc_rc_averaging_period = -1;
static int xvidenc_rc_buffer = -1;
static int xvidenc_min_quantizer = 2;
static int xvidenc_max_quantizer = -1;
static int xvidenc_min_key_interval = 0;
static int xvidenc_max_key_interval = -1;
static int xvidenc_mpeg_quant = 0;
static int xvidenc_mod_quant = 0;
static int xvidenc_lumi_mask = 0;
static int xvidenc_keyframe_boost = 0;
static int xvidenc_kfthreshold = -1;
static int xvidenc_kfreduction = -1;
static int xvidenc_fixed_quant = 0;
static int xvidenc_debug = 0;
static int xvidenc_hintedme = 0;
static char* xvidenc_hintfile = "xvid_hint_me.dat";

struct config xvidencopts_conf[] = {
    { "pass", &xvidenc_pass, CONF_TYPE_INT, CONF_RANGE, 0, 2, NULL},
    { "quality", &xvidenc_quality, CONF_TYPE_INT, CONF_RANGE, 0,
      sizeof(divx4_motion_presets) / sizeof(divx4_motion_presets[0]) - 1, NULL},
    { "br", &xvidenc_bitrate, CONF_TYPE_INT, CONF_RANGE, 4, 24000000, NULL},
    { "rc_reaction_delay_factor", &xvidenc_rc_reaction_delay_factor, CONF_TYPE_INT, 0, 0, 0, NULL},
    { "rc_averaging_period", &xvidenc_rc_averaging_period, CONF_TYPE_INT, 0, 0, 0, NULL},
    { "rc_buffer", &xvidenc_rc_buffer, CONF_TYPE_INT, 0, 0, 0, NULL},
    { "min_quantizer", &xvidenc_min_quantizer, CONF_TYPE_INT, CONF_RANGE, 1, 31, NULL},
    { "max_quantizer", &xvidenc_max_quantizer, CONF_TYPE_INT, CONF_RANGE, 1, 31, NULL},
    { "min_key_interval", &xvidenc_min_key_interval, CONF_TYPE_INT, 0, 0, 0, NULL}, /* for XVID_MODE_2PASS_2 */
    { "max_key_interval", &xvidenc_max_key_interval, CONF_TYPE_INT, 0, 0, 0, NULL},
    { "mpeg_quant", &xvidenc_mpeg_quant, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    { "mod_quant", &xvidenc_mod_quant, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    { "lumi_mask", &xvidenc_lumi_mask, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    { "keyframe_boost", &xvidenc_keyframe_boost, CONF_TYPE_INT, CONF_RANGE, 0, 10000, NULL}, /* for XVID_MODE_2PASS_2 */
    { "kfthreshold", &xvidenc_kfthreshold, CONF_TYPE_INT, 0, 0, 0, NULL}, /* for XVID_MODE_2PASS_2 */
    { "kfreduction", &xvidenc_kfreduction, CONF_TYPE_INT, CONF_RANGE, 0, 100, NULL}, /* for XVID_MODE_2PASS_2 */
    { "fixed_quant", &xvidenc_fixed_quant, CONF_TYPE_INT, CONF_RANGE, 1, 31, NULL}, /* for XVID_MODE_FIXED_QUANT */
    { "debug", &xvidenc_debug, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    { "hintedme", &xvidenc_hintedme, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    { "hintfile", &xvidenc_hintfile, CONF_TYPE_STRING, 0, 0, 0, NULL},
    { NULL, NULL, 0, 0, 0, 0, NULL}
};

struct vf_priv_s {
    aviwrite_stream_t* mux;
    XVID_ENC_FRAME enc_frame;
    void* enc_handle;
    vbr_control_t vbr_state;
    FILE *hintfile;
    void *hintstream;
};

static int
config(struct vf_instance_s* vf,
       int width, int height, int d_width, int d_height,
       unsigned int flags, unsigned int outfmt)
{
    XVID_ENC_PARAM enc_param;
    struct vf_priv_s *fp = vf->priv;

    fp->mux->bih->biWidth = width;
    fp->mux->bih->biHeight = height;
    fp->mux->bih->biSizeImage = fp->mux->bih->biWidth * fp->mux->bih->biHeight * 3;
    mp_msg(MSGT_MENCODER,MSGL_INFO,"videocodec: XViD (%dx%d fourcc=%x [%.4s])\n",
	width, height, fp->mux->bih->biCompression, (char *)&fp->mux->bih->biCompression);

    // initialize XViD core parameters
    // ===============================
    memset(&enc_param, 0, sizeof(enc_param));
    enc_param.width = width;
    enc_param.height = height;
    enc_param.fincr = fp->mux->h.dwScale;
    enc_param.fbase = fp->mux->h.dwRate;
    if (xvidenc_bitrate > 16000)
	enc_param.rc_bitrate = xvidenc_bitrate;
    else if (xvidenc_bitrate > 0)
	enc_param.rc_bitrate = xvidenc_bitrate * 1000;
    else
	enc_param.rc_bitrate = -1;
    enc_param.rc_reaction_delay_factor = xvidenc_rc_reaction_delay_factor;
    enc_param.rc_averaging_period = xvidenc_rc_averaging_period;
    enc_param.rc_buffer = xvidenc_rc_buffer;
    enc_param.min_quantizer = xvidenc_min_quantizer;
    enc_param.max_quantizer = xvidenc_max_quantizer;
    if( xvidenc_max_key_interval > 0 )
	enc_param.max_key_interval = xvidenc_max_key_interval;
    else
	enc_param.max_key_interval = 10 * enc_param.fbase / enc_param.fincr;
    switch (xvid_encore(NULL, XVID_ENC_CREATE, &enc_param, NULL)) {
    case XVID_ERR_FAIL:
	mp_msg(MSGT_MENCODER,MSGL_ERR, "xvid: encoder creation failed\n");
	return 0;
    case XVID_ERR_MEMORY:
	mp_msg(MSGT_MENCODER,MSGL_ERR, "xvid: encoder creation failed, out of memory\n");
	return 0;
    case XVID_ERR_FORMAT:
	mp_msg(MSGT_MENCODER,MSGL_ERR, "xvid: encoder creation failed, bad format\n");
	return 0;
    }
    fp->enc_handle = enc_param.handle;

    // initialize XViD per-frame static parameters
    // ===========================================
    fp->enc_frame.general = divx4_general_presets[xvidenc_quality];
    fp->enc_frame.motion = divx4_motion_presets[xvidenc_quality];
    if (xvidenc_mpeg_quant) {
	fp->enc_frame.general &= ~XVID_H263QUANT;
	fp->enc_frame.general |= XVID_MPEGQUANT;
    }
    if (xvidenc_lumi_mask)
	fp->enc_frame.general |= XVID_LUMIMASKING;

    switch (outfmt) {
    case IMGFMT_YV12:
	fp->enc_frame.colorspace = XVID_CSP_YV12;
	break;
    case IMGFMT_IYUV: case IMGFMT_I420:
	fp->enc_frame.colorspace = XVID_CSP_I420;
	break;
    case IMGFMT_YUY2:
	fp->enc_frame.colorspace = XVID_CSP_YUY2;
	break;
    case IMGFMT_UYVY:
	fp->enc_frame.colorspace = XVID_CSP_UYVY;
	break;
    case IMGFMT_RGB24: case IMGFMT_BGR24:
    	fp->enc_frame.colorspace = XVID_CSP_RGB24;
	break;
    default:
	mp_msg(MSGT_MENCODER,MSGL_ERR,"xvid: unsupported picture format (%s)!\n",
	       vo_format_name(outfmt));
	return 0;
    }
    fp->enc_frame.quant_intra_matrix = 0;
    fp->enc_frame.quant_inter_matrix = 0;

    // hinted ME
    fp->hintstream = NULL;
    fp->hintfile = NULL;
    if (xvidenc_hintedme && (xvidenc_pass == 1 || xvidenc_pass == 2)) {
	fp->hintstream = malloc( 100000 ); // this is what the vfw code in XViD CVS allocates
	if (fp->hintstream == NULL)
	    mp_msg(MSGT_MENCODER,MSGL_ERR, "xvid: cannot allocate memory for hinted ME\n");
	else {
	    fp->hintfile = fopen(xvidenc_hintfile, xvidenc_pass == 1 ? "w" : "r");
	    if (fp->hintfile == NULL) {
		mp_msg(MSGT_MENCODER,MSGL_ERR, "xvid: %s: %s\n", strerror(errno), xvidenc_hintfile);
		free(fp->hintstream);
	    }
	}
	if (fp->hintstream == NULL || fp->hintfile == NULL)
	    xvidenc_hintedme = 0;
    }

    // initialize VBR engine
    // =====================
    vbrSetDefaults(&fp->vbr_state);
    if (xvidenc_pass == 0) {
	if (xvidenc_fixed_quant >= 1) {
	    fp->vbr_state.mode = VBR_MODE_FIXED_QUANT;
	    fp->vbr_state.fixed_quant = xvidenc_fixed_quant;
	} else
	    fp->vbr_state.mode = VBR_MODE_1PASS;
    }
    else if (xvidenc_pass == 1)
	fp->vbr_state.mode = VBR_MODE_2PASS_1;
    else if (xvidenc_pass == 2)
	fp->vbr_state.mode = VBR_MODE_2PASS_2;
    else
	return -1;
    fp->vbr_state.fps = (double)enc_param.fbase / enc_param.fincr;
    fp->vbr_state.filename = passtmpfile;
    fp->vbr_state.desired_bitrate = enc_param.rc_bitrate;
    fp->vbr_state.min_iquant = fp->vbr_state.min_pquant = enc_param.min_quantizer;
    fp->vbr_state.max_iquant = fp->vbr_state.max_pquant = enc_param.max_quantizer;
    if (xvidenc_keyframe_boost)
	fp->vbr_state.keyframe_boost = xvidenc_keyframe_boost;
    if (xvidenc_kfthreshold >= 0)
	fp->vbr_state.kftreshold = xvidenc_kfthreshold;
    if (xvidenc_kfreduction >= 0)
	fp->vbr_state.kfreduction = xvidenc_kfreduction;
    if (xvidenc_min_key_interval)
	fp->vbr_state.min_key_interval = xvidenc_min_key_interval;
    fp->vbr_state.max_key_interval = enc_param.max_key_interval;
    fp->vbr_state.debug = xvidenc_debug;
    vbrInit(&fp->vbr_state);

    return 1;
}

static void
uninit(struct vf_instance_s* vf)
{
    struct vf_priv_s *fp = vf->priv;

    if (fp->hintfile)
	fclose(fp->hintfile);
    if (fp->hintstream)
	free(fp->hintstream);
    vbrFinish(&fp->vbr_state);
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
    struct vf_priv_s *fp = vf->priv;

    fp->enc_frame.bitstream = fp->mux->buffer;
    fp->enc_frame.length = -1 /* fp->mux->buffer_size */;
    fp->enc_frame.image = mpi->planes[0];

    // get quantizers & I/P decision from the VBR engine
    fp->enc_frame.quant = vbrGetQuant(&fp->vbr_state);
    fp->enc_frame.intra = vbrGetIntra(&fp->vbr_state);

    // modulated quantizer type
    if (xvidenc_mod_quant && xvidenc_pass == 2) {
	fp->enc_frame.general |= (fp->enc_frame.quant < 4) ? XVID_MPEGQUANT : XVID_H263QUANT;
	fp->enc_frame.general &= (fp->enc_frame.quant < 4) ? ~XVID_H263QUANT : ~XVID_MPEGQUANT;
    }

    // hinted ME, 1st part
    if (xvidenc_hintedme && xvidenc_pass == 1) {
	fp->enc_frame.hint.hintstream = fp->hintstream;
	fp->enc_frame.hint.rawhints = 0;
	fp->enc_frame.general |= XVID_HINTEDME_GET;
    }
    else if (xvidenc_hintedme && xvidenc_pass == 2) {
	size_t read;
	int blocksize;
	fp->enc_frame.general &= ~XVID_HINTEDME_SET;
	read = fread(&blocksize, sizeof(blocksize), 1, fp->hintfile);
	if (read == 1) {
	    read = fread(fp->hintstream, (size_t)blocksize, 1, fp->hintfile);
	    if (read == 1) {
		fp->enc_frame.hint.hintstream = fp->hintstream;
		fp->enc_frame.hint.hintlength = 0;
		fp->enc_frame.hint.rawhints = 0;
		fp->enc_frame.general |= XVID_HINTEDME_SET;
	    } 
	    else
		perror("xvid: hint file read block failure");
	} 
	else
	    perror("xvid: hint file read failure");
    }

    // encode frame
    switch (xvid_encore(fp->enc_handle, XVID_ENC_ENCODE, &fp->enc_frame, &enc_stats)) {
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
    
    // write output
    mencoder_write_chunk(fp->mux, fp->enc_frame.length, fp->enc_frame.intra ? 0x10 : 0);

    // update the VBR engine
    vbrUpdate(&fp->vbr_state, enc_stats.quant, fp->enc_frame.intra,
	      enc_stats.hlength, fp->enc_frame.length, enc_stats.kblks, enc_stats.mblks, enc_stats.ublks);

    // hinted ME, 2nd part
    if (fp->enc_frame.general & XVID_HINTEDME_GET) {
	size_t wrote = fwrite(&fp->enc_frame.hint.hintlength, sizeof(fp->enc_frame.hint.hintlength), 1, fp->hintfile);
	if (wrote == 1) {
	    wrote = fwrite(fp->enc_frame.hint.hintstream, fp->enc_frame.hint.hintlength, 1, fp->hintfile);
	    if (wrote != 1)
		perror("xvid: hint write block failure");
	}
	else
	    perror("xvid: hint write failure");
    }
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
	mp_msg(MSGT_MENCODER,MSGL_ERR, "xvid: initialisation failure\n");
	abort();
    }
    if (params.api_version != API_VERSION) {
	mp_msg(MSGT_MENCODER,MSGL_ERR, "xvid: XviD library API version mismatch\n"
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
    "Kim Minh Kaplan & Rémi Guyomarch",
    "for internal use by mencoder",
    vf_open
};

//===========================================================================//
#endif
