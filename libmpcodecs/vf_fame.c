#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "../config.h"
#include "../mp_msg.h"

//#ifdef USE_LIBFAME

// 100=best >=80 very good >=50 fast
#define QUALITY 90

#include "img_format.h"
#include "mp_image.h"
#include "vf.h"

//#include "../libvo/fastmemcpy.h"
#include "../libfame/fame.h"

struct vf_priv_s {
    unsigned char* outbuf;
    int outbuf_size;
    fame_parameters_t params;
    fame_context_t *ctx;
    vo_mpegpes_t pes;
};

//===========================================================================//

static int config(struct vf_instance_s* vf,
        int width, int height, int d_width, int d_height,
	unsigned int flags, unsigned int outfmt){
    if(vf_next_query_format(vf,IMGFMT_MPEGPES)<=0) return 0;

    vf->priv->params.width=width;
    vf->priv->params.height=height;

    vf->priv->outbuf_size=10000+width*height;  // must be enough!
    if(vf->priv->outbuf) free(vf->priv->outbuf);
    vf->priv->outbuf = malloc(vf->priv->outbuf_size);

    fame_init(vf->priv->ctx,&vf->priv->params,vf->priv->outbuf,vf->priv->outbuf_size);

    return vf_next_config(vf,width,height,d_width,d_height,flags,IMGFMT_MPEGPES);
}

static void put_image(struct vf_instance_s* vf, mp_image_t *mpi){
    fame_yuv_t yuv;
    mp_image_t *dmpi;
    int out_size;

    yuv.w=mpi->width;
    yuv.h=mpi->height;
    yuv.y=mpi->planes[0];
    yuv.u=mpi->planes[1];
    yuv.v=mpi->planes[2];

    out_size = fame_encode_frame(vf->priv->ctx, &yuv, NULL);
    
    if(out_size<=0) return;

    dmpi=vf_get_image(vf->next,IMGFMT_MPEGPES,
	MP_IMGTYPE_EXPORT, 0,
	mpi->w, mpi->h);
    
    vf->priv->pes.data=vf->priv->outbuf;
    vf->priv->pes.size=out_size;
    vf->priv->pes.id=0x1E0;
    vf->priv->pes.timestamp=-1; // dunno
    
    dmpi->planes[0]=&vf->priv->pes;
    
    vf_next_put_image(vf,dmpi);
}

//===========================================================================//

static int query_format(struct vf_instance_s* vf, unsigned int fmt){
    switch(fmt){
    case IMGFMT_YV12:
    case IMGFMT_I420:
    case IMGFMT_IYUV:
	return (vf_next_query_format(vf,IMGFMT_MPEGPES) & (~(VFCAP_CSP_SUPPORTED_BY_HW|VFCAP_ACCEPT_STRIDE)));
    }
    return 0;
}

static int open(vf_instance_t *vf, char* args){
    vf->config=config;
    vf->put_image=put_image;
    vf->query_format=query_format;
    vf->priv=malloc(sizeof(struct vf_priv_s));
    vf->default_caps=0;
    memset(vf->priv,0,sizeof(struct vf_priv_s));

    vf->priv->ctx=fame_open();
    if(!vf->priv->ctx){
	printf("FATAL: cannot open libFAME!\n");
	return 0;
    }

    // TODO: parse args ->
    vf->priv->params.coding="I";
    vf->priv->params.quality=QUALITY;
    vf->priv->params.bitrate=0;
    vf->priv->params.slices_per_frame=1;
    vf->priv->params.frames_per_sequence=25; //0xffffffff;
    vf->priv->params.frame_rate_num=25;
    vf->priv->params.frame_rate_den=1;
    vf->priv->params.shape_quality=100;
    vf->priv->params.search_range=8; // for "IPPP" only
    vf->priv->params.verbose=0;
    vf->priv->params.profile=NULL; // TODO

    return 1;
}

vf_info_t vf_info_fame = {
    "realtime mpeg1 encoding with libFAME",
    "fame",
    "A'rpi",
    "",
    open
};

//===========================================================================//
//#endif
