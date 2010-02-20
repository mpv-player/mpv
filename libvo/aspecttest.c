/*
 * test app for aspect.[ch] by Atmos
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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "aspect.h"

/* default zoom state 0 off, 1 on */
#define DEF_ZOOM 1

extern float monitor_aspect;
int vo_dheight;
int vo_dwidth;
int vo_fs;
float vo_panscan;
int64_t WinID = -1;

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
