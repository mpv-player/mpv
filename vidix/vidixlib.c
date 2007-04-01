/*
 * vidixlib.c
 * VIDIXLib - Library for VIDeo Interface for *niX
 *   This interface is introduced as universal one to MPEG decoder,
 *   BES == Back End Scaler and YUV2RGB hw accelerators.
 * In the future it may be expanded up to capturing and audio things.
 * Main goal of this this interface imlpementation is providing DGA
 * everywhere where it's possible (unlike X11 and other).
 * Copyright 2002 Nick Kurshev
 * Licence: GPL
 * This interface is based on v4l2, fbvid.h, mga_vid.h projects
 * and personally my ideas.
 * NOTE: This interface is introduces as APP interface.
 * Don't use it for driver.
 * It provides multistreaming. This mean that APP can handle
 * several streams simultaneously. (Example: Video capturing and video
 * playback or capturing, video playback, audio encoding and so on).
*/
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "vidixlib.h"
#include "drivers.h"
#include "../config.h"
#include "../libavutil/common.h"
#include "../mpbswap.h"

extern unsigned int  vdlGetVersion( void )
{
   return VIDIX_VERSION;
}

VDL_HANDLE vdlOpen(const char *path,const char *name,unsigned cap,int verbose)
{
  VDXContext *ctx;

  if (!(ctx = malloc (sizeof (VDXContext))))
    return NULL;
  memset (ctx, 0, sizeof (VDXContext));

  /* register all drivers */
  vidix_register_all_drivers ();
  
  if (!vidix_find_driver (ctx, name, cap, verbose))
  {
    free (ctx);
    return NULL;
  }

  if (verbose)
    printf ("vidixlib: will use %s driver\n", ctx->drv->name);

  if (!ctx->drv || !ctx->drv->init)
  {
    if (verbose)
      printf ("vidixlib: Can't init driver\n");
    free (ctx);
    return NULL;
  }
  
  if (verbose)
    printf ("vidixlib: Attempt to initialize driver at: %p\n",
            ctx->drv->init);

  if (ctx->drv->init () !=0)
  {
    if (verbose)
      printf ("vidixlib: Can't init driver\n");
    free (ctx);
    return NULL;
  }
  
  if (verbose)
    printf("vidixlib: '%s'successfully loaded\n", ctx->drv->name);
  
  return ctx;
}

void vdlClose(VDL_HANDLE stream)
{
  VDXContext *ctx;

  ctx = (VDXContext *) stream;
  if (ctx->drv->destroy)
    ctx->drv->destroy ();
  
  memset (ctx, 0, sizeof (VDXContext)); /* <- it's not stupid */
  free (ctx);
}

int  vdlGetCapability(VDL_HANDLE handle, vidix_capability_t *cap)
{
  VDXContext *ctx;

  ctx = (VDXContext *) handle;
  return ctx->drv->get_caps (cap);
}

#define MPLAYER_IMGFMT_RGB (('R'<<24)|('G'<<16)|('B'<<8))
#define MPLAYER_IMGFMT_BGR (('B'<<24)|('G'<<16)|('R'<<8))
#define MPLAYER_IMGFMT_RGB_MASK 0xFFFFFF00

static uint32_t normalize_fourcc(uint32_t fourcc)
{
  if((fourcc & MPLAYER_IMGFMT_RGB_MASK) == (MPLAYER_IMGFMT_RGB|0) ||
     (fourcc & MPLAYER_IMGFMT_RGB_MASK) == (MPLAYER_IMGFMT_BGR|0))
	return bswap_32(fourcc);
  else  return fourcc;
}

int  vdlQueryFourcc(VDL_HANDLE handle,vidix_fourcc_t *f)
{
  VDXContext *ctx;

  ctx = (VDXContext *) handle;
  f->fourcc = normalize_fourcc(f->fourcc);
  return ctx->drv->query_fourcc (f);
}

int  vdlConfigPlayback(VDL_HANDLE handle,vidix_playback_t *p)
{
  VDXContext *ctx;

  ctx = (VDXContext *) handle;
  p->fourcc = normalize_fourcc(p->fourcc);
  return ctx->drv->config_playback (p);
}

int  vdlPlaybackOn(VDL_HANDLE handle)
{
  VDXContext *ctx;

  ctx = (VDXContext *) handle;
  return ctx->drv->playback_on ();
}

int  vdlPlaybackOff(VDL_HANDLE handle)
{
  VDXContext *ctx;

  ctx = (VDXContext *) handle;
  return ctx->drv->playback_off ();
}

int  vdlPlaybackFrameSelect(VDL_HANDLE handle, unsigned frame_idx )
{
  VDXContext *ctx;

  ctx = (VDXContext *) handle;
  if (ctx->drv->frame_sel)
    return ctx->drv->frame_sel (frame_idx);

  return ENOSYS;
}

int  vdlPlaybackGetEq(VDL_HANDLE handle, vidix_video_eq_t * e)
{
  VDXContext *ctx;

  ctx = (VDXContext *) handle;
  if (ctx->drv->get_eq)
    return ctx->drv->get_eq (e);

  return ENOSYS;
}

int  vdlPlaybackSetEq(VDL_HANDLE handle, const vidix_video_eq_t * e)
{
  VDXContext *ctx;

  ctx = (VDXContext *) handle;
  if (ctx->drv->set_eq)
    return ctx->drv->set_eq (e);

  return ENOSYS;
}

int  vdlPlaybackCopyFrame(VDL_HANDLE handle, const vidix_dma_t * f)
{
  VDXContext *ctx;

  ctx = (VDXContext *) handle;
  if (ctx->drv->copy_frame)
    return ctx->drv->copy_frame (f);

  return ENOSYS;
}

int 	  vdlGetGrKeys(VDL_HANDLE handle, vidix_grkey_t * k)
{
  VDXContext *ctx;

  ctx = (VDXContext *) handle;
  if (ctx->drv->get_gkey)
    return ctx->drv->get_gkey (k);

  return ENOSYS;
}

int 	  vdlSetGrKeys(VDL_HANDLE handle, const vidix_grkey_t * k)
{
  VDXContext *ctx;

  ctx = (VDXContext *) handle;
  if (ctx->drv->set_gkey)
    return ctx->drv->set_gkey (k);

  return ENOSYS;
}

int	  vdlPlaybackGetDeint(VDL_HANDLE handle, vidix_deinterlace_t * d)
{
  VDXContext *ctx;

  ctx = (VDXContext *) handle;
  if (ctx->drv->get_deint)
    return ctx->drv->get_deint (d);

  return ENOSYS;
}

int 	  vdlPlaybackSetDeint(VDL_HANDLE handle, const vidix_deinterlace_t * d)
{
  VDXContext *ctx;

  ctx = (VDXContext *) handle;
  if (ctx->drv->set_deint)
    return ctx->drv->set_deint (d);

  return ENOSYS;
}

int	  vdlQueryNumOemEffects(VDL_HANDLE handle, unsigned * number )
{
  VDXContext *ctx;

  ctx = (VDXContext *) handle;
  if (ctx->drv->get_num_fx)
    return ctx->drv->get_num_fx (number);

  return ENOSYS;
}

int	  vdlGetOemEffect(VDL_HANDLE handle, vidix_oem_fx_t * f)
{
  VDXContext *ctx;

  ctx = (VDXContext *) handle;
  if (ctx->drv->get_fx)
    return ctx->drv->get_fx (f);

  return ENOSYS;
}

int	  vdlSetOemEffect(VDL_HANDLE handle, const vidix_oem_fx_t * f)
{
  VDXContext *ctx;

  ctx = (VDXContext *) handle;
  if (ctx->drv->set_fx)
    return ctx->drv->set_fx (f);

  return ENOSYS;
}
