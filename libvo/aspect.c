/* Stuff for correct aspect scaling. */

float monitor_aspect=4.0/3.0;

/* aspect is called with the source resolution and the
 * resolution, that the scaled image should fit into
 */

void aspect(int *srcw, int *srch, int fitinw, int fitinh){
  int srcwcp, srchcp;
  srcwcp=*srcw; srchcp=*srch;
  *srcw=fitinw;
  *srch=(int)(((float)fitinw / (float)srcwcp * (float)srchcp)
            * ((float)fitinh/((float)fitinw/monitor_aspect)));
  *srch+=*srch%2; // round
  //printf("aspect rez wh: %dx%d\n",*srcw,*srch);
  if(*srch>fitinh || *srch<srchcp){
    *srch=fitinh;
    *srcw=(int)(((float)fitinh / (float)srchcp * (float)srcwcp)
              * ((float)fitinw/((float)fitinh/(1/monitor_aspect))));
    *srcw+=*srcw%2; // round
  }
  //printf("aspect ret wh: %dx%d\n",*srcw,*srch);
}

