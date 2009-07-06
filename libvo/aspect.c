/*
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

/* Stuff for correct aspect scaling. */
#include "aspect.h"
#include "geometry.h"
#include "video_out.h"
//#ifndef ASPECT_TEST
#include "mp_msg.h"
#include "help_mp.h"
#include "options.h"
//#endif

//#define ASPECT_DEBUG

#if defined(ASPECT_DEBUG) || defined(ASPECT_TEST)
#include <stdio.h>
#endif

#include "video_out.h"

void aspect_save_orig(struct vo *vo, int orgw, int orgh)
{
#ifdef ASPECT_DEBUG
  printf("aspect_save_orig %dx%d \n",orgw,orgh);
#endif
    vo->aspdat.orgw = orgw;
    vo->aspdat.orgh = orgh;
}

void aspect_save_prescale(struct vo *vo, int prew, int preh)
{
#ifdef ASPECT_DEBUG
  printf("aspect_save_prescale %dx%d \n",prew,preh);
#endif
    vo->aspdat.prew = prew;
    vo->aspdat.preh = preh;
}

void aspect_save_screenres(struct vo *vo, int scrw, int scrh)
{
#ifdef ASPECT_DEBUG
  printf("aspect_save_screenres %dx%d \n",scrw,scrh);
#endif
    struct MPOpts *opts = vo->opts;
    vo->aspdat.scrw = scrw;
    vo->aspdat.scrh = scrh;
    if (opts->force_monitor_aspect)
        vo->monitor_aspect = opts->force_monitor_aspect;
    else
        vo->monitor_aspect = opts->monitor_pixel_aspect * scrw / scrh;
}

/* aspect is called with the source resolution and the
 * resolution, that the scaled image should fit into
 */

void aspect_fit(struct vo *vo, int *srcw, int *srch, int fitw, int fith)
{
    struct aspect_data *aspdat = &vo->aspdat;
  int tmpw;

#ifdef ASPECT_DEBUG
  printf("aspect(0) fitin: %dx%d screenaspect: %.2f\n",aspdat->scrw,aspdat->scrh,
      monitor_aspect);
  printf("aspect(1) wh: %dx%d (org: %dx%d)\n",*srcw,*srch,aspdat->prew,aspdat->preh);
#endif
    *srcw = fitw;
    *srch = (int)(((float)fitw / (float)aspdat->prew * (float)aspdat->preh)
               * ((float)aspdat->scrh / ((float)aspdat->scrw / vo->monitor_aspect)));
  *srch+= *srch%2; // round
#ifdef ASPECT_DEBUG
  printf("aspect(2) wh: %dx%d (org: %dx%d)\n",*srcw,*srch,aspdat->prew,aspdat->preh);
#endif
  if(*srch>aspdat->scrh || *srch<aspdat->orgh){
      tmpw = (int)(((float)fith / (float)aspdat->preh * (float)aspdat->prew)
                * ((float)aspdat->scrw / ((float)aspdat->scrh / (1.0/vo->monitor_aspect))));
    tmpw+= tmpw%2; // round
    if(tmpw<=aspdat->scrw /*&& tmpw>=aspdat->orgw*/){
      *srch = fith;
      *srcw = tmpw;
    }else{
#ifndef ASPECT_TEST
      mp_tmsg(MSGT_VO,MSGL_WARN,"[ASPECT] Warning: No suitable new res found!\n");
#else
      mp_tmsg(MSGT_VO,MSGL_WARN,"[ASPECT] Error: No new size found that fits into res!\n");
#endif
    }
  }
  aspdat->asp=*srcw / (float)*srch;
#ifdef ASPECT_DEBUG
  printf("aspect(3) wh: %dx%d (org: %dx%d)\n",*srcw,*srch,aspdat->prew,aspdat->preh);
#endif
}

void aspect(struct vo *vo, int *srcw, int *srch, int zoom){
  int fitw = zoom ? vo->aspdat.scrw : vo->aspdat.prew;
  int fith = zoom ? vo->aspdat.scrh : vo->aspdat.preh;
  if( !zoom && geometry_wh_changed ) {
#ifdef ASPECT_DEBUG
    printf("aspect(0) no aspect forced!\n");
#endif
    return; // the user doesn't want to fix aspect
  }
  aspect_fit(vo, srcw, srch, fitw, fith);
}

void panscan_init(struct vo *vo)
{
    vo->panscan_x = 0;
    vo->panscan_y = 0;
    vo->panscan_amount = 0.0f;
}

void panscan_calc(struct vo *vo)
{
 int fwidth,fheight;
 int vo_panscan_area;
    struct MPOpts *opts = vo->opts;

    if (opts->vo_panscanrange > 0) {
        aspect(vo, &fwidth, &fheight, A_ZOOM);
        vo_panscan_area = (vo->aspdat.scrh - fheight);
        if (!vo_panscan_area)
            vo_panscan_area = vo->aspdat.scrw - fwidth;
        vo_panscan_area *= opts->vo_panscanrange;
    } else
        vo_panscan_area = -opts->vo_panscanrange * vo->aspdat.scrh;

    vo->panscan_amount = vo_fs ? vo_panscan : 0;
    vo->panscan_x = vo_panscan_area * vo->panscan_amount * vo->aspdat.asp;
    vo->panscan_y = vo_panscan_area * vo->panscan_amount;
}

