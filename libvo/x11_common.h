
#ifndef X11_COMMON_H
#define X11_COMMON_H

#ifdef X11_FULLSCREEN

#include <X11/Xlib.h>
#include <X11/Xutil.h>

extern int vo_depthonscreen;
extern int vo_screenwidth;
extern int vo_screenheight;
extern int vo_dwidth;
extern int vo_dheight;
extern int vo_fs;
extern int vo_wm_type;

extern char *mDisplayName;
extern Display *mDisplay;
extern Window mRootWin;
extern int mScreen;
extern int mLocalDisplay;
extern int WinID;

extern int vo_mouse_timer_const;
extern int vo_mouse_autohide;

extern int vo_init( void );
extern void vo_uninit( void );
extern void vo_hidecursor ( Display* , Window );
extern void vo_showcursor( Display *disp, Window win );
extern void vo_x11_decoration( Display * vo_Display,Window w,int d );
extern void vo_x11_classhint( Display * display,Window window,char *name );
extern void vo_x11_sizehint( int x, int y, int width, int height, int max );
extern int vo_x11_check_events(Display *mydisplay);
extern void vo_x11_selectinput_witherr(Display *display, Window w, long event_mask);
extern void vo_x11_fullscreen( void );
extern void vo_x11_uninit();

#endif

extern Window     vo_window;
extern GC         vo_gc;
extern XSizeHints vo_hint;

#ifdef HAVE_NEW_GUI
 extern void vo_setwindow( Window w,GC g );
 extern void vo_x11_putkey(int key);
#endif

void saver_off( Display * );
void saver_on( Display * );

#ifdef HAVE_XINERAMA
void vo_x11_xinerama_move(Display *dsp, Window w);
#endif

#ifdef HAVE_XF86VM
void vo_vm_switch(uint32_t, uint32_t, int*, int*);
void vo_vm_close(Display*);
#endif

#endif
