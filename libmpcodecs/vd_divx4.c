#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"

#ifdef USE_DIVX
#ifdef NEW_DECORE

#include "vd_internal.h"

static vd_info_t info = {
#ifdef DECORE_DIVX5
	"DivX5Linux lib",
#else
#ifdef DECORE_XVID
	"XviD lib (divx4 compat.)",
#else
	"DivX4Linux lib",
#endif
#endif
	"divx4",
	"A'rpi",
#ifdef DECORE_XVID
	"http://www.xvid.com",
#else
	"http://www.divx.com",
#endif
	"native binary codec"
};

LIBVD_EXTERN(divx4)

#ifdef DECORE_XVID
#include <divx4.h>
#else
#include <decore.h>
#endif

#define USE_DIVX_BUILTIN_PP

#ifndef DECORE_VERSION
#define DECORE_VERSION 0
#endif

#if DECORE_VERSION >= 20021112
static void* dec_handle = NULL;
#endif

// to set/get/query special features/parameters
static int control(sh_video_t *sh,int cmd,void* arg,...){
    switch(cmd){
#ifdef USE_DIVX_BUILTIN_PP
    case VDCTRL_QUERY_MAX_PP_LEVEL:
#if DECORE_VERSION >= 20021112
        return 6; // divx4linux >= 5.0.5 -> 0..60
#else
        return 10; // divx4linux < 5.0.5 -> 0..100
#endif 
    case VDCTRL_SET_PP_LEVEL: {
        int quality=*((int*)arg);
#if DECORE_VERSION >= 20021112
        int32_t iInstruction, iPostproc;
        if(quality<0 || quality>6) quality=6;
        iInstruction = DEC_ADJ_POSTPROCESSING | DEC_ADJ_SET;
        iPostproc = quality*10;
        decore(dec_handle, DEC_OPT_ADJUST, &iInstruction, &iPostproc);
#else
	DEC_SET dec_set;
	if(quality<0 || quality>10) quality=10;
	dec_set.postproc_level=quality*10;
	decore(0x123,DEC_OPT_SETPP,&dec_set,NULL);
#endif
	return CONTROL_OK;
    }
#endif
#if DECORE_VERSION >= 20011010
    case VDCTRL_SET_EQUALIZER: {
	va_list ap;
	int value;
        int option;
	va_start(ap, arg);
	value=va_arg(ap, int);
	va_end(ap);

        if(!strcasecmp(arg,"Brightness"))
#if DECORE_VERSION >= 20021112
            option=DEC_ADJ_BRIGHTNESS | DEC_ADJ_SET;
#else
            option=DEC_GAMMA_BRIGHTNESS;
#endif
        else if(!strcasecmp(arg, "Contrast"))
#if DECORE_VERSION >= 20021112
            option=DEC_ADJ_CONTRAST | DEC_ADJ_SET;
#else
            option=DEC_GAMMA_CONTRAST;
#endif
        else if(!strcasecmp(arg,"Saturation"))
#if DECORE_VERSION >= 20021112
            option=DEC_ADJ_SATURATION | DEC_ADJ_SET;
#else
            option=DEC_GAMMA_SATURATION;
#endif
        else return CONTROL_FALSE;
	
        value = (value * 128) / 100;
#if DECORE_VERSION >= 20021112
        decore(dec_handle, DEC_OPT_ADJUST, &option, &value);
#else
        decore(0x123, DEC_OPT_GAMMA, (void *)option, (void *) value);
#endif
	return CONTROL_OK;
    }
#endif
    
    }

    return CONTROL_UNKNOWN;
}

// init driver
static int init(sh_video_t *sh){
#if DECORE_VERSION >= 20021112
    DEC_INIT dec_init;
    int iSize=sizeof(DivXBitmapInfoHeader);
    DivXBitmapInfoHeader* pbi=malloc(iSize);
    int32_t iInstruction;

    if(!mpcodecs_config_vo(sh,sh->disp_w,sh->disp_h,IMGFMT_YUY2)) return 0;

    memset(&dec_init, 0, sizeof(dec_init));
    memset(pbi, 0, iSize);

    switch(sh->format) {
      case mmioFOURCC('D','I','V','3'):
        dec_init.codec_version = 311;
        break;
      case mmioFOURCC('D','I','V','X'):
        dec_init.codec_version = 412;
        break;
      case mmioFOURCC('D','X','5','0'):
      default: // Fallback to DivX 5 behaviour
        dec_init.codec_version = 500;
    }

    // no smoothing of the CPU load
    dec_init.smooth_playback = 0;

    pbi->biSize=iSize;

    switch(sh->codec->outfmt[sh->outfmtidx]){
        case IMGFMT_YV12: {
            pbi->biCompression=mmioFOURCC('Y','V','1','2');
            break;
        }
        case IMGFMT_YUY2: {
            pbi->biCompression=mmioFOURCC('Y','U','Y','2');
            break;
        }
        case IMGFMT_UYVY: {
            pbi->biCompression=mmioFOURCC('U','Y','V','Y');
            break;
        }
        case IMGFMT_I420: {
            pbi->biCompression=mmioFOURCC('I','4','2','0');
            break;
        }
        case IMGFMT_BGR15: {
            pbi->biCompression=0;
            pbi->biBitCount=16;
            break;
        }
        case IMGFMT_BGR16: {
            pbi->biCompression=3;
            pbi->biBitCount=16;
            break;
        }
        case IMGFMT_BGR24: {
            pbi->biCompression=0;
            pbi->biBitCount=24;
            break;
        }
        case IMGFMT_BGR32: {
            pbi->biCompression=0;
            pbi->biBitCount=32;
            break;
        }
        default:
          mp_msg(MSGT_DECVIDEO,MSGL_ERR,"Unsupported out_fmt: 0x%X\n",sh->codec->outfmt[sh->outfmtidx]);
          return 0;
    }

    pbi->biWidth = sh->disp_w;
    pbi->biHeight = sh->disp_h;

    decore(&dec_handle, DEC_OPT_INIT, &dec_init, NULL);
    decore(dec_handle, DEC_OPT_SETOUT, pbi, NULL);

#ifdef USE_DIVX_BUILTIN_PP
    iInstruction = DEC_ADJ_POSTPROCESSING | DEC_ADJ_SET;
    decore(dec_handle, DEC_OPT_ADJUST, &iInstruction, &divx_quality);
#endif

    free(pbi);
#else // DECORE_VERSION < 20021112
    DEC_PARAM dec_param;
    DEC_SET dec_set;
    int bits=16;

#ifndef NEW_DECORE
    if(sh->format==mmioFOURCC('D','I','V','3')){
	mp_msg(MSGT_DECVIDEO,MSGL_INFO,"DivX 3.x not supported by opendivx decore - it requires divx4linux\n");
	return 0; // not supported
    }
#endif
#ifndef DECORE_DIVX5
    if(sh->format==mmioFOURCC('D','X','5','0')){
	mp_msg(MSGT_DECVIDEO,MSGL_INFO,"DivX 5.00 not supported by divx4linux decore - it requires divx5linux\n");
	return 0; // not supported
    }
#endif

    if(!mpcodecs_config_vo(sh,sh->disp_w,sh->disp_h,IMGFMT_YUY2)) return 0;

    memset(&dec_param,0,sizeof(dec_param));

    switch(sh->codec->outfmt[sh->outfmtidx]){
	case IMGFMT_YV12: dec_param.output_format=DEC_YV12;bits=12;break;
	case IMGFMT_YUY2: dec_param.output_format=DEC_YUY2;break;
	case IMGFMT_UYVY: dec_param.output_format=DEC_UYVY;break;
	case IMGFMT_I420: dec_param.output_format=DEC_420;bits=12;break;
	case IMGFMT_BGR15: dec_param.output_format=DEC_RGB555_INV;break;
	case IMGFMT_BGR16: dec_param.output_format=DEC_RGB565_INV;break;
	case IMGFMT_BGR24: dec_param.output_format=DEC_RGB24_INV;bits=24;break;
	case IMGFMT_BGR32: dec_param.output_format=DEC_RGB32_INV;bits=32;break;
	default:
	  mp_msg(MSGT_DECVIDEO,MSGL_ERR,"Unsupported out_fmt: 0x%X\n",sh->codec->outfmt[sh->outfmtidx]);
	  return 0;
    }
#ifdef DECORE_DIVX5
    switch(sh->format) {
      case mmioFOURCC('D','I','V','3'):
       	dec_param.codec_version = 311;
	break;
      case mmioFOURCC('D','I','V','X'):
       	dec_param.codec_version = 400;
	break;
      case mmioFOURCC('D','X','5','0'):
      default: // Fallback to DivX 5 behaviour
       	dec_param.codec_version = 500;
    }
    dec_param.build_number = 0;
#endif
    dec_param.x_dim = sh->disp_w;
    dec_param.y_dim = sh->disp_h;
    decore(0x123, DEC_OPT_INIT, &dec_param, NULL);

#ifdef USE_DIVX_BUILTIN_PP
    dec_set.postproc_level = divx_quality;
    decore(0x123, DEC_OPT_SETPP, &dec_set, NULL);
#endif
#endif // DECORE_VERSION    

    mp_msg(MSGT_DECVIDEO,MSGL_V,"INFO: DivX4Linux video codec init OK!\n");

    return 1;
}

// uninit driver
static void uninit(sh_video_t *sh){
#if DECORE_VERSION >= 20021112
    decore(dec_handle, DEC_OPT_RELEASE, NULL, NULL);
    dec_handle = NULL;
#else
    decore(0x123,DEC_OPT_RELEASE,NULL,NULL);
#endif
}

//mp_image_t* mpcodecs_get_image(sh_video_t *sh, int mp_imgtype, int mp_imgflag, int w, int h);

// decode a frame
static mp_image_t* decode(sh_video_t *sh,void* data,int len,int flags){
    mp_image_t* mpi;
    DEC_FRAME dec_frame;
#ifndef USE_DIVX_BUILTIN_PP
    DEC_FRAME_INFO frameinfo;
#endif

    if(len<=0) return NULL; // skipped frame

    dec_frame.length = len;
    dec_frame.bitstream = data;
    dec_frame.render_flag = (flags&VDFLAGS_DROPFRAME)?0:1;

    mpi=mpcodecs_get_image(sh, MP_IMGTYPE_TEMP, MP_IMGFLAG_PRESERVE | MP_IMGFLAG_ACCEPT_WIDTH,
	sh->disp_w, sh->disp_h);
    if(!mpi) return NULL;

    dec_frame.bmp=mpi->planes[0];
    dec_frame.stride=mpi->width;

    decore(
#if DECORE_VERSION >= 20021112
        dec_handle,
#else
        0x123,
#endif
#ifndef DEC_OPT_FRAME_311
        DEC_OPT_FRAME,
#else
	(sh->format==mmioFOURCC('D','I','V','3'))?DEC_OPT_FRAME_311:DEC_OPT_FRAME,
#endif
	&dec_frame,
#ifndef USE_DIVX_BUILTIN_PP
	&frameinfo
#else
	NULL
#endif
    );

#ifndef USE_DIVX_BUILTIN_PP
    mpi->qscale = frameinfo.quant_store;
    mpi->qstride = frameinfo.quant_stride;
#endif

    return mpi;
}
#endif
#endif
