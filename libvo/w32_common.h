extern int vo_depthonscreen;
extern int vo_screenwidth;
extern int vo_screenheight;
extern uint32_t o_dwidth;
extern uint32_t o_dheight;
extern HWND vo_w32_window;
extern HDC vo_hdc;
extern int vo_fs;
extern int vo_vm;
extern int vo_ontop;

extern int vo_w32_init(void);
extern void vo_w32_uninit(void);
extern void vo_w32_ontop(void);
extern void vo_w32_fullscreen(void);
extern int vo_w32_check_events(void);
extern int vo_w32_config(uint32_t, uint32_t, uint32_t);
extern void destroyRenderingContext(void);
extern void w32_update_xinerama_info(void);
