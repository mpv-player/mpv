#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "mp_msg.h"

#include "m_option.h"

#include "codec-cfg.h"
#include "stream/stream.h"
#include "libmpdemux/demuxer.h"
#include "libmpdemux/stheader.h"

#include "stream/stream.h"
#include "libmpdemux/muxer.h"

#include "img_format.h"
#include "mp_image.h"
#include "vf.h"

#include "libmpdemux/nuppelvideo.h"
#include <lzo/lzo1x.h>
#include "native/rtjpegn.h"

#define LZO_AL(size) (((size) + (sizeof(long) - 1)) / sizeof(long))
#define LZO_IN_LEN          (1024*1024L)
#define LZO_OUT_LEN         (LZO_IN_LEN + LZO_IN_LEN / 64 + 16 + 3)

//===========================================================================//

struct vf_priv_s {
  int raw; // Do not use RTjpeg
  int lzo; // Use lzo
  unsigned int l,c,q; // Mjpeg param
  muxer_stream_t* mux;
  uint8_t* buffer;

  int buf_size;
  int tbl_wrote;
  lzo_byte *zbuffer;
  long __LZO_MMODEL *zmem;
};
#define mux_v (vf->priv->mux)

struct vf_priv_s nuv_priv_dflt = {
  0, // raw
  1, // lzo
  1,1, // l,c
  255, // q
  NULL,
  NULL,
  0,0,
  NULL,NULL
};

m_option_t nuvopts_conf[]={
  {"raw", &nuv_priv_dflt.raw, CONF_TYPE_FLAG, 0, 0, 1, NULL},
  {"rtjpeg", &nuv_priv_dflt.raw, CONF_TYPE_FLAG, 0, 1, 0, NULL},
  {"lzo", &nuv_priv_dflt.lzo, CONF_TYPE_FLAG, 0, 0, 1, NULL},
  {"nolzo", &nuv_priv_dflt.lzo, CONF_TYPE_FLAG, 0, 1, 0, NULL},
  {"q", &nuv_priv_dflt.q, CONF_TYPE_INT, M_OPT_RANGE,3,255, NULL},
  {"l", &nuv_priv_dflt.l, CONF_TYPE_INT, M_OPT_RANGE,0,20, NULL},
  {"c", &nuv_priv_dflt.c, CONF_TYPE_INT, M_OPT_RANGE,0,20, NULL},
  {NULL, NULL, 0, 0, 0, 0, NULL}
};  

//===========================================================================//


static int config(struct vf_instance_s* vf,
        int width, int height, int d_width, int d_height,
	unsigned int flags, unsigned int outfmt){

  // We need a buffer wich can holda header and a whole YV12 picture
  // or a RTJpeg table
  vf->priv->buf_size = width*height*3/2+FRAMEHEADERSIZE;
  if(vf->priv->buf_size < (int)(128*sizeof(long int) + FRAMEHEADERSIZE))
    vf->priv->buf_size = 128*sizeof(long int) + FRAMEHEADERSIZE;

  mux_v->bih->biWidth=width;
  mux_v->bih->biHeight=height;
  mux_v->bih->biSizeImage=mux_v->bih->biWidth*mux_v->bih->biHeight*(mux_v->bih->biBitCount/8);
  mux_v->aspect = (float)d_width/d_height;
  vf->priv->buffer = realloc(vf->priv->buffer,vf->priv->buf_size);
  vf->priv->tbl_wrote = 0;

  return 1;
}

static int control(struct vf_instance_s* vf, int request, void* data){

  return CONTROL_UNKNOWN;
}

static int query_format(struct vf_instance_s* vf, unsigned int fmt){
  if(fmt==IMGFMT_I420) return VFCAP_CSP_SUPPORTED | VFCAP_CSP_SUPPORTED_BY_HW;
  return 0;
}

static int put_image(struct vf_instance_s* vf, mp_image_t *mpi, double pts){
  struct rtframeheader* ench = (struct rtframeheader*)vf->priv->buffer;
  uint8_t* data = vf->priv->buffer + FRAMEHEADERSIZE;
  uint8_t* zdata = vf->priv->zbuffer + FRAMEHEADERSIZE;
  int len = 0, zlen = 0,r;

  memset(vf->priv->buffer,0,FRAMEHEADERSIZE); // Reset the header
  if(vf->priv->lzo)
    memset(vf->priv->zbuffer,0,FRAMEHEADERSIZE);
    
  // This has to be don here otherwise tv with sound doesn't work
  if(!vf->priv->tbl_wrote) {    
    RTjpeg_init_compress((long int*)data,mpi->width,mpi->height,vf->priv->q);
    RTjpeg_init_mcompress();

    ench->frametype = 'D'; // compressor data
    ench->comptype  = 'R'; // compressor data for RTjpeg
    ench->packetlength = 128*sizeof(long int);
  
    le2me_rtframeheader(ench);
    mux_v->buffer=vf->priv->buffer;
    muxer_write_chunk(mux_v,FRAMEHEADERSIZE + 128*sizeof(long int), 0x10, MP_NOPTS_VALUE, MP_NOPTS_VALUE);
    vf->priv->tbl_wrote = 1;
    memset(ench,0,FRAMEHEADERSIZE); // Reset the header
  }

  // Raw picture
  if(vf->priv->raw) {
    len = mpi->width*mpi->height*3/2;
    // Try lzo ???
    if(vf->priv->lzo) {
      r = lzo1x_1_compress(mpi->planes[0],mpi->width*mpi->height*3/2,
			   zdata,&zlen,vf->priv->zmem);
      if(r != LZO_E_OK) {
	mp_msg(MSGT_VFILTER,MSGL_ERR,"LZO compress error\n");
	zlen = 0;
      }
    }

    if(zlen <= 0 || zlen > len) {
      memcpy(data,mpi->planes[0],len);
      ench->comptype = '0';
    } else { // Use lzo only if it's littler
      ench = (struct rtframeheader*)vf->priv->zbuffer;
      ench->comptype = '3';
      len = zlen;
    }

  } else { // RTjpeg compression
    len = RTjpeg_mcompressYUV420(data,mpi->planes[0],vf->priv->l,
				 vf->priv->c);
    if(len <= 0) {
      mp_msg(MSGT_VFILTER,MSGL_ERR,"RTjpeg_mcompressYUV420 error (%d)\n",len);
      return 0;
    }

    if(vf->priv->lzo) {
      r = lzo1x_1_compress(data,len,zdata,&zlen,vf->priv->zmem);
      if(r != LZO_E_OK) {
	mp_msg(MSGT_VFILTER,MSGL_ERR,"LZO compress error\n");
	zlen = 0;
      }
    }

    if(zlen <= 0 || zlen > len)
      ench->comptype = '1';
    else {
      ench = (struct rtframeheader*)vf->priv->zbuffer;
      ench->comptype = '2';
      len = zlen;
    }

  }
    
  ench->frametype = 'V'; // video frame
  ench->packetlength = len;
  le2me_rtframeheader(ench);
  mux_v->buffer=(void*)ench;
  muxer_write_chunk(mux_v, len + FRAMEHEADERSIZE, 0x10, pts, pts);
  return 1;
}

static void uninit(struct vf_instance_s* vf) {

  if(vf->priv->buffer)
    free(vf->priv->buffer);
  if(vf->priv->zbuffer)
    free(vf->priv->zbuffer);
  if(vf->priv->zmem)
    free(vf->priv->zmem);

}

//===========================================================================//

static int vf_open(vf_instance_t *vf, char* args){
  vf->config=config;
  vf->default_caps=VFCAP_CONSTANT;
  vf->control=control;
  vf->query_format=query_format;
  vf->put_image=put_image;
  vf->uninit = uninit;
  vf->priv=malloc(sizeof(struct vf_priv_s));
  memcpy(vf->priv, &nuv_priv_dflt,sizeof(struct vf_priv_s));
  vf->priv->mux=(muxer_stream_t*)args;
  
  mux_v->bih=calloc(1, sizeof(BITMAPINFOHEADER));
  mux_v->bih->biSize=sizeof(BITMAPINFOHEADER);
  mux_v->bih->biWidth=0;
  mux_v->bih->biHeight=0;
  mux_v->bih->biPlanes=1;
  mux_v->bih->biBitCount=12;
  mux_v->bih->biCompression = mmioFOURCC('N','U','V','1');

  if(vf->priv->lzo) {
    if(lzo_init() != LZO_E_OK) {
      mp_msg(MSGT_VFILTER,MSGL_WARN,"LZO init failed: no lzo compression\n");
      vf->priv->lzo = 0;
    }
    vf->priv->zbuffer = (lzo_bytep)malloc(FRAMEHEADERSIZE + LZO_OUT_LEN);
    vf->priv->zmem = malloc(sizeof(long)*LZO_AL(LZO1X_1_MEM_COMPRESS));
  }

  return 1;
}

vf_info_t ve_info_nuv = {
  "nuv encoder",
  "nuv",
  "Albeu",
  "for internal use by mencoder",
  vf_open
};

//===========================================================================//
