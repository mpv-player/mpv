#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#ifdef USE_WIN32DLL

#include "mp_msg.h"
#include "help_mp.h"

#include "vd_internal.h"

#include "dll_init.h"

static vd_info_t info_vfw = {
	"Win32/VfW video codecs",
	"vfw",
	VFM_VFW,
	"A'rpi",
	"based on http://avifile.sf.net",
	"win32 codecs"
};

static vd_info_t info_vfwex = {
	"Win32/VfWex video codecs",
	"vfwex",
	VFM_VFWEX,
	"A'rpi",
	"based on http://avifile.sf.net",
	"win32 codecs"
};

#define info info_vfw
LIBVD_EXTERN(vfw)
#undef info

#define info info_vfwex
LIBVD_EXTERN(vfwex)
#undef info

// to set/get/query special features/parameters
static int control(sh_video_t *sh,int cmd,void* arg,...){
    switch(cmd){
    case VDCTRL_QUERY_MAX_PP_LEVEL:
	return 9;
    case VDCTRL_SET_PP_LEVEL:
	vfw_set_postproc(sh,10*(*((int*)arg)));
	return CONTROL_OK;
    }
    return CONTROL_UNKNOWN;
}

// init driver
static int init(sh_video_t *sh){
    if(!mpcodecs_config_vo(sh,sh->disp_w,sh->disp_h,IMGFMT_YUY2)) return 0;
    if(!init_vfw_video_codec(sh,(sh->codec->driver==VFM_VFWEX))) return 0;
    mp_msg(MSGT_DECVIDEO,MSGL_V,"INFO: Win32 video codec init OK!\n");
    return 1;
}

// uninit driver
static void uninit(sh_video_t *sh){
    vfw_close_video_codec(sh, (sh->codec->driver==VFM_VFWEX));
}

//mp_image_t* mpcodecs_get_image(sh_video_t *sh, int mp_imgtype, int mp_imgflag, int w, int h);

// decode a frame
static mp_image_t* decode(sh_video_t *sh,void* data,int len,int flags){
    mp_image_t* mpi;
    int ret;
    if(len<=0) return NULL; // skipped frame

    mpi=mpcodecs_get_image(sh, 
	(sh->codec->outflags[sh->outfmtidx] & CODECS_FLAG_STATIC) ?
	MP_IMGTYPE_STATIC : MP_IMGTYPE_TEMP, MP_IMGFLAG_ACCEPT_WIDTH, 
	sh->disp_w, sh->disp_h);
    if(!mpi){	// temporary!
	printf("couldn't allocate image for cinepak codec\n");
	return NULL;
    }

    // set buffer:
    sh->our_out_buffer=mpi->planes[0];

    // set stride:  (trick discovered by Andreas Ackermann - thanx!)
    sh->bih->biWidth=mpi->width; //mpi->stride[0]/(mpi->bpp/8);
    sh->o_bih.biWidth=mpi->width; //mpi->stride[0]/(mpi->bpp/8);

    if((ret=vfw_decode_video(sh,data,len,flags&3,(sh->codec->driver==VFM_VFWEX) ))){
      mp_msg(MSGT_DECVIDEO,MSGL_WARN,"Error decompressing frame, err=%d\n",ret);
      return NULL;
    }
    
    if(mpi->imgfmt==IMGFMT_RGB8 || mpi->imgfmt==IMGFMT_BGR8){
	// export palette:
// FIXME: sh->o_bih is cutted down to 40 bytes!!!
//	if(sh->o_bih->biSize>40)
//	    mpi->planes[1]=((unsigned char*)&sh->o_bih)+40;
//	else
	    mpi->planes[1]=NULL;
    }
    
    return mpi;
}

#endif
