#ifndef MPLAYER_VOBSUB_H
#define MPLAYER_VOBSUB_H

extern void *vobsub_open(const char *subname);
extern void vobsub_process(void *vob, float pts);
extern void vobsub_reset(void *vob);
extern void vobsub_draw(void *vob, int dxs, int dys, void (*draw_alpha)(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride));

#endif MPLAYER_VOBSUB_H
