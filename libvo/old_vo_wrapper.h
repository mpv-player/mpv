#ifndef MPLAYER_OLD_VO_WRAPPER_H
#define MPLAYER_OLD_VO_WRAPPER_H

#include <stdint.h>
#include "video_out.h"

extern struct vo *global_vo;
extern struct osd_state *global_osd;

int old_vo_preinit(struct vo *vo, const char *);
int old_vo_config(struct vo *vo, uint32_t width, uint32_t height,
                  uint32_t d_width, uint32_t d_height,
                  uint32_t flags, uint32_t format);
int old_vo_control(struct vo *vo, uint32_t request, void *data);
int old_vo_draw_frame(struct vo *vo, uint8_t *src[]);
int old_vo_draw_slice(struct vo *vo, uint8_t *src[], int stride[],
                      int w, int h, int x, int y);
void old_vo_draw_osd(struct vo *vo, struct osd_state *osd);
void old_vo_flip_page(struct vo *vo);
void old_vo_check_events(struct vo *vo);
void old_vo_uninit(struct vo *vo);

void vo_draw_text(int dxs,int dys,void (*draw_alpha)(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride));
void vo_draw_text_ext(int dxs, int dys, int left_border, int top_border,
                      int right_border, int bottom_border, int orig_w, int orig_h,
                      void (*draw_alpha)(int x0, int y0, int w,int h,unsigned char* src, unsigned char *srca, int stride));
int vo_update_osd(int dxs, int dys);

#endif
