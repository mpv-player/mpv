#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../config.h"
#ifdef USE_WIN32DLL

#include "../mp_msg.h"

#include "codec-cfg.h"
#include "stream.h"
#include "demuxer.h"
#include "stheader.h"

#include "aviwrite.h"

#include "img_format.h"
#include "mp_image.h"
#include "vf.h"

//===========================================================================//

#include "dll_init.h"

struct vf_priv_s {
    aviwrite_stream_t* mux;
    BITMAPINFOHEADER* bih;
};

#define mux_v (vf->priv->mux)
#define vfw_bih (vf->priv->bih)

static int config(struct vf_instance_s* vf,
        int width, int height, int d_width, int d_height,
	unsigned int flags, unsigned int outfmt){

    vfw_bih->biWidth=width;
    vfw_bih->biHeight=height;
    vfw_bih->biSizeImage=width*height*((vfw_bih->biBitCount+7)/8);

    if(!vfw_start_encoder(vfw_bih, mux_v->bih)) return 0;

//    mux_v->bih->biWidth=width;
//    mux_v->bih->biHeight=height;
//    mux_v->bih->biSizeImage=width*height*((mux_v->bih->biBitCount+7)/8);

    return 1;
}

static int control(struct vf_instance_s* vf, int request, void* data){

    return CONTROL_UNKNOWN;
}

static int query_format(struct vf_instance_s* vf, unsigned int fmt){
    if(fmt==IMGFMT_BGR24) return 3 | VFCAP_FLIPPED;
    return 0;
}

static void put_image(struct vf_instance_s* vf, mp_image_t *mpi){
    long flags=0;
    int ret;
//    flip_upside_down(vo_image_ptr,vo_image_ptr,3*vo_w,vo_h); // dirty hack
    ret=vfw_encode_frame(mux_v->bih, mux_v->buffer, vfw_bih, mpi->planes[0], &flags, 10000);
    mencoder_write_chunk(mux_v,mux_v->bih->biSizeImage,flags);
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

    vfw_bih=malloc(sizeof(BITMAPINFOHEADER));
    vfw_bih->biSize=sizeof(BITMAPINFOHEADER);
    vfw_bih->biWidth=0; // FIXME ?
    vfw_bih->biHeight=0;
    vfw_bih->biPlanes=1;
    vfw_bih->biBitCount=24;
    vfw_bih->biCompression=0;
//    vfw_bih->biSizeImage=vo_w*vo_h*((vfw_bih->biBitCount+7)/8);
//    mux_v->bih=vfw_open_encoder("divxc32.dll",vfw_bih,mmioFOURCC('D', 'I', 'V', '3'));
    mux_v->bih=vfw_open_encoder("AvidAVICodec.dll",vfw_bih, 0);
    if(!mux_v->bih) return 0;

    return 1;
}

vf_info_t ve_info_vfw = {
    "Win32/VfW encoders",
    "vfw",
    "A'rpi",
    "for internal use by mencoder",
    vf_open
};

//===========================================================================//
#endif
