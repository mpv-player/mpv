/*
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef MPLAYER_X11_COMMON_H
#define MPLAYER_X11_COMMON_H

#include <stdint.h>
#include <stdbool.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "config.h"

struct vo;

struct vo_x11_state {
    Display *display;
    Window window;
    Window rootwin;
    int screen;
    int display_is_local;
    int depthonscreen;

    XIM xim;
    XIC xic;

    GC vo_gc;

    struct xv_ck_info_s {
        int method; ///< CK_METHOD_* constants
        int source; ///< CK_SRC_* constants
    } xv_ck_info;
    unsigned long xv_colorkey;
    unsigned int xv_port;

    int wm_type;
    int fs_type;
    int window_state;
    int fs_flip;

    GC f_gc;
    XSizeHints vo_hint;
    unsigned int mouse_timer;
    int mouse_waiting_hide;
    int orig_layer;
    int old_gravity;
    int vo_old_x;
    int vo_old_y;
    int vo_old_width;
    int vo_old_height;

    /* Keep track of original video width/height to determine when to
     * resize window when reconfiguring. Resize window when video size
     * changes, but don't force window size changes as long as video size
     * stays the same (even if that size is different from the current
     * window size after the user modified the latter). */
    int last_video_width;
    int last_video_height;
    /* Video size changed during fullscreen when we couldn't tell the new
     * size to the window manager. Must set window size when turning
     * fullscreen off. */
    bool size_changed_during_fs;

    unsigned int olddecor;
    unsigned int oldfuncs;
    XComposeStatus compose_status;

    Atom XA_NET_SUPPORTED;
    Atom XA_NET_WM_STATE;
    Atom XA_NET_WM_STATE_FULLSCREEN;
    Atom XA_NET_WM_STATE_ABOVE;
    Atom XA_NET_WM_STATE_STAYS_ON_TOP;
    Atom XA_NET_WM_STATE_BELOW;
    Atom XA_NET_WM_PID;
    Atom XA_NET_WM_NAME;
    Atom XA_NET_WM_ICON_NAME;
    Atom XA_WIN_PROTOCOLS;
    Atom XA_WIN_LAYER;
    Atom XA_WIN_HINTS;
    Atom XAWM_PROTOCOLS;
    Atom XAWM_DELETE_WINDOW;
    Atom XAUTF8_STRING;
    Atom XA_NET_WM_CM;
};

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

extern char** vo_fstype_list;

extern char *mDisplayName;

struct vo_x11_state *vo_x11_init_state(void);
int vo_init(struct vo *vo);
void vo_uninit(struct vo_x11_state *x11);
void vo_x11_decoration(struct vo *vo, int d );
void vo_x11_classhint(struct vo *vo, Window window, const char *name);
void vo_x11_sizehint(struct vo *vo, int x, int y, int width, int height, int max);
int vo_x11_check_events(struct vo *vo);
void vo_x11_selectinput_witherr(Display *display, Window w, long event_mask);
void vo_x11_fullscreen(struct vo *vo);
int vo_x11_update_geometry(struct vo *vo, bool update_pos);
void vo_x11_setlayer(struct vo *vo, Window vo_window, int layer);
void vo_x11_uninit(struct vo *vo);
Colormap vo_x11_create_colormap(struct vo *vo, XVisualInfo *vinfo);
uint32_t vo_x11_set_equalizer(struct vo *vo, const char *name, int value);
uint32_t vo_x11_get_equalizer(const char *name, int *value);
bool vo_x11_screen_is_composited(struct vo *vo);
void fstype_help(void);
void vo_x11_create_vo_window(struct vo *vo, XVisualInfo *vis,
        int x, int y, unsigned int width, unsigned int height, int flags,
	Colormap col_map, const char *classname);
void vo_x11_clearwindow_part(struct vo *vo, Window vo_window,
	int img_width, int img_height);
void vo_x11_clearwindow(struct vo *vo, Window vo_window);
void vo_x11_ontop(struct vo *vo);
void vo_x11_border(struct vo *vo);
void vo_x11_ewmh_fullscreen(struct vo_x11_state *x11, int action);


int vo_xv_set_eq(struct vo *vo, uint32_t xv_port, const char *name, int value);
int vo_xv_get_eq(struct vo *vo, uint32_t xv_port, const char *name, int *value);

int vo_xv_enable_vsync(struct vo *vo);

void vo_xv_get_max_img_dim(struct vo *vo, uint32_t * width, uint32_t * height);

/*** colorkey handling ***/

#define CK_METHOD_NONE       0 ///< no colorkey drawing
#define CK_METHOD_BACKGROUND 1 ///< set colorkey as window background
#define CK_METHOD_AUTOPAINT  2 ///< let xv draw the colorkey
#define CK_METHOD_MANUALFILL 3 ///< manually draw the colorkey
#define CK_SRC_USE           0 ///< use specified / default colorkey
#define CK_SRC_SET           1 ///< use and set specified / default colorkey
#define CK_SRC_CUR           2 ///< use current colorkey ( get it from xv )

int vo_xv_init_colorkey(struct vo *vo);
void vo_xv_draw_colorkey(struct vo *vo, int32_t x, int32_t y, int32_t w, int32_t h);
void xv_setup_colorkeyhandling(struct vo *vo, const char *ck_method_str, const char *ck_str);

/*** test functions for common suboptions ***/
int xv_test_ck( void * arg );
int xv_test_ckm( void * arg );

#ifdef CONFIG_XF86VM
void vo_vm_switch(struct vo *vo);
void vo_vm_close(struct vo *vo);
double vo_vm_get_fps(struct vo *vo);
#endif

void update_xinerama_info(struct vo *vo);

int vo_find_depth_from_visuals(Display *dpy, int screen, Visual **visual_return);
void xscreensaver_heartbeat(struct vo_x11_state *x11);

#endif /* MPLAYER_X11_COMMON_H */
