#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "mp_msg.h"

#ifdef HAVE_XVID

#include "vd_internal.h"
#include "cfgparser.h"

#include <divx4.h>
#include <xvid.h>

#ifndef XVID_CSP_EXTERN
#error "You need lastest XviD CVS"
#endif

static vd_info_t info = 
{
	"xvid decoder",
	"xvid",
	VFM_XVID,
	"Albeu",
	"Albeu",
	""
};

LIBVD_EXTERN(xvid)

typedef struct {
  int cs;
  unsigned char img_type;
  void* hdl;
  mp_image_t* mpi;
} priv_t;

static int do_dr2 = 0;

struct config xvid_dec_opts[] = {
  { "dr2", &do_dr2, CONF_TYPE_FLAG, 0, 0, 1, NULL},
  { "nodr2", &do_dr2, CONF_TYPE_FLAG, 0, 1, 0, NULL},
  {NULL, NULL, 0, 0, 0, 0, NULL}
};

// to set/get/query special features/parameters
static int control(sh_video_t *sh,int cmd,void* arg,...){
  return CONTROL_UNKNOWN;
}

// init driver
static int init(sh_video_t *sh){
  XVID_INIT_PARAM ini;
  XVID_DEC_PARAM dec_p;
  priv_t* p;
  int cs;

  memset(&ini,0,sizeof(XVID_INIT_PARAM));
  memset(&dec_p,0,sizeof(XVID_DEC_PARAM));

  if(!mpcodecs_config_vo(sh,sh->disp_w,sh->disp_h,IMGFMT_YV12))
    return 0;

  switch(sh->codec->outfmt[sh->outfmtidx]){
  case IMGFMT_YV12:
    cs= do_dr2 ? XVID_CSP_EXTERN : XVID_CSP_USER;
    break;
  case IMGFMT_YUY2:
    cs=XVID_CSP_YUY2;
    break;
  case IMGFMT_UYVY:
    cs=XVID_CSP_UYVY;
    break;
  case IMGFMT_I420: 
    cs=XVID_CSP_I420;
    break;
  case IMGFMT_BGR15: 
    cs=XVID_CSP_RGB555;
    break;
  case IMGFMT_BGR16: 
    cs=XVID_CSP_RGB565;
    break;
  case IMGFMT_BGR24:
    cs=XVID_CSP_RGB24;
    break;
  case IMGFMT_BGR32:
    cs=XVID_CSP_RGB32;
    break;
  case IMGFMT_YVYU:
    cs=XVID_CSP_YVYU;
    break;
  default:
    mp_msg(MSGT_DECVIDEO,MSGL_ERR,"Unsupported out_fmt: 0x%X\n",sh->codec->outfmt[sh->outfmtidx]);
    return 0;
  }
  
  if(xvid_init(NULL, 0, &ini, NULL))
    return 0;

  if(ini.api_version != API_VERSION) {
    if(ini.api_version < API_VERSION) {
      mp_msg(MSGT_DECVIDEO,MSGL_ERR,"Too old version of xivd (min. %d)\n",API_VERSION);
      return 0;
    }
    mp_msg(MSGT_DECVIDEO,MSGL_WARN,"Bad xvid version %d was compiled with %d\n",
	   ini.api_version,API_VERSION);
  }

  dec_p.width = sh->disp_w;
  dec_p.height =  sh->disp_h;

  if(xvid_decore(NULL, XVID_DEC_CREATE, &dec_p, NULL)) {
    mp_msg(MSGT_DECVIDEO,MSGL_ERR,"xvid init failed\n");
    return 0;
  }

  p = (priv_t*)malloc(sizeof(priv_t));
  p->cs = cs;
  p->hdl = dec_p.handle;
  sh->context = p;

  switch(cs) {
  case XVID_CSP_EXTERN:
    p->img_type = MP_IMGTYPE_STATIC;
    break;
  case XVID_CSP_USER:
    p->img_type = MP_IMGTYPE_EXPORT;
    break;
  default:
    p->img_type = MP_IMGTYPE_TEMP;
    break;
  }

  return 1;
}

// uninit driver
static void uninit(sh_video_t *sh){
  priv_t* p = sh->context;
  if(!p)
    return;
  xvid_decore(p->hdl,XVID_DEC_DESTROY, NULL, NULL);
  free(p);
}

// decode a frame
static mp_image_t* decode(sh_video_t *sh,void* data,int len,int flags){
  XVID_DEC_FRAME dec;
  DEC_PICTURE d4_pic;
  XVID_DEC_PICTURE pic;
  priv_t* p = sh->context;

  mp_image_t* mpi = mpcodecs_get_image(sh,  p->img_type,
				       MP_IMGFLAG_ACCEPT_STRIDE,
				       sh->disp_w,sh->disp_h);

  if(!data || !mpi || len <= 0)
    return NULL;

  memset(&dec,0,sizeof(XVID_DEC_FRAME));

  dec.bitstream = data;
  dec.length = len;
  switch(p->cs) {
  case XVID_CSP_USER:
    dec.image = &d4_pic;
    break;
  case XVID_CSP_EXTERN:
    pic.y = mpi->planes[0];
    pic.u = mpi->planes[1];
    pic.v = mpi->planes[2];
    pic.stride_y = mpi->stride[0];
    pic.stride_u = mpi->stride[1];
    pic.stride_v = mpi->stride[2];
    dec.image = &pic;
    break;
  default:
    dec.image = mpi->planes[0];
    if(IMGFMT_IS_BGR(mpi->imgfmt) || IMGFMT_IS_RGB(mpi->imgfmt))
      dec.stride = mpi->width;
    else
      dec.stride = mpi->stride[0];
  }
  dec.colorspace = p->cs;

  if(xvid_decore(p->hdl,XVID_DEC_DECODE,&dec,NULL)) {
    mp_msg(MSGT_DECVIDEO,MSGL_ERR,"decoding error\n");
    return NULL;
  }

  if(p->cs == XVID_CSP_USER) {
    mpi->planes[0] = d4_pic.y;
    mpi->planes[1] = d4_pic.u;
    mpi->planes[2] = d4_pic.v;
    mpi->stride[0] = d4_pic.stride_y;
    mpi->stride[1] = mpi->stride[2] = d4_pic.stride_uv;
  }

  return mpi;
}

#endif
