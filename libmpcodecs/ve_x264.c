/*****************************************************************************
 *
 * - H.264 encoder for mencoder using x264 -
 *
 * Copyright (C) 2004 LINUX4MEDIA GmbH
 * Copyright (C) 2004 Ark Linux
 *
 * Written by Bernhard Rosenkraenzer <bero@arklinux.org>
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
 *
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "config.h"
#include "mp_msg.h"

#include "m_option.h"
#include "codec-cfg.h"
#include "stream/stream.h"
#include "libmpdemux/demuxer.h"
#include "libmpdemux/stheader.h"

#include "stream/stream.h"
#include "libmpdemux/muxer.h"

#include "img_format.h"
#include "mp_image.h"
#include "vf.h"

#include <x264.h>

typedef struct h264_module_t {
    muxer_stream_t *mux;
    x264_t *    x264;
    x264_picture_t  pic;
} h264_module_t;

extern char* passtmpfile;
static int turbo = 0;
static x264_param_t param;
static int parse_error = 0;

static int encode_nals(uint8_t *buf, int size, x264_nal_t *nals, int nnal){
    uint8_t *p = buf;
    int i;

    for(i = 0; i < nnal; i++){
        int s = x264_nal_encode(p, &size, 1, nals + i);
        if(s < 0)
            return -1;
        p += s;
    }

    return p - buf;
}

static int put_image(struct vf_instance_s *vf, mp_image_t *mpi, double pts);
static int encode_frame(struct vf_instance_s *vf, x264_picture_t *pic_in);

void x264enc_set_param(const m_option_t* opt, char* arg)
{
    static int initted = 0;
    if(!initted) {
        x264_param_default(&param);
        x264_param_parse(&param, "psnr", "no");
        x264_param_parse(&param, "ssim", "no");
        initted = 1;
    }

    if(!arg) {
        parse_error = 1;
        return;
    }

    while(*arg) {
        char *name = arg;
        char *value;
        int ret;

        arg += strcspn(arg, ":");
        if(*arg) {
            *arg = 0;
            arg++;
        }

        value = strchr( name, '=' );
        if(value) {
            *value = 0;
            value++;
        }

        if(!strcmp(name, "turbo")) {
            turbo = value ? atoi(value) : 1;
            continue;
        }

        ret = x264_param_parse(&param, name, value);
        if(ret == X264_PARAM_BAD_NAME)
	    mp_msg(MSGT_CFGPARSER, MSGL_ERR, "Option x264encopts: Unknown suboption %s\n", name);
        if(ret == X264_PARAM_BAD_VALUE)
	    mp_msg(MSGT_CFGPARSER, MSGL_ERR, "Option x264encopts: Bad argument %s=%s\n", name, value ? value : "(null)");

        /* mark this option as done, so it's not reparsed if there's another -x264encopts */
        *name = 0;

        parse_error |= ret;
    }

    if(param.rc.b_stat_write && !param.rc.b_stat_read) {
        /* Adjust or disable some flags to gain speed in the first pass */
        if(turbo == 1)
        {
            param.i_frame_reference = ( param.i_frame_reference + 1 ) >> 1;
            param.analyse.i_subpel_refine = FFMAX( FFMIN( 3, param.analyse.i_subpel_refine - 1 ), 1 );
            param.analyse.inter &= ( ~X264_ANALYSE_PSUB8x8 );
            param.analyse.inter &= ( ~X264_ANALYSE_BSUB16x16 );
            param.analyse.i_trellis = 0;
        }
        else if(turbo >= 2)
        {
            param.i_frame_reference = 1;
            param.analyse.i_subpel_refine = 1;
            param.analyse.i_me_method = X264_ME_DIA;
            param.analyse.inter = 0;
            param.analyse.b_transform_8x8 = 0;
            param.analyse.b_weighted_bipred = 0;
            param.analyse.i_trellis = 0;
        }
    }
}

static int config(struct vf_instance_s* vf, int width, int height, int d_width, int d_height, unsigned int flags, unsigned int outfmt) {
    h264_module_t *mod=(h264_module_t*)vf->priv;

    if(parse_error)
        return 0;

    mod->mux->bih->biWidth = width;
    mod->mux->bih->biHeight = height;
    mod->mux->bih->biSizeImage = width * height * 3;
    mod->mux->aspect = (float)d_width/d_height;
    
    // make sure param is initialized
    x264enc_set_param(NULL, "");
    param.i_width = width;
    param.i_height = height;
    param.i_fps_num = mod->mux->h.dwRate;
    param.i_fps_den = mod->mux->h.dwScale;
    param.vui.i_sar_width = d_width*height;
    param.vui.i_sar_height = d_height*width;

    x264_param_parse(&param, "stats", passtmpfile);

    switch(outfmt) {
    case IMGFMT_I420:
        param.i_csp = X264_CSP_I420;
        break;
    case IMGFMT_YV12:
        param.i_csp = X264_CSP_YV12;
        break;
    default:
        mp_msg(MSGT_MENCODER, MSGL_ERR, "Wrong colorspace.\n");
        return 0;
    }
    
    mod->x264 = x264_encoder_open(&param);
    if(!mod->x264) {
        mp_msg(MSGT_MENCODER, MSGL_ERR, "x264_encoder_open failed.\n");
        return 0;
    }

    if(!param.b_repeat_headers){
        uint8_t *extradata;
        x264_nal_t *nal;
        int extradata_size, nnal, i, s = 0;

        x264_encoder_headers(mod->x264, &nal, &nnal);

        /* 5 bytes NAL header + worst case escaping */
        for(i = 0; i < nnal; i++)
            s += 5 + nal[i].i_payload * 4 / 3;

        extradata = malloc(s);
        extradata_size = encode_nals(extradata, s, nal, nnal);

        mod->mux->bih= realloc(mod->mux->bih, sizeof(BITMAPINFOHEADER) + extradata_size);
        memcpy(mod->mux->bih + 1, extradata, extradata_size);
        mod->mux->bih->biSize= sizeof(BITMAPINFOHEADER) + extradata_size;
    }
    
    if (param.i_bframe > 1 && param.b_bframe_pyramid)
        mod->mux->decoder_delay = 2;
    else
        mod->mux->decoder_delay = param.i_bframe ? 1 : 0;
    
    return 1;
}

static int control(struct vf_instance_s* vf, int request, void *data)
{
    h264_module_t *mod=(h264_module_t*)vf->priv;
    switch(request){
        case VFCTRL_FLUSH_FRAMES:
            if(param.i_bframe)
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
        return VFCAP_CSP_SUPPORTED | VFCAP_CSP_SUPPORTED_BY_HW;
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

static int put_image(struct vf_instance_s *vf, mp_image_t *mpi, double pts)
{
    h264_module_t *mod=(h264_module_t*)vf->priv;
    int i;
    
    memset(&mod->pic, 0, sizeof(x264_picture_t));
    mod->pic.img.i_csp=param.i_csp;
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
                        && param.i_frame_reference == 1
                        && !param.i_bframe);
        muxer_write_chunk(mod->mux, i_size, keyframe?0x10:0, MP_NOPTS_VALUE, MP_NOPTS_VALUE);
    }
    else
        ++mod->mux->encoder_delay;

    return i_size;
}

static void uninit(struct vf_instance_s *vf)
{
    h264_module_t *mod=(h264_module_t*)vf->priv;
    if (mod->x264)
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
