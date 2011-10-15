#ifndef MPLAYER_COCOA_COMMON_H
#define MPLAYER_COCOA_COMMON_H

#include "gl_common.h"

int vo_cocoa_init(struct vo *vo);
void vo_cocoa_uninit(struct vo *vo);

void vo_cocoa_update_xinerama_info(struct vo *vo);

int vo_cocoa_change_attributes(struct MPGLContext *ctx);
int vo_cocoa_create_window(struct MPGLContext *ctx, uint32_t d_width,
                           uint32_t d_height, uint32_t flags,
                           const char *title);

void vo_cocoa_swap_buffers(void);
int vo_cocoa_check_events(struct vo *vo);
void vo_cocoa_fullscreen(struct vo *vo);

#endif /* MPLAYER_COCOA_COMMON_H */
