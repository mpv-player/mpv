/* Stuff for correct aspect scaling. */
#include "aspect.h"

float monitor_aspect=4.0/3.0;

/* aspect is called with the source resolution and the
 * resolution, that the scaled image should fit into
 */

rect_t aspect(int srcw, int srch, int fitinw, int fitinh){
  rect_t r,z;
  r.w=fitinw;
  r.x=0;
  r.h=(int)(((float)fitinw / (float)srcw * (float)srch)
            * ((float)fitinh/((float)fitinw/monitor_aspect)));
  r.h+=r.h%2; // round
  r.y=(fitinh-r.h)/2;
  z=r;
  //printf("aspect rez x: %d y: %d  wh: %dx%d\n",r.x,r.y,r.w,r.h);
  if(r.h>fitinh || r.h<srch){
    r.h=fitinh;
    r.y=0;
    r.w=(int)(((float)fitinh / (float)srch * (float)srcw)
              * ((float)fitinw/((float)fitinh/(1/monitor_aspect))));
    r.w+=r.w%2; // round
    r.x=(fitinw-r.w)/2;
  }
  if(r.w>fitinw) r=z;
  //printf("aspect ret x: %d y: %d  wh: %dx%d\n",r.x,r.y,r.w,r.h);
  return r;
}

