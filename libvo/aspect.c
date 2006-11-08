/* Stuff for correct aspect scaling. */
#include "aspect.h"
#include "geometry.h"
//#ifndef ASPECT_TEST
#include "mp_msg.h"
#include "help_mp.h"
//#endif

//#define ASPECT_DEBUG

#if defined(ASPECT_DEBUG) || defined(ASPECT_TEST)
#include <stdio.h>
#endif

int vo_panscan_x = 0;
int vo_panscan_y = 0;
float vo_panscan_amount = 0;
float vo_panscanrange = 1.0;

#include "video_out.h"

float monitor_aspect=4.0/3.0;
float monitor_pixel_aspect=0;
extern float movie_aspect;

static struct {
  int orgw; // real width
  int orgh; // real height
  int prew; // prescaled width
  int preh; // prescaled height
  int scrw; // horizontal resolution
  int scrh; // vertical resolution
  float asp;
} aspdat;

void aspect_save_orig(int orgw, int orgh){
#ifdef ASPECT_DEBUG
  printf("aspect_save_orig %dx%d \n",orgw,orgh);
#endif
  aspdat.orgw = orgw;
  aspdat.orgh = orgh;
}

void aspect_save_prescale(int prew, int preh){
#ifdef ASPECT_DEBUG
  printf("aspect_save_prescale %dx%d \n",prew,preh);
#endif
  aspdat.prew = prew;
  aspdat.preh = preh;
}

void aspect_save_screenres(int scrw, int scrh){
#ifdef ASPECT_DEBUG
  printf("aspect_save_screenres %dx%d \n",scrw,scrh);
#endif
  aspdat.scrw = scrw;
  aspdat.scrh = scrh;
  if (monitor_pixel_aspect)
    monitor_aspect = monitor_pixel_aspect * scrw / scrh;
}

/* aspect is called with the source resolution and the
 * resolution, that the scaled image should fit into
 */

void aspect(int *srcw, int *srch, int zoom){
  int tmpw;

  if( !zoom && geometry_wh_changed ) {
#ifdef ASPECT_DEBUG
    printf("aspect(0) no aspect forced!\n");
#endif
    return; // the user doesn't want to fix aspect
  }

#ifdef ASPECT_DEBUG
  printf("aspect(0) fitin: %dx%d zoom: %d screenaspect: %.2f\n",aspdat.scrw,aspdat.scrh,
      zoom,monitor_aspect);
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
      mp_msg(MSGT_VO,MSGL_WARN,MSGTR_LIBVO_ASPECT_NoSuitableNewResFound);
#else
      mp_msg(MSGT_VO,MSGL_WARN,MSGTR_LIBVO_ASPECT_NoNewSizeFoundThatFitsIntoRes);
#endif
    }
  }
  aspdat.asp=*srcw / (float)*srch;
#ifdef ASPECT_DEBUG
  printf("aspect(3) wh: %dx%d (org: %dx%d)\n",*srcw,*srch,aspdat.prew,aspdat.preh);
#endif
}

void panscan_init( void )
{
 vo_panscan_x=0;
 vo_panscan_y=0;
 vo_panscan_amount=0.0f;
}

void panscan_calc( void )
{
 int fwidth,fheight;
 int vo_panscan_area;

 if (vo_panscanrange > 0) {
 aspect(&fwidth,&fheight,A_ZOOM);
 vo_panscan_area = (aspdat.scrh-fheight);
   vo_panscan_area *= vo_panscanrange;
 } else
   vo_panscan_area = -vo_panscanrange * aspdat.scrh;

 vo_panscan_amount = vo_fs ? vo_panscan : 0;
 vo_panscan_x = vo_panscan_area * vo_panscan_amount * aspdat.asp;
 vo_panscan_y = vo_panscan_area * vo_panscan_amount;
}

