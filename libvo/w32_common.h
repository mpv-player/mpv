#ifndef MPLAYER_W32_COMMON_H
#define MPLAYER_W32_COMMON_H

#include <stdint.h>
#include <windows.h>

extern HWND vo_w32_window;
extern int vo_vm;

int vo_w32_init(void);
void vo_w32_uninit(void);
void vo_w32_ontop(void);
void vo_w32_border(void);
void vo_w32_fullscreen(void);
int vo_w32_check_events(void);
int vo_w32_config(uint32_t, uint32_t, uint32_t);
void destroyRenderingContext(void);
void w32_update_xinerama_info(void);

#endif /* MPLAYER_W32_COMMON_H */
