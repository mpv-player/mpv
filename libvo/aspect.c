/* Stuff for correct aspect scaling. */
#include "aspect.h"
#ifndef ASPECT_TEST
#include "../mp_msg.h"
#endif

//#define ASPECT_DEBUG

#if defined(ASPECT_DEBUG) || defined(ASPECT_TEST)
#include <stdio.h>
#endif

float monitor_aspect=4.0/3.0;

static struct {
  int orgw; // real width
  int orgh; // real height
  int prew; // prescaled width
  int preh; // prescaled height
  int scrw; // horizontal resolution
  int scrh; // vertical resolution
} aspdat;

void aspect_save_orig(int orgw, int orgh){
  aspdat.orgw = orgw;
  aspdat.orgh = orgh;
}

void aspect_save_prescale(int prew, int preh){
  aspdat.prew = prew;
  aspdat.preh = preh;
}

void aspect_save_screenres(int scrw, int scrh){
  aspdat.scrw = scrw;
  aspdat.scrh = scrh;
}

/* aspect is called with the source resolution and the
 * resolution, that the scaled image should fit into
 */

void aspect(int *srcw, int *srch, int zoom){
  int tmpw;

#ifdef ASPECT_DEBUG
  printf("aspect(0) fitin: %dx%d zoom: %d \n",aspdat.scrw,aspdat.scrh,zoom);
  printf("aspect(1) wh: %dx%d (org: %dx%d)\n",*srcw,*srch,aspdat.prew,aspdat.preh);
#endif
  if(zoom){
    *srcw = aspdat.scrw;
    *srch = (int)(((float)aspdat.scrw / (float)aspdat.prew * (float)aspdat.preh)
               * ((float)aspdat.scrh / ((float)aspdat.scrw / monitor_aspect)));
  }else{
    *srcw = aspdat.prew;
    *srch = (int)((float)aspdat.preh
               * ((float)aspdat.scrh / ((float)aspdat.scrw / monitor_aspect)));
  }
  *srch+= *srch%2; // round
#ifdef ASPECT_DEBUG
  printf("aspect(2) wh: %dx%d (org: %dx%d)\n",*srcw,*srch,aspdat.prew,aspdat.preh);
#endif
  if(*srch>aspdat.scrh || *srch<aspdat.orgh){
    if(zoom)
      tmpw = (int)(((float)aspdat.scrh / (float)aspdat.preh * (float)aspdat.prew)
                * ((float)aspdat.scrw / ((float)aspdat.scrh / (1.0/monitor_aspect))));
    else
      tmpw = (int)((float)aspdat.prew
                * ((float)aspdat.scrw / ((float)aspdat.scrh / (1.0/monitor_aspect))));
    tmpw+= tmpw%2; // round
    if(tmpw<=aspdat.scrw /*&& tmpw>=aspdat.orgw*/){
      *srch = zoom?aspdat.scrh:aspdat.preh;
      *srcw = tmpw;
    }else{
#ifndef ASPECT_TEST
      mp_msg(MSGT_VO,MSGL_WARN,"aspect: Warning: no suitable new res found!\n");
#else
      printf("error: no new size found that fits into res!\n");
#endif
    }
  }
#ifdef ASPECT_DEBUG
  printf("aspect(3) wh: %dx%d (org: %dx%d)\n",*srcw,*srch,aspdat.prew,aspdat.preh);
#endif
}

