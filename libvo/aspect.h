#ifndef MPLAYER_ASPECT_H
#define MPLAYER_ASPECT_H
/* Stuff for correct aspect scaling. */

struct vo;
extern void panscan_init(struct vo *vo);
extern void panscan_calc(struct vo *vo);

void aspect_save_orig(struct vo *vo, int orgw, int orgh);

void aspect_save_prescale(struct vo *vo, int prew, int preh);

void aspect_save_screenres(struct vo *vo, int scrw, int scrh);

#define A_ZOOM 1
#define A_NOZOOM 0

void aspect(struct vo *vo, int *srcw, int *srch, int zoom);
void aspect_fit(struct vo *vo, int *srcw, int *srch, int fitw, int fith);


#ifdef IS_OLD_VO
#define vo_panscan_x global_vo->panscan_x
#define vo_panscan_y global_vo->panscan_y
#define vo_panscan_amount global_vo->panscan_amount
#define monitor_aspect global_vo->monitor_aspect

#define panscan_init() panscan_init(global_vo)
#define panscan_calc() panscan_calc(global_vo)
#define aspect_save_orig(...) aspect_save_orig(global_vo, __VA_ARGS__)
#define aspect_save_prescale(...) aspect_save_prescale(global_vo, __VA_ARGS__)
#define aspect_save_screenres(...) aspect_save_screenres(global_vo, __VA_ARGS__)
#define aspect(...) aspect(global_vo, __VA_ARGS__)
#endif

#endif /* MPLAYER_ASPECT_H */
