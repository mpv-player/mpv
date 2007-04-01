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

void vdlClose(VDL_HANDLE ctx)
{
  if (ctx->drv->destroy)
    ctx->drv->destroy ();
  
  memset (ctx, 0, sizeof (VDXContext)); /* <- it's not stupid */
  free (ctx);
}

int  vdlGetCapability(VDL_HANDLE ctx, vidix_capability_t *cap)
{
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

int  vdlQueryFourcc(VDL_HANDLE ctx,vidix_fourcc_t *f)
{
  f->fourcc = normalize_fourcc(f->fourcc);
  return ctx->drv->query_fourcc (f);
}

int  vdlConfigPlayback(VDL_HANDLE ctx,vidix_playback_t *p)
{
  p->fourcc = normalize_fourcc(p->fourcc);
  return ctx->drv->config_playback (p);
}

int  vdlPlaybackOn(VDL_HANDLE ctx)
{
  return ctx->drv->playback_on ();
}

int  vdlPlaybackOff(VDL_HANDLE ctx)
{
  return ctx->drv->playback_off ();
}

int  vdlPlaybackFrameSelect(VDL_HANDLE ctx, unsigned frame_idx )
{
  return ctx->drv->frame_sel ? ctx->drv->frame_sel (frame_idx) : ENOSYS;
}

int  vdlPlaybackGetEq(VDL_HANDLE ctx, vidix_video_eq_t * e)
{
  return ctx->drv->get_eq ? ctx->drv->get_eq (e) : ENOSYS;
}

int  vdlPlaybackSetEq(VDL_HANDLE ctx, const vidix_video_eq_t * e)
{
  return ctx->drv->set_eq ? ctx->drv->set_eq (e) : ENOSYS;
}

int  vdlPlaybackCopyFrame(VDL_HANDLE ctx, const vidix_dma_t * f)
{
  return ctx->drv->copy_frame ? ctx->drv->copy_frame (f) : ENOSYS;
}

int 	  vdlGetGrKeys(VDL_HANDLE ctx, vidix_grkey_t * k)
{
  return ctx->drv->get_gkey ? ctx->drv->get_gkey (k) : ENOSYS;
}

int 	  vdlSetGrKeys(VDL_HANDLE ctx, const vidix_grkey_t * k)
{
  return ctx->drv->set_gkey ? ctx->drv->set_gkey (k) : ENOSYS;
}

int	  vdlPlaybackGetDeint(VDL_HANDLE ctx, vidix_deinterlace_t * d)
{
  return ctx->drv->get_deint ? ctx->drv->get_deint (d) : ENOSYS;
}

int 	  vdlPlaybackSetDeint(VDL_HANDLE ctx, const vidix_deinterlace_t * d)
{
  return ctx->drv->set_deint ? ctx->drv->set_deint (d) : ENOSYS;
}

int	  vdlQueryNumOemEffects(VDL_HANDLE ctx, unsigned * number )
{
  return ctx->drv->get_num_fx ? ctx->drv->get_num_fx (number) : ENOSYS;
}

int	  vdlGetOemEffect(VDL_HANDLE ctx, vidix_oem_fx_t * f)
{
  return ctx->drv->get_fx ? ctx->drv->get_fx (f) : ENOSYS;
}

int	  vdlSetOemEffect(VDL_HANDLE ctx, const vidix_oem_fx_t * f)
{
  return ctx->drv->set_fx ? ctx->drv->set_fx (f) : ENOSYS;
}
