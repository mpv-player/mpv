#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../config.h"
#include "../mp_msg.h"

#ifdef HAVE_DIVX4ENCORE

#include "codec-cfg.h"
#include "stream.h"
#include "demuxer.h"
#include "stheader.h"

#include "aviwrite.h"

#include "img_format.h"
#include "mp_image.h"
#include "vf.h"

//===========================================================================//

#include "divx4_vbr.h"

extern int pass;
extern char* passtmpfile;

#include <encore2.h>

ENC_PARAM divx4_param;
int divx4_crispness;

#include "cfgparser.h"

struct config divx4opts_conf[]={
	{"br", &divx4_param.bitrate, CONF_TYPE_INT, CONF_RANGE, 4, 24000000, NULL},
	{"rc_period", &divx4_param.rc_period, CONF_TYPE_INT, 0,0,0, NULL},
	{"rc_reaction_period", &divx4_param.rc_reaction_period, CONF_TYPE_INT, 0,0,0, NULL},
	{"rc_reaction_ratio", &divx4_param.rc_reaction_ratio, CONF_TYPE_INT, 0,0,0, NULL},
	{"min_quant", &divx4_param.min_quantizer, CONF_TYPE_INT, CONF_RANGE,0,32, NULL},
	{"max_quant", &divx4_param.max_quantizer, CONF_TYPE_INT, CONF_RANGE,0,32, NULL},
	{"key", &divx4_param.max_key_interval, CONF_TYPE_INT, CONF_MIN,0,0, NULL},
	{"deinterlace", &divx4_param.deinterlace, CONF_TYPE_FLAG, 0,0,1, NULL},
	{"q", &divx4_param.quality, CONF_TYPE_INT, CONF_RANGE, 1, 5, NULL},
	{"crispness", &divx4_crispness, CONF_TYPE_INT, CONF_RANGE,0,100, NULL},
	{"help", "TODO: divx4opts help!\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
	{NULL, NULL, 0, 0, 0, 0, NULL}
};

struct vf_priv_s {
    aviwrite_stream_t* mux;
    ENC_RESULT enc_result;
    ENC_FRAME enc_frame;
    void* enc_handle;
};

#define mux_v (vf->priv->mux)

static int config(struct vf_instance_s* vf,
        int width, int height, int d_width, int d_height,
	unsigned int flags, unsigned int outfmt){

    mux_v->bih->biWidth=width;
    mux_v->bih->biHeight=height;

    divx4_param.x_dim=width;
    divx4_param.y_dim=height;
    divx4_param.framerate=(float)mux_v->h.dwRate/mux_v->h.dwScale;
    mux_v->bih->biSizeImage=mux_v->bih->biWidth*mux_v->bih->biHeight*3;

    if(!divx4_param.bitrate) divx4_param.bitrate=800000;
    else if(divx4_param.bitrate<=16000) divx4_param.bitrate*=1000;
    if(!divx4_param.quality) divx4_param.quality=5; // the quality of compression ( 1 - fastest, 5 - best )

    // set some usefull defaults:
    if(!divx4_param.min_quantizer) divx4_param.min_quantizer=2;
    if(!divx4_param.max_quantizer) divx4_param.max_quantizer=31;
    if(!divx4_param.rc_period) divx4_param.rc_period=2000;
    if(!divx4_param.rc_reaction_period) divx4_param.rc_reaction_period=10;
    if(!divx4_param.rc_reaction_ratio) divx4_param.rc_reaction_ratio=20;

    divx4_param.handle=NULL;
    encore(NULL,ENC_OPT_INIT,&divx4_param,NULL);
    vf->priv->enc_handle=divx4_param.handle;
    switch(outfmt){
    case IMGFMT_YV12:	vf->priv->enc_frame.colorspace=ENC_CSP_YV12; break;
    case IMGFMT_IYUV:
    case IMGFMT_I420:	vf->priv->enc_frame.colorspace=ENC_CSP_I420; break;
    case IMGFMT_YUY2:	vf->priv->enc_frame.colorspace=ENC_CSP_YUY2; break;
    case IMGFMT_UYVY:	vf->priv->enc_frame.colorspace=ENC_CSP_UYVY; break;
    case IMGFMT_RGB24:
    case IMGFMT_BGR24:
    	vf->priv->enc_frame.colorspace=ENC_CSP_RGB24; break;
    default:
	mp_msg(MSGT_MENCODER,MSGL_ERR,"divx4: unsupported picture format (%s)!\n",
	    vo_format_name(outfmt));
	return 0;
    }

    switch(pass){
    case 1:
	if (VbrControl_init_2pass_vbr_analysis(passtmpfile, divx4_param.quality) == -1){
	    mp_msg(MSGT_MENCODER,MSGL_ERR,"2pass failed: filename=%s\n", passtmpfile);
	    pass=0;
	}
	break;
    case 2:
        if (VbrControl_init_2pass_vbr_encoding(passtmpfile,
					 divx4_param.bitrate,
					 divx4_param.framerate,
					 divx4_crispness,
					 divx4_param.quality) == -1){
	    mp_msg(MSGT_MENCODER,MSGL_ERR,"2pass failed: filename=%s\n", passtmpfile);
	    pass=0;
	}
	break;
    }

    return 1;
}

static int control(struct vf_instance_s* vf, int request, void* data){

    return CONTROL_UNKNOWN;
}

static int query_format(struct vf_instance_s* vf, unsigned int fmt){
    switch(fmt){
    case IMGFMT_YV12:
    case IMGFMT_IYUV:
    case IMGFMT_I420:
	return 3; // no conversion
    case IMGFMT_YUY2:
    case IMGFMT_UYVY:
	return 1; // conversion
    case IMGFMT_RGB24:
    case IMGFMT_BGR24:
	return 1 | VFCAP_FLIPPED; // conversion+flipped
    }
    return 0;
}

static void put_image(struct vf_instance_s* vf, mp_image_t *mpi){
    ENC_RESULT enc_result;
    vf->priv->enc_frame.image=mpi->planes[0];
    vf->priv->enc_frame.bitstream=mux_v->buffer;
    vf->priv->enc_frame.length=mux_v->buffer_size;
    vf->priv->enc_frame.mvs=NULL;
    if(pass==2){	// handle 2-pass:
    	vf->priv->enc_frame.quant = VbrControl_get_quant();
	vf->priv->enc_frame.intra = VbrControl_get_intra();
	encore(vf->priv->enc_handle,ENC_OPT_ENCODE_VBR,&vf->priv->enc_frame,&enc_result);
        VbrControl_update_2pass_vbr_encoding(enc_result.motion_bits,
					    enc_result.texture_bits,
					    enc_result.total_bits);
    } else {
	vf->priv->enc_frame.quant=0;
	vf->priv->enc_frame.intra=0;
	encore(vf->priv->enc_handle,ENC_OPT_ENCODE,&vf->priv->enc_frame,&enc_result);
	if(pass==1){
	  VbrControl_update_2pass_vbr_analysis(enc_result.is_key_frame, 
					       enc_result.motion_bits, 
					       enc_result.texture_bits, 
					       enc_result.total_bits, 
					       enc_result.quantizer);
	}
    }
    mencoder_write_chunk(mux_v,vf->priv->enc_frame.length,enc_result.is_key_frame?0x10:0);
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
    mux_v->bih->biPlanes=1;
    mux_v->bih->biBitCount=24;
    mux_v->bih->biCompression=mmioFOURCC('d','i','v','x');

    return 1;
}

vf_info_t ve_info_divx4 = {
    "divx4 encoder",
    "divx4",
    "A'rpi",
    "for internal use by mencoder",
    vf_open
};

//===========================================================================//
#endif
