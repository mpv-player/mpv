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
	"DivX5Linux lib (divx4 mode)",
#else
	"DivX4Linux lib (divx4 mode)",
#endif
	"divx4",
	VFM_DIVX4,
	"A'rpi",
	"http://www.divx.com",
	"native codecs"
};

LIBVD_EXTERN(divx4)

#ifdef HAVE_DIVX4_H
#include <divx4.h>
#else
#include <decore.h>
#endif

// to set/get/query special features/parameters
static int control(sh_video_t *sh,int cmd,void* arg,...){
    switch(cmd){
    case VDCTRL_QUERY_MAX_PP_LEVEL:
	return 9; // for divx4linux
    case VDCTRL_SET_PP_LEVEL: {
	DEC_SET dec_set;
	int quality=*((int*)arg);
	if(quality<0 || quality>9) quality=9;
	dec_set.postproc_level=quality*10;
	decore(0x123,DEC_OPT_SETPP,&dec_set,NULL);
	return CONTROL_OK;
    }
#ifdef DECORE_VERSION
#if DECORE_VERSION >= 20011010
    case VDCTRL_SET_EQUALIZER: {
	va_list ap;
	int value;
        int option;
	va_start(ap, arg);
	value=va_arg(ap, int);
	va_end(ap);

        if(!strcmp(arg,"Brightness")) option=DEC_GAMMA_BRIGHTNESS;
        else if(!strcmp(arg, "Contrast")) option=DEC_GAMMA_CONTRAST;
        else if(!strcmp(arg,"Saturation")) option=DEC_GAMMA_SATURATION;
        else return CONTROL_FALSE;
	
        value = (value * 256) / 100 - 128;
        decore(0x123, DEC_OPT_GAMMA, (void *)option, (void *) value);
	return CONTROL_OK;
    }
#endif
#endif
    
    }

    return CONTROL_UNKNOWN;
}

// init driver
static int init(sh_video_t *sh){
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

    dec_set.postproc_level = divx_quality;
    decore(0x123, DEC_OPT_SETPP, &dec_set, NULL);
    
    mp_msg(MSGT_DECVIDEO,MSGL_V,"INFO: DivX4Linux video codec init OK!\n");

    return 1;
}

// uninit driver
static void uninit(sh_video_t *sh){
    decore(0x123,DEC_OPT_RELEASE,NULL,NULL);
}

//mp_image_t* mpcodecs_get_image(sh_video_t *sh, int mp_imgtype, int mp_imgflag, int w, int h);

// decode a frame
static mp_image_t* decode(sh_video_t *sh,void* data,int len,int flags){
    mp_image_t* mpi;
    DEC_FRAME dec_frame;

    if(len<=0) return NULL; // skipped frame

    dec_frame.length = len;
    dec_frame.bitstream = data;
    dec_frame.render_flag = (flags&3)?0:1;

    mpi=mpcodecs_get_image(sh, MP_IMGTYPE_TEMP, MP_IMGFLAG_PRESERVE | MP_IMGFLAG_ACCEPT_WIDTH,
	sh->disp_w, sh->disp_h);
    if(!mpi) return NULL;

    dec_frame.bmp=mpi->planes[0];
    dec_frame.stride=mpi->width;
    
#ifndef DEC_OPT_FRAME_311
    decore(0x123, DEC_OPT_FRAME, &dec_frame, NULL);
#else
    decore(0x123, (sh->format==mmioFOURCC('D','I','V','3'))?DEC_OPT_FRAME_311:DEC_OPT_FRAME, &dec_frame, NULL);
#endif

    return mpi;
}

#endif
#endif

