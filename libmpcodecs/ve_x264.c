/*****************************************************************************
 *
 * - H.264 encoder for mencoder using x264 -
 *
 * Copyright (C) 2004 LINUX4MEDIA GmbH
 * Copyright (C) 2004 Ark Linux
 *
 * Written by Bernhard Rosenkraenzer <bero@arklinux.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation, or if, and only if,
 * version 2 is ruled invalid in a court of law, any later version
 * of the GNU General Public License published by the Free Software
 * Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTIBILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "config.h"
#include "mp_msg.h"

#ifdef HAVE_X264

#include "m_option.h"
#include "codec-cfg.h"
#include "stream.h"
#include "demuxer.h"
#include "stheader.h"

#include "muxer.h"

#include "img_format.h"
#include "mp_image.h"
#include "vf.h"

#include <x264.h>

typedef struct _h264_module_t {
    muxer_stream_t *mux;
    x264_param_t    param;
    x264_t *    x264;
    x264_picture_t  pic;
} h264_module_t;

extern char* passtmpfile;

static int bitrate = -1;
static int qp_constant = 26;
static int rf_constant = 0;
static int frame_ref = 1;
static int keyint_max = 250;
static int keyint_min = 25;
static int scenecut_threshold = 40;
static int bframe = 0;
static int bframe_adaptive = 1;
static int bframe_bias = 0;
static int bframe_pyramid = 0;
static int deblock = 1;
static int deblockalpha = 0;
static int deblockbeta = 0;
static int cabac = 1;
static int p4x4mv = 0;
static int p8x8mv = 1;
static int b8x8mv = 1;
static int i8x8 = 1;
static int i4x4 = 1;
static int dct8 = 0;
static int direct_pred = X264_DIRECT_PRED_TEMPORAL;
static int weight_b = 0;
static int chroma_me = 1;
static int mixed_references = 0;
static int chroma_qp_offset = 0;
static float ip_factor = 1.4;
static float pb_factor = 1.3;
static float ratetol = 1.0;
static int vbv_maxrate = 0;
static int vbv_bufsize = 0;
static float vbv_init = 0.9;
static int qp_min = 10;
static int qp_max = 51;
static int qp_step = 2;
static int pass = 0;
static float qcomp = 0.6;
static float qblur = 0.5;
static float complexity_blur = 20;
static char *rc_eq = "blurCplx^(1-qComp)";
static char *zones = NULL;
static int subq = 5;
static int bframe_rdo = 0;
static int bidir_me = 0;
static int me_method = 2;
static int me_range = 16;
static int trellis = 1;
static int fast_pskip = 1;
static int noise_reduction = 0;
static int threads = 1;
static int level_idc = 51;
static int psnr = 0;
static int log_level = 2;
static int turbo = 0;
static int visualize = 0;
static char *cqm = NULL;
static char *cqm4iy = NULL;
static char *cqm4ic = NULL;
static char *cqm4py = NULL;
static char *cqm4pc = NULL;
static char *cqm8iy = NULL;
static char *cqm8py = NULL;

m_option_t x264encopts_conf[] = {
    {"bitrate", &bitrate, CONF_TYPE_INT, CONF_RANGE, 0, 24000000, NULL},
    {"qp_constant", &qp_constant, CONF_TYPE_INT, CONF_RANGE, 0, 51, NULL},
    {"qp", &qp_constant, CONF_TYPE_INT, CONF_RANGE, 0, 51, NULL},
    {"crf", &rf_constant, CONF_TYPE_INT, CONF_RANGE, 1, 50, NULL},
    {"frameref", &frame_ref, CONF_TYPE_INT, CONF_RANGE, 1, 16, NULL},
    {"keyint", &keyint_max, CONF_TYPE_INT, CONF_RANGE, 1, 24000000, NULL},
    {"keyint_min", &keyint_min, CONF_TYPE_INT, CONF_RANGE, 1, 24000000, NULL},
    {"scenecut", &scenecut_threshold, CONF_TYPE_INT, CONF_RANGE, -1, 100, NULL},
    {"bframes", &bframe, CONF_TYPE_INT, CONF_RANGE, 0, 16, NULL},
    {"b_adapt", &bframe_adaptive, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"nob_adapt", &bframe_adaptive, CONF_TYPE_FLAG, 0, 1, 0, NULL},
    {"b_bias", &bframe_bias, CONF_TYPE_INT, CONF_RANGE, -100, 100, NULL},
    {"b_pyramid", &bframe_pyramid, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"nob_pyramid", &bframe_pyramid, CONF_TYPE_FLAG, 0, 1, 0, NULL},
    {"deblock", &deblock, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"nodeblock", &deblock, CONF_TYPE_FLAG, 0, 1, 0, NULL},
    {"deblockalpha", &deblockalpha, CONF_TYPE_INT, CONF_RANGE, -6, 6, NULL},
    {"deblockbeta", &deblockbeta, CONF_TYPE_INT, CONF_RANGE, -6, 6, NULL},
    {"cabac", &cabac, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"nocabac", &cabac, CONF_TYPE_FLAG, 0, 1, 0, NULL},
    {"4x4mv", &p4x4mv, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"no4x4mv", &p4x4mv, CONF_TYPE_FLAG, 0, 1, 0, NULL},
    {"8x8mv", &p8x8mv, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"no8x8mv", &p8x8mv, CONF_TYPE_FLAG, 0, 1, 0, NULL},
    {"b8x8mv", &b8x8mv, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"nob8x8mv", &b8x8mv, CONF_TYPE_FLAG, 0, 1, 0, NULL},
    {"i4x4", &i4x4, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"noi4x4", &i4x4, CONF_TYPE_FLAG, 0, 0, 0, NULL},
    {"i8x8", &i8x8, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"noi8x8", &i8x8, CONF_TYPE_FLAG, 0, 0, 0, NULL},
    {"8x8dct", &dct8, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"no8x8dct", &dct8, CONF_TYPE_FLAG, 0, 0, 0, NULL},
    {"direct_pred", &direct_pred, CONF_TYPE_INT, CONF_RANGE, 0, 2, NULL},
    {"weight_b", &weight_b, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"noweight_b", &weight_b, CONF_TYPE_FLAG, 0, 1, 0, NULL},
    {"bime", &bidir_me, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"nobime", &bidir_me, CONF_TYPE_FLAG, 0, 0, 0, NULL},
    {"chroma_me", &chroma_me, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"nochroma_me", &chroma_me, CONF_TYPE_FLAG, 0, 1, 0, NULL},
    {"mixed_refs", &mixed_references, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"nomixed_refs", &mixed_references, CONF_TYPE_FLAG, 0, 1, 0, NULL},
    {"chroma_qp_offset", &chroma_qp_offset, CONF_TYPE_INT, CONF_RANGE, -12, 12, NULL},
    {"ip_factor", &ip_factor, CONF_TYPE_FLOAT, CONF_RANGE, -10.0, 10.0, NULL},
    {"pb_factor", &pb_factor, CONF_TYPE_FLOAT, CONF_RANGE, -10.0, 10.0, NULL},
    {"ratetol", &ratetol, CONF_TYPE_FLOAT, CONF_RANGE, 0.1, 100.0, NULL},
    {"vbv_maxrate", &vbv_maxrate, CONF_TYPE_INT, CONF_RANGE, 0, 24000000, NULL},
    {"vbv_bufsize", &vbv_bufsize, CONF_TYPE_INT, CONF_RANGE, 0, 24000000, NULL},
    {"vbv_init", &vbv_init, CONF_TYPE_FLOAT, CONF_RANGE, 0.0, 1.0, NULL},
    {"qp_min", &qp_min, CONF_TYPE_INT, CONF_RANGE, 1, 51, NULL},
    {"qp_max", &qp_max, CONF_TYPE_INT, CONF_RANGE, 1, 51, NULL},
    {"qp_step", &qp_step, CONF_TYPE_INT, CONF_RANGE, 1, 50, NULL},
    {"pass", &pass, CONF_TYPE_INT, CONF_RANGE, 1, 3, NULL},
    {"rc_eq", &rc_eq, CONF_TYPE_STRING, 0, 0, 0, NULL},
    {"cqm", &cqm, CONF_TYPE_STRING, 0, 0, 0, NULL},
    {"cqm4iy", &cqm4iy, CONF_TYPE_STRING, 0, 0, 0, NULL},
    {"cqm4ic", &cqm4ic, CONF_TYPE_STRING, 0, 0, 0, NULL},
    {"cqm4py", &cqm4py, CONF_TYPE_STRING, 0, 0, 0, NULL},
    {"cqm4pc", &cqm4pc, CONF_TYPE_STRING, 0, 0, 0, NULL},
    {"cqm8iy", &cqm8iy, CONF_TYPE_STRING, 0, 0, 0, NULL},
    {"cqm8py", &cqm8py, CONF_TYPE_STRING, 0, 0, 0, NULL},
    {"qcomp", &qcomp, CONF_TYPE_FLOAT, CONF_RANGE, 0, 1, NULL},
    {"qblur", &qblur, CONF_TYPE_FLOAT, CONF_RANGE, 0, 99, NULL},
    {"cplx_blur", &complexity_blur, CONF_TYPE_FLOAT, CONF_RANGE, 0, 999, NULL},
    {"zones", &zones, CONF_TYPE_STRING, 0, 0, 0, NULL},
    {"subq", &subq, CONF_TYPE_INT, CONF_RANGE, 1, 6, NULL},
    {"brdo", &bframe_rdo, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"nobrdo", &bframe_rdo, CONF_TYPE_FLAG, 0, 0, 0, NULL},
    {"me", &me_method, CONF_TYPE_INT, CONF_RANGE, 1, 4, NULL},
    {"me_range", &me_range, CONF_TYPE_INT, CONF_RANGE, 4, 64, NULL},
    {"trellis", &trellis, CONF_TYPE_INT, CONF_RANGE, 0, 2, NULL},
    {"fast_pskip", &fast_pskip, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"nofast_pskip", &fast_pskip, CONF_TYPE_FLAG, 0, 0, 0, NULL},
    {"nr", &noise_reduction, CONF_TYPE_INT, CONF_RANGE, 0, 100000, NULL},
    {"level_idc", &level_idc, CONF_TYPE_INT, CONF_RANGE, 10, 51, NULL},
    {"threads", &threads, CONF_TYPE_INT, CONF_RANGE, 1, 4, NULL},
    {"psnr", &psnr, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"nopsnr", &psnr, CONF_TYPE_FLAG, 0, 1, 0, NULL},
    {"log", &log_level, CONF_TYPE_INT, CONF_RANGE, -1, 3, NULL},
    {"turbo", &turbo, CONF_TYPE_INT, CONF_RANGE, 0, 2, NULL},
    {"visualize", &visualize, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"novisualize", &visualize, CONF_TYPE_FLAG, 0, 1, 0, NULL},
    {NULL, NULL, 0, 0, 0, 0, NULL}
};

static int parse_cqm(const char *str, uint8_t *cqm, int length,
                     h264_module_t *mod, char *matrix_name) {
    int i;
    if (!str) return 0;
    for (i = 0; i < length; i++) {
        long coef = strtol(str, &str, 0);
        if (coef < 1 || coef > 255 || str[0] != ((i + 1 == length)?0:',')) {
            mp_msg( MSGT_MENCODER, MSGL_ERR, "x264: Invalid entry in cqm%s at position %d.\n", matrix_name, i+1 );
            return -1;
        }
        cqm[i] = coef;
        str = &str[1];
    }
    mod->param.i_cqm_preset = X264_CQM_CUSTOM;
    return 0;
}

static int put_image(struct vf_instance_s *vf, mp_image_t *mpi);
static int encode_frame(struct vf_instance_s *vf, x264_picture_t *pic_in);

static int config(struct vf_instance_s* vf, int width, int height, int d_width, int d_height, unsigned int flags, unsigned int outfmt) {
    h264_module_t *mod=(h264_module_t*)vf->priv;
    mod->mux->bih->biWidth = width;
    mod->mux->bih->biHeight = height;
    mod->mux->aspect = (float)d_width/d_height;
    
    x264_param_default(&mod->param);
    mod->param.i_frame_reference = frame_ref;
    mod->param.i_keyint_max = keyint_max;
    mod->param.i_keyint_min = keyint_min;
    mod->param.i_scenecut_threshold = scenecut_threshold;
    mod->param.i_bframe = bframe;
    mod->param.b_bframe_adaptive = bframe_adaptive;
    mod->param.i_bframe_bias = bframe_bias;
    mod->param.b_bframe_pyramid = bframe_pyramid;
    mod->param.b_deblocking_filter = deblock;
    mod->param.i_deblocking_filter_alphac0 = deblockalpha;
    mod->param.i_deblocking_filter_beta = deblockbeta;
    mod->param.b_cabac = cabac;

    mod->param.rc.i_qp_constant = qp_constant;
    mod->param.rc.i_rf_constant = rf_constant;
    if(qp_min > qp_constant)
        qp_min = qp_constant;
    if(qp_max < qp_constant)
        qp_max = qp_constant;
    mod->param.rc.i_qp_min = qp_min;
    mod->param.rc.i_qp_max = qp_max;
    mod->param.rc.i_qp_step = qp_step;
    mod->param.rc.psz_rc_eq = rc_eq;
    mod->param.rc.f_qcompress = qcomp;
    mod->param.rc.f_qblur = qblur;
    mod->param.rc.f_complexity_blur = complexity_blur;
    mod->param.analyse.i_subpel_refine = subq;
    mod->param.rc.psz_stat_out = passtmpfile;
    mod->param.rc.psz_stat_in = passtmpfile;
    if((pass & 2) && bitrate <= 0)
    {
        mp_msg(MSGT_MENCODER, MSGL_ERR,
               "2 pass encoding enabled, but no bitrate specified.\n");
        return 0;
    }
    if(bitrate > 0) {
        if((vbv_maxrate > 0) != (vbv_bufsize > 0)) {
            mp_msg(MSGT_MENCODER, MSGL_ERR,
                   "VBV requires both vbv_maxrate and vbv_bufsize.\n");
            return 0;
        }
        mod->param.rc.b_cbr = 1;
        mod->param.rc.i_bitrate = bitrate;
        mod->param.rc.f_rate_tolerance = ratetol;
        mod->param.rc.i_vbv_max_bitrate = vbv_maxrate;
        mod->param.rc.i_vbv_buffer_size = vbv_bufsize;
        mod->param.rc.f_vbv_buffer_init = vbv_init;
    }
    mod->param.rc.f_ip_factor = ip_factor;
    mod->param.rc.f_pb_factor = pb_factor;
    mod->param.rc.psz_zones = zones;
    switch(me_method) {
        case 1: mod->param.analyse.i_me_method = X264_ME_DIA; break;
        case 2: mod->param.analyse.i_me_method = X264_ME_HEX; break;
        case 3: mod->param.analyse.i_me_method = X264_ME_UMH; break;
        case 4: mod->param.analyse.i_me_method = X264_ME_ESA; break;
    }
    mod->param.analyse.inter = 0;
    if(p4x4mv) mod->param.analyse.inter |= X264_ANALYSE_PSUB8x8;
    if(p8x8mv) mod->param.analyse.inter |= X264_ANALYSE_PSUB16x16;
    if(b8x8mv) mod->param.analyse.inter |= X264_ANALYSE_BSUB16x16;
    if(i4x4)   mod->param.analyse.inter |= X264_ANALYSE_I4x4;
    if(i8x8)   mod->param.analyse.inter |= X264_ANALYSE_I8x8;
    mod->param.analyse.b_transform_8x8 = dct8;
    mod->param.analyse.i_direct_mv_pred = direct_pred;
    mod->param.analyse.b_weighted_bipred = weight_b;
    mod->param.analyse.i_chroma_qp_offset = chroma_qp_offset;
    mod->param.analyse.b_bidir_me = bidir_me;
    mod->param.analyse.b_chroma_me = chroma_me;
    mod->param.analyse.b_mixed_references = mixed_references;
    mod->param.analyse.i_trellis = trellis;
    mod->param.analyse.b_fast_pskip = fast_pskip;
    mod->param.analyse.i_noise_reduction = noise_reduction;
    mod->param.analyse.b_bframe_rdo = bframe_rdo;

    mod->param.i_width = width;
    mod->param.i_height = height;
    mod->param.i_fps_num = mod->mux->h.dwRate;
    mod->param.i_fps_den = mod->mux->h.dwScale;
    mod->param.i_level_idc = level_idc;
    mod->param.analyse.b_psnr = psnr;
    mod->param.i_log_level = log_level;
    mod->param.b_visualize = visualize;
    mod->param.vui.i_sar_width = d_width*height;
    mod->param.vui.i_sar_height = d_height*width;
    mod->param.i_threads = threads;

    if(cqm != NULL)
    {
        if( !strcmp(cqm, "flat") )
            mod->param.i_cqm_preset = X264_CQM_FLAT;
        else if( !strcmp(cqm, "jvt") )
            mod->param.i_cqm_preset = X264_CQM_JVT;
        else
        {
            FILE *cqm_test;
            cqm_test = fopen( cqm, "rb" );
            if( cqm_test )
            {
                mod->param.i_cqm_preset = X264_CQM_CUSTOM;
                mod->param.psz_cqm_file = cqm;
                fclose( cqm_test );
            }
            else
            {
                mp_msg( MSGT_MENCODER, MSGL_ERR, "x264: CQM file failed to open.\n" );
                return 0;
            }
        }
    }

    if( (parse_cqm(cqm4iy, mod->param.cqm_4iy, 16, mod, "4iy") < 0) ||
        (parse_cqm(cqm4ic, mod->param.cqm_4ic, 16, mod, "4ic") < 0) ||
        (parse_cqm(cqm4py, mod->param.cqm_4py, 16, mod, "4py") < 0) ||
        (parse_cqm(cqm4pc, mod->param.cqm_4pc, 16, mod, "4pc") < 0) ||
        (parse_cqm(cqm8iy, mod->param.cqm_8iy, 64, mod, "8iy") < 0) ||
        (parse_cqm(cqm8py, mod->param.cqm_8py, 64, mod, "8py") < 0) )
        return 0;

    switch(pass) {
    case 0:
        mod->param.rc.b_stat_write = 0;
        mod->param.rc.b_stat_read = 0;
        break;
    case 1:
        /* Adjust or disable some flags to gain speed in the first pass */
        if(turbo == 1)
        {
            mod->param.i_frame_reference = ( frame_ref + 1 ) >> 1;
            mod->param.analyse.i_subpel_refine = max( min( 3, subq - 1 ), 1 );
            mod->param.analyse.inter &= ( ~X264_ANALYSE_PSUB8x8 );
            mod->param.analyse.inter &= ( ~X264_ANALYSE_BSUB16x16 );
            mod->param.analyse.i_trellis = 0;
        }
        else if(turbo == 2)
        {
            mod->param.i_frame_reference = 1;
            mod->param.analyse.i_subpel_refine = 1;
            mod->param.analyse.i_me_method = X264_ME_DIA;
            mod->param.analyse.inter = 0;
            mod->param.analyse.b_transform_8x8 = 0;
            mod->param.analyse.b_weighted_bipred = 0;
            mod->param.analyse.i_trellis = 0;
        }
        mod->param.rc.b_stat_write = 1;
        mod->param.rc.b_stat_read = 0;
        break;
    case 2:
        mod->param.rc.b_stat_write = 0;
        mod->param.rc.b_stat_read = 1;
        break;
    case 3:
        mod->param.rc.b_stat_write = 1;
        mod->param.rc.b_stat_read = 1;
        break;
    }

    if(me_method >= 3)
        mod->param.analyse.i_me_range = me_range;

    switch(outfmt) {
    case IMGFMT_I420:
        mod->param.i_csp = X264_CSP_I420;
        mod->mux->bih->biSizeImage = width * height * 3;
        break;
    case IMGFMT_YV12:
        mod->param.i_csp = X264_CSP_YV12;
        mod->mux->bih->biSizeImage = width * height * 3;
        break;
    case IMGFMT_422P:
        mod->param.i_csp = X264_CSP_I422;
        mod->mux->bih->biSizeImage = width * height * 3;
        break;
    case IMGFMT_444P:
        mod->param.i_csp = X264_CSP_I444;
        mod->mux->bih->biSizeImage = width * height * 3;
        break;
    case IMGFMT_YVYU:
        mod->param.i_csp = X264_CSP_YUYV;
        mod->mux->bih->biSizeImage = width * height * 3;
        break;
    case IMGFMT_RGB:
        mod->param.i_csp = X264_CSP_RGB;
        mod->mux->bih->biSizeImage = width * height * 3;
        break;
    case IMGFMT_BGR:
        mod->param.i_csp = X264_CSP_BGR;
        mod->mux->bih->biSizeImage = width * height * 3;
        break;
    case IMGFMT_BGR32:
        mod->param.i_csp = X264_CSP_BGRA;
        mod->mux->bih->biSizeImage = width * height * 4;
        break;
    default:
        mp_msg(MSGT_MENCODER, MSGL_ERR, "Wrong colorspace.\n");
        return 0;
    }
    
    mod->x264 = x264_encoder_open(&mod->param);
    if(!mod->x264) {
        mp_msg(MSGT_MENCODER, MSGL_ERR, "x264_encoder_open failed.\n");
        return 0;
    }
    
    return 1;
}

static int control(struct vf_instance_s* vf, int request, void *data)
{
    switch(request){
        case VFCTRL_FLUSH_FRAMES:
            if(bframe)
                while(encode_frame(vf, NULL) > 0);
            return CONTROL_TRUE;
        default:
            return CONTROL_UNKNOWN;
    }
}

static int query_format(struct vf_instance_s* vf, unsigned int fmt)
{
    switch(fmt) {
    case IMGFMT_I420:
        return (VFCAP_CSP_SUPPORTED|VFCAP_CSP_SUPPORTED_BY_HW);
    case IMGFMT_YV12:
    case IMGFMT_422P:
    case IMGFMT_444P:
    case IMGFMT_YVYU:
    case IMGFMT_RGB:
    case IMGFMT_BGR:
    case IMGFMT_BGR32:
        /* These colorspaces are supported, but they'll just have
         * to be converted to I420 internally */
        return 0; /* VFCAP_CSP_SUPPORTED */
    }
    return 0;
}

static int put_image(struct vf_instance_s *vf, mp_image_t *mpi)
{
    h264_module_t *mod=(h264_module_t*)vf->priv;
    int i;
    
    memset(&mod->pic, 0, sizeof(x264_picture_t));
    mod->pic.img.i_csp=mod->param.i_csp;
    mod->pic.img.i_plane=3;
    for(i=0; i<4; i++) {
        mod->pic.img.plane[i] = mpi->planes[i];
        mod->pic.img.i_stride[i] = mpi->stride[i];
    }

    mod->pic.i_type = X264_TYPE_AUTO;

    return encode_frame(vf, &mod->pic) >= 0;
}

static int encode_frame(struct vf_instance_s *vf, x264_picture_t *pic_in)
{
    h264_module_t *mod=(h264_module_t*)vf->priv;
    x264_picture_t pic_out;
    x264_nal_t *nal;
    int i_nal;
    int i_size = 0;
    int i;

    if(x264_encoder_encode(mod->x264, &nal, &i_nal, pic_in, &pic_out) < 0) {
        mp_msg(MSGT_MENCODER, MSGL_ERR, "x264_encoder_encode failed\n");
        return -1;
    }
    
    for(i=0; i < i_nal; i++) {
        int i_data = mod->mux->buffer_size - i_size;
        i_size += x264_nal_encode(mod->mux->buffer + i_size, &i_data, 1, &nal[i]);
    }
    if(i_size>0) {
        int keyframe = (pic_out.i_type == X264_TYPE_IDR) ||
                       (pic_out.i_type == X264_TYPE_I
                        && frame_ref == 1 && !bframe);
        muxer_write_chunk(mod->mux, i_size, keyframe?0x10:0, MP_NOPTS_VALUE, MP_NOPTS_VALUE);
    }

    return i_size;
}

static void uninit(struct vf_instance_s *vf)
{
    h264_module_t *mod=(h264_module_t*)vf->priv;
    x264_encoder_close(mod->x264);
}

static int vf_open(vf_instance_t *vf, char *args) {
    h264_module_t *mod;

    vf->config = config;
    vf->default_caps = VFCAP_CONSTANT;
    vf->control = control;
    vf->query_format = query_format;
    vf->put_image = put_image;
    vf->uninit = uninit;
    vf->priv = malloc(sizeof(h264_module_t));

    mod=(h264_module_t*)vf->priv;
    mod->mux = (muxer_stream_t*)args;
    mod->mux->bih = malloc(sizeof(BITMAPINFOHEADER));
    memset(mod->mux->bih, 0, sizeof(BITMAPINFOHEADER));
    mod->mux->bih->biSize = sizeof(BITMAPINFOHEADER);
    mod->mux->bih->biPlanes = 1;
    mod->mux->bih->biBitCount = 24;
    mod->mux->bih->biCompression = mmioFOURCC('h', '2', '6', '4');

    return 1;
}

vf_info_t ve_info_x264 = {
    "H.264 encoder",
    "x264",
    "Bernhard Rosenkraenzer <bero@arklinux.org>",
    "(C) 2004 LINUX4MEDIA GmbH; (C) 2004 Ark Linux",
    vf_open
};
#endif
