
#ifdef X11_FULLSCREEN

extern int vo_depthonscreen;
extern int vo_screenwidth;
extern int vo_screenheight;
extern int vo_dwidth;
extern int vo_dheight;

int vo_init( void );
int vo_hidecursor ( Display* , Window );
void vo_x11_decoration( Display * vo_Display,Window w,int d );
int vo_x11_check_events(Display *mydisplay);
#endif

void saver_off( Display * );
void saver_on( Display * );
