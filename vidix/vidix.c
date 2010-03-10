/*
 * VIDIX - VIDeo Interface for *niX.
 *
 * This interface is introduced as universal one to MPEG decoder,
 * Back End Scaler (BES) and YUV2RGB hw accelerators.
 *
 * In the future it may be expanded up to capturing and audio things.
 * Main goal of this this interface imlpementation is providing DGA
 * everywhere where it's possible (unlike X11 and other).
 *
 * This interface is based on v4l2, fbvid.h, mga_vid.h projects
 * and personally my ideas.
 *
 * NOTE: This interface is introduced as driver interface.
 *
 * Copyright (C) 2002 Nick Kurshev
 * Copyright (C) 2007 Benjamin Zores <ben@geexbox.org>
 *
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "config.h"
#include "vidix.h"
#include "drivers.h"
#include "libavutil/common.h"
#include "mpbswap.h"

VDXContext *vdlOpen(const char *name,unsigned cap,int verbose)
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

void vdlClose(VDXContext *ctx)
{
  if (ctx->drv->destroy)
    ctx->drv->destroy ();

  memset (ctx, 0, sizeof (VDXContext)); /* <- it's not stupid */
  free (ctx);
}

int  vdlGetCapability(VDXContext *ctx, vidix_capability_t *cap)
{
  return ctx->drv->get_caps (cap);
}

#define MPLAYER_IMGFMT_RGB (('R'<<24)|('G'<<16)|('B'<<8))
#define MPLAYER_IMGFMT_BGR (('B'<<24)|('G'<<16)|('R'<<8))
#define MPLAYER_IMGFMT_RGB_MASK 0xFFFFFF00

static uint32_t normalize_fourcc (uint32_t fourcc)
{
  if((fourcc & MPLAYER_IMGFMT_RGB_MASK) == (MPLAYER_IMGFMT_RGB|0) ||
     (fourcc & MPLAYER_IMGFMT_RGB_MASK) == (MPLAYER_IMGFMT_BGR|0))
	return bswap_32(fourcc);
  return fourcc;
}

int vdlQueryFourcc (VDXContext *ctx, vidix_fourcc_t *f)
{
  f->fourcc = normalize_fourcc(f->fourcc);
  return ctx->drv->query_fourcc (f);
}

int vdlConfigPlayback (VDXContext *ctx, vidix_playback_t *p)
{
  p->fourcc = normalize_fourcc(p->fourcc);
  return ctx->drv->config_playback (p);
}

int vdlPlaybackOn (VDXContext *ctx)
{
  return ctx->drv->playback_on ();
}

int vdlPlaybackOff (VDXContext *ctx)
{
  return ctx->drv->playback_off ();
}

int vdlPlaybackFrameSelect (VDXContext *ctx, unsigned frame_idx)
{
  return ctx->drv->frame_sel ? ctx->drv->frame_sel (frame_idx) : ENOSYS;
}

int vdlPlaybackGetEq (VDXContext *ctx, vidix_video_eq_t *e)
{
  return ctx->drv->get_eq ? ctx->drv->get_eq (e) : ENOSYS;
}

int vdlPlaybackSetEq (VDXContext *ctx, const vidix_video_eq_t *e)
{
  return ctx->drv->set_eq ? ctx->drv->set_eq (e) : ENOSYS;
}

int vdlGetGrKeys (VDXContext *ctx, vidix_grkey_t *k)
{
  return ctx->drv->get_gkey ? ctx->drv->get_gkey (k) : ENOSYS;
}

int vdlSetGrKeys (VDXContext *ctx, const vidix_grkey_t *k)
{
  return ctx->drv->set_gkey ? ctx->drv->set_gkey (k) : ENOSYS;
}

int vdlPlaybackGetDeint (VDXContext *ctx, vidix_deinterlace_t *d)
{
  return ctx->drv->get_deint ? ctx->drv->get_deint (d) : ENOSYS;
}

int vdlPlaybackSetDeint (VDXContext *ctx, const vidix_deinterlace_t *d)
{
  return ctx->drv->set_deint ? ctx->drv->set_deint (d) : ENOSYS;
}
