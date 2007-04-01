
#ifndef X11_COMMON_H
#define X11_COMMON_H

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#ifdef X11_FULLSCREEN

#define vo_wm_LAYER 1
#define vo_wm_FULLSCREEN 2
#define vo_wm_STAYS_ON_TOP 4
#define vo_wm_ABOVE 8
#define vo_wm_BELOW 16
#define vo_wm_NETWM (vo_wm_FULLSCREEN | vo_wm_STAYS_ON_TOP | vo_wm_ABOVE | vo_wm_BELOW)

/* EWMH state actions, see
	 http://freedesktop.org/Standards/wm-spec/index.html#id2768769 */
#define _NET_WM_STATE_REMOVE        0    /* remove/unset property */
#define _NET_WM_STATE_ADD           1    /* add/set property */
#define _NET_WM_STATE_TOGGLE        2    /* toggle property  */

extern int metacity_hack;

extern int vo_fs_layer;
extern int vo_wm_type;
extern int vo_fs_type;
extern char** vo_fstype_list;

extern char *mDisplayName;
extern Display *mDisplay;
extern Window mRootWin;
extern int mScreen;
extern int mLocalDisplay;

extern int vo_mouse_autohide;

extern int vo_init( void );
extern void vo_uninit( void );
extern void vo_hidecursor ( Display* , Window );
extern void vo_showcursor( Display *disp, Window win );
extern void vo_x11_decoration( Display * vo_Display,Window w,int d );
extern void vo_x11_classhint( Display * display,Window window,char *name );
extern void vo_x11_nofs_sizepos(int x, int y, int width, int height);
extern void vo_x11_sizehint( int x, int y, int width, int height, int max );
extern int vo_x11_check_events(Display *mydisplay);
extern void vo_x11_selectinput_witherr(Display *display, Window w, long event_mask);
extern void vo_x11_fullscreen( void );
extern void vo_x11_setlayer( Display * mDisplay,Window vo_window,int layer );
extern void vo_x11_uninit(void);
extern Colormap vo_x11_create_colormap(XVisualInfo *vinfo);
extern uint32_t vo_x11_set_equalizer(char *name, int value);
extern uint32_t vo_x11_get_equalizer(char *name, int *value);
extern void fstype_help(void);
extern Window vo_x11_create_smooth_window( Display *mDisplay, Window mRoot,
	Visual *vis, int x, int y, unsigned int width, unsigned int height,
	int depth, Colormap col_map);
extern void vo_x11_clearwindow_part(Display *mDisplay, Window vo_window,
	int img_width, int img_height, int use_fs);
extern void vo_x11_clearwindow( Display *mDisplay, Window vo_window );
extern void vo_x11_ontop(void);
extern void vo_x11_ewmh_fullscreen( int action );

#endif

extern Window     vo_window;
extern GC         vo_gc;
extern XSizeHints vo_hint;

#ifdef HAVE_XV
//XvPortID xv_port;
extern unsigned int xv_port;

extern int vo_xv_set_eq(uint32_t xv_port, char * name, int value);
extern int vo_xv_get_eq(uint32_t xv_port, char * name, int *value);

extern int vo_xv_enable_vsync(void);

extern void vo_xv_get_max_img_dim( uint32_t * width, uint32_t * height );

/*** colorkey handling ***/
typedef struct xv_ck_info_s
{
  int method; ///< CK_METHOD_* constants
  int source; ///< CK_SRC_* constants
} xv_ck_info_t;

#define CK_METHOD_NONE       0 ///< no colorkey drawing
#define CK_METHOD_BACKGROUND 1 ///< set colorkey as window background
#define CK_METHOD_AUTOPAINT  2 ///< let xv draw the colorkey
#define CK_METHOD_MANUALFILL 3 ///< manually draw the colorkey
#define CK_SRC_USE           0 ///< use specified / default colorkey
#define CK_SRC_SET           1 ///< use and set specified / default colorkey
#define CK_SRC_CUR           2 ///< use current colorkey ( get it from xv )

extern xv_ck_info_t xv_ck_info;
extern unsigned long xv_colorkey;

extern int vo_xv_init_colorkey(void);
extern void vo_xv_draw_colorkey(int32_t x, int32_t y, int32_t w, int32_t h);
extern void xv_setup_colorkeyhandling(char const * ck_method_str, char const * ck_str);

/*** test functions for common suboptions ***/
int xv_test_ck( void * arg );
int xv_test_ckm( void * arg );
#endif

#ifdef HAVE_NEW_GUI
 extern void vo_setwindow( Window w,GC g );
 extern void vo_x11_putkey(int key);
#endif

void saver_off( Display * );
void saver_on( Display * );

#ifdef HAVE_XF86VM
void vo_vm_switch(uint32_t, uint32_t, int*, int*);
void vo_vm_close(Display*);
#endif

void update_xinerama_info(void);

int vo_find_depth_from_visuals(Display *dpy, int screen, Visual **visual_return);

#endif

