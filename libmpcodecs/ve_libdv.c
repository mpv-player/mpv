// requires libdv-0.9.5 !!!
// (v0.9.0 is too old and has no encoding functionality exported!)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../config.h"
#include "../mp_msg.h"

#ifdef HAVE_LIBDV095

#include "codec-cfg.h"
#include "stream.h"
#include "demuxer.h"
#include "stheader.h"

#include "aviwrite.h"

#include "../libvo/img_format.h"
#include "../mp_image.h"
#include "vf.h"

#include <libdv/dv.h>

#ifndef DV_WIDTH
#define DV_WIDTH       720
#define DV_PAL_HEIGHT  576
#define DV_NTSC_HEIGHT 480
#endif

struct vf_priv_s {
    aviwrite_stream_t* mux;
    dv_encoder_t* enc;
    
};
#define mux_v (vf->priv->mux)

//===========================================================================//

static int config(struct vf_instance_s* vf,
        int width, int height, int d_width, int d_height,
	unsigned int flags, unsigned int outfmt){

    if(width!=DV_WIDTH || (height!=DV_PAL_HEIGHT && height!=DV_NTSC_HEIGHT)){
	mp_msg(MSGT_VFILTER,MSGL_ERR,"DV: only 720x480 (NTSC) and 720x576 (PAL) resolutions allowed! try with -vop scale=720:480\n");
    }
    
    vf->priv->enc->isPAL=(height==DV_PAL_HEIGHT);
    vf->priv->enc->is16x9=(d_width/(float)d_height > 1.7); // 16:9=1.777777
    vf->priv->enc->vlc_encode_passes=3;
    vf->priv->enc->static_qno=0;
    vf->priv->enc->force_dct=0;

    mux_v->bih->biWidth=width;
    mux_v->bih->biHeight=height;
    mux_v->bih->biSizeImage=mux_v->bih->biWidth*mux_v->bih->biHeight*(mux_v->bih->biBitCount/8);

    return 1;
}

static int control(struct vf_instance_s* vf, int request, void* data){

    return CONTROL_UNKNOWN;
}

static int query_format(struct vf_instance_s* vf, unsigned int fmt){
    if(fmt==IMGFMT_YUY2) return 3;
    if(fmt==IMGFMT_RGB24) return 1;
    return 0;
}

static void put_image(struct vf_instance_s* vf, mp_image_t *mpi){

    dv_encode_full_frame(vf->priv->enc, mpi->planes, 
	(mpi->flags&MP_IMGFLAG_YUV) ? e_dv_color_yuv : e_dv_color_rgb,
	mux_v->buffer);

    mencoder_write_chunk(mux_v, 480 * (vf->priv->enc->isPAL ? 300 : 250) , 0x10);
}

//===========================================================================//

static int vf_open(vf_instance_t *vf, char* args){
    vf->config=config;
    vf->control=control;
    vf->query_format=query_format;
    vf->put_image=put_image;
    vf->priv=malloc(sizeof(struct vf_priv_s));
    memset(vf->priv,0,sizeof(struct vf_priv_s));
    vf->priv->mux=args;
    
    vf->priv->enc=dv_encoder_new(1,1,1); // FIXME, parse some options!
    if(!vf->priv->enc) return 0;
    
    mux_v->bih=malloc(sizeof(BITMAPINFOHEADER));
    mux_v->bih->biSize=sizeof(BITMAPINFOHEADER);
    mux_v->bih->biWidth=0;
    mux_v->bih->biHeight=0;
    mux_v->bih->biCompression=mmioFOURCC('d','v','s','d');
    mux_v->bih->biPlanes=1;
    mux_v->bih->biBitCount=24;

    return 1;
}

vf_info_t ve_info_libdv = {
    "DV encoder using libdv",
    "libdv",
    "A'rpi",
    "for internal use by mencoder",
    vf_open
};

//===========================================================================//
#endif
