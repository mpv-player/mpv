#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../config.h"
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

struct vf_priv_s {
    aviwrite_stream_t* mux;
};
#define mux_v (vf->priv->mux)

static int config(struct vf_instance_s* vf,
        int width, int height, int d_width, int d_height,
	unsigned int flags, unsigned int outfmt){

    mux_v->bih->biWidth=width;
    mux_v->bih->biHeight=height;
    mux_v->bih->biSizeImage=mux_v->bih->biWidth*mux_v->bih->biHeight*(mux_v->bih->biBitCount/8);

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
    mux_v->buffer=mpi->planes[0];
    mencoder_write_chunk(mux_v, mpi->width*mpi->height*3, 0x10);
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
    
    mux_v->bih=malloc(sizeof(BITMAPINFOHEADER));
    mux_v->bih->biSize=sizeof(BITMAPINFOHEADER);
    mux_v->bih->biWidth=0;
    mux_v->bih->biHeight=0;
    mux_v->bih->biCompression=0;
    mux_v->bih->biPlanes=1;
    mux_v->bih->biBitCount=24;

    return 1;
}

vf_info_t ve_info_rawrgb = {
    "rawrgb encoder",
    "rawrgb",
    "A'rpi",
    "for internal use by mencoder",
    vf_open
};

//===========================================================================//
