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

#include <stdio.h>
#include <limits.h>
#include <assert.h>
#include <windows.h>
#include <windowsx.h>

// To get "#define vo_ontop global_vo->opts->vo_ontop" etc
#include "old_vo_defines.h"
#include "input/keycodes.h"
#include "input/input.h"
#include "mp_msg.h"
#include "video_out.h"
#include "aspect.h"
#include "w32_common.h"
#include "mp_fifo.h"
#include "osdep/io.h"
#include "talloc.h"

#ifndef WM_XBUTTONDOWN
# define WM_XBUTTONDOWN    0x020B
# define WM_XBUTTONUP      0x020C
# define WM_XBUTTONDBLCLK  0x020D
#endif

#ifndef MONITOR_DEFAULTTOPRIMARY
#define MONITOR_DEFAULTTOPRIMARY 1
#endif

#define WIN_ID_TO_HWND(x) ((HWND)(uint32_t)(x))

static const char classname[] = "mplayer2";
int vo_vm = 0;

static int depthonscreen;
// last non-fullscreen extends (updated only on fullscreen or on initialization)
static int prev_width;
static int prev_height;
static int prev_x;
static int prev_y;

// whether the window position and size were intialized
static bool window_bounds_initialized;

static bool current_fs;

static int window_x;
static int window_y;

// video size
static uint32_t o_dwidth;
static uint32_t o_dheight;

static HINSTANCE hInstance;
#define vo_window vo_w32_window
HWND vo_window = 0;
/** HDC used when rendering to a device instead of window */
static HDC dev_hdc;
static int event_flags;
static int mon_cnt;

static HMONITOR (WINAPI* myMonitorFromWindow)(HWND, DWORD);
static BOOL (WINAPI* myGetMonitorInfo)(HMONITOR, LPMONITORINFO);
static BOOL (WINAPI* myEnumDisplayMonitors)(HDC, LPCRECT, MONITORENUMPROC, LPARAM);

static const struct mp_keymap vk_map[] = {
    // special keys
    {VK_ESCAPE, KEY_ESC}, {VK_BACK, KEY_BS}, {VK_TAB, KEY_TAB}, {VK_CONTROL, KEY_CTRL},

    // cursor keys
    {VK_LEFT, KEY_LEFT}, {VK_UP, KEY_UP}, {VK_RIGHT, KEY_RIGHT}, {VK_DOWN, KEY_DOWN},

    // navigation block
    {VK_INSERT, KEY_INSERT}, {VK_DELETE, KEY_DELETE}, {VK_HOME, KEY_HOME}, {VK_END, KEY_END},
    {VK_PRIOR, KEY_PAGE_UP}, {VK_NEXT, KEY_PAGE_DOWN},

    // F-keys
    {VK_F1, KEY_F+1}, {VK_F2, KEY_F+2}, {VK_F3, KEY_F+3}, {VK_F4, KEY_F+4},
    {VK_F5, KEY_F+5}, {VK_F6, KEY_F+6}, {VK_F7, KEY_F+7}, {VK_F8, KEY_F+8},
    {VK_F9, KEY_F+9}, {VK_F10, KEY_F+10}, {VK_F11, KEY_F+11}, {VK_F1, KEY_F+12},
    // numpad
    {VK_NUMPAD0, KEY_KP0}, {VK_NUMPAD1, KEY_KP1}, {VK_NUMPAD2, KEY_KP2},
    {VK_NUMPAD3, KEY_KP3}, {VK_NUMPAD4, KEY_KP4}, {VK_NUMPAD5, KEY_KP5},
    {VK_NUMPAD6, KEY_KP6}, {VK_NUMPAD7, KEY_KP7}, {VK_NUMPAD8, KEY_KP8},
    {VK_NUMPAD9, KEY_KP9}, {VK_DECIMAL, KEY_KPDEC},

    {0, 0}
};

static void vo_rect_add_window_borders(RECT *rc)
{
    AdjustWindowRect(rc, GetWindowLong(vo_window, GWL_STYLE), 0);
}

// basically a reverse AdjustWindowRect (win32 doesn't appear to have this)
static void subtract_window_borders(RECT *rc)
{
    RECT b = { 0, 0, 0, 0 };
    vo_rect_add_window_borders(&b);
    rc->left -= b.left;
    rc->top -= b.top;
    rc->right -= b.right;
    rc->bottom -= b.bottom;
}

// turn a WMSZ_* input value in v into the border that should be resized
// returns: 0=left, 1=top, 2=right, 3=bottom, -1=undefined
static int get_resize_border(int v) {
    switch (v) {
    case WMSZ_LEFT: return 3;
    case WMSZ_TOP: return 2;
    case WMSZ_RIGHT: return 3;
    case WMSZ_BOTTOM: return 2;
    case WMSZ_TOPLEFT: return 1;
    case WMSZ_TOPRIGHT: return 1;
    case WMSZ_BOTTOMLEFT: return 3;
    case WMSZ_BOTTOMRIGHT: return 3;
    default: return -1;
    }
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    RECT r;
    POINT p;
    int mpkey;
    switch (message) {
        case WM_ERASEBKGND: // no need to erase background seperately
            return 1;
        case WM_PAINT:
            event_flags |= VO_EVENT_EXPOSE;
            break;
        case WM_MOVE:
            event_flags |= VO_EVENT_MOVE;
            p.x = 0;
            p.y = 0;
            ClientToScreen(vo_window, &p);
            window_x = p.x;
            window_y = p.y;
            mp_msg(MSGT_VO, MSGL_V, "[vo] move window: %d:%d\n",
                   window_x, window_y);
            break;
        case WM_SIZE:
            event_flags |= VO_EVENT_RESIZE;
            GetClientRect(vo_window, &r);
            vo_dwidth = r.right;
            vo_dheight = r.bottom;
            mp_msg(MSGT_VO, MSGL_V, "[vo] resize window: %d:%d\n",
                   vo_dwidth, vo_dheight);
            break;
        case WM_SIZING:
            if (vo_keepaspect && !vo_fs && WinID < 0) {
                RECT *rc = (RECT*)lParam;
                // get client area of the windows if it had the rect rc
                // (subtracting the window borders)
                r = *rc;
                subtract_window_borders(&r);
                int c_w = r.right - r.left, c_h = r.bottom - r.top;
                float aspect = global_vo->aspdat.asp;
                int d_w = c_h * aspect - c_w;
                int d_h = c_w / aspect - c_h;
                int d_corners[4] = { d_w, d_h, -d_w, -d_h };
                int corners[4] = { rc->left, rc->top, rc->right, rc->bottom };
                int corner = get_resize_border(wParam);
                if (corner >= 0)
                    corners[corner] -= d_corners[corner];
                *rc = (RECT) { corners[0], corners[1], corners[2], corners[3] };
                return TRUE;
            }
            break;
        case WM_CLOSE:
            mplayer_put_key(KEY_CLOSE_WIN);
            break;
        case WM_SYSCOMMAND:
            switch (wParam) {
                case SC_SCREENSAVE:
                case SC_MONITORPOWER:
                    mp_msg(MSGT_VO, MSGL_V, "vo: win32: killing screensaver\n");
                    return 0;
            }
            break;
        case WM_KEYDOWN:
            mpkey = lookup_keymap_table(vk_map, wParam);
            if (mpkey)
                mplayer_put_key(mpkey);
            break;
        case WM_CHAR:
            mplayer_put_key(wParam);
            break;
        case WM_LBUTTONDOWN:
            if (!vo_nomouse_input && (vo_fs || (wParam & MK_CONTROL))) {
                mplayer_put_key(MOUSE_BTN0);
                break;
            }
            if (!vo_fs) {
                ReleaseCapture();
                SendMessage(hWnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
                return 0;
            }
            break;
        case WM_MBUTTONDOWN:
            if (!vo_nomouse_input)
                mplayer_put_key(MOUSE_BTN1);
            break;
        case WM_RBUTTONDOWN:
            if (!vo_nomouse_input)
                mplayer_put_key(MOUSE_BTN2);
            break;
        case WM_MOUSEMOVE:
            vo_mouse_movement(global_vo, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            break;
        case WM_MOUSEWHEEL:
            if (!vo_nomouse_input) {
                int x = GET_WHEEL_DELTA_WPARAM(wParam);
                if (x > 0)
                    mplayer_put_key(MOUSE_BTN3);
                else
                    mplayer_put_key(MOUSE_BTN4);
            }
            break;
        case WM_XBUTTONDOWN:
            if (!vo_nomouse_input) {
                int x = HIWORD(wParam);
                if (x == 1)
                    mplayer_put_key(MOUSE_BTN5);
                else // if (x == 2)
                    mplayer_put_key(MOUSE_BTN6);
            }
            break;
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
}

/**
 * \brief Dispatch incoming window events and handle them.
 *
 * This function should be placed inside libvo's function "check_events".
 *
 * Global libvo variables changed:
 * vo_dwidth:  new window client area width
 * vo_dheight: new window client area height
 *
 * \return int with these flags possibly set, take care to handle in the right order
 *         if it matters in your driver:
 *
 * VO_EVENT_RESIZE = The window was resized. If necessary reinit your
 *                   driver render context accordingly.
 * VO_EVENT_EXPOSE = The window was exposed. Call e.g. flip_frame() to redraw
 *                   the window if the movie is paused.
 */
int vo_w32_check_events(void) {
    MSG msg;
    event_flags = 0;
    while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    if (WinID >= 0) {
        BOOL res;
        RECT r;
        POINT p;
        res = GetClientRect(vo_window, &r);
        if (res && (r.right != vo_dwidth || r.bottom != vo_dheight)) {
            vo_dwidth = r.right; vo_dheight = r.bottom;
            event_flags |= VO_EVENT_RESIZE;
        }
        p.x = 0; p.y = 0;
        ClientToScreen(vo_window, &p);
        if (p.x != window_x || p.y != window_y) {
            window_x = p.x; window_y = p.y;
            event_flags |= VO_EVENT_MOVE;
        }
        res = GetClientRect(WIN_ID_TO_HWND(WinID), &r);
        if (res && (r.right != vo_dwidth || r.bottom != vo_dheight))
            MoveWindow(vo_window, 0, 0, r.right, r.bottom, FALSE);
        if (!IsWindow(WIN_ID_TO_HWND(WinID)))
            // Window has probably been closed, e.g. due to program crash
            mplayer_put_key(KEY_CLOSE_WIN);
    }

    return event_flags;
}

static BOOL CALLBACK mon_enum(HMONITOR hmon, HDC hdc, LPRECT r, LPARAM p) {
    // this defaults to the last screen if specified number does not exist
    xinerama_x = r->left;
    xinerama_y = r->top;
    vo_screenwidth = r->right - r->left;
    vo_screenheight = r->bottom - r->top;
    if (mon_cnt == xinerama_screen)
        return FALSE;
    mon_cnt++;
    return TRUE;
}

/**
 * \brief Update screen information.
 *
 * This function should be called in libvo's "control" callback
 * with parameter VOCTRL_UPDATE_SCREENINFO.
 * Note that this also enables the new API where geometry and aspect
 * calculations are done in video_out.c:config_video_out
 *
 * Global libvo variables changed:
 * xinerama_x
 * xinerama_y
 * vo_screenwidth
 * vo_screenheight
 */
void w32_update_xinerama_info(void) {
    xinerama_x = xinerama_y = 0;
    if (xinerama_screen < -1) {
        int tmp;
        xinerama_x = GetSystemMetrics(SM_XVIRTUALSCREEN);
        xinerama_y = GetSystemMetrics(SM_YVIRTUALSCREEN);
        tmp = GetSystemMetrics(SM_CXVIRTUALSCREEN);
        if (tmp) vo_screenwidth = tmp;
        tmp = GetSystemMetrics(SM_CYVIRTUALSCREEN);
        if (tmp) vo_screenheight = tmp;
    } else if (xinerama_screen == -1 && myMonitorFromWindow && myGetMonitorInfo) {
        MONITORINFO mi;
        HMONITOR m = myMonitorFromWindow(vo_window, MONITOR_DEFAULTTOPRIMARY);
        mi.cbSize = sizeof(mi);
        myGetMonitorInfo(m, &mi);
        xinerama_x = mi.rcMonitor.left;
        xinerama_y = mi.rcMonitor.top;
        vo_screenwidth = mi.rcMonitor.right - mi.rcMonitor.left;
        vo_screenheight = mi.rcMonitor.bottom - mi.rcMonitor.top;
    } else if (xinerama_screen > 0 && myEnumDisplayMonitors) {
        mon_cnt = 0;
        myEnumDisplayMonitors(NULL, NULL, mon_enum, 0);
    }
    aspect_save_screenres(vo_screenwidth, vo_screenheight);
}

static void updateScreenProperties(void) {
    DEVMODE dm;
    dm.dmSize = sizeof dm;
    dm.dmDriverExtra = 0;
    dm.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
    if (!EnumDisplaySettings(0, ENUM_CURRENT_SETTINGS, &dm)) {
        mp_msg(MSGT_VO, MSGL_ERR, "vo: win32: unable to enumerate display settings!\n");
        return;
    }

    vo_screenwidth = dm.dmPelsWidth;
    vo_screenheight = dm.dmPelsHeight;
    depthonscreen = dm.dmBitsPerPel;
    w32_update_xinerama_info();
}

static void changeMode(void) {
    DEVMODE dm;
    dm.dmSize = sizeof dm;
    dm.dmDriverExtra = 0;

    dm.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
    dm.dmBitsPerPel = depthonscreen;
    dm.dmPelsWidth = vo_screenwidth;
    dm.dmPelsHeight = vo_screenheight;

    if (vo_vm) {
        int bestMode = -1;
        int bestScore = INT_MAX;
        int i;
        for (i = 0; EnumDisplaySettings(0, i, &dm); ++i) {
            int score = (dm.dmPelsWidth - o_dwidth) * (dm.dmPelsHeight - o_dheight);
            if (dm.dmBitsPerPel != depthonscreen) continue;
            if (dm.dmPelsWidth < o_dwidth) continue;
            if (dm.dmPelsHeight < o_dheight) continue;

            if (score < bestScore) {
                bestScore = score;
                bestMode = i;
            }
        }

        if (bestMode != -1)
            EnumDisplaySettings(0, bestMode, &dm);

        ChangeDisplaySettings(&dm, CDS_FULLSCREEN);
    }
}

static void resetMode(void) {
    if (vo_vm)
        ChangeDisplaySettings(0, 0);
}

// Update the window title, position, size, and border style from vo_* values.
static int reinit_window_state(void) {
    const LONG NO_FRAME = WS_POPUP;
    const LONG FRAME = WS_OVERLAPPEDWINDOW | WS_SIZEBOX;
    HWND layer = HWND_NOTOPMOST;
    RECT r;

    if (WinID >= 0)
        return 1;

    wchar_t *title = mp_from_utf8(NULL, vo_get_window_title(global_vo));
    SetWindowTextW(vo_window, title);
    talloc_free(title);

    bool toggle_fs = current_fs != vo_fs;
    current_fs = vo_fs;

    LONG style = GetWindowLong(vo_window, GWL_STYLE);
    style &= ~(NO_FRAME | FRAME);
    style |= (vo_border && !vo_fs) ? FRAME : NO_FRAME;

    if (vo_fs || vo_ontop)
        layer = HWND_TOPMOST;

    // xxx not sure if this can trigger any unwanted messages (WM_MOVE/WM_SIZE)
    if (vo_fs) {
        changeMode();
        while (ShowCursor(0) >= 0) /**/ ;
    } else {
        resetMode();
        while (ShowCursor(1) < 0) /**/ ;
    }
    updateScreenProperties();

    if (vo_fs) {
        // Save window position and size when switching to fullscreen.
        if (toggle_fs) {
            prev_width = vo_dwidth;
            prev_height = vo_dheight;
            prev_x = window_x;
            prev_y = window_y;
            mp_msg(MSGT_VO, MSGL_V, "[vo] save window bounds: %d:%d:%d:%d\n",
                   prev_x, prev_y, prev_width, prev_height);
        }
        vo_dwidth = vo_screenwidth;
        vo_dheight = vo_screenheight;
        window_x = xinerama_x;
        window_y = xinerama_y;
    } else {
        if (toggle_fs) {
            // Restore window position and size when switching from fullscreen.
            mp_msg(MSGT_VO, MSGL_V, "[vo] restore window bounds: %d:%d:%d:%d\n",
                   prev_x, prev_y, prev_width, prev_height);
            vo_dwidth = prev_width;
            vo_dheight = prev_height;
            window_x = prev_x;
            window_y = prev_y;
        }
    }

    r.left = window_x;
    r.right = r.left + vo_dwidth;
    r.top = window_y;
    r.bottom = r.top + vo_dheight;

    SetWindowLong(vo_window, GWL_STYLE, style);
    vo_rect_add_window_borders(&r);

    mp_msg(MSGT_VO, MSGL_V, "[vo] reset window bounds: %ld:%ld:%ld:%ld\n",
           r.left, r.top, r.right - r.left, r.bottom - r.top);

    SetWindowPos(vo_window, layer, r.left, r.top, r.right - r.left,
                 r.bottom - r.top, SWP_FRAMECHANGED);
    // For some reason, moving SWP_SHOWWINDOW to a second call works better
    // with wine: returning from fullscreen doesn't cause a bogus resize to
    // screen size.
    // It's not needed on Windows XP or wine with a virtual desktop.
    // It doesn't seem to have any negative effects.
    SetWindowPos(vo_window, NULL, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW);

    return 1;
}

/**
 * \brief Configure and show window on the screen.
 *
 * This function should be called in libvo's "config" callback.
 * It configures a window and shows it on the screen.
 *
 * \return 1 - Success, 0 - Failure
 */
int vo_w32_config(uint32_t width, uint32_t height, uint32_t flags) {
    PIXELFORMATDESCRIPTOR pfd;
    int pf;
    HDC vo_hdc = vo_w32_get_dc(vo_window);

    memset(&pfd, 0, sizeof pfd);
    pfd.nSize = sizeof pfd;
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    if (flags & VOFLAG_STEREO)
        pfd.dwFlags |= PFD_STEREO;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 24;
    pfd.iLayerType = PFD_MAIN_PLANE;
    pf = ChoosePixelFormat(vo_hdc, &pfd);
    if (!pf) {
        mp_msg(MSGT_VO, MSGL_ERR, "vo: win32: unable to select a valid pixel format!\n");
        vo_w32_release_dc(vo_window, vo_hdc);
        return 0;
    }

    SetPixelFormat(vo_hdc, pf, &pfd);
    vo_w32_release_dc(vo_window, vo_hdc);

    // we already have a fully initialized window, so nothing needs to be done
    if (flags & VOFLAG_HIDDEN)
        return 1;

    bool reset_size = !(o_dwidth == width && o_dheight == height);

    o_dwidth = width;
    o_dheight = height;

    // the desired size is ignored in wid mode, it always matches the window size.
    if (WinID < 0) {
        if (window_bounds_initialized) {
            // restore vo_dwidth/vo_dheight, which are reset against our will
            // in vo_config()
            RECT r;
            GetClientRect(vo_window, &r);
            vo_dwidth = r.right;
            vo_dheight = r.bottom;
        } else {
            // first vo_config call; vo_config() will always set vo_dx/dy so
            // that the window is centered on the screen, and this is the only
            // time we actually want to use vo_dy/dy (this is not sane, and
            // video_out.h should provide a function to query the initial
            // window position instead)
            window_bounds_initialized = true;
            reset_size = true;
            window_x = prev_x = vo_dx;
            window_y = prev_y = vo_dy;
        }
        if (reset_size) {
            prev_width = vo_dwidth = width;
            prev_height = vo_dheight = height;
        }
    }

    vo_fs = flags & VOFLAG_FULLSCREEN;
    vo_vm = flags & VOFLAG_MODESWITCHING;
    return reinit_window_state();
}

/**
 * \brief return the name of the selected device if it is indepedant
 * \return pointer to string, must be freed.
 */
static char *get_display_name(void) {
    DISPLAY_DEVICE disp;
    disp.cb = sizeof(disp);
    EnumDisplayDevices(NULL, vo_adapter_num, &disp, 0);
    if (disp.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP)
        return NULL;
    return strdup(disp.DeviceName);
}

/**
 * \brief Initialize w32_common framework.
 *
 * The first function that should be called from the w32_common framework.
 * It handles window creation on the screen with proper title and attributes.
 * It also initializes the framework's internal variables. The function should
 * be called after your own preinit initialization and you shouldn't do any
 * window management on your own.
 *
 * Global libvo variables changed:
 * vo_w32_window
 * vo_screenwidth
 * vo_screenheight
 *
 * \return 1 = Success, 0 = Failure
 */
int vo_w32_init(void) {
    HICON mplayerIcon = 0;
    char exedir[MAX_PATH];
    HINSTANCE user32;
    char *dev;

    if (vo_window)
        return 1;

    hInstance = GetModuleHandle(0);

    if (GetModuleFileName(0, exedir, MAX_PATH))
        mplayerIcon = ExtractIcon(hInstance, exedir, 0);
    if (!mplayerIcon)
        mplayerIcon = LoadIcon(0, IDI_APPLICATION);

  {
    WNDCLASSEX wcex = { sizeof wcex, CS_OWNDC | CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW, WndProc, 0, 0, hInstance, mplayerIcon, LoadCursor(0, IDC_ARROW), NULL, 0, classname, mplayerIcon };

    if (!RegisterClassEx(&wcex)) {
        mp_msg(MSGT_VO, MSGL_ERR, "vo: win32: unable to register window class!\n");
        return 0;
    }
  }

    if (WinID >= 0)
    {
        RECT r;
        GetClientRect(WIN_ID_TO_HWND(WinID), &r);
        vo_dwidth = r.right; vo_dheight = r.bottom;
        vo_window = CreateWindowEx(WS_EX_NOPARENTNOTIFY, classname, classname,
                     WS_CHILD | WS_VISIBLE, 0, 0, vo_dwidth, vo_dheight,
                     WIN_ID_TO_HWND(WinID), 0, hInstance, 0);
        EnableWindow(vo_window, 0);
    } else
        vo_window = CreateWindowEx(0, classname, classname,
                      vo_border ? (WS_OVERLAPPEDWINDOW | WS_SIZEBOX) : WS_POPUP,
                      CW_USEDEFAULT, 0, 100, 100, 0, 0, hInstance, 0);
    if (!vo_window) {
        mp_msg(MSGT_VO, MSGL_ERR, "vo: win32: unable to create window!\n");
        return 0;
    }

    myMonitorFromWindow = NULL;
    myGetMonitorInfo = NULL;
    myEnumDisplayMonitors = NULL;
    user32 = GetModuleHandle("user32.dll");
    if (user32) {
        myMonitorFromWindow = (void *)GetProcAddress(user32, "MonitorFromWindow");
        myGetMonitorInfo = GetProcAddress(user32, "GetMonitorInfoA");
        myEnumDisplayMonitors = GetProcAddress(user32, "EnumDisplayMonitors");
    }
    dev_hdc = 0;
    dev = get_display_name();
    if (dev) dev_hdc = CreateDC(dev, NULL, NULL, NULL);
    free(dev);
    updateScreenProperties();

    mp_msg(MSGT_VO, MSGL_V, "vo: win32: running at %dx%d with depth %d\n", vo_screenwidth, vo_screenheight, depthonscreen);

    return 1;
}

/**
 * \brief Toogle fullscreen / windowed mode.
 *
 * Should be called on VOCTRL_FULLSCREEN event. The window is
 * always resized after this call, so the rendering context
 * should be reinitialized with the new dimensions.
 * It is unspecified if vo_check_events will create a resize
 * event in addition or not.
 *
 * Global libvo variables changed:
 * vo_dwidth
 * vo_dheight
 * vo_fs
 */

void vo_w32_fullscreen(void) {
    vo_fs = !vo_fs;

    reinit_window_state();
}

/**
 * \brief Toogle window border attribute.
 *
 * Should be called on VOCTRL_BORDER event.
 *
 * Global libvo variables changed:
 * vo_border
 */
void vo_w32_border(void) {
    vo_border = !vo_border;
    reinit_window_state();
}

/**
 * \brief Toogle window ontop attribute.
 *
 * Should be called on VOCTRL_ONTOP event.
 *
 * Global libvo variables changed:
 * vo_ontop
 */
void vo_w32_ontop( void )
{
    vo_ontop = !vo_ontop;
    if (!vo_fs) {
        reinit_window_state();
    }
}

/**
 * \brief Uninitialize w32_common framework.
 *
 * Should be called last in video driver's uninit function. First release
 * anything built on top of the created window e.g. rendering context inside
 * and call vo_w32_uninit at the end.
 */
void vo_w32_uninit(void) {
    mp_msg(MSGT_VO, MSGL_V, "vo: win32: uninit\n");
    resetMode();
    ShowCursor(1);
    depthonscreen = 0;
    if (dev_hdc) DeleteDC(dev_hdc);
    dev_hdc = 0;
    DestroyWindow(vo_window);
    vo_window = 0;
    UnregisterClass(classname, 0);
    o_dwidth = o_dheight = 0;
}

/**
 * \brief get a device context to draw in
 *
 * \param wnd window the DC should belong to if it makes sense
 */
HDC vo_w32_get_dc(HWND wnd) {
    if (dev_hdc) return dev_hdc;
    return GetDC(wnd);
}

/**
 * \brief release a device context
 *
 * \param wnd window the DC probably belongs to
 */
void vo_w32_release_dc(HWND wnd, HDC dc) {
    if (dev_hdc) return;
    ReleaseDC(wnd, dc);
}
