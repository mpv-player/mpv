#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <time.h>
#include <assert.h>

#if !defined(INFINITY) && defined(HUGE_VAL)
#define INFINITY HUGE_VAL
#endif

#include "config.h"

#include "mp_msg.h"
#include "help_mp.h"
#include "av_opts.h"

#include "codec-cfg.h"
#include "stream/stream.h"
#include "libmpdemux/demuxer.h"
#include "libmpdemux/stheader.h"

#include "libmpdemux/muxer.h"

#include "img_format.h"
#include "mp_image.h"
#include "vf.h"

extern char* passtmpfile;

//===========================================================================//

#include "libavcodec/avcodec.h"

extern int avcodec_initialized;

/* video options */
static char *lavc_param_vcodec = "mpeg4";
static int lavc_param_vbitrate = -1;
static int lavc_param_vrate_tolerance = 1000*8;
static int lavc_param_mb_decision = 0; /* default is realtime encoding */
static int lavc_param_v4mv = 0;
static int lavc_param_vme = 4;
static float lavc_param_vqscale = -1;
static int lavc_param_vqmin = 2;
static int lavc_param_vqmax = 31;
static int lavc_param_mb_qmin = 2;
static int lavc_param_mb_qmax = 31;
static float lavc_param_lmin = 2;
static float lavc_param_lmax = 31;
static float lavc_param_mb_lmin = 2;
static float lavc_param_mb_lmax = 31;
static int lavc_param_vqdiff = 3;
static float lavc_param_vqcompress = 0.5;
static float lavc_param_vqblur = 0.5;
static float lavc_param_vb_qfactor = 1.25;
static float lavc_param_vb_qoffset = 1.25;
static float lavc_param_vi_qfactor = 0.8;
static float lavc_param_vi_qoffset = 0.0;
static int lavc_param_vmax_b_frames = 0;
static int lavc_param_keyint = -1;
static int lavc_param_vpass = 0;
static int lavc_param_vrc_strategy = 0;
static int lavc_param_vb_strategy = 0;
static int lavc_param_luma_elim_threshold = 0;
static int lavc_param_chroma_elim_threshold = 0;
static int lavc_param_packet_size= 0;
static int lavc_param_strict= -1;
static int lavc_param_data_partitioning= 0;
static int lavc_param_gray=0;
static float lavc_param_rc_qsquish=1.0;
static float lavc_param_rc_qmod_amp=0;
static int lavc_param_rc_qmod_freq=0;
static char *lavc_param_rc_override_string=NULL;
static char *lavc_param_rc_eq="tex^qComp";
static int lavc_param_rc_buffer_size=0;
static float lavc_param_rc_buffer_aggressivity=1.0;
static int lavc_param_rc_max_rate=0;
static int lavc_param_rc_min_rate=0;
static float lavc_param_rc_initial_cplx=0;
static float lavc_param_rc_initial_buffer_occupancy=0.9;
static int lavc_param_mpeg_quant=0;
static int lavc_param_fdct=0;
static int lavc_param_idct=0;
static char* lavc_param_aspect = NULL;
static int lavc_param_autoaspect = 0;
static float lavc_param_lumi_masking= 0.0;
static float lavc_param_dark_masking= 0.0;
static float lavc_param_temporal_cplx_masking= 0.0;
static float lavc_param_spatial_cplx_masking= 0.0;
static float lavc_param_p_masking= 0.0;
static float lavc_param_border_masking= 0.0;
static int lavc_param_normalize_aqp= 0;
static int lavc_param_interlaced_dct= 0;
static int lavc_param_prediction_method= FF_PRED_LEFT;
static int lavc_param_format= IMGFMT_YV12;
static int lavc_param_debug= 0;
static int lavc_param_psnr= 0;
static int lavc_param_me_pre_cmp= 0;
static int lavc_param_me_cmp= 0;
static int lavc_param_me_sub_cmp= 0;
static int lavc_param_mb_cmp= 0;
#ifdef FF_CMP_VSAD
static int lavc_param_ildct_cmp= FF_CMP_VSAD;
#endif
static int lavc_param_pre_dia_size= 0;
static int lavc_param_dia_size= 0;
static int lavc_param_qpel= 0;
static int lavc_param_trell= 0;
static int lavc_param_lowdelay= 0;
static int lavc_param_bit_exact = 0;
static int lavc_param_aic= 0;
static int lavc_param_aiv= 0;
static int lavc_param_umv= 0;
static int lavc_param_gmc= 0;
static int lavc_param_obmc= 0;
static int lavc_param_loop= 0;
static int lavc_param_last_pred= 0;
static int lavc_param_pre_me= 1;
static int lavc_param_me_subpel_quality= 8;
static int lavc_param_me_range= 0;
static int lavc_param_ibias= FF_DEFAULT_QUANT_BIAS;
static int lavc_param_pbias= FF_DEFAULT_QUANT_BIAS;
static int lavc_param_coder= 0;
static int lavc_param_context= 0;
static char *lavc_param_intra_matrix = NULL;
static char *lavc_param_inter_matrix = NULL;
static int lavc_param_cbp= 0;
static int lavc_param_mv0= 0;
static int lavc_param_noise_reduction= 0;
static int lavc_param_qns= 0;
static int lavc_param_qp_rd= 0;
static int lavc_param_inter_threshold= 0;
static int lavc_param_sc_threshold= 0;
static int lavc_param_ss= 0;
static int lavc_param_top= -1;
static int lavc_param_alt= 0;
static int lavc_param_ilme= 0;
static int lavc_param_nssew= 8;
static int lavc_param_closed_gop = 0;
static int lavc_param_dc_precision = 8;
static int lavc_param_threads= 1;
static int lavc_param_turbo = 0;
static int lavc_param_skip_threshold=0;
static int lavc_param_skip_factor=0;
static int lavc_param_skip_exp=0;
static int lavc_param_skip_cmp=0;
static int lavc_param_brd_scale = 0;
static int lavc_param_bidir_refine = 0;
static int lavc_param_sc_factor = 1;
static int lavc_param_video_global_header= 0;
static int lavc_param_mv0_threshold = 256;
static int lavc_param_refs = 1;
static int lavc_param_b_sensitivity = 40;
static int lavc_param_level = FF_LEVEL_UNKNOWN;

char *lavc_param_acodec = "mp2";
int lavc_param_atag = 0;
int lavc_param_abitrate = 224;
int lavc_param_audio_global_header= 0;
static char *lavc_param_avopt = NULL;

#include "m_option.h"

#ifdef CONFIG_LIBAVCODEC
m_option_t lavcopts_conf[]={
	{"acodec", &lavc_param_acodec, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"abitrate", &lavc_param_abitrate, CONF_TYPE_INT, CONF_RANGE, 1, 1000000, NULL},
	{"atag", &lavc_param_atag, CONF_TYPE_INT, CONF_RANGE, 0, 0xffff, NULL},
	{"vcodec", &lavc_param_vcodec, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"vbitrate", &lavc_param_vbitrate, CONF_TYPE_INT, CONF_RANGE, 4, 24000000, NULL},
	{"vratetol", &lavc_param_vrate_tolerance, CONF_TYPE_INT, CONF_RANGE, 4, 24000000, NULL},
	{"vhq", &lavc_param_mb_decision, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"mbd", &lavc_param_mb_decision, CONF_TYPE_INT, CONF_RANGE, 0, 9, NULL},
	{"v4mv", &lavc_param_v4mv, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"vme", &lavc_param_vme, CONF_TYPE_INT, CONF_RANGE, 0, 8, NULL},
	{"vqscale", &lavc_param_vqscale, CONF_TYPE_FLOAT, CONF_RANGE, 0.0, 255.0, NULL},
	{"vqmin", &lavc_param_vqmin, CONF_TYPE_INT, CONF_RANGE, 1, 31, NULL},
	{"vqmax", &lavc_param_vqmax, CONF_TYPE_INT, CONF_RANGE, 1, 31, NULL},
	{"mbqmin", &lavc_param_mb_qmin, CONF_TYPE_INT, CONF_RANGE, 1, 31, NULL},
	{"mbqmax", &lavc_param_mb_qmax, CONF_TYPE_INT, CONF_RANGE, 1, 31, NULL},
	{"lmin", &lavc_param_lmin, CONF_TYPE_FLOAT, CONF_RANGE, 0.01, 255.0, NULL},
	{"lmax", &lavc_param_lmax, CONF_TYPE_FLOAT, CONF_RANGE, 0.01, 255.0, NULL},
	{"mblmin", &lavc_param_mb_lmin, CONF_TYPE_FLOAT, CONF_RANGE, 0.01, 255.0, NULL},
	{"mblmax", &lavc_param_mb_lmax, CONF_TYPE_FLOAT, CONF_RANGE, 0.01, 255.0, NULL},
	{"vqdiff", &lavc_param_vqdiff, CONF_TYPE_INT, CONF_RANGE, 1, 31, NULL},
	{"vqcomp", &lavc_param_vqcompress, CONF_TYPE_FLOAT, CONF_RANGE, 0.0, 1.0, NULL},
	{"vqblur", &lavc_param_vqblur, CONF_TYPE_FLOAT, CONF_RANGE, 0.0, 1.0, NULL},
	{"vb_qfactor", &lavc_param_vb_qfactor, CONF_TYPE_FLOAT, CONF_RANGE, -31.0, 31.0, NULL},
	{"vmax_b_frames", &lavc_param_vmax_b_frames, CONF_TYPE_INT, CONF_RANGE, 0, FF_MAX_B_FRAMES, NULL},
	{"vpass", &lavc_param_vpass, CONF_TYPE_INT, CONF_RANGE, 0, 3, NULL},
	{"vrc_strategy", &lavc_param_vrc_strategy, CONF_TYPE_INT, CONF_RANGE, 0, 2, NULL},
	{"vb_strategy", &lavc_param_vb_strategy, CONF_TYPE_INT, CONF_RANGE, 0, 10, NULL},
	{"vb_qoffset", &lavc_param_vb_qoffset, CONF_TYPE_FLOAT, CONF_RANGE, 0.0, 31.0, NULL},
	{"vlelim", &lavc_param_luma_elim_threshold, CONF_TYPE_INT, CONF_RANGE, -99, 99, NULL},
	{"vcelim", &lavc_param_chroma_elim_threshold, CONF_TYPE_INT, CONF_RANGE, -99, 99, NULL},
	{"vpsize", &lavc_param_packet_size, CONF_TYPE_INT, CONF_RANGE, 0, 100000000, NULL},
	{"vstrict", &lavc_param_strict, CONF_TYPE_INT, CONF_RANGE, -99, 99, NULL},
	{"vdpart", &lavc_param_data_partitioning, CONF_TYPE_FLAG, 0, 0, CODEC_FLAG_PART, NULL},
	{"keyint", &lavc_param_keyint, CONF_TYPE_INT, 0, 0, 0, NULL},
	{"gray", &lavc_param_gray, CONF_TYPE_FLAG, 0, 0, CODEC_FLAG_PART, NULL},
	{"mpeg_quant", &lavc_param_mpeg_quant, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"vi_qfactor", &lavc_param_vi_qfactor, CONF_TYPE_FLOAT, CONF_RANGE, -31.0, 31.0, NULL},
	{"vi_qoffset", &lavc_param_vi_qoffset, CONF_TYPE_FLOAT, CONF_RANGE, 0.0, 31.0, NULL},
	{"vqsquish", &lavc_param_rc_qsquish, CONF_TYPE_FLOAT, CONF_RANGE, 0.0, 99.0, NULL},
	{"vqmod_amp", &lavc_param_rc_qmod_amp, CONF_TYPE_FLOAT, CONF_RANGE, 0.0, 99.0, NULL},
	{"vqmod_freq", &lavc_param_rc_qmod_freq, CONF_TYPE_INT, 0, 0, 0, NULL},
	{"vrc_eq", &lavc_param_rc_eq, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"vrc_override", &lavc_param_rc_override_string, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"vrc_maxrate", &lavc_param_rc_max_rate, CONF_TYPE_INT, CONF_RANGE, 0, 24000000, NULL},
	{"vrc_minrate", &lavc_param_rc_min_rate, CONF_TYPE_INT, CONF_RANGE, 0, 24000000, NULL},
	{"vrc_buf_size", &lavc_param_rc_buffer_size, CONF_TYPE_INT, CONF_RANGE, 4, 24000000, NULL},
	{"vrc_buf_aggressivity", &lavc_param_rc_buffer_aggressivity, CONF_TYPE_FLOAT, CONF_RANGE, 0.0, 99.0, NULL},
	{"vrc_init_cplx", &lavc_param_rc_initial_cplx, CONF_TYPE_FLOAT, CONF_RANGE, 0.0, 9999999.0, NULL},
	{"vrc_init_occupancy", &lavc_param_rc_initial_buffer_occupancy, CONF_TYPE_FLOAT, CONF_RANGE, 0.0, 1.0, NULL},
        {"vfdct", &lavc_param_fdct, CONF_TYPE_INT, CONF_RANGE, 0, 10, NULL},
	{"aspect", &lavc_param_aspect, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"autoaspect", &lavc_param_autoaspect, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"lumi_mask", &lavc_param_lumi_masking, CONF_TYPE_FLOAT, CONF_RANGE, -1.0, 1.0, NULL},
	{"tcplx_mask", &lavc_param_temporal_cplx_masking, CONF_TYPE_FLOAT, CONF_RANGE, -1.0, 1.0, NULL},
	{"scplx_mask", &lavc_param_spatial_cplx_masking, CONF_TYPE_FLOAT, CONF_RANGE, -1.0, 1.0, NULL},
	{"p_mask", &lavc_param_p_masking, CONF_TYPE_FLOAT, CONF_RANGE, -1.0, 1.0, NULL},
	{"naq", &lavc_param_normalize_aqp, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"dark_mask", &lavc_param_dark_masking, CONF_TYPE_FLOAT, CONF_RANGE, -1.0, 1.0, NULL},
	{"ildct", &lavc_param_interlaced_dct, CONF_TYPE_FLAG, 0, 0, 1, NULL},
        {"idct", &lavc_param_idct, CONF_TYPE_INT, CONF_RANGE, 0, 20, NULL},
        {"pred", &lavc_param_prediction_method, CONF_TYPE_INT, CONF_RANGE, 0, 20, NULL},
        {"format", &lavc_param_format, CONF_TYPE_IMGFMT, 0, 0, 0, NULL},
        {"debug", &lavc_param_debug, CONF_TYPE_INT, CONF_RANGE, 0, 100000000, NULL},
        {"psnr", &lavc_param_psnr, CONF_TYPE_FLAG, 0, 0, CODEC_FLAG_PSNR, NULL},
        {"precmp", &lavc_param_me_pre_cmp, CONF_TYPE_INT, CONF_RANGE, 0, 2000, NULL},
        {"cmp", &lavc_param_me_cmp, CONF_TYPE_INT, CONF_RANGE, 0, 2000, NULL},
        {"subcmp", &lavc_param_me_sub_cmp, CONF_TYPE_INT, CONF_RANGE, 0, 2000, NULL},
        {"mbcmp", &lavc_param_mb_cmp, CONF_TYPE_INT, CONF_RANGE, 0, 2000, NULL},
        {"skipcmp", &lavc_param_skip_cmp, CONF_TYPE_INT, CONF_RANGE, 0, 2000, NULL},
#ifdef FF_CMP_VSAD
        {"ildctcmp", &lavc_param_ildct_cmp, CONF_TYPE_INT, CONF_RANGE, 0, 2000, NULL},
#endif
        {"bit_exact", &lavc_param_bit_exact, CONF_TYPE_FLAG, 0, 0, CODEC_FLAG_BITEXACT, NULL},
        {"predia", &lavc_param_pre_dia_size, CONF_TYPE_INT, CONF_RANGE, -2000, 2000, NULL},
        {"dia", &lavc_param_dia_size, CONF_TYPE_INT, CONF_RANGE, -2000, 2000, NULL},
	{"qpel", &lavc_param_qpel, CONF_TYPE_FLAG, 0, 0, CODEC_FLAG_QPEL, NULL},
	{"trell", &lavc_param_trell, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"lowdelay", &lavc_param_lowdelay, CONF_TYPE_FLAG, 0, 0, CODEC_FLAG_LOW_DELAY, NULL},
	{"last_pred", &lavc_param_last_pred, CONF_TYPE_INT, CONF_RANGE, 0, 2000, NULL},
	{"preme", &lavc_param_pre_me, CONF_TYPE_INT, CONF_RANGE, 0, 2000, NULL},
	{"subq", &lavc_param_me_subpel_quality, CONF_TYPE_INT, CONF_RANGE, 0, 8, NULL},
	{"me_range", &lavc_param_me_range, CONF_TYPE_INT, CONF_RANGE, 0, 16000, NULL},
#ifdef CODEC_FLAG_AC_PRED
	{"aic", &lavc_param_aic, CONF_TYPE_FLAG, 0, 0, CODEC_FLAG_AC_PRED, NULL},
	{"umv", &lavc_param_umv, CONF_TYPE_FLAG, 0, 0, CODEC_FLAG_H263P_UMV, NULL},
#endif
#ifdef CODEC_FLAG_H263P_AIV
	{"aiv", &lavc_param_aiv, CONF_TYPE_FLAG, 0, 0, CODEC_FLAG_H263P_AIV, NULL},
#endif
#ifdef CODEC_FLAG_OBMC
	{"obmc", &lavc_param_obmc, CONF_TYPE_FLAG, 0, 0, CODEC_FLAG_OBMC, NULL},
#endif
#ifdef CODEC_FLAG_LOOP_FILTER
	{"loop", &lavc_param_loop, CONF_TYPE_FLAG, 0, 0, CODEC_FLAG_LOOP_FILTER, NULL},
#endif
	{"ibias", &lavc_param_ibias, CONF_TYPE_INT, CONF_RANGE, -512, 512, NULL},
	{"pbias", &lavc_param_pbias, CONF_TYPE_INT, CONF_RANGE, -512, 512, NULL},
	{"coder", &lavc_param_coder, CONF_TYPE_INT, CONF_RANGE, 0, 10, NULL},
	{"context", &lavc_param_context, CONF_TYPE_INT, CONF_RANGE, 0, 10, NULL},
	{"intra_matrix", &lavc_param_intra_matrix, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"inter_matrix", &lavc_param_inter_matrix, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"cbp", &lavc_param_cbp, CONF_TYPE_FLAG, 0, 0, CODEC_FLAG_CBP_RD, NULL},
	{"mv0", &lavc_param_mv0, CONF_TYPE_FLAG, 0, 0, CODEC_FLAG_MV0, NULL},
	{"nr", &lavc_param_noise_reduction, CONF_TYPE_INT, CONF_RANGE, 0, 1000000, NULL},
#ifdef CODEC_FLAG_QP_RD
	{"qprd", &lavc_param_qp_rd, CONF_TYPE_FLAG, 0, 0, CODEC_FLAG_QP_RD, NULL},
#endif
#ifdef CODEC_FLAG_H263P_SLICE_STRUCT
	{"ss", &lavc_param_ss, CONF_TYPE_FLAG, 0, 0, CODEC_FLAG_H263P_SLICE_STRUCT, NULL},
#endif
#ifdef CODEC_FLAG_ALT_SCAN
	{"alt", &lavc_param_alt, CONF_TYPE_FLAG, 0, 0, CODEC_FLAG_ALT_SCAN, NULL},
#endif
#ifdef CODEC_FLAG_INTERLACED_ME
	{"ilme", &lavc_param_ilme, CONF_TYPE_FLAG, 0, 0, CODEC_FLAG_INTERLACED_ME, NULL},
#endif
#ifdef CODEC_FLAG_CLOSED_GOP
	{"cgop", &lavc_param_closed_gop, CONF_TYPE_FLAG, 0, 0, CODEC_FLAG_CLOSED_GOP, NULL},
#endif
#ifdef CODEC_FLAG_GMC
	{"gmc", &lavc_param_gmc, CONF_TYPE_FLAG, 0, 0, CODEC_FLAG_GMC, NULL},
#endif
	{"dc", &lavc_param_dc_precision, CONF_TYPE_INT, CONF_RANGE, 8, 11, NULL},
	{"border_mask", &lavc_param_border_masking, CONF_TYPE_FLOAT, CONF_RANGE, 0.0, 1.0, NULL},
	{"inter_threshold", &lavc_param_inter_threshold, CONF_TYPE_INT, CONF_RANGE, -1000000, 1000000, NULL},
	{"sc_threshold", &lavc_param_sc_threshold, CONF_TYPE_INT, CONF_RANGE, -1000000000, 1000000000, NULL},
	{"top", &lavc_param_top, CONF_TYPE_INT, CONF_RANGE, -1, 1, NULL},
        {"qns", &lavc_param_qns, CONF_TYPE_INT, CONF_RANGE, 0, 1000000, NULL},
        {"nssew", &lavc_param_nssew, CONF_TYPE_INT, CONF_RANGE, 0, 1000000, NULL},
	{"threads", &lavc_param_threads, CONF_TYPE_INT, CONF_RANGE, 1, 8, NULL},
	{"turbo", &lavc_param_turbo, CONF_TYPE_FLAG, 0, 0, 1, NULL},
        {"skip_threshold", &lavc_param_skip_threshold, CONF_TYPE_INT, CONF_RANGE, 0, 1000000, NULL},
        {"skip_factor", &lavc_param_skip_factor, CONF_TYPE_INT, CONF_RANGE, 0, 1000000, NULL},
        {"skip_exp", &lavc_param_skip_exp, CONF_TYPE_INT, CONF_RANGE, 0, 1000000, NULL},
	{"brd_scale", &lavc_param_brd_scale, CONF_TYPE_INT, CONF_RANGE, 0, 10, NULL},
	{"bidir_refine", &lavc_param_bidir_refine, CONF_TYPE_INT, CONF_RANGE, 0, 4, NULL},
	{"sc_factor", &lavc_param_sc_factor, CONF_TYPE_INT, CONF_RANGE, 1, INT_MAX, NULL},
	{"vglobal", &lavc_param_video_global_header, CONF_TYPE_INT, CONF_RANGE, 0, INT_MAX, NULL},
	{"aglobal", &lavc_param_audio_global_header, CONF_TYPE_INT, CONF_RANGE, 0, INT_MAX, NULL},
	{"mv0_threshold", &lavc_param_mv0_threshold, CONF_TYPE_INT, CONF_RANGE, 0, INT_MAX, NULL},
	{"refs", &lavc_param_refs, CONF_TYPE_INT, CONF_RANGE, 1, 16, NULL},
        {"b_sensitivity", &lavc_param_b_sensitivity, CONF_TYPE_INT, CONF_RANGE, 1, INT_MAX, NULL},
	{"level", &lavc_param_level, CONF_TYPE_INT, CONF_RANGE, INT_MIN, INT_MAX, NULL},
        {"o", &lavc_param_avopt, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{NULL, NULL, 0, 0, 0, 0, NULL}
};
#endif

struct vf_priv_s {
    muxer_stream_t* mux;
    AVCodecContext *context;
    AVFrame *pic;
    AVCodec *codec;
    FILE *stats_file;
};

#define stats_file (vf->priv->stats_file)
#define mux_v (vf->priv->mux)
#define lavc_venc_context (vf->priv->context)

static int encode_frame(struct vf_instance_s* vf, AVFrame *pic, double pts);

static int config(struct vf_instance_s* vf,
        int width, int height, int d_width, int d_height,
	unsigned int flags, unsigned int outfmt){
    int size, i;
    void *p;
        
    mux_v->bih->biWidth=width;
    mux_v->bih->biHeight=height;
    mux_v->bih->biSizeImage=mux_v->bih->biWidth*mux_v->bih->biHeight*(mux_v->bih->biBitCount/8);

    mp_msg(MSGT_MENCODER, MSGL_INFO,"videocodec: libavcodec (%dx%d fourcc=%x [%.4s])\n",
	mux_v->bih->biWidth, mux_v->bih->biHeight, mux_v->bih->biCompression,
	    (char *)&mux_v->bih->biCompression);

    lavc_venc_context->width = width;
    lavc_venc_context->height = height;
    if (lavc_param_vbitrate > 16000) /* != -1 */
	lavc_venc_context->bit_rate = lavc_param_vbitrate;
    else if (lavc_param_vbitrate >= 0) /* != -1 */
	lavc_venc_context->bit_rate = lavc_param_vbitrate*1000;
    else
	lavc_venc_context->bit_rate = 800000; /* default */

    mux_v->avg_rate= lavc_venc_context->bit_rate;

    lavc_venc_context->bit_rate_tolerance= lavc_param_vrate_tolerance*1000;
    lavc_venc_context->time_base= (AVRational){mux_v->h.dwScale, mux_v->h.dwRate};
    lavc_venc_context->qmin= lavc_param_vqmin;
    lavc_venc_context->qmax= lavc_param_vqmax;
    lavc_venc_context->mb_qmin= lavc_param_mb_qmin;
    lavc_venc_context->mb_qmax= lavc_param_mb_qmax;
    lavc_venc_context->lmin= (int)(FF_QP2LAMBDA * lavc_param_lmin + 0.5);
    lavc_venc_context->lmax= (int)(FF_QP2LAMBDA * lavc_param_lmax + 0.5);
    lavc_venc_context->mb_lmin= (int)(FF_QP2LAMBDA * lavc_param_mb_lmin + 0.5);
    lavc_venc_context->mb_lmax= (int)(FF_QP2LAMBDA * lavc_param_mb_lmax + 0.5);
    lavc_venc_context->max_qdiff= lavc_param_vqdiff;
    lavc_venc_context->qcompress= lavc_param_vqcompress;
    lavc_venc_context->qblur= lavc_param_vqblur;
    lavc_venc_context->max_b_frames= lavc_param_vmax_b_frames;
    lavc_venc_context->b_quant_factor= lavc_param_vb_qfactor;
    lavc_venc_context->rc_strategy= lavc_param_vrc_strategy;
    lavc_venc_context->b_frame_strategy= lavc_param_vb_strategy;
    lavc_venc_context->b_quant_offset= (int)(FF_QP2LAMBDA * lavc_param_vb_qoffset + 0.5);
    lavc_venc_context->luma_elim_threshold= lavc_param_luma_elim_threshold;
    lavc_venc_context->chroma_elim_threshold= lavc_param_chroma_elim_threshold;
    lavc_venc_context->rtp_payload_size= lavc_param_packet_size;
    lavc_venc_context->strict_std_compliance= lavc_param_strict;
    lavc_venc_context->i_quant_factor= lavc_param_vi_qfactor;
    lavc_venc_context->i_quant_offset= (int)(FF_QP2LAMBDA * lavc_param_vi_qoffset + 0.5);
    lavc_venc_context->rc_qsquish= lavc_param_rc_qsquish;
    lavc_venc_context->rc_qmod_amp= lavc_param_rc_qmod_amp;
    lavc_venc_context->rc_qmod_freq= lavc_param_rc_qmod_freq;
    lavc_venc_context->rc_eq= lavc_param_rc_eq;

    mux_v->max_rate=
    lavc_venc_context->rc_max_rate= lavc_param_rc_max_rate*1000;
    lavc_venc_context->rc_min_rate= lavc_param_rc_min_rate*1000;

    mux_v->vbv_size=
    lavc_venc_context->rc_buffer_size= lavc_param_rc_buffer_size*1000;

    lavc_venc_context->rc_initial_buffer_occupancy=
            lavc_venc_context->rc_buffer_size *
            lavc_param_rc_initial_buffer_occupancy;
    lavc_venc_context->rc_buffer_aggressivity= lavc_param_rc_buffer_aggressivity;
    lavc_venc_context->rc_initial_cplx= lavc_param_rc_initial_cplx;
    lavc_venc_context->debug= lavc_param_debug;
    lavc_venc_context->last_predictor_count= lavc_param_last_pred;
    lavc_venc_context->pre_me= lavc_param_pre_me;
    lavc_venc_context->me_pre_cmp= lavc_param_me_pre_cmp;
    lavc_venc_context->pre_dia_size= lavc_param_pre_dia_size;
    lavc_venc_context->me_subpel_quality= lavc_param_me_subpel_quality;
    lavc_venc_context->me_range= lavc_param_me_range;
    lavc_venc_context->intra_quant_bias= lavc_param_ibias;
    lavc_venc_context->inter_quant_bias= lavc_param_pbias;
    lavc_venc_context->coder_type= lavc_param_coder;
    lavc_venc_context->context_model= lavc_param_context;
    lavc_venc_context->scenechange_threshold= lavc_param_sc_threshold;
    lavc_venc_context->noise_reduction= lavc_param_noise_reduction;
    lavc_venc_context->quantizer_noise_shaping= lavc_param_qns;
    lavc_venc_context->inter_threshold= lavc_param_inter_threshold;
    lavc_venc_context->nsse_weight= lavc_param_nssew;
    lavc_venc_context->frame_skip_threshold= lavc_param_skip_threshold;
    lavc_venc_context->frame_skip_factor= lavc_param_skip_factor;
    lavc_venc_context->frame_skip_exp= lavc_param_skip_exp;
    lavc_venc_context->frame_skip_cmp= lavc_param_skip_cmp;

    if (lavc_param_intra_matrix)
    {
	char *tmp;

	lavc_venc_context->intra_matrix =
	    av_malloc(sizeof(*lavc_venc_context->intra_matrix)*64);

	i = 0;
	while ((tmp = strsep(&lavc_param_intra_matrix, ",")) && (i < 64))
	{
	    if (!tmp || (tmp && !strlen(tmp)))
		break;
	    lavc_venc_context->intra_matrix[i++] = atoi(tmp);
	}
	
	if (i != 64)
	    av_freep(&lavc_venc_context->intra_matrix);
	else
	    mp_msg(MSGT_MENCODER, MSGL_V, "Using user specified intra matrix\n");
    }
    if (lavc_param_inter_matrix)
    {
	char *tmp;

	lavc_venc_context->inter_matrix =
	    av_malloc(sizeof(*lavc_venc_context->inter_matrix)*64);

	i = 0;
	while ((tmp = strsep(&lavc_param_inter_matrix, ",")) && (i < 64))
	{
	    if (!tmp || (tmp && !strlen(tmp)))
		break;
	    lavc_venc_context->inter_matrix[i++] = atoi(tmp);
	}
	
	if (i != 64)
	    av_freep(&lavc_venc_context->inter_matrix);
	else
	    mp_msg(MSGT_MENCODER, MSGL_V, "Using user specified inter matrix\n");
    }

    p= lavc_param_rc_override_string;
    for(i=0; p; i++){
        int start, end, q;
        int e=sscanf(p, "%d,%d,%d", &start, &end, &q);
        if(e!=3){
	    mp_msg(MSGT_MENCODER,MSGL_ERR,"error parsing vrc_q\n");
            return 0;
        }
        lavc_venc_context->rc_override= 
            realloc(lavc_venc_context->rc_override, sizeof(RcOverride)*(i+1));
        lavc_venc_context->rc_override[i].start_frame= start;
        lavc_venc_context->rc_override[i].end_frame  = end;
        if(q>0){
            lavc_venc_context->rc_override[i].qscale= q;
            lavc_venc_context->rc_override[i].quality_factor= 1.0;
        }
        else{
            lavc_venc_context->rc_override[i].qscale= 0;
            lavc_venc_context->rc_override[i].quality_factor= -q/100.0;
        }
        p= strchr(p, '/');
        if(p) p++;
    }
    lavc_venc_context->rc_override_count=i;

    lavc_venc_context->mpeg_quant=lavc_param_mpeg_quant;

    lavc_venc_context->dct_algo= lavc_param_fdct;
    lavc_venc_context->idct_algo= lavc_param_idct;

    lavc_venc_context->lumi_masking= lavc_param_lumi_masking;
    lavc_venc_context->temporal_cplx_masking= lavc_param_temporal_cplx_masking;
    lavc_venc_context->spatial_cplx_masking= lavc_param_spatial_cplx_masking;
    lavc_venc_context->p_masking= lavc_param_p_masking;
    lavc_venc_context->dark_masking= lavc_param_dark_masking;
        lavc_venc_context->border_masking = lavc_param_border_masking;

    if (lavc_param_aspect != NULL)
    {
	int par_width, par_height, e;
	float ratio=0;
	
	e= sscanf (lavc_param_aspect, "%d/%d", &par_width, &par_height);
	if(e==2){
            if(par_height)
                ratio= (float)par_width / (float)par_height;
        }else{
	    e= sscanf (lavc_param_aspect, "%f", &ratio);
	}

	if (e && ratio > 0.1 && ratio < 10.0) {
	    lavc_venc_context->sample_aspect_ratio= av_d2q(ratio * height / width, 255);
	    mp_dbg(MSGT_MENCODER, MSGL_DBG2, "sample_aspect_ratio: %d/%d\n", 
                lavc_venc_context->sample_aspect_ratio.num,
                lavc_venc_context->sample_aspect_ratio.den);
	    mux_v->aspect = ratio;
	    mp_dbg(MSGT_MENCODER, MSGL_DBG2, "aspect_ratio: %f\n", ratio);
	} else {
	    mp_dbg(MSGT_MENCODER, MSGL_ERR, "aspect ratio: cannot parse \"%s\"\n", lavc_param_aspect);
	    return 0;
	}
    }
    else if (lavc_param_autoaspect) {
	lavc_venc_context->sample_aspect_ratio = av_d2q((float)d_width/d_height*height / width, 255);
	mux_v->aspect = (float)d_width/d_height;
    }

    /* keyframe interval */
    if (lavc_param_keyint >= 0) /* != -1 */
	lavc_venc_context->gop_size = lavc_param_keyint;
    else
	lavc_venc_context->gop_size = 250; /* default */

    lavc_venc_context->flags = 0;
    if (lavc_param_mb_decision)
    {
	mp_msg(MSGT_MENCODER, MSGL_INFO, MSGTR_MPCODECS_HighQualityEncodingSelected);
        lavc_venc_context->mb_decision= lavc_param_mb_decision;
    }

    lavc_venc_context->me_cmp= lavc_param_me_cmp;
    lavc_venc_context->me_sub_cmp= lavc_param_me_sub_cmp;
    lavc_venc_context->mb_cmp= lavc_param_mb_cmp;
#ifdef FF_CMP_VSAD
    lavc_venc_context->ildct_cmp= lavc_param_ildct_cmp;
#endif    
    lavc_venc_context->dia_size= lavc_param_dia_size;
    lavc_venc_context->flags|= lavc_param_qpel;
    lavc_venc_context->trellis = lavc_param_trell;
    lavc_venc_context->flags|= lavc_param_lowdelay;
    lavc_venc_context->flags|= lavc_param_bit_exact;
    lavc_venc_context->flags|= lavc_param_aic;
    lavc_venc_context->flags|= lavc_param_aiv;
    lavc_venc_context->flags|= lavc_param_umv;
    lavc_venc_context->flags|= lavc_param_obmc;
    lavc_venc_context->flags|= lavc_param_loop;
    lavc_venc_context->flags|= lavc_param_v4mv ? CODEC_FLAG_4MV : 0;
    lavc_venc_context->flags|= lavc_param_data_partitioning;
    lavc_venc_context->flags|= lavc_param_cbp;
    lavc_venc_context->flags|= lavc_param_mv0;
    lavc_venc_context->flags|= lavc_param_qp_rd;
    lavc_venc_context->flags|= lavc_param_ss;
    lavc_venc_context->flags|= lavc_param_alt;
    lavc_venc_context->flags|= lavc_param_ilme;
    lavc_venc_context->flags|= lavc_param_gmc;
#ifdef CODEC_FLAG_CLOSED_GOP
    lavc_venc_context->flags|= lavc_param_closed_gop;
#endif    
    if(lavc_param_gray) lavc_venc_context->flags|= CODEC_FLAG_GRAY;

    if(lavc_param_normalize_aqp) lavc_venc_context->flags|= CODEC_FLAG_NORMALIZE_AQP;
    if(lavc_param_interlaced_dct) lavc_venc_context->flags|= CODEC_FLAG_INTERLACED_DCT;
    lavc_venc_context->flags|= lavc_param_psnr;
    lavc_venc_context->intra_dc_precision = lavc_param_dc_precision - 8;
    lavc_venc_context->prediction_method= lavc_param_prediction_method;
    lavc_venc_context->brd_scale = lavc_param_brd_scale;
    lavc_venc_context->bidir_refine = lavc_param_bidir_refine;
    lavc_venc_context->scenechange_factor = lavc_param_sc_factor;
    if((lavc_param_video_global_header&1)
       /*|| (video_global_header==0 && (oc->oformat->flags & AVFMT_GLOBALHEADER))*/){
        lavc_venc_context->flags |= CODEC_FLAG_GLOBAL_HEADER;
    }
    if(lavc_param_video_global_header&2){
        lavc_venc_context->flags2 |= CODEC_FLAG2_LOCAL_HEADER;
    }
    lavc_venc_context->mv0_threshold = lavc_param_mv0_threshold;
    lavc_venc_context->refs = lavc_param_refs;
    lavc_venc_context->b_sensitivity = lavc_param_b_sensitivity;
    lavc_venc_context->level = lavc_param_level;

    if(lavc_param_avopt){
        if(parse_avopts(lavc_venc_context, lavc_param_avopt) < 0){
            mp_msg(MSGT_MENCODER,MSGL_ERR, "Your options /%s/ look like gibberish to me pal\n", lavc_param_avopt);
            return 0;
        }
    }

    mux_v->imgfmt = lavc_param_format;
    switch(lavc_param_format)
    {
	case IMGFMT_YV12:
	    lavc_venc_context->pix_fmt = PIX_FMT_YUV420P;
	    break;
	case IMGFMT_422P:
	    lavc_venc_context->pix_fmt = PIX_FMT_YUV422P;
	    break;
	case IMGFMT_444P:
	    lavc_venc_context->pix_fmt = PIX_FMT_YUV444P;
	    break;
	case IMGFMT_411P:
	    lavc_venc_context->pix_fmt = PIX_FMT_YUV411P;
	    break;
	case IMGFMT_YVU9:
	    lavc_venc_context->pix_fmt = PIX_FMT_YUV410P;
	    break;
	case IMGFMT_BGR32:
	    lavc_venc_context->pix_fmt = PIX_FMT_RGB32;
	    break;
	default:
    	    mp_msg(MSGT_MENCODER,MSGL_ERR,"%s is not a supported format\n", vo_format_name(lavc_param_format));
    	    return 0;
    }

    if(!stats_file) {
    /* lavc internal 2pass bitrate control */
    switch(lavc_param_vpass){
    case 2:
    case 3:
	lavc_venc_context->flags|= CODEC_FLAG_PASS2; 
	stats_file= fopen(passtmpfile, "rb");
	if(stats_file==NULL){
	    mp_msg(MSGT_MENCODER,MSGL_ERR,"2pass failed: filename=%s\n", passtmpfile);
            return 0;
	}
	fseek(stats_file, 0, SEEK_END);
	size= ftell(stats_file);
	fseek(stats_file, 0, SEEK_SET);
	
	lavc_venc_context->stats_in= av_malloc(size + 1);
	lavc_venc_context->stats_in[size]=0;

	if(fread(lavc_venc_context->stats_in, size, 1, stats_file)<1){
	    mp_msg(MSGT_MENCODER,MSGL_ERR,"2pass failed: reading from filename=%s\n", passtmpfile);
            return 0;
	}        
	if(lavc_param_vpass == 2)
	    break;
	else
	    fclose(stats_file);
	    /* fall through */
    case 1: 
	lavc_venc_context->flags|= CODEC_FLAG_PASS1; 
	stats_file= fopen(passtmpfile, "wb");
	if(stats_file==NULL){
	    mp_msg(MSGT_MENCODER,MSGL_ERR,"2pass failed: filename=%s\n", passtmpfile);
            return 0;
	}
	if(lavc_param_turbo && (lavc_param_vpass == 1)) {
	  /* uses SAD comparison functions instead of other hungrier */
	  lavc_venc_context->me_pre_cmp = 0;
	  lavc_venc_context->me_cmp = 0;
	  lavc_venc_context->me_sub_cmp = 0;
	  lavc_venc_context->mb_cmp = 2;

	  /* Disables diamond motion estimation */
	  lavc_venc_context->pre_dia_size = 0;
	  lavc_venc_context->dia_size = 1;

	  lavc_venc_context->quantizer_noise_shaping = 0; // qns=0
	  lavc_venc_context->noise_reduction = 0; // nr=0
	  lavc_venc_context->mb_decision = 0; // mbd=0 ("realtime" encoding)

	  lavc_venc_context->flags &= ~CODEC_FLAG_QPEL;
	  lavc_venc_context->flags &= ~CODEC_FLAG_4MV;
	  lavc_venc_context->trellis = 0;
	  lavc_venc_context->flags &= ~CODEC_FLAG_CBP_RD;
	  lavc_venc_context->flags &= ~CODEC_FLAG_QP_RD;
	  lavc_venc_context->flags &= ~CODEC_FLAG_MV0;
	}
	break;
    }
    }

    lavc_venc_context->me_method = ME_ZERO+lavc_param_vme;

    /* fixed qscale :p */
    if (lavc_param_vqscale >= 0.0)
    {
	mp_msg(MSGT_MENCODER, MSGL_INFO, MSGTR_MPCODECS_UsingConstantQscale, lavc_param_vqscale);
	lavc_venc_context->flags |= CODEC_FLAG_QSCALE;
        lavc_venc_context->global_quality= 
	vf->priv->pic->quality = (int)(FF_QP2LAMBDA * lavc_param_vqscale + 0.5);
    }

    if(lavc_param_threads > 1)
	avcodec_thread_init(lavc_venc_context, lavc_param_threads);

    if (avcodec_open(lavc_venc_context, vf->priv->codec) != 0) {
	mp_msg(MSGT_MENCODER,MSGL_ERR,MSGTR_CantOpenCodec);
	return 0;
    }

    if (lavc_venc_context->codec->encode == NULL) {
	mp_msg(MSGT_MENCODER,MSGL_ERR,"avcodec init failed (ctx->codec->encode == NULL)!\n");
	return 0;
    }
    
    /* free second pass buffer, its not needed anymore */
    av_freep(&lavc_venc_context->stats_in);
    if(lavc_venc_context->bits_per_coded_sample)
        mux_v->bih->biBitCount= lavc_venc_context->bits_per_coded_sample;
    if(lavc_venc_context->extradata_size){
        mux_v->bih= realloc(mux_v->bih, sizeof(BITMAPINFOHEADER) + lavc_venc_context->extradata_size);
        memcpy(mux_v->bih + 1, lavc_venc_context->extradata, lavc_venc_context->extradata_size);
        mux_v->bih->biSize= sizeof(BITMAPINFOHEADER) + lavc_venc_context->extradata_size;
    }
    
    mux_v->decoder_delay = lavc_venc_context->max_b_frames ? 1 : 0;
    
    return 1;
}

static int control(struct vf_instance_s* vf, int request, void* data){

    switch(request){
        case VFCTRL_FLUSH_FRAMES:
            if(vf->priv->codec->capabilities & CODEC_CAP_DELAY)
                while(encode_frame(vf, NULL, MP_NOPTS_VALUE) > 0);
            return CONTROL_TRUE;
        default:
            return CONTROL_UNKNOWN;
    }
}

static int query_format(struct vf_instance_s* vf, unsigned int fmt){
    switch(fmt){
    case IMGFMT_YV12:
    case IMGFMT_IYUV:
    case IMGFMT_I420:
        if(lavc_param_format == IMGFMT_YV12)
            return VFCAP_CSP_SUPPORTED | VFCAP_ACCEPT_STRIDE;
        break;
    case IMGFMT_411P:
        if(lavc_param_format == IMGFMT_411P)
            return VFCAP_CSP_SUPPORTED | VFCAP_ACCEPT_STRIDE;
        break;
    case IMGFMT_422P:
        if(lavc_param_format == IMGFMT_422P)
            return VFCAP_CSP_SUPPORTED | VFCAP_ACCEPT_STRIDE;
        break;
    case IMGFMT_444P:
        if(lavc_param_format == IMGFMT_444P)
            return VFCAP_CSP_SUPPORTED | VFCAP_ACCEPT_STRIDE;
        break;
    case IMGFMT_YVU9:
        if(lavc_param_format == IMGFMT_YVU9)
            return VFCAP_CSP_SUPPORTED | VFCAP_ACCEPT_STRIDE;
        break;
    case IMGFMT_BGR32:
        if(lavc_param_format == IMGFMT_BGR32)
            return VFCAP_CSP_SUPPORTED | VFCAP_ACCEPT_STRIDE;
        break;
    }
    return 0;
}

static double psnr(double d){
    if(d==0) return INFINITY;
    return -10.0*log(d)/log(10);
}

static int put_image(struct vf_instance_s* vf, mp_image_t *mpi, double pts){
    AVFrame *pic= vf->priv->pic;

    pic->data[0]=mpi->planes[0];
    pic->data[1]=mpi->planes[1];
    pic->data[2]=mpi->planes[2];
    pic->linesize[0]=mpi->stride[0];
    pic->linesize[1]=mpi->stride[1];
    pic->linesize[2]=mpi->stride[2];

    if(lavc_param_interlaced_dct){
        if((mpi->fields & MP_IMGFIELD_ORDERED) && (mpi->fields & MP_IMGFIELD_INTERLACED))
            pic->top_field_first= !!(mpi->fields & MP_IMGFIELD_TOP_FIRST);
        else 
            pic->top_field_first= 1;
    
        if(lavc_param_top!=-1)
            pic->top_field_first= lavc_param_top;
    }

    return encode_frame(vf, pic, pts) >= 0;
}

static int encode_frame(struct vf_instance_s* vf, AVFrame *pic, double pts){
    const char pict_type_char[5]= {'?', 'I', 'P', 'B', 'S'};
    int out_size;
    double dts;

    if(pts == MP_NOPTS_VALUE)
        pts= lavc_venc_context->frame_number * av_q2d(lavc_venc_context->time_base);

    if(pic){
#if 0
        pic->opaque= malloc(sizeof(pts));
        memcpy(pic->opaque, &pts, sizeof(pts));
#else
        if(pts != MP_NOPTS_VALUE)
            pic->pts= floor(pts / av_q2d(lavc_venc_context->time_base) + 0.5);
        else
            pic->pts= MP_NOPTS_VALUE;
#endif
    }
	out_size = avcodec_encode_video(lavc_venc_context, mux_v->buffer, mux_v->buffer_size,
	    pic);

    if(pts != MP_NOPTS_VALUE) 
        dts= pts - lavc_venc_context->delay * av_q2d(lavc_venc_context->time_base);
    else
        dts= MP_NOPTS_VALUE;
#if 0
    pts= lavc_venc_context->coded_frame->opaque ?
           *(double*)lavc_venc_context->coded_frame->opaque
         : MP_NOPTS_VALUE;
#else
    if(lavc_venc_context->coded_frame->pts != MP_NOPTS_VALUE)
        pts= lavc_venc_context->coded_frame->pts * av_q2d(lavc_venc_context->time_base);
    else
        pts= MP_NOPTS_VALUE;
    assert(MP_NOPTS_VALUE == AV_NOPTS_VALUE);
#endif
//fprintf(stderr, "ve_lavc %f/%f\n", dts, pts);
    if(out_size == 0 && lavc_param_skip_threshold==0 && lavc_param_skip_factor==0){
        ++mux_v->encoder_delay;
        return 0;
    }
           
    muxer_write_chunk(mux_v,out_size,lavc_venc_context->coded_frame->key_frame?0x10:0, 
                      dts, pts);
    free(lavc_venc_context->coded_frame->opaque);
    lavc_venc_context->coded_frame->opaque= NULL;
        
    /* store psnr / pict size / type / qscale */
    if(lavc_param_psnr){
        static FILE *fvstats=NULL;
        char filename[20];
        double f= lavc_venc_context->width*lavc_venc_context->height*255.0*255.0;
	double quality=0.0;
	int8_t *q;

        if(!fvstats) {
            time_t today2;
            struct tm *today;
            today2 = time(NULL);
            today = localtime(&today2);
            sprintf(filename, "psnr_%02d%02d%02d.log", today->tm_hour,
                today->tm_min, today->tm_sec);
            fvstats = fopen(filename,"w");
            if(!fvstats) {
                perror("fopen");
                lavc_param_psnr=0; // disable block
                mp_msg(MSGT_MENCODER,MSGL_ERR,"Can't open %s for writing. Check its permissions.\n",filename);
                return -1;
                /*exit(1);*/
            }
        }
	
	// average MB quantizer
	q = lavc_venc_context->coded_frame->qscale_table;
	if(q) {
	    int x, y;
	    int w = (lavc_venc_context->width+15) >> 4;
	    int h = (lavc_venc_context->height+15) >> 4;
	    for( y = 0; y < h; y++ ) {
		for( x = 0; x < w; x++ )
		    quality += (double)*(q+x);
		q += lavc_venc_context->coded_frame->qstride;
	    }
	    quality /= w * h;
	} else 
	    quality = lavc_venc_context->coded_frame->quality / (float)FF_QP2LAMBDA;

        fprintf(fvstats, "%6d, %2.2f, %6d, %2.2f, %2.2f, %2.2f, %2.2f %c\n",
            lavc_venc_context->coded_frame->coded_picture_number,
            quality,
            out_size,
            psnr(lavc_venc_context->coded_frame->error[0]/f),
            psnr(lavc_venc_context->coded_frame->error[1]*4/f),
            psnr(lavc_venc_context->coded_frame->error[2]*4/f),
            psnr((lavc_venc_context->coded_frame->error[0]+lavc_venc_context->coded_frame->error[1]+lavc_venc_context->coded_frame->error[2])/(f*1.5)),
            pict_type_char[lavc_venc_context->coded_frame->pict_type]
            );
    }
    /* store stats if there are any */
    if(lavc_venc_context->stats_out && stats_file) 
        fprintf(stats_file, "%s", lavc_venc_context->stats_out);
    return out_size;
}

static void uninit(struct vf_instance_s* vf){

    if(lavc_param_psnr){
        double f= lavc_venc_context->width*lavc_venc_context->height*255.0*255.0;
        f*= lavc_venc_context->coded_frame->coded_picture_number;
        
        mp_msg(MSGT_MENCODER, MSGL_INFO, "PSNR: Y:%2.2f, Cb:%2.2f, Cr:%2.2f, All:%2.2f\n",
            psnr(lavc_venc_context->error[0]/f),
            psnr(lavc_venc_context->error[1]*4/f),
            psnr(lavc_venc_context->error[2]*4/f),
            psnr((lavc_venc_context->error[0]+lavc_venc_context->error[1]+lavc_venc_context->error[2])/(f*1.5))
            );
    }

    av_freep(&lavc_venc_context->intra_matrix);
    av_freep(&lavc_venc_context->inter_matrix);

    avcodec_close(lavc_venc_context);

    if(stats_file) fclose(stats_file);
    
    /* free rc_override */
    av_freep(&lavc_venc_context->rc_override);

    av_freep(&vf->priv->context);
}

//===========================================================================//

static int vf_open(vf_instance_t *vf, char* args){
    vf->uninit=uninit;
    vf->config=config;
    vf->default_caps=VFCAP_CONSTANT;
    vf->control=control;
    vf->query_format=query_format;
    vf->put_image=put_image;
    vf->priv=malloc(sizeof(struct vf_priv_s));
    memset(vf->priv,0,sizeof(struct vf_priv_s));
    vf->priv->mux=(muxer_stream_t*)args;

    /* XXX: hack: some of the MJPEG decoder DLL's needs exported huffman
       table, so we define a zero-table, also lavc mjpeg encoder is putting
       huffman tables into the stream, so no problem */
    if (lavc_param_vcodec && !strcasecmp(lavc_param_vcodec, "mjpeg"))
    {
	mux_v->bih=malloc(sizeof(BITMAPINFOHEADER)+28);
	memset(mux_v->bih, 0, sizeof(BITMAPINFOHEADER)+28);
	mux_v->bih->biSize=sizeof(BITMAPINFOHEADER)+28;
    }
    else if (lavc_param_vcodec && (!strcasecmp(lavc_param_vcodec, "huffyuv")
                                || !strcasecmp(lavc_param_vcodec, "ffvhuff")))
    {
    /* XXX: hack: huffyuv needs to store huffman tables (allthough we dunno the size yet ...) */
	mux_v->bih=malloc(sizeof(BITMAPINFOHEADER)+1000);
	memset(mux_v->bih, 0, sizeof(BITMAPINFOHEADER)+1000);
	mux_v->bih->biSize=sizeof(BITMAPINFOHEADER)+1000;
    }
    else if (lavc_param_vcodec && !strcasecmp(lavc_param_vcodec, "asv1"))
    {
	mux_v->bih=malloc(sizeof(BITMAPINFOHEADER)+8);
	memset(mux_v->bih, 0, sizeof(BITMAPINFOHEADER)+8);
	mux_v->bih->biSize=sizeof(BITMAPINFOHEADER)+8;
    }
    else if (lavc_param_vcodec && !strcasecmp(lavc_param_vcodec, "asv2"))
    {
	mux_v->bih=malloc(sizeof(BITMAPINFOHEADER)+8);
	memset(mux_v->bih, 0, sizeof(BITMAPINFOHEADER)+8);
	mux_v->bih->biSize=sizeof(BITMAPINFOHEADER)+8;
    }
    else if (lavc_param_vcodec && !strcasecmp(lavc_param_vcodec, "wmv2"))
    {
	mux_v->bih=malloc(sizeof(BITMAPINFOHEADER)+4);
	memset(mux_v->bih, 0, sizeof(BITMAPINFOHEADER)+4);
	mux_v->bih->biSize=sizeof(BITMAPINFOHEADER)+4;
    }
    else
    {
	mux_v->bih=malloc(sizeof(BITMAPINFOHEADER));
	memset(mux_v->bih, 0, sizeof(BITMAPINFOHEADER));
	mux_v->bih->biSize=sizeof(BITMAPINFOHEADER);
    }
    mux_v->bih->biWidth=0;
    mux_v->bih->biHeight=0;
    mux_v->bih->biPlanes=1;
    mux_v->bih->biBitCount=24;
    if (!lavc_param_vcodec)
    {
	printf("No libavcodec codec specified! It's required!\n");
	return 0;
    }

    if (!strcasecmp(lavc_param_vcodec, "mpeg1") || !strcasecmp(lavc_param_vcodec, "mpeg1video"))
	mux_v->bih->biCompression = mmioFOURCC('m', 'p', 'g', '1');
    else if (!strcasecmp(lavc_param_vcodec, "mpeg2") || !strcasecmp(lavc_param_vcodec, "mpeg2video"))
	mux_v->bih->biCompression = mmioFOURCC('m', 'p', 'g', '2');
    else if (!strcasecmp(lavc_param_vcodec, "h263") || !strcasecmp(lavc_param_vcodec, "h263p"))
	mux_v->bih->biCompression = mmioFOURCC('h', '2', '6', '3');
    else if (!strcasecmp(lavc_param_vcodec, "rv10"))
	mux_v->bih->biCompression = mmioFOURCC('R', 'V', '1', '0');
    else if (!strcasecmp(lavc_param_vcodec, "mjpeg"))
	mux_v->bih->biCompression = mmioFOURCC('M', 'J', 'P', 'G');
    else if (!strcasecmp(lavc_param_vcodec, "ljpeg"))
	mux_v->bih->biCompression = mmioFOURCC('L', 'J', 'P', 'G');
    else if (!strcasecmp(lavc_param_vcodec, "mpeg4"))
	mux_v->bih->biCompression = mmioFOURCC('F', 'M', 'P', '4');
    else if (!strcasecmp(lavc_param_vcodec, "msmpeg4"))
	mux_v->bih->biCompression = mmioFOURCC('d', 'i', 'v', '3');
    else if (!strcasecmp(lavc_param_vcodec, "msmpeg4v2"))
	mux_v->bih->biCompression = mmioFOURCC('M', 'P', '4', '2');
    else if (!strcasecmp(lavc_param_vcodec, "wmv1"))
	mux_v->bih->biCompression = mmioFOURCC('W', 'M', 'V', '1');
    else if (!strcasecmp(lavc_param_vcodec, "wmv2"))
	mux_v->bih->biCompression = mmioFOURCC('W', 'M', 'V', '2');
    else if (!strcasecmp(lavc_param_vcodec, "huffyuv"))
	mux_v->bih->biCompression = mmioFOURCC('H', 'F', 'Y', 'U');
    else if (!strcasecmp(lavc_param_vcodec, "ffvhuff"))
	mux_v->bih->biCompression = mmioFOURCC('F', 'F', 'V', 'H');
    else if (!strcasecmp(lavc_param_vcodec, "asv1"))
	mux_v->bih->biCompression = mmioFOURCC('A', 'S', 'V', '1');
    else if (!strcasecmp(lavc_param_vcodec, "asv2"))
	mux_v->bih->biCompression = mmioFOURCC('A', 'S', 'V', '2');
    else if (!strcasecmp(lavc_param_vcodec, "ffv1"))
	mux_v->bih->biCompression = mmioFOURCC('F', 'F', 'V', '1');
    else if (!strcasecmp(lavc_param_vcodec, "snow"))
	mux_v->bih->biCompression = mmioFOURCC('S', 'N', 'O', 'W');
    else if (!strcasecmp(lavc_param_vcodec, "flv"))
	mux_v->bih->biCompression = mmioFOURCC('F', 'L', 'V', '1');
    else if (!strcasecmp(lavc_param_vcodec, "dvvideo"))
	mux_v->bih->biCompression = mmioFOURCC('d', 'v', 's', 'd');
    else if (!strcasecmp(lavc_param_vcodec, "libx264"))
	mux_v->bih->biCompression = mmioFOURCC('h', '2', '6', '4');
    else if (!strcasecmp(lavc_param_vcodec, "libschroedinger"))
	mux_v->bih->biCompression = mmioFOURCC('d', 'r', 'a', 'c');
    else if (!strcasecmp(lavc_param_vcodec, "libdirac"))
	mux_v->bih->biCompression = mmioFOURCC('d', 'r', 'a', 'c');
    else
	mux_v->bih->biCompression = mmioFOURCC(lavc_param_vcodec[0],
		lavc_param_vcodec[1], lavc_param_vcodec[2], lavc_param_vcodec[3]); /* FIXME!!! */

    if (!avcodec_initialized){
	avcodec_init();
	avcodec_register_all();
	avcodec_initialized=1;
    }

    vf->priv->codec = (AVCodec *)avcodec_find_encoder_by_name(lavc_param_vcodec);
    if (!vf->priv->codec) {
	mp_msg(MSGT_MENCODER,MSGL_ERR,MSGTR_MissingLAVCcodec, lavc_param_vcodec);
	return 0;
    }

    vf->priv->pic = avcodec_alloc_frame();
    vf->priv->context = avcodec_alloc_context();

    return 1;
}

vf_info_t ve_info_lavc = {
    "libavcodec encoder",
    "lavc",
    "A'rpi, Alex, Michael",
    "for internal use by mencoder",
    vf_open
};

//===========================================================================//
