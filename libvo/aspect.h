#ifndef __ASPECT_H
#define __ASPECT_H
/* Stuff for correct aspect scaling. */

typedef struct {
  int x; /* x,y starting coordinate */
  int y; /* of upper left corner    */
  int w; /* width  */
  int h; /* height */
} rect_t;

rect_t aspect(int srcw, int srch, int fitinw, int fitinh);

#endif

