/* Stuff for correct aspect scaling. */

float monitor_aspect=4.0/3.0;

/* aspect is called with the source resolution and the
 * resolution, that the scaled image should fit into
 */

void aspect(int *srcw, int *srch, int fitinw, int fitinh){
  int srcwcp, srchcp;
  srcwcp=*srcw; srchcp=*srch;
  srcwcp=fitinw;
  srchcp=(int)(((float)fitinw / (float)*srcw * (float)*srch)
            * ((float)fitinh/((float)fitinw/monitor_aspect)));
  srchcp+=srchcp%2; // round
  //printf("aspect rez wh: %dx%d (org: %dx%d)\n",srcwcp,srchcp,*srcw,*srch);
  if(srchcp>fitinh || srchcp<*srch){
    srchcp=fitinh;
    srcwcp=(int)(((float)fitinh / (float)*srch * (float)*srcw)
              * ((float)fitinw/((float)fitinh/(1/monitor_aspect))));
    srcwcp+=srcwcp%2; // round
  }
  //printf("aspect ret wh: %dx%d (org: %dx%d)\n",srcwcp,srchcp,*srcw,*srch);
  *srcw=srcwcp; *srch=srchcp;
}

