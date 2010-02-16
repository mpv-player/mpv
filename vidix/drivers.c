/*
 * VIDIX Drivers Registry Handler.
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
#include "config.h"

VDXDriver *first_driver = NULL;

extern VDXDriver cyberblade_drv;
extern VDXDriver ivtv_drv;
extern VDXDriver mach64_drv;
extern VDXDriver mga_drv;
extern VDXDriver mga_crtc2_drv;
extern VDXDriver nvidia_drv;
extern VDXDriver pm2_drv;
extern VDXDriver pm3_drv;
extern VDXDriver radeon_drv;
extern VDXDriver rage128_drv;
extern VDXDriver s3_drv;
extern VDXDriver sh_veu_drv;
extern VDXDriver sis_drv;
extern VDXDriver unichrome_drv;

static void vidix_register_driver (VDXDriver *drv)
{
  VDXDriver **d;

  d = &first_driver;
  while (*d != NULL)
    d = &(*d)->next;
  *d = drv;
  drv->next = NULL;
}

void vidix_register_all_drivers (void)
{
#ifdef CONFIG_VIDIX_DRV_CYBERBLADE
  vidix_register_driver (&cyberblade_drv);
#endif
#ifdef CONFIG_VIDIX_DRV_IVTV
  vidix_register_driver (&ivtv_drv);
#endif
#ifdef CONFIG_VIDIX_DRV_MACH64
  vidix_register_driver (&mach64_drv);
#endif
#ifdef CONFIG_VIDIX_DRV_MGA
  vidix_register_driver (&mga_drv);
#endif
#ifdef CONFIG_VIDIX_DRV_MGA_CRTC2
  vidix_register_driver (&mga_crtc2_drv);
#endif
#ifdef CONFIG_VIDIX_DRV_NVIDIA
  vidix_register_driver (&nvidia_drv);
#endif
#ifdef CONFIG_VIDIX_DRV_PM2
  vidix_register_driver (&pm2_drv);
#endif
#ifdef CONFIG_VIDIX_DRV_PM3
  vidix_register_driver (&pm3_drv);
#endif
#ifdef CONFIG_VIDIX_DRV_RADEON
  vidix_register_driver (&radeon_drv);
#endif
#ifdef CONFIG_VIDIX_DRV_RAGE128
  vidix_register_driver (&rage128_drv);
#endif
#ifdef CONFIG_VIDIX_DRV_S3
  vidix_register_driver (&s3_drv);
#endif
#ifdef CONFIG_VIDIX_DRV_SH_VEU
  vidix_register_driver (&sh_veu_drv);
#endif
#ifdef CONFIG_VIDIX_DRV_SIS
  vidix_register_driver (&sis_drv);
#endif
#ifdef CONFIG_VIDIX_DRV_UNICHROME
  vidix_register_driver (&unichrome_drv);
#endif
}

static int vidix_probe_driver (VDXContext *ctx, VDXDriver *drv,
                               unsigned int cap, int verbose)
{
  vidix_capability_t vid_cap;

  if (verbose)
    printf ("vidixlib: PROBING: %s\n", drv->name);

  if (!drv->probe || drv->probe (verbose, PROBE_NORMAL) != 0)
    return 0;

  if (!drv->get_caps || drv->get_caps (&vid_cap) != 0)
    return 0;

  if ((vid_cap.type & cap) != cap)
  {
    if (verbose)
      printf ("vidixlib: Found %s but has no required capability\n",
              drv->name);
     return 0;
  }

  if (verbose)
    printf ("vidixlib: %s probed o'k\n", drv->name);

  ctx->drv = drv;
  return 1;
}

static void vidix_list_drivers (void)
{
  VDXDriver *drv;

  printf ("Available VIDIX drivers:\n");

  drv = first_driver;
  while (drv)
  {
    vidix_capability_t cap;
    drv->get_caps (&cap);
    printf (" * %s - %s\n", drv->name, cap.name);
    drv = drv->next;
  }
}

int vidix_find_driver (VDXContext *ctx, const char *name,
                       unsigned int cap, int verbose)
{
  VDXDriver *drv;

  if (name && !strcmp (name, "help"))
  {
    vidix_list_drivers ();
    ctx->drv = NULL;
    return 0;
  }

  drv = first_driver;
  while (drv)
  {
    if (name) /* forced driver */
    {
      if (!strcmp (drv->name, name))
      {
        if (vidix_probe_driver (ctx, drv, cap, verbose))
          return 1;
        else
        {
          ctx->drv = NULL;
          return 0;
        }
      }
    }
    else /* auto-probe */
    {
      if (vidix_probe_driver (ctx, drv, cap, verbose))
        return 1;
    }
    drv = drv->next;
  }

  if (verbose)
    printf ("vidixlib: No suitable driver can be found.\n");
  ctx->drv = NULL;
  return 0;
}
