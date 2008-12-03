
#ifndef MPLAYER_OSD_H
#define MPLAYER_OSD_H

// Generic alpha renderers for all YUV modes and RGB depths.
// These are "reference implementations", should be optimized later (MMX, etc)

void vo_draw_alpha_init(void); // build tables

void vo_draw_alpha_yv12(int w,  int h, unsigned char* src, unsigned char *srca, int srcstride, unsigned char* dstbase, int dststride);
void vo_draw_alpha_yuy2(int w,  int h, unsigned char* src, unsigned char *srca, int srcstride, unsigned char* dstbase, int dststride);
void vo_draw_alpha_uyvy(int w,  int h, unsigned char* src, unsigned char *srca, int srcstride, unsigned char* dstbase, int dststride);
void vo_draw_alpha_rgb24(int w, int h, unsigned char* src, unsigned char *srca, int srcstride, unsigned char* dstbase, int dststride);
void vo_draw_alpha_rgb32(int w, int h, unsigned char* src, unsigned char *srca, int srcstride, unsigned char* dstbase, int dststride);
void vo_draw_alpha_rgb15(int w, int h, unsigned char* src, unsigned char *srca, int srcstride, unsigned char* dstbase, int dststride);
void vo_draw_alpha_rgb16(int w, int h, unsigned char* src, unsigned char *srca, int srcstride, unsigned char* dstbase, int dststride);

#endif /* MPLAYER_OSD_H */
