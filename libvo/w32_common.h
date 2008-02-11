#ifndef W32COMMON_H
#define W32COMMON_H

extern HWND vo_w32_window;
extern int vo_vm;

extern int vo_w32_init(void);
extern void vo_w32_uninit(void);
extern void vo_w32_ontop(void);
extern void vo_w32_border(void);
extern void vo_w32_fullscreen(void);
extern int vo_w32_check_events(void);
extern int vo_w32_config(uint32_t, uint32_t, uint32_t);
extern void destroyRenderingContext(void);
extern void w32_update_xinerama_info(void);

#endif /* W32COMMON_H */
