#ifndef MPLAYER_VOBSUB_H
#define MPLAYER_VOBSUB_H

extern void *vobsub_open(const char *subname, const int force);
extern void vobsub_process(void *vob, float pts);
extern void vobsub_reset(void *vob);
extern void vobsub_draw(void *vob, int dxs, int dys, void (*draw_alpha)(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride));
extern int vobsub_parse_ifo(const char *const name, unsigned int *palette, unsigned int *width, unsigned int *height, int force);

#endif /* MPLAYER_VOBSUB_H */
