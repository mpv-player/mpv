
#ifdef X11_FULLSCREEN

extern int vo_depthonscreen;
extern int vo_screenwidth;
extern int vo_screenheight;
extern int vo_dwidth;
extern int vo_dheight;

extern char *mDisplayName;
extern Display *mDisplay;
extern Window *mRootWin;
extern int mScreen;
extern int mLocalDisplay;

int vo_init( void );
int vo_hidecursor ( Display* , Window );
void vo_x11_decoration( Display * vo_Display,Window w,int d );
void vo_x11_classhint( Display * display,Window window,char *name );
int vo_x11_check_events(Display *mydisplay);
#endif

#ifdef HAVE_NEW_GUI
 extern Window    vo_window;
 extern GC        vo_gc;
 extern void vo_setwindow( Window w,GC g );
 extern void vo_setwindowsize( int w,int h );
 extern int       vo_xeventhandling;
 extern int       vo_expose;
 extern int       vo_resize;
#endif
#ifdef HAVE_GUI
 extern Display * vo_display;
#endif

void saver_off( Display * );
void saver_on( Display * );
