#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"

#ifdef USE_DIVX

#include "vd_internal.h"

static vd_info_t info = {
#ifdef NEW_DECORE
#ifdef DECORE_DIVX5
	"DivX5Linux lib (odivx mode)",
#else
	"DivX4Linux lib (odivx mode)",
#endif
#else
	"Opendivx 0.48 codec",
#endif
	"odivx",
	"A'rpi",
#ifdef NEW_DECORE
	"http://www.divx.com",
#else
	"http://www.projectmayo.org",
#endif
	"native codecs"
};

LIBVD_EXTERN(odivx)

#ifndef NEW_DECORE
#include "opendivx/decore.h"
#include "postproc/postprocess.h"
#elif HAVE_DIVX4_H
#include <divx4.h>
#else
#include <decore.h>
#endif

//**************************************************************************//
//             The OpenDivX stuff:
//**************************************************************************//

#ifndef NEW_DECORE

static unsigned char *opendivx_src[3];
static int opendivx_stride[3];

// callback, the opendivx decoder calls this for each frame:
void convert_linux(unsigned char *puc_y, int stride_y,
	unsigned char *puc_u, unsigned char *puc_v, int stride_uv,
	unsigned char *bmp, int width_y, int height_y){

//    printf("convert_yuv called  %dx%d  stride: %d,%d\n",width_y,height_y,stride_y,stride_uv);

    opendivx_src[0]=puc_y;
    opendivx_src[1]=puc_u;
    opendivx_src[2]=puc_v;
    
    opendivx_stride[0]=stride_y;
    opendivx_stride[1]=stride_uv;
    opendivx_stride[2]=stride_uv;
}
#endif


// to set/get/query special features/parameters
static int control(sh_video_t *sh,int cmd,void* arg,...){
    switch(cmd){
    case VDCTRL_QUERY_MAX_PP_LEVEL:
#ifdef NEW_DECORE
	return 9; // for divx4linux
#else
	return GET_PP_QUALITY_MAX;  // for opendivx
#endif
    case VDCTRL_SET_PP_LEVEL: {
	DEC_SET dec_set;
	int quality=*((int*)arg);
#ifdef NEW_DECORE
	if(quality<0 || quality>9) quality=9;
	dec_set.postproc_level=quality*10;
#else
	if(quality<0 || quality>GET_PP_QUALITY_MAX) quality=GET_PP_QUALITY_MAX;
	dec_set.postproc_level=getPpModeForQuality(quality);
#endif
	decore(0x123,DEC_OPT_SETPP,&dec_set,NULL);
	return CONTROL_OK;
    }
    
    }

    return CONTROL_UNKNOWN;
}

// init driver
static int init(sh_video_t *sh){
    DEC_PARAM dec_param;
    DEC_SET dec_set;

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

    memset(&dec_param,0,sizeof(dec_param));
#ifdef NEW_DECORE
    dec_param.output_format=DEC_USER;
#else
    dec_param.color_depth = 32;
#endif
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
    
    mp_msg(MSGT_DECVIDEO,MSGL_V,"INFO: OpenDivX video codec init OK!\n");

    return mpcodecs_config_vo(sh,sh->disp_w,sh->disp_h,IMGFMT_YV12);
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
#ifdef NEW_DECORE
    DEC_PICTURE dec_pic;
#endif

    if(len<=0) return NULL; // skipped frame

    dec_frame.length = len;
    dec_frame.bitstream = data;
    dec_frame.render_flag = (flags&3)?0:1;

#ifdef NEW_DECORE
    dec_frame.bmp=&dec_pic;
    dec_pic.y=dec_pic.u=dec_pic.v=NULL;
#ifndef DEC_OPT_FRAME_311
    decore(0x123, DEC_OPT_FRAME, &dec_frame, NULL);
#else
    decore(0x123, (sh->format==mmioFOURCC('D','I','V','3'))?DEC_OPT_FRAME_311:DEC_OPT_FRAME, &dec_frame, NULL);
#endif
#else
    // opendivx:
    opendivx_src[0]=NULL;
    decore(0x123, 0, &dec_frame, NULL);
#endif

    if(flags&3) return NULL; // framedrop

#ifdef NEW_DECORE
    if(!dec_pic.y) return NULL; // bad frame
#else
    if(!opendivx_src[0]) return NULL; // bad frame
#endif
    
    mpi=mpcodecs_get_image(sh, MP_IMGTYPE_EXPORT, MP_IMGFLAG_PRESERVE,
	sh->disp_w, sh->disp_h);
    if(!mpi) return NULL;
    
#ifdef NEW_DECORE
    mpi->planes[0]=dec_pic.y;
    mpi->planes[1]=dec_pic.u;
    mpi->planes[2]=dec_pic.v;
    mpi->stride[0]=dec_pic.stride_y;
    mpi->stride[1]=mpi->stride[2]=dec_pic.stride_uv;
#else
    mpi->planes[0]=opendivx_src[0];
    mpi->planes[1]=opendivx_src[1];
    mpi->planes[2]=opendivx_src[2];
    mpi->stride[0]=opendivx_stride[0];
    mpi->stride[1]=opendivx_stride[1];
    mpi->stride[2]=opendivx_stride[2];
#endif

    return mpi;
}

#endif

