/*
 *
 * QuickTime 8BPS decoder for Mplayer
 * (c) 2003 Roberto Togni
 *
 * Fourcc: 8BPS
 *
 * Supports 8bpp (paletted), 24bpp and 32bpp (4th plane ignored)
 *
 */

#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "bswap.h"

#include "mp_msg.h"

#include "vd_internal.h"


static vd_info_t info = {
  "8BPS Video decoder",
  "qt8bps",
  "Roberto Togni",
  "Roberto Togni",
  "native codec"
};

LIBVD_EXTERN(qt8bps)


/*
 * Decoder context
 */
typedef struct {
  unsigned char planes;
  unsigned char planemap[4];
  unsigned char *palette;
} qt8bps_context_t;


/*
 * Internal function prototypes
 */


// to set/get/query special features/parameters
static int control(sh_video_t *sh,int cmd,void* arg,...)
{
  qt8bps_context_t *hc = (qt8bps_context_t *) sh->context; // Decoder context

  if (cmd == VDCTRL_QUERY_FORMAT)
    switch (hc->planes) {
      case 1:
        if (*((int*)arg) == IMGFMT_BGR8)
          return CONTROL_TRUE;
        else
          return CONTROL_FALSE;
        break;
      case 3:
      case 4:
        if ((*((int*)arg) == IMGFMT_BGR24) || (*((int*)arg) == IMGFMT_BGR32))
          return CONTROL_TRUE;
        else
          return CONTROL_FALSE;
        break;
      default:
        return CONTROL_FALSE;
    }

  return CONTROL_UNKNOWN;
}


/*
 *
 * Init 8BPS decoder
 *
 */
static int init(sh_video_t *sh)
{
  int vo_ret; // Video output init ret value
  qt8bps_context_t *hc; // Decoder context
  BITMAPINFOHEADER *bih = sh->bih;

  if ((hc = malloc(sizeof(qt8bps_context_t))) == NULL) {
    mp_msg(MSGT_DECVIDEO, MSGL_ERR, "Can't allocate memory for 8BPS decoder context.\n");
    return 0;
  }

  sh->context = (void *)hc;
  hc->palette = NULL;

  switch (bih->biBitCount) {
    case 8:
      hc->planes = 1;
      hc->planemap[0] = 0; // 1st plane is palette indexes
      if (bih->biSize > sizeof(BITMAPINFOHEADER)) {
        hc->palette = (unsigned char *)malloc(256*4);
        memcpy(hc->palette, (unsigned char*)bih + sizeof(BITMAPINFOHEADER), 256*4);
      }
      break;
    case 24:
      hc->planes = 3;
      hc->planemap[0] = 2; // 1st plane is red
      hc->planemap[1] = 1; // 2nd plane is green
      hc->planemap[2] = 0; // 3rd plane is blue
      break;
    case 32:
      hc->planes = 4;
      hc->planemap[0] = 2; // 1st plane is red
      hc->planemap[1] = 1; // 2nd plane is green
      hc->planemap[2] = 0; // 3rd plane is blue
      hc->planemap[3] = 3; // 4th plane is alpha???
      mp_msg(MSGT_DECVIDEO, MSGL_WARN, "[8BPS] Ignoring 4th (alpha?) plane.\n");
      break;
    default:
    mp_msg(MSGT_DECVIDEO, MSGL_ERR, "[8BPS] Unsupported color depth: %u.\n", bih->biBitCount);
    return 0;    
  }

  /*
   * Initialize video output device
   */
  vo_ret = mpcodecs_config_vo(sh,sh->disp_w,sh->disp_h,IMGFMT_BGR24);

  return vo_ret;
}




/*
 *
 * Uninit 8BPS decoder
 *
 */
static void uninit(sh_video_t *sh)
{
  qt8bps_context_t *hc = (qt8bps_context_t *) sh->context; // Decoder context

  if (sh->context) {
    if (hc->palette)
      free (hc->palette);
    free(sh->context);
  }
}



/*
 *
 * Decode a frame
 *
 */
static mp_image_t* decode(sh_video_t *sh,void* data,int len,int flags)
{
  mp_image_t* mpi;
  unsigned char *encoded = (unsigned char *)data;
  qt8bps_context_t *hc = (qt8bps_context_t *) sh->context; // Decoder context
  unsigned char *outptr, *pixptr;
  unsigned int height = sh->disp_h; // Real image height
  unsigned int dlen, p, row;
  unsigned char *lp, *dp;
  unsigned char count;
  unsigned int px_inc;
  unsigned int planes = hc->planes;
  unsigned char *planemap = hc->planemap;
  
  
  // Skipped frame
  if(len <= 0)
    return NULL;

  /* Get output image buffer */
  mpi=mpcodecs_get_image(sh, MP_IMGTYPE_STATIC, MP_IMGFLAG_ACCEPT_STRIDE, sh->disp_w, sh->disp_h);
  if (!mpi) {
    mp_msg(MSGT_DECVIDEO, MSGL_ERR, "Can't allocate mpi image for 8BPS codec.\n");
    return NULL;
  }

  outptr = mpi->planes[0]; // Output image pointer
  px_inc = mpi->bpp/8;

  /* Set data pointer after line lengths */
  dp = encoded + hc->planes*(height << 1);

  /* Ignore alpha plane, don't know what to do with it */
  if (planes == 4)
    planes--;

  for (p = 0; p < planes; p++) {
    /* Lines length pointer for this plane */
    lp = encoded + p*(height << 1);

    /* Decode a plane */
    for(row = 0; row < height; row++) {
      pixptr = outptr + row * mpi->stride[0] + planemap[p];
      dlen = be2me_16(*(unsigned short *)(lp+row*2));
      /* Decode a row of this plane */
      while(dlen > 0) {
        if ((count = *dp++) <= 127) {
          count++;
          dlen -= count + 1;
          while(count--) {
            *pixptr = *dp++;
            pixptr += px_inc;
          }
        } else {
          count = 257 - count;
          while(count--) {
            *pixptr = *dp;
            pixptr += px_inc;
          }
          dp++;
          dlen -= 2;
        }
      }
    }
  }

  if (hc->palette)
    mpi->planes[1] = hc->palette;

  return mpi;
}
