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

#ifdef USE_LIBAVCODEC_SO
#include <libffmpeg/avcodec.h>
#else
#include "libavcodec/avcodec.h"
#endif

int avcodec_inited=0;

typedef struct {
    AVCodec *lavc_codec;
    AVCodecContext *avctx;
    int last_aspect;    
} vd_ffmpeg_ctx;

//#ifdef FF_POSTPROCESS
//unsigned int lavc_pp=0;
//#endif

// to set/get/query special features/parameters
static int control(sh_video_t *sh,int cmd,void* arg,...){
    return CONTROL_UNKNOWN;
}

// init driver
static int init(sh_video_t *sh){
    AVCodecContext *avctx;
    vd_ffmpeg_ctx *ctx;

    if(!avcodec_inited){
      avcodec_init();
      avcodec_register_all();
      avcodec_inited=1;
    }

    ctx = sh->context = malloc(sizeof(vd_ffmpeg_ctx));
    if (!ctx)
	return(0);
    memset(ctx, 0, sizeof(vd_ffmpeg_ctx));
    
    ctx->lavc_codec = (AVCodec *)avcodec_find_decoder_by_name(sh->codec->dll);
    if(!ctx->lavc_codec){
	mp_msg(MSGT_DECVIDEO,MSGL_ERR,MSGTR_MissingLAVCcodec,sh->codec->dll);
	return 0;
    }
    
    ctx->avctx = malloc(sizeof(AVCodecContext));
    memset(ctx->avctx, 0, sizeof(AVCodecContext));
    avctx = ctx->avctx;
    
    avctx->width = sh->disp_w;
    avctx->height= sh->disp_h;
    mp_dbg(MSGT_DECVIDEO,MSGL_DBG2,"libavcodec.size: %d x %d\n",avctx->width,avctx->height);
    if (sh->format == mmioFOURCC('R', 'V', '1', '3'))
	avctx->sub_id = 3;
    /* open it */
    if (avcodec_open(avctx, ctx->lavc_codec) < 0) {
        mp_msg(MSGT_DECVIDEO,MSGL_ERR, MSGTR_CantOpenCodec);
        return 0;
    }
    mp_msg(MSGT_DECVIDEO,MSGL_V,"INFO: libavcodec init OK!\n");
    return mpcodecs_config_vo(sh,sh->disp_w,sh->disp_h,IMGFMT_YV12);
}

// uninit driver
static void uninit(sh_video_t *sh){
    vd_ffmpeg_ctx *ctx = sh->context;
    AVCodecContext *avctx = ctx->avctx;

    if (avcodec_close(avctx) < 0)
    	    mp_msg(MSGT_DECVIDEO,MSGL_ERR, MSGTR_CantCloseCodec);
    if (avctx)
	free(avctx);
    if (ctx)
	free(ctx);
}

//mp_image_t* mpcodecs_get_image(sh_video_t *sh, int mp_imgtype, int mp_imgflag, int w, int h);

// decode a frame
static mp_image_t* decode(sh_video_t *sh,void* data,int len,int flags){
    int got_picture=0;
    int ret;
    AVPicture lavc_picture;
    vd_ffmpeg_ctx *ctx = sh->context;
    AVCodecContext *avctx = ctx->avctx;
    mp_image_t* mpi;

    if(len<=0) return NULL; // skipped frame
    
    ret = avcodec_decode_video(avctx, &lavc_picture,
	     &got_picture, data, len);
    
    if(ret<0) mp_msg(MSGT_DECVIDEO,MSGL_WARN, "Error while decoding frame!\n");
    if(!got_picture) return NULL;	// skipped image

    if (avctx->aspect_ratio_info != ctx->last_aspect)
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
	    default:
		sh->aspect = 0.0;
		break;
	}
        if (mpcodecs_config_vo(sh,sh->disp_w,sh->disp_h,IMGFMT_YV12))
    	    return NULL;
    }
    
    if ((avctx->width != sh->disp_w) ||
	(avctx->height != sh->disp_h))
    {
	/* and what about the sh->bih (BitmapInfoHeader) width/height values? */
	sh->disp_w = avctx->width;
	sh->disp_h = avctx->height;
	if (mpcodecs_config_vo(sh,sh->disp_w,sh->disp_h,IMGFMT_YV12))
	    return NULL;
    }
    
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
	mpi->stride[1]*=2;
	mpi->stride[2]*=2;
    }
    
    return mpi;
}

#endif

