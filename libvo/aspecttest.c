/* testapp for aspect.[ch] by Atmos
 * gcc aspecttest.c aspect.c -o aspecttest -DASPECT_TEST [-DASPECT_DEBUG]
 */

#include <stdio.h>

#include "aspect.h"

/* default zoom state 0 off, 1 on */
#define DEF_ZOOM 1

extern float monitor_aspect;

int main(int argc, char *argv[]) {
  int w,h,z=DEF_ZOOM;
  //printf("argc: %d\n",argc);
  switch(argc) {
    case 10:
      z = atoi(argv[9]);
    case 9:
      monitor_aspect = (float)atoi(argv[7])/(float)atoi(argv[8]);
    case 7:
      aspect_save_prescale(atoi(argv[5]),atoi(argv[6]));
      printf("prescale size:  %sx%s\n",argv[5],argv[6]);
    case 5:
      aspect_save_screenres(atoi(argv[1]),atoi(argv[2]));
      printf("screenres:      %sx%s\n",argv[1],argv[2]);
      aspect_save_orig(atoi(argv[3]),atoi(argv[4]));
      printf("original size:  %sx%s\n",argv[3],argv[4]);
      w=atoi(argv[3]); h=atoi(argv[4]);
    break;
    default:
      printf("USAGE: %s <screenw> <screenh> <origw> <origh>\n[<prescalew> "
             "<prescaleh>] [<screenaspectw> <screenaspecth>] [<zoom 0/1>]\n",
        argv[0]);
      return 1;
  }
  printf("monitor_aspect: %f\n",monitor_aspect);
  aspect(&w,&h,z); 
  printf("new size:       %dx%d\n",w,h);
  return 0;
}

