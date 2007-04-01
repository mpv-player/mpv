#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "vidixlib.h"
#include "../config.h"
#include "../libavutil/common.h"
#include "../mpbswap.h"

VDXDriver *first_driver = NULL;

extern VDXDriver cyberblade_drv;
extern VDXDriver mach64_drv;
extern VDXDriver mga_drv;
extern VDXDriver mga_crtc2_drv;
extern VDXDriver nvidia_drv;
extern VDXDriver pm3_drv;
extern VDXDriver radeon_drv;
extern VDXDriver rage128_drv;
extern VDXDriver savage_drv;
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
  vidix_register_driver (&cyberblade_drv);
  vidix_register_driver (&mach64_drv);
  vidix_register_driver (&mga_drv);
  vidix_register_driver (&mga_crtc2_drv);
  vidix_register_driver (&nvidia_drv);
  vidix_register_driver (&pm3_drv);
  vidix_register_driver (&radeon_drv);
  vidix_register_driver (&rage128_drv);
  vidix_register_driver (&savage_drv);
  vidix_register_driver (&sis_drv);
  vidix_register_driver (&unichrome_drv);
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
