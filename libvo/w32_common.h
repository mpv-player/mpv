extern int vo_depthonscreen;
extern int vo_screenwidth;
extern int vo_screenheight;
extern uint32_t o_dwidth;
extern uint32_t o_dheight;
extern HWND vo_window;
extern HDC vo_hdc;
extern int vo_fs;
extern int vo_vm;
extern int vo_ontop;

extern int vo_init(void);
extern void vo_w32_uninit(void);
extern void vo_w32_fullscreen(void);
extern int vo_w32_check_events(void);
extern int createRenderingContext(void);
extern void destroyRenderingContext(void);
