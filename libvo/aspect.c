/* Stuff for correct aspect scaling. */
//#define ASPECT_DEBUG

#ifdef ASPECT_DEBUG
#include <stdio.h>
#endif

float monitor_aspect=4.0/3.0;

/* aspect is called with the source resolution and the
 * resolution, that the scaled image should fit into
 */

void aspect(int *srcw, int *srch, int fitinw, int fitinh){
  int srcwcp, srchcp, tmp;
  srcwcp=*srcw; srchcp=*srch;
  srcwcp=fitinw;
#ifdef ASPECT_DEBUG
  printf("aspect(0) fitin: %dx%d \n",fitinw,fitinh);
  printf("aspect(1) wh: %dx%d (org: %dx%d)\n",srcwcp,srchcp,*srcw,*srch);
#endif
  srchcp=(int)(((float)fitinw / (float)*srcw * (float)*srch)
            * ((float)fitinh / ((float)fitinw / monitor_aspect)));
  srchcp+=srchcp%2; // round
#ifdef ASPECT_DEBUG
  printf("aspect(2) wh: %dx%d (org: %dx%d)\n",srcwcp,srchcp,*srcw,*srch);
#endif
  if(srchcp>fitinh || srchcp<*srch){
    tmp=(int)(((float)fitinh / (float)*srch * (float)*srcw)
           * ((float)fitinw / ((float)fitinh / (1/monitor_aspect))));
    if(srcwcp>fitinw){
      srchcp=fitinh;
      srcwcp=tmp;
      srcwcp+=srcwcp%2; // round
    }
  }
#ifdef ASPECT_DEBUG
  printf("aspect(3) wh: %dx%d (org: %dx%d)\n",srcwcp,srchcp,*srcw,*srch);
#endif
  *srcw=srcwcp; *srch=srchcp;
#ifdef ASPECT_DEBUG
  printf("aspect(4) wh: %dx%d (org: %dx%d)\n",srcwcp,srchcp,*srcw,*srch);
#endif
}

