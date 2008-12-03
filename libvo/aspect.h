#ifndef MPLAYER_ASPECT_H
#define MPLAYER_ASPECT_H
/* Stuff for correct aspect scaling. */

extern int vo_panscan_x;
extern int vo_panscan_y;
extern float vo_panscan_amount;

void panscan_init(void);
void panscan_calc(void);

void aspect_save_orig(int orgw, int orgh);

void aspect_save_prescale(int prew, int preh);

void aspect_save_screenres(int scrw, int scrh);

#define A_ZOOM 1
#define A_NOZOOM 0

void aspect(int *srcw, int *srch, int zoom);
void aspect_fit(int *srcw, int *srch, int fitw, int fith);

#endif /* MPLAYER_ASPECT_H */
