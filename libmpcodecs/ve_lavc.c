#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../config.h"

#ifdef USE_LIBAVCODEC

#include "../mp_msg.h"
#include "../help_mp.h"

#include "codec-cfg.h"
#include "stream.h"
#include "demuxer.h"
#include "stheader.h"

#include "aviwrite.h"

#include "img_format.h"
#include "mp_image.h"
#include "vf.h"

#include "divx4_vbr.h"

extern int pass;
extern char* passtmpfile;

//===========================================================================//

#ifdef USE_LIBAVCODEC_SO
#include <libffmpeg/avcodec.h>
#else
#include "libavcodec/avcodec.h"
#endif

extern int avcodec_inited;

/* video options */
static char *lavc_param_vcodec = NULL;
static int lavc_param_vbitrate = -1;
static int lavc_param_vrate_tolerance = 1024*8;
static int lavc_param_vhq = 0; /* default is realtime encoding */
static int lavc_param_v4mv = 0;
static int lavc_param_vme = 4;
static int lavc_param_vqscale = 0;
static int lavc_param_vqmin = 3;
static int lavc_param_vqmax = 15;
static int lavc_param_vqdiff = 3;
static float lavc_param_vqcompress = 0.5;
static float lavc_param_vqblur = 0.5;
static int lavc_param_keyint = -1;

#include "cfgparser.h"

#ifdef USE_LIBAVCODEC
struct config lavcopts_conf[]={
	{"vcodec", &lavc_param_vcodec, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"vbitrate", &lavc_param_vbitrate, CONF_TYPE_INT, CONF_RANGE, 4, 24000000, NULL},
	{"vratetol", &lavc_param_vrate_tolerance, CONF_TYPE_INT, CONF_RANGE, 4, 24000000, NULL},
	{"vhq", &lavc_param_vhq, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"v4mv", &lavc_param_v4mv, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"vme", &lavc_param_vme, CONF_TYPE_INT, CONF_RANGE, 0, 5, NULL},
	{"vqscale", &lavc_param_vqscale, CONF_TYPE_INT, CONF_RANGE, 1, 31, NULL},
	{"vqmin", &lavc_param_vqmin, CONF_TYPE_INT, CONF_RANGE, 1, 31, NULL},
	{"vqmax", &lavc_param_vqmax, CONF_TYPE_INT, CONF_RANGE, 1, 31, NULL},
	{"vqdiff", &lavc_param_vqdiff, CONF_TYPE_INT, CONF_RANGE, 1, 31, NULL},
	{"vqcomp", &lavc_param_vqcompress, CONF_TYPE_FLOAT, CONF_RANGE, 0.0, 1.0, NULL},
	{"vqblur", &lavc_param_vqblur, CONF_TYPE_FLOAT, CONF_RANGE, 0.0, 1.0, NULL},
	{"keyint", &lavc_param_keyint, CONF_TYPE_INT, 0, 0, 0, NULL},
	{NULL, NULL, 0, 0, 0, 0, NULL}
};
#endif

struct vf_priv_s {
    aviwrite_stream_t* mux;
    AVCodecContext context;
    AVCodec *codec;
};

#define mux_v (vf->priv->mux)
#define lavc_venc_context (vf->priv->context)

static int config(struct vf_instance_s* vf,
        int width, int height, int d_width, int d_height,
	unsigned int flags, unsigned int outfmt){

    mux_v->bih->biWidth=width;
    mux_v->bih->biHeight=height;
    mux_v->bih->biSizeImage=mux_v->bih->biWidth*mux_v->bih->biHeight*(mux_v->bih->biBitCount/8);

    memset(&lavc_venc_context, 0, sizeof(lavc_venc_context));

    printf("videocodec: libavcodec (%dx%d fourcc=%x [%.4s])\n",
	mux_v->bih->biWidth, mux_v->bih->biHeight, mux_v->bih->biCompression,
	    (char *)&mux_v->bih->biCompression);

    lavc_venc_context.width = width;
    lavc_venc_context.height = height;
    if (lavc_param_vbitrate >= 0) /* != -1 */
	lavc_venc_context.bit_rate = lavc_param_vbitrate*1000;
    else
	lavc_venc_context.bit_rate = 800000; /* default */
    lavc_venc_context.bit_rate_tolerance= lavc_param_vrate_tolerance*1000;
    lavc_venc_context.frame_rate = (float)mux_v->h.dwRate/mux_v->h.dwScale * FRAME_RATE_BASE;
    lavc_venc_context.qmin= lavc_param_vqmin;
    lavc_venc_context.qmax= lavc_param_vqmax;
    lavc_venc_context.max_qdiff= lavc_param_vqdiff;
    lavc_venc_context.qcompress= lavc_param_vqcompress;
    lavc_venc_context.qblur= lavc_param_vqblur;
    /* keyframe interval */
    if (lavc_param_keyint >= 0) /* != -1 */
	lavc_venc_context.gop_size = lavc_param_keyint;
    else
	lavc_venc_context.gop_size = 250; /* default */

    if (lavc_param_vhq)
    {
	printf("High quality encoding selected (non real time)!\n");
	lavc_venc_context.flags = CODEC_FLAG_HQ;
    }
    else
	lavc_venc_context.flags = 0;

    lavc_venc_context.flags|= lavc_param_v4mv ? CODEC_FLAG_4MV : 0;

#ifdef ME_ZERO
    // workaround Juanjo's stupid incompatible change:
    motion_estimation_method = lavc_param_vme;
#else
    lavc_venc_context.me_method = ME_ZERO+lavc_param_vme;
#endif

    /* fixed qscale :p */
    if (lavc_param_vqscale)
    {
	printf("Using constant qscale = %d (VBR)\n", lavc_param_vqscale);
	lavc_venc_context.flags |= CODEC_FLAG_QSCALE;
	lavc_venc_context.quality = lavc_param_vqscale;
    }

    switch(pass){
    case 1:
	if (VbrControl_init_2pass_vbr_analysis(passtmpfile, 5) == -1){
	    mp_msg(MSGT_MENCODER,MSGL_ERR,"2pass failed: filename=%s\n", passtmpfile);
	    pass=0;
	}
	break;
    case 2:
        if (VbrControl_init_2pass_vbr_encoding(passtmpfile,
		    lavc_venc_context.bit_rate,
		    (float)mux_v->h.dwRate/mux_v->h.dwScale,
		    100, /* crispness */
		    5) == -1){
	    mp_msg(MSGT_MENCODER,MSGL_ERR,"2pass failed: filename=%s\n", passtmpfile);
	    pass=0;
	} else
	    lavc_venc_context.flags|=CODEC_FLAG_QSCALE|CODEC_FLAG_TYPE; // VBR
	break;
    }

    if (avcodec_open(&lavc_venc_context, vf->priv->codec) != 0) {
	mp_msg(MSGT_MENCODER,MSGL_ERR,MSGTR_CantOpenCodec);
	return 0;
    }

    if (lavc_venc_context.codec->encode == NULL) {
	mp_msg(MSGT_MENCODER,MSGL_ERR,"avcodec init failed (ctx->codec->encode == NULL)!\n");
	return 0;
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
	return VFCAP_CSP_SUPPORTED | VFCAP_ACCEPT_STRIDE;
    }
    return 0;
}

static void put_image(struct vf_instance_s* vf, mp_image_t *mpi){
    int out_size;
    AVPicture lavc_venc_picture;

    lavc_venc_picture.data[0]=mpi->planes[0];
    lavc_venc_picture.data[1]=mpi->planes[1];
    lavc_venc_picture.data[2]=mpi->planes[2];
    lavc_venc_picture.linesize[0]=mpi->stride[0];
    lavc_venc_picture.linesize[1]=mpi->stride[1];
    lavc_venc_picture.linesize[2]=mpi->stride[2];

    if(pass==2){ // handle 2-pass:
	lavc_venc_context.flags|=CODEC_FLAG_QSCALE; // enable VBR
	lavc_venc_context.quality=VbrControl_get_quant();
	lavc_venc_context.key_frame=VbrControl_get_intra();
	lavc_venc_context.gop_size=0x3fffffff;
	out_size = avcodec_encode_video(&lavc_venc_context, mux_v->buffer, mux_v->buffer_size,
	    &lavc_venc_picture);
	VbrControl_update_2pass_vbr_encoding(lavc_venc_context.mv_bits,
	      lavc_venc_context.i_tex_bits+lavc_venc_context.p_tex_bits,
	      8*out_size);
    } else {
	out_size = avcodec_encode_video(&lavc_venc_context, mux_v->buffer, mux_v->buffer_size,
	    &lavc_venc_picture);

	if(pass==1){
	  VbrControl_update_2pass_vbr_analysis(lavc_venc_context.key_frame,
	      lavc_venc_context.mv_bits,
	      lavc_venc_context.i_tex_bits+lavc_venc_context.p_tex_bits,
	      8*out_size, lavc_venc_context.quality);
	}
	
    }

    mencoder_write_chunk(mux_v,out_size,lavc_venc_context.key_frame?0x10:0);
}

static void uninit(struct vf_instance_s* vf){
    avcodec_close(&lavc_venc_context);
}

//===========================================================================//

static int vf_open(vf_instance_t *vf, char* args){
    vf->uninit=uninit;
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
    if (!lavc_param_vcodec)
    {
	printf("No libavcodec codec specified! It's requested!\n");
	return 0;
    }

    if (!strcasecmp(lavc_param_vcodec, "mpeg1") || !strcasecmp(lavc_param_vcodec, "mpeg1video"))
	mux_v->bih->biCompression = mmioFOURCC('m', 'p', 'g', '1');
    else if (!strcasecmp(lavc_param_vcodec, "h263") || !strcasecmp(lavc_param_vcodec, "h263p"))
	mux_v->bih->biCompression = mmioFOURCC('h', '2', '6', '3');
    else if (!strcasecmp(lavc_param_vcodec, "rv10"))
	mux_v->bih->biCompression = mmioFOURCC('R', 'V', '1', '0');
    else if (!strcasecmp(lavc_param_vcodec, "mjpeg"))
	mux_v->bih->biCompression = mmioFOURCC('M', 'J', 'P', 'G');
    else if (!strcasecmp(lavc_param_vcodec, "mpeg4"))
	mux_v->bih->biCompression = mmioFOURCC('D', 'I', 'V', 'X');
    else if (!strcasecmp(lavc_param_vcodec, "msmpeg4"))
	mux_v->bih->biCompression = mmioFOURCC('d', 'i', 'v', '3');
    else
	mux_v->bih->biCompression = mmioFOURCC(lavc_param_vcodec[0],
		lavc_param_vcodec[1], lavc_param_vcodec[2], lavc_param_vcodec[3]); /* FIXME!!! */

    if (!avcodec_inited){
	avcodec_init();
	avcodec_register_all();
	avcodec_inited=1;
    }

    vf->priv->codec = (AVCodec *)avcodec_find_encoder_by_name(lavc_param_vcodec);
    if (!vf->priv->codec) {
	mp_msg(MSGT_MENCODER,MSGL_ERR,MSGTR_MissingLAVCcodec, lavc_param_vcodec);
	return 0;
    }

    return 1;
}

vf_info_t ve_info_lavc = {
    "libavcodec encoder",
    "lavc",
    "A'rpi",
    "for internal use by mencoder",
    vf_open
};

//===========================================================================//
#endif
