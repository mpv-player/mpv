#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"

#include "img_format.h"
#include "mp_image.h"
#include "vf.h"
#include "libavcodec/avcodec.h"

extern int avcodec_initialized;

struct vf_priv_s 
{
  int       width, height;
  int       pix_fmt;
};

/* Support for avcodec's built-in deinterlacer.
 * Based on vf_lavc.c
 */

//===========================================================================//


/* Convert mplayer's IMGFMT_* to avcodec's PIX_FMT_* for the supported
 * IMGFMT's, and return -1 if the deinterlacer doesn't support
 * that format (-1 because 0 is a valid PIX_FMT).
 */
/* The deinterlacer supports planer 4:2:0, 4:2:2, and 4:4:4 YUV */
static int
imgfmt_to_pixfmt (int imgfmt)
{
  switch(imgfmt)
    {
      /* I hope I got all the supported formats */

      /* 4:2:0 */
    case IMGFMT_YV12:
    case IMGFMT_I420:
    case IMGFMT_IYUV:
      return PIX_FMT_YUV420P;
      break;

#if 0
      /* 4:2:2 */
    case IMGFMT_UYVY:
    case IMGFMT_UYNV:
    case IMGFMT_Y422:
    case IMGFMT_YUY2:
    case IMGFMT_YUNV:
    case IMGFMT_YVYU:
    case IMGFMT_Y42T:
    case IMGFMT_V422:
    case IMGFMT_V655:
      return PIX_FMT_YUV422P;
      break;
#endif

      /* Are there any _planar_ YUV 4:4:4 formats? */

    default:
      return -1;
    }
}


static int 
config (struct vf_instance_s* vf,
        int width, int height, int d_width, int d_height,
        unsigned int flags, unsigned int outfmt)
{
  struct vf_priv_s *priv = vf->priv;

  priv->pix_fmt = imgfmt_to_pixfmt(outfmt);
  if(priv->pix_fmt == -1)
    return 0;
  
  /* The deinterlacer will fail if this is false */
  if ((width & 3) != 0 || (height & 3) != 0)
    return 0;

  /* If we get here, the deinterlacer is guaranteed not to fail */

  priv->width  = width;
  priv->height = height;

  return vf_next_config(vf,
			width, height,
			d_width, d_height,
			flags, outfmt);
}

static int 
put_image (struct vf_instance_s* vf, mp_image_t *mpi, double pts)
{
  struct vf_priv_s *priv = vf->priv;
  mp_image_t* dmpi;
  AVPicture pic;
  AVPicture lavc_picture;
  
  lavc_picture.data[0]     = mpi->planes[0];
  lavc_picture.data[1]     = mpi->planes[1];
  lavc_picture.data[2]     = mpi->planes[2];
  lavc_picture.linesize[0] = mpi->stride[0];
  lavc_picture.linesize[1] = mpi->stride[1];
  lavc_picture.linesize[2] = mpi->stride[2];
  
  dmpi = vf_get_image(vf->next, mpi->imgfmt,
		      MP_IMGTYPE_TEMP, MP_IMGFLAG_ACCEPT_STRIDE,
		      priv->width, priv->height);

  pic.data[0]     = dmpi->planes[0];
  pic.data[1]     = dmpi->planes[1];
  pic.data[2]     = dmpi->planes[2];
  pic.linesize[0] = dmpi->stride[0];
  pic.linesize[1] = dmpi->stride[1];
  pic.linesize[2] = dmpi->stride[2];

  if (avpicture_deinterlace(&pic, &lavc_picture, 
			    priv->pix_fmt, priv->width, priv->height) < 0)
    {
      /* This should not happen -- see config() */
      return 0;
    }
  
  return vf_next_put_image(vf, dmpi, pts);
}


static int 
query_format (struct vf_instance_s* vf, unsigned int fmt)
{
  if(imgfmt_to_pixfmt(fmt) == -1)
    return 0;

  return vf_next_query_format(vf,fmt);
}


static int 
open (vf_instance_t *vf, char* args)
{
  /* We don't have any args */
  (void) args;

  vf->config       = config;
  vf->put_image    = put_image;
  vf->query_format = query_format;
  vf->priv         = malloc(sizeof(struct vf_priv_s));
  memset(vf->priv,0,sizeof(struct vf_priv_s));

  /* This may not technically be necessary just for a deinterlace,
   * but it seems like a good idea.
   */
  if(!avcodec_initialized)
    {
      avcodec_init();
      avcodec_register_all();
      avcodec_initialized=1;
    }

  return 1;
}


const vf_info_t vf_info_lavcdeint = {
    "libavcodec's deinterlacing filter",
    "lavcdeint",
    "Joe Rabinoff",
    "libavcodec's internal deinterlacer, in case you don't like "
      "the builtin ones (invoked with -pp or -npp)",
    open,
    NULL
};


//===========================================================================//
