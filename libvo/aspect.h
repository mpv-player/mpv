#ifndef __ASPECT_H
#define __ASPECT_H
/* Stuff for correct aspect scaling. */

extern int vo_panscan_x;
extern int vo_panscan_y;
extern float vo_panscan_amount;

extern void panscan_init( void );
extern void panscan_calc( void );

void aspect_save_orig(int orgw, int orgh);

void aspect_save_prescale(int prew, int preh);

void aspect_save_screenres(int scrw, int scrh);

#define A_ZOOM 1
#define A_NOZOOM 0

void aspect(int *srcw, int *srch, int zoom);

#endif

