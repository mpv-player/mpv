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

#include "../config.h"
#include "../mp_msg.h"

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

#if X264_BUILD < 0x000c
#error We do not support old versions of x264. Get the latest from SVN.
#endif

typedef struct _h264_module_t {
    muxer_stream_t *mux;
    x264_param_t    param;
    x264_t *    x264;
    x264_picture_t  pic;
} h264_module_t;

extern char* passtmpfile;

static int bitrate = -1;
static int qp_constant = 26;
static int frame_ref = 1;
static int iframe = 250;
static int idrframe = 2;
static int scenecut_threshold = 40;
static int bframe = 0;
static int deblock = 1;
static int deblockalpha = 0;
static int deblockbeta = 0;
static int cabac = 1;
static int cabacidc = -1;
static int p4x4mv = 0;
static float ip_factor = 1.4;
static float pb_factor = 1.4;
static int rc_buffer_size = -1;
static int rc_init_buffer = -1;
static int rc_sens = 4;
static int qp_min = 10;
static int qp_max = 51;
static int qp_step = 1;
static int pass = 0;
static float qcomp = 0.6;
static float qblur = 0.5;
static float complexity_blur = 20;
static char *rc_eq = "tex*blurTex^(qComp-1)";
static int subq = 1;
static int psnr = 0;
static int log_level = 2;

m_option_t x264encopts_conf[] = {
    {"bitrate", &bitrate, CONF_TYPE_INT, CONF_RANGE, 0, 24000000, NULL},
    {"qp_constant", &qp_constant, CONF_TYPE_INT, CONF_RANGE, 1, 51, NULL},
    {"frameref", &frame_ref, CONF_TYPE_INT, CONF_RANGE, 1, 15, NULL},
    {"keyint", &iframe, CONF_TYPE_INT, CONF_RANGE, 1, 24000000, NULL},
    {"idrint", &idrframe, CONF_TYPE_INT, CONF_RANGE, 1, 24000000, NULL},
    {"scenecut", &scenecut_threshold, CONF_TYPE_INT, CONF_RANGE, -1, 100, NULL},
    {"bframes", &bframe, CONF_TYPE_INT, CONF_RANGE, 0, 16, NULL},
    {"deblock", &deblock, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"nodeblock", &deblock, CONF_TYPE_FLAG, 0, 1, 0, NULL},
    {"deblockalpha", &deblockalpha, CONF_TYPE_INT, CONF_RANGE, -6, 6, NULL},
    {"deblockbeta", &deblockbeta, CONF_TYPE_INT, CONF_RANGE, -6, 6, NULL},
    {"cabac", &cabac, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"nocabac", &cabac, CONF_TYPE_FLAG, 0, 1, 0, NULL},
    {"cabacidc", &cabacidc, CONF_TYPE_INT, CONF_RANGE, -1, 2, NULL},
    {"4x4mv", &p4x4mv, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"no4x4mv", &p4x4mv, CONF_TYPE_FLAG, 0, 1, 0, NULL},
    {"ip_factor", &ip_factor, CONF_TYPE_FLOAT, CONF_RANGE, -10.0, 10.0, NULL},
    {"pb_factor", &pb_factor, CONF_TYPE_FLOAT, CONF_RANGE, -10.0, 10.0, NULL},
    {"rc_buffer_size", &rc_buffer_size, CONF_TYPE_INT, CONF_RANGE, 0, 24000000, NULL},
    {"rc_init_buffer", &rc_init_buffer, CONF_TYPE_INT, CONF_RANGE, 0, 24000000, NULL},
    {"rc_sens", &rc_sens, CONF_TYPE_INT, CONF_RANGE, 0, 100, NULL},
    {"qp_min", &qp_min, CONF_TYPE_INT, CONF_RANGE, 1, 51, NULL},
    {"qp_max", &qp_max, CONF_TYPE_INT, CONF_RANGE, 1, 51, NULL},
    {"qp_step", &qp_step, CONF_TYPE_INT, CONF_RANGE, 0, 50, NULL},
    {"pass", &pass, CONF_TYPE_INT, CONF_RANGE, 1, 3, NULL},
    {"rc_eq", &rc_eq, CONF_TYPE_STRING, 0, 0, 0, NULL},
    {"qcomp", &qcomp, CONF_TYPE_FLOAT, CONF_RANGE, 0, 1, NULL},
    {"qblur", &qblur, CONF_TYPE_FLOAT, CONF_RANGE, 0, 99, NULL},
    {"cplx_blur", &complexity_blur, CONF_TYPE_FLOAT, CONF_RANGE, 0, 999, NULL},
    {"subq", &subq, CONF_TYPE_INT, CONF_RANGE, 0, 5, NULL},
    {"psnr", &psnr, CONF_TYPE_FLAG, 0, 0, 1, NULL},
    {"nopsnr", &psnr, CONF_TYPE_FLAG, 0, 1, 0, NULL},
    {"log", &log_level, CONF_TYPE_INT, CONF_RANGE, -1, 3, NULL},
    {NULL, NULL, 0, 0, 0, 0, NULL}
};
    

static int config(struct vf_instance_s* vf, int width, int height, int d_width, int d_height, unsigned int flags, unsigned int outfmt) {
    h264_module_t *mod=(h264_module_t*)vf->priv;
    mod->mux->bih->biWidth = width;
    mod->mux->bih->biHeight = height;
    mod->mux->aspect = (float)d_width/d_height;
    
    x264_param_default(&mod->param);
    mod->param.i_frame_reference = frame_ref;
    mod->param.i_idrframe = idrframe;
    mod->param.i_iframe = iframe;
    mod->param.i_scenecut_threshold = scenecut_threshold;
    mod->param.i_bframe = bframe;
    mod->param.b_deblocking_filter = deblock;
    mod->param.i_deblocking_filter_alphac0 = deblockalpha;
    mod->param.i_deblocking_filter_beta = deblockbeta;
    mod->param.b_cabac = cabac;
    mod->param.i_cabac_init_idc = cabacidc;

    mod->param.rc.i_qp_constant = qp_constant;
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
    switch(pass) {
    case 0:
        mod->param.rc.b_stat_write = 0;
        mod->param.rc.b_stat_read = 0;
        break;
    case 1:
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
    if(bitrate > 0) {
        if(rc_buffer_size <= 0)
            rc_buffer_size = bitrate;
        if(rc_init_buffer < 0)
            rc_init_buffer = rc_buffer_size/4;
        mod->param.rc.b_cbr = 1;
        mod->param.rc.i_bitrate = bitrate;
        mod->param.rc.i_rc_buffer_size = rc_buffer_size;
        mod->param.rc.i_rc_init_buffer = rc_init_buffer;
        mod->param.rc.i_rc_sens = rc_sens;
    }
    if(p4x4mv)
        mod->param.analyse.inter = X264_ANALYSE_I4x4 | X264_ANALYSE_PSUB16x16 | X264_ANALYSE_PSUB8x8;
    mod->param.rc.f_ip_factor = ip_factor;
    mod->param.rc.f_pb_factor = pb_factor;

    mod->param.i_width = width;
    mod->param.i_height = height;
    mod->param.i_fps_num = mod->mux->h.dwRate;
    mod->param.i_fps_den = mod->mux->h.dwScale;
    mod->param.analyse.b_psnr = psnr;
    mod->param.i_log_level = log_level;
    mod->param.vui.i_sar_width = d_width*height;
    mod->param.vui.i_sar_height = d_height*width;

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
    
    x264_picture_alloc(&mod->pic, mod->param.i_csp, mod->param.i_width, mod->param.i_height);
    return 1;
}

static int control(struct vf_instance_s* vf, int request, void *data)
{
    return CONTROL_UNKNOWN;
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
        /* 2004/08/05: There seems to be some, but not complete,
           support for these colorspaces in X264. Better to stay
           on the safe side for now. */
        return 0; /* VFCAP_CSP_SUPPORTED */
    }
    return 0;
}

static int put_image(struct vf_instance_s *vf, mp_image_t *mpi)
{
    h264_module_t *mod=(h264_module_t*)vf->priv;
    int i_nal;
    x264_nal_t *nal;
    int i;
    int i_size = 0;
    
    int csp=mod->pic.img.i_csp;
    memset(&mod->pic, 0, sizeof(x264_picture_t));
    mod->pic.img.i_csp=csp;
    mod->pic.img.i_plane=3;
    for(i=0; i<4; i++) {
        mod->pic.img.plane[i] = mpi->planes[i];
        mod->pic.img.i_stride[i] = mpi->stride[i];
    }

    mod->pic.i_type = X264_TYPE_AUTO;
    if(x264_encoder_encode(mod->x264, &nal, &i_nal, &mod->pic) < 0) {
        mp_msg(MSGT_MENCODER, MSGL_ERR, "x264_encoder_encode failed\n");
        return 0;
    }
    
    for(i=0; i < i_nal; i++) {
        int i_data = mod->mux->buffer_size - i_size;
        i_size += x264_nal_encode(mod->mux->buffer + i_size, &i_data, 1, &nal[i]);
    }
    if(i_size>0) {
        int keyframe = (mod->pic.i_type == X264_TYPE_IDR) ||
                       (mod->pic.i_type == X264_TYPE_I && frame_ref == 1);
        muxer_write_chunk(mod->mux, i_size, keyframe?0x10:0);
    }
    return 1;
}

static void uninit(struct vf_instance_s *vf)
{
    // FIXME: flush delayed frames
    h264_module_t *mod=(h264_module_t*)vf->priv;
    x264_encoder_close(mod->x264);
    //x264_picture_clean(&mod->pic);
}

static int vf_open(vf_instance_t *vf, char *args) {
    h264_module_t *mod;

    vf->config = config;
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
