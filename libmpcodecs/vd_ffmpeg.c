#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"

#ifdef USE_LIBAVCODEC

#include "bswap.h"

#include "vd_internal.h"

static vd_info_t info = {
	"FFmpeg's libavcodec codec family",
	"ffmpeg",
	VFM_FFMPEG,
	"A'rpi",
	"http://ffmpeg.sf.net",
	"native codecs"
};

LIBVD_EXTERN(ffmpeg)

#include "../postproc/rgb2rgb.h"


#ifdef USE_LIBAVCODEC_SO
#include <libffmpeg/avcodec.h>
#else
#include "libavcodec/avcodec.h"
#endif

int avcodec_inited=0;

#ifdef FF_POSTPROCESS
int quant_store[MBR+1][MBC+1];
#endif

typedef struct {
    AVCodecContext *avctx;
    int last_aspect;
    int do_slices;
    int vo_inited;
    int convert;
    int yuy2_support;
} vd_ffmpeg_ctx;

//#ifdef FF_POSTPROCESS
//unsigned int lavc_pp=0;
//#endif

#include "cfgparser.h"

static int lavc_param_workaround_bugs=0;
static int lavc_param_error_resilience=0;
static int lavc_param_gray=0;

struct config lavc_decode_opts_conf[]={
#if LIBAVCODEC_BUILD >= 4611
	{"bug", &lavc_param_workaround_bugs, CONF_TYPE_INT, CONF_RANGE, 0, 99, NULL},
	{"ver", &lavc_param_error_resilience, CONF_TYPE_INT, CONF_RANGE, -1, 99, NULL},
#endif
#if LIBAVCODEC_BUILD >= 4614
	{"gray", &lavc_param_gray, CONF_TYPE_FLAG, 0, 0, CODEC_FLAG_PART, NULL},
#endif
	{NULL, NULL, 0, 0, 0, 0, NULL}
};

// to set/get/query special features/parameters
static int control(sh_video_t *sh,int cmd,void* arg,...){
    vd_ffmpeg_ctx *ctx = sh->context;
    switch(cmd){
    case VDCTRL_QUERY_FORMAT:
	if( (*((int*)arg)) == IMGFMT_YV12 ) return CONTROL_TRUE;
	if( (*((int*)arg)) == IMGFMT_IYUV ) return CONTROL_TRUE;
	if( (*((int*)arg)) == IMGFMT_I420 ) return CONTROL_TRUE;
	if( (*((int*)arg)) == IMGFMT_YUY2 && ctx->yuy2_support ) return CONTROL_TRUE;
	return CONTROL_FALSE;
    }
    return CONTROL_UNKNOWN;
}

// init driver
static int init(sh_video_t *sh){
    AVCodecContext *avctx;
    vd_ffmpeg_ctx *ctx;
    AVCodec *lavc_codec;

    if(!avcodec_inited){
      avcodec_init();
      avcodec_register_all();
      avcodec_inited=1;
    }

    ctx = sh->context = malloc(sizeof(vd_ffmpeg_ctx));
    if (!ctx)
	return(0);
    memset(ctx, 0, sizeof(vd_ffmpeg_ctx));
    
    lavc_codec = (AVCodec *)avcodec_find_decoder_by_name(sh->codec->dll);
    if(!lavc_codec){
	mp_msg(MSGT_DECVIDEO,MSGL_ERR,MSGTR_MissingLAVCcodec,sh->codec->dll);
	return 0;
    }

    if(vd_use_slices && lavc_codec->capabilities&CODEC_CAP_DRAW_HORIZ_BAND)
	ctx->do_slices=1;
    
    ctx->avctx = malloc(sizeof(AVCodecContext));
    memset(ctx->avctx, 0, sizeof(AVCodecContext));
    avctx = ctx->avctx;
    
    avctx->width = sh->disp_w;
    avctx->height= sh->disp_h;
#if LIBAVCODEC_BUILD >= 4611
    avctx->workaround_bugs= lavc_param_workaround_bugs;
    avctx->error_resilience= lavc_param_error_resilience;
#endif
#if LIBAVCODEC_BUILD >= 4614
    if(lavc_param_gray) avctx->flags|= CODEC_FLAG_GRAY;
#endif
    
    mp_dbg(MSGT_DECVIDEO,MSGL_DBG2,"libavcodec.size: %d x %d\n",avctx->width,avctx->height);
    if (sh->format == mmioFOURCC('R', 'V', '1', '3'))
	avctx->sub_id = 3;
#if LIBAVCODEC_BUILD >= 4605
    /* AVRn stores huffman table in AVI header */
    /* Pegasus MJPEG stores it also in AVI header, but it uses the common
       MJPG fourcc :( */
    if (sh->bih && (sh->bih->biSize != sizeof(BITMAPINFOHEADER)) &&
	(sh->format == mmioFOURCC('A','V','R','n') ||
	sh->format == mmioFOURCC('M','J','P','G')))
    {
	avctx->flags |= CODEC_FLAG_EXTERN_HUFF;
	avctx->extradata_size = sh->bih->biSize-sizeof(BITMAPINFOHEADER);
	avctx->extradata = malloc(avctx->extradata_size);
	memcpy(avctx->extradata, sh->bih+sizeof(BITMAPINFOHEADER),
	    avctx->extradata_size);

#if 0
	{
	    int x;
	    uint8_t *p = avctx->extradata;
	    
	    for (x=0; x<avctx->extradata_size; x++)
		printf("[%x] ", p[x]);
	    printf("\n");
	}
#endif
    }
#endif
    /* open it */
    if (avcodec_open(avctx, lavc_codec) < 0) {
        mp_msg(MSGT_DECVIDEO,MSGL_ERR, MSGTR_CantOpenCodec);
        return 0;
    }
    mp_msg(MSGT_DECVIDEO,MSGL_V,"INFO: libavcodec init OK!\n");
    ctx->last_aspect=-3;
    return 1; //mpcodecs_config_vo(sh,sh->disp_w,sh->disp_h,IMGFMT_YV12);
}

// uninit driver
static void uninit(sh_video_t *sh){
    vd_ffmpeg_ctx *ctx = sh->context;
    AVCodecContext *avctx = ctx->avctx;

    if (avcodec_close(avctx) < 0)
    	    mp_msg(MSGT_DECVIDEO,MSGL_ERR, MSGTR_CantCloseCodec);

#if LIBAVCODEC_BUILD >= 4605
    if (avctx->extradata_size)
	free(avctx->extradata);
#endif

    if (avctx)
	free(avctx);
    if (ctx)
	free(ctx);
}

#include "libvo/video_out.h"	// FIXME!!!

static void draw_slice(struct AVCodecContext *s,
                	UINT8 **src, int linesize,
                	int y, int width, int height){
    vo_functions_t * output = s->opaque;
    int stride[3];

    stride[0]=linesize;
    stride[1]=stride[2]=stride[0]/2;

    output->draw_slice (src, stride, width, height, 0, y);
    
}

// decode a frame
static mp_image_t* decode(sh_video_t *sh,void* data,int len,int flags){
    int got_picture=0;
    int ret;
    AVPicture lavc_picture;
    vd_ffmpeg_ctx *ctx = sh->context;
    AVCodecContext *avctx = ctx->avctx;
    mp_image_t* mpi=NULL;

    if(len<=0) return NULL; // skipped frame
    
    avctx->draw_horiz_band=NULL;
    if(ctx->vo_inited && !ctx->convert && !(flags&3)){
	mpi=mpcodecs_get_image(sh, MP_IMGTYPE_EXPORT, MP_IMGFLAG_PRESERVE |
	    (ctx->do_slices?MP_IMGFLAG_DRAW_CALLBACK:0),
	    sh->disp_w, sh->disp_h);
	if(mpi && mpi->flags&MP_IMGFLAG_DRAW_CALLBACK){
	    // vd core likes slices!
	    avctx->draw_horiz_band=draw_slice;
	    avctx->opaque=sh->video_out;
	}
    }

#if LIBAVCODEC_BUILD > 4603
    avctx->hurry_up=(flags&3)?((flags&2)?2:1):0;
#endif

    ret = avcodec_decode_video(avctx, &lavc_picture,
	     &got_picture, data, len);
    if(ret<0) mp_msg(MSGT_DECVIDEO,MSGL_WARN, "Error while decoding frame!\n");
    if(!got_picture) return NULL;	// skipped image
                                    
    if (avctx->aspect_ratio_info != ctx->last_aspect ||
	avctx->width != sh->disp_w ||
	avctx->height != sh->disp_h ||
	!ctx->vo_inited)
    {
	ctx->last_aspect = avctx->aspect_ratio_info;
	switch(avctx->aspect_ratio_info)
	{
	    case FF_ASPECT_4_3_625:
	    case FF_ASPECT_4_3_525:
		sh->aspect = 4.0/3.0;
		break;
	    case FF_ASPECT_16_9_625:
	    case FF_ASPECT_16_9_525:
		sh->aspect = 16.0/9.0;
		break;
	    case FF_ASPECT_SQUARE:
		sh->aspect = 0.0;
		break;
	}
	sh->disp_w = avctx->width;
	sh->disp_h = avctx->height;
	ctx->vo_inited=1;
	ctx->yuy2_support=(avctx->pix_fmt==PIX_FMT_YUV422P);
    	if (!mpcodecs_config_vo(sh,sh->disp_w,sh->disp_h,
	    ctx->yuy2_support ? IMGFMT_YUY2 : IMGFMT_YV12))
    		return NULL;
	ctx->convert=(sh->codec->outfmt[sh->outfmtidx]==IMGFMT_YUY2);
    }
    
    if(!mpi && ctx->convert){
	// do yuv422p -> yuy2 conversion:
        mpi=mpcodecs_get_image(sh, MP_IMGTYPE_TEMP, MP_IMGFLAG_ACCEPT_STRIDE,
	    avctx->width, avctx->height);
	if(!mpi) return NULL;
	yuv422ptoyuy2(lavc_picture.data[0],lavc_picture.data[1],lavc_picture.data[2],
	    mpi->planes[0],avctx->width,avctx->height,
	    lavc_picture.linesize[0],lavc_picture.linesize[1],mpi->stride[0]);
	return mpi;
    }
    
    if(!mpi)
    mpi=mpcodecs_get_image(sh, MP_IMGTYPE_EXPORT, MP_IMGFLAG_PRESERVE,
	avctx->width, avctx->height);
    if(!mpi){	// temporary!
	printf("couldn't allocate image for codec\n");
	return NULL;
    }
    
    mpi->planes[0]=lavc_picture.data[0];
    mpi->planes[1]=lavc_picture.data[1];
    mpi->planes[2]=lavc_picture.data[2];
    mpi->stride[0]=lavc_picture.linesize[0];
    mpi->stride[1]=lavc_picture.linesize[1];
    mpi->stride[2]=lavc_picture.linesize[2];

    if(avctx->pix_fmt==PIX_FMT_YUV422P){
	// we have 422p but user wants yv12 (420p)
	mpi->stride[1]*=2;
	mpi->stride[2]*=2;
    }
    
#ifdef FF_POSTPROCESS
    mpi->qscale=&quant_store[0][0];
    mpi->qstride=MBC+1;
#endif
    
    return mpi;
}

#endif

