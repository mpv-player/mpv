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
#include <ole2.h>

#include "options/options.h"
#include "input/keycodes.h"
#include "input/input.h"
#include "input/event.h"
#include "common/msg.h"
#include "common/common.h"
#include "vo.h"
#include "aspect.h"
#include "w32_common.h"
#include "osdep/io.h"
#include "talloc.h"

#define WIN_ID_TO_HWND(x) ((HWND)(intptr_t)(x))

static const wchar_t classname[] = L"mpv";

static const struct mp_keymap vk_map[] = {
    // special keys
    {VK_ESCAPE, MP_KEY_ESC}, {VK_BACK, MP_KEY_BS}, {VK_TAB, MP_KEY_TAB},
    {VK_RETURN, MP_KEY_ENTER}, {VK_PAUSE, MP_KEY_PAUSE}, {VK_SNAPSHOT, MP_KEY_PRINT},

    // cursor keys
    {VK_LEFT, MP_KEY_LEFT}, {VK_UP, MP_KEY_UP}, {VK_RIGHT, MP_KEY_RIGHT}, {VK_DOWN, MP_KEY_DOWN},

    // navigation block
    {VK_INSERT, MP_KEY_INSERT}, {VK_DELETE, MP_KEY_DELETE}, {VK_HOME, MP_KEY_HOME}, {VK_END, MP_KEY_END},
    {VK_PRIOR, MP_KEY_PAGE_UP}, {VK_NEXT, MP_KEY_PAGE_DOWN},

    // F-keys
    {VK_F1, MP_KEY_F+1}, {VK_F2, MP_KEY_F+2}, {VK_F3, MP_KEY_F+3}, {VK_F4, MP_KEY_F+4},
    {VK_F5, MP_KEY_F+5}, {VK_F6, MP_KEY_F+6}, {VK_F7, MP_KEY_F+7}, {VK_F8, MP_KEY_F+8},
    {VK_F9, MP_KEY_F+9}, {VK_F10, MP_KEY_F+10}, {VK_F11, MP_KEY_F+11}, {VK_F12, MP_KEY_F+12},
    // numpad
    {VK_NUMPAD0, MP_KEY_KP0}, {VK_NUMPAD1, MP_KEY_KP1}, {VK_NUMPAD2, MP_KEY_KP2},
    {VK_NUMPAD3, MP_KEY_KP3}, {VK_NUMPAD4, MP_KEY_KP4}, {VK_NUMPAD5, MP_KEY_KP5},
    {VK_NUMPAD6, MP_KEY_KP6}, {VK_NUMPAD7, MP_KEY_KP7}, {VK_NUMPAD8, MP_KEY_KP8},
    {VK_NUMPAD9, MP_KEY_KP9}, {VK_DECIMAL, MP_KEY_KPDEC},

    {0, 0}
};

typedef struct tagDropTarget {
    IDropTarget iface;
    ULONG refCnt;
    DWORD lastEffect;
    IDataObject* dataObj;
    struct vo *vo;
} DropTarget;

static FORMATETC fmtetc_file = { CF_HDROP, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
static FORMATETC fmtetc_url = { 0, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };

static void DropTarget_Destroy(DropTarget* This)
{
    if (This->dataObj != NULL) {
        This->dataObj->lpVtbl->Release(This->dataObj);
        This->dataObj->lpVtbl = NULL;
    }

    talloc_free(This);
}

static HRESULT STDMETHODCALLTYPE DropTarget_QueryInterface(IDropTarget* This,
                                                           REFIID riid,
                                                           void** ppvObject)
{
    if (!IsEqualGUID(riid, &IID_IUnknown) ||
        !IsEqualGUID(riid, &IID_IDataObject)) {
        *ppvObject = NULL;
        return E_NOINTERFACE;
    }

    *ppvObject = This;
    This->lpVtbl->AddRef(This);
    return S_OK;
}

static ULONG STDMETHODCALLTYPE DropTarget_AddRef(IDropTarget* This)
{
    DropTarget* t = (DropTarget*)This;
    return ++(t->refCnt);
}

static ULONG STDMETHODCALLTYPE DropTarget_Release(IDropTarget* This)
{
    DropTarget* t = (DropTarget*)This;
    ULONG cRef = --(t->refCnt);

    if (cRef == 0) {
        DropTarget_Destroy(t);
    }

    return cRef;
}

static HRESULT STDMETHODCALLTYPE DropTarget_DragEnter(IDropTarget* This,
                                                      IDataObject* pDataObj,
                                                      DWORD grfKeyState,
                                                      POINTL pt,
                                                      DWORD* pdwEffect)
{
    DropTarget* t = (DropTarget*)This;

    pDataObj->lpVtbl->AddRef(pDataObj);
    if (pDataObj->lpVtbl->QueryGetData(pDataObj, &fmtetc_file) != S_OK &&
        pDataObj->lpVtbl->QueryGetData(pDataObj, &fmtetc_url) != S_OK) {

        *pdwEffect = DROPEFFECT_NONE;
    }

    if (t->dataObj != NULL) {
        t->dataObj->lpVtbl->Release(t->dataObj);
    }

    t->dataObj = pDataObj;
    t->lastEffect = *pdwEffect;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE DropTarget_DragOver(IDropTarget* This,
                                                     DWORD grfKeyState,
                                                     POINTL pt,
                                                     DWORD* pdwEffect)
{
    DropTarget* t = (DropTarget*)This;

    *pdwEffect = t->lastEffect;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE DropTarget_DragLeave(IDropTarget* This)
{
    DropTarget* t = (DropTarget*)This;

    if (t->dataObj != NULL) {
        t->dataObj->lpVtbl->Release(t->dataObj);
        t->dataObj = NULL;
    }

    return S_OK;
}

static HRESULT STDMETHODCALLTYPE DropTarget_Drop(IDropTarget* This,
                                                 IDataObject* pDataObj,
                                                 DWORD grfKeyState, POINTL pt,
                                                 DWORD* pdwEffect)
{
    DropTarget* t = (DropTarget*)This;

    STGMEDIUM medium;

    if (t->dataObj != NULL) {
        t->dataObj->lpVtbl->Release(t->dataObj);
        t->dataObj = NULL;
    }

    pDataObj->lpVtbl->AddRef(pDataObj);

    if (pDataObj->lpVtbl->GetData(pDataObj, &fmtetc_file, &medium) == S_OK) {
        if (GlobalLock(medium.hGlobal) != NULL) {
            HDROP hDrop = (HDROP)medium.hGlobal;

            UINT numFiles = DragQueryFileW(hDrop, 0xFFFFFFFF, NULL, 0);
            char** files = talloc_zero_array(NULL, char*, numFiles);

            UINT nrecvd_files = 0;
            for (UINT i = 0; i < numFiles; i++) {
                UINT len = DragQueryFileW(hDrop, i, NULL, 0);
                wchar_t* buf = talloc_array(NULL, wchar_t, len + 1);

                if (DragQueryFileW(hDrop, i, buf, len + 1) == len) {
                    char* fname = mp_to_utf8(files, buf);
                    files[nrecvd_files++] = fname;

                    MP_VERBOSE(t->vo, "win32: received dropped file: %s\n",
                               fname);
                } else {
                    MP_ERR(t->vo, "win32: error getting dropped file name\n");
                }

                talloc_free(buf);
            }

            GlobalUnlock(medium.hGlobal);
            mp_event_drop_files(t->vo->input_ctx, nrecvd_files, files);

            talloc_free(files);
        }

        ReleaseStgMedium(&medium);
    } else if (pDataObj->lpVtbl->GetData(pDataObj,
                                         &fmtetc_url, &medium) == S_OK) {
        // get the URL encoded in US-ASCII
        char* url = (char*)GlobalLock(medium.hGlobal);
        if (url != NULL) {
            if (mp_event_drop_mime_data(t->vo->input_ctx, "text/uri-list",
                                        bstr0(url)) > 0) {
                MP_VERBOSE(t->vo, "win32: received dropped URL: %s\n", url);
            } else {
                MP_ERR(t->vo, "win32: error getting dropped URL\n");
            }

            GlobalUnlock(medium.hGlobal);
        }

        ReleaseStgMedium(&medium);
    }
    else {
        t->lastEffect = DROPEFFECT_NONE;
    }

    pDataObj->lpVtbl->Release(pDataObj);
    *pdwEffect = t->lastEffect;
    return S_OK;
}


static void DropTarget_Init(DropTarget* This, struct vo *vo)
{
    IDropTargetVtbl* vtbl = talloc(This, IDropTargetVtbl);
    *vtbl = (IDropTargetVtbl){
        DropTarget_QueryInterface, DropTarget_AddRef, DropTarget_Release,
        DropTarget_DragEnter, DropTarget_DragOver, DropTarget_DragLeave,
        DropTarget_Drop
    };

    This->iface.lpVtbl = vtbl;
    This->refCnt = 0;
    This->lastEffect = 0;
    This->dataObj = NULL;
    This->vo = vo;
}

static void add_window_borders(HWND hwnd, RECT *rc)
{
    AdjustWindowRect(rc, GetWindowLong(hwnd, GWL_STYLE), 0);
}

// basically a reverse AdjustWindowRect (win32 doesn't appear to have this)
static void subtract_window_borders(HWND hwnd, RECT *rc)
{
    RECT b = { 0, 0, 0, 0 };
    add_window_borders(hwnd, &b);
    rc->left -= b.left;
    rc->top -= b.top;
    rc->right -= b.right;
    rc->bottom -= b.bottom;
}

// turn a WMSZ_* input value in v into the border that should be resized
// returns: 0=left, 1=top, 2=right, 3=bottom, -1=undefined
static int get_resize_border(int v)
{
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

static bool key_state(struct vo *vo, int vk)
{
    return GetKeyState(vk) & 0x8000;
}

static int mod_state(struct vo *vo)
{
    int res = 0;
    if (key_state(vo, VK_CONTROL))
        res |= MP_KEY_MODIFIER_CTRL;
    if (key_state(vo, VK_SHIFT))
        res |= MP_KEY_MODIFIER_SHIFT;
    if (key_state(vo, VK_MENU))
        res |= MP_KEY_MODIFIER_ALT;
    return res;
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam,
                                LPARAM lParam)
{
    if (message == WM_NCCREATE) {
        CREATESTRUCT *cs = (void*)lParam;
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
    }
    struct vo *vo = (void*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    // message before WM_NCCREATE, pray to Raymond Chen that it's not important
    if (!vo)
        return DefWindowProcW(hWnd, message, wParam, lParam);
    struct vo_w32_state *w32 = vo->w32;
    int mouse_button = 0;

    switch (message) {
    case WM_ERASEBKGND: // no need to erase background seperately
        return 1;
    case WM_PAINT:
        w32->event_flags |= VO_EVENT_EXPOSE;
        break;
    case WM_MOVE: {
        POINT p = {0};
        ClientToScreen(w32->window, &p);
        w32->window_x = p.x;
        w32->window_y = p.y;
        MP_VERBOSE(vo, "move window: %d:%d\n",
                w32->window_x, w32->window_y);
        break;
    }
    case WM_SIZE: {
        w32->event_flags |= VO_EVENT_RESIZE;
        RECT r;
        GetClientRect(w32->window, &r);
        vo->dwidth = r.right;
        vo->dheight = r.bottom;
        MP_VERBOSE(vo, "resize window: %d:%d\n",
                vo->dwidth, vo->dheight);
        break;
    }
    case WM_SIZING:
        if (vo->opts->keepaspect && !vo->opts->fullscreen &&
            vo->opts->WinID < 0)
        {
            RECT *rc = (RECT*)lParam;
            // get client area of the windows if it had the rect rc
            // (subtracting the window borders)
            RECT r = *rc;
            subtract_window_borders(w32->window, &r);
            int c_w = r.right - r.left, c_h = r.bottom - r.top;
            float aspect = w32->o_dwidth / (float) MPMAX(w32->o_dheight, 1);
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
        mp_input_put_key(vo->input_ctx, MP_KEY_CLOSE_WIN);
        break;
    case WM_SYSCOMMAND:
        switch (wParam) {
        case SC_SCREENSAVE:
        case SC_MONITORPOWER:
            if (w32->disable_screensaver) {
                MP_VERBOSE(vo, "win32: killing screensaver\n");
                return 0;
            }
            break;
        }
        break;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN: {
        int mpkey = lookup_keymap_table(vk_map, wParam);
        if (mpkey)
            mp_input_put_key(vo->input_ctx, mpkey | mod_state(vo));
        if (wParam == VK_F10)
            return 0;
        break;
    }
    case WM_CHAR:
    case WM_SYSCHAR: {
        int mods = mod_state(vo);
        int code = wParam;
        // Windows enables Ctrl+Alt when AltGr (VK_RMENU) is pressed.
        // E.g. AltGr+9 on a German keyboard would yield Ctrl+Alt+[
        // Warning: wine handles this differently. Don't test this on wine!
        if (key_state(vo, VK_RMENU) && mp_input_use_alt_gr(vo->input_ctx))
            mods &= ~(MP_KEY_MODIFIER_CTRL | MP_KEY_MODIFIER_ALT);
        // Apparently Ctrl+A to Ctrl+Z is special cased, and produces
        // character codes from 1-26. Work it around.
        // Also, enter/return (including the keypad variant) and CTRL+J both
        // map to wParam==10. As a workaround, check VK_RETURN to
        // distinguish these two key combinations.
        if ((mods & MP_KEY_MODIFIER_CTRL) && code >= 1 && code <= 26
            && !key_state(vo, VK_RETURN))
            code = code - 1 + (mods & MP_KEY_MODIFIER_SHIFT ? 'A' : 'a');
        if (code >= 32 && code < (1<<21)) {
            mp_input_put_key(vo->input_ctx, code | mods);
            // At least with Alt+char, not calling DefWindowProcW stops
            // Windows from emitting a beep.
            return 0;
        }
        break;
    }
    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT && !w32->cursor_visible) {
            SetCursor(NULL);
            return TRUE;
        }
        break;
    case WM_MOUSELEAVE:
        w32->tracking = FALSE;
        mp_input_put_key(vo->input_ctx, MP_KEY_MOUSE_LEAVE);
        break;
    case WM_MOUSEMOVE: {
        if (!w32->tracking)
            w32->tracking = TrackMouseEvent(&w32->trackEvent);
        // Windows can send spurious mouse events, which would make the mpv
        // core unhide the mouse cursor on completely unrelated events. See:
        //  https://blogs.msdn.com/b/oldnewthing/archive/2003/10/01/55108.aspx
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);
        if (x != w32->mouse_x || y != w32->mouse_y) {
            w32->mouse_x = x;
            w32->mouse_y = y;
            vo_mouse_movement(vo, x, y);
        }
        break;
    }
    case WM_LBUTTONDOWN:
        mouse_button = MP_MOUSE_BTN0 | MP_KEY_STATE_DOWN;
        break;
    case WM_LBUTTONUP:
        mouse_button = MP_MOUSE_BTN0 | MP_KEY_STATE_UP;
        break;
    case WM_MBUTTONDOWN:
        mouse_button = MP_MOUSE_BTN1 | MP_KEY_STATE_DOWN;
        break;
    case WM_MBUTTONUP:
        mouse_button = MP_MOUSE_BTN1 | MP_KEY_STATE_UP;
        break;
    case WM_RBUTTONDOWN:
        mouse_button = MP_MOUSE_BTN2 | MP_KEY_STATE_DOWN;
        break;
    case WM_RBUTTONUP:
        mouse_button = MP_MOUSE_BTN2 | MP_KEY_STATE_UP;
        break;
    case WM_MOUSEWHEEL: {
        int x = GET_WHEEL_DELTA_WPARAM(wParam);
        mouse_button = x > 0 ? MP_MOUSE_BTN3 : MP_MOUSE_BTN4;
        break;
    }
    case WM_XBUTTONDOWN:
        mouse_button = HIWORD(wParam) == 1 ? MP_MOUSE_BTN5 : MP_MOUSE_BTN6;
        mouse_button |= MP_KEY_STATE_DOWN;
        break;
    case WM_XBUTTONUP:
        mouse_button = HIWORD(wParam) == 1 ? MP_MOUSE_BTN5 : MP_MOUSE_BTN6;
        mouse_button |= MP_KEY_STATE_UP;
        break;
    }

    if (mouse_button) {
        mouse_button |= mod_state(vo);
        mp_input_put_key(vo->input_ctx, mouse_button);

        if (vo->opts->enable_mouse_movements) {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);

            if (mouse_button == (MP_MOUSE_BTN0 | MP_KEY_STATE_DOWN) &&
                !vo->opts->fullscreen &&
                !mp_input_test_dragging(vo->input_ctx, x, y))
            {
                // Window dragging hack
                ReleaseCapture();
                SendMessage(hWnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
                return 0;
            }
        }
    }

    return DefWindowProcW(hWnd, message, wParam, lParam);
}

/**
 * \brief Dispatch incoming window events and handle them.
 *
 * This function should be placed inside libvo's function "check_events".
 *
 * \return int with these flags possibly set, take care to handle in the right order
 *         if it matters in your driver:
 *
 * VO_EVENT_RESIZE = The window was resized. If necessary reinit your
 *                   driver render context accordingly.
 * VO_EVENT_EXPOSE = The window was exposed. Call e.g. flip_frame() to redraw
 *                   the window if the movie is paused.
 */
int vo_w32_check_events(struct vo *vo)
{
    struct vo_w32_state *w32 = vo->w32;
    MSG msg;
    w32->event_flags = 0;

    while (PeekMessageW(&msg, 0, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (vo->opts->WinID >= 0) {
        BOOL res;
        RECT r;
        POINT p;
        res = GetClientRect(w32->window, &r);

        if (res && (r.right != vo->dwidth || r.bottom != vo->dheight)) {
            vo->dwidth = r.right; vo->dheight = r.bottom;
            w32->event_flags |= VO_EVENT_RESIZE;
        }

        p.x = 0; p.y = 0;
        ClientToScreen(w32->window, &p);

        if (p.x != w32->window_x || p.y != w32->window_y) {
            w32->window_x = p.x; w32->window_y = p.y;
        }

        res = GetClientRect(WIN_ID_TO_HWND(vo->opts->WinID), &r);

        if (res && (r.right != vo->dwidth || r.bottom != vo->dheight))
            MoveWindow(w32->window, 0, 0, r.right, r.bottom, FALSE);

        if (!IsWindow(WIN_ID_TO_HWND(vo->opts->WinID))) {
            // Window has probably been closed, e.g. due to program crash
            mp_input_put_key(vo->input_ctx, MP_KEY_CLOSE_WIN);
        }
    }

    return w32->event_flags;
}

static BOOL CALLBACK mon_enum(HMONITOR hmon, HDC hdc, LPRECT r, LPARAM p)
{
    struct vo *vo = (void*)p;
    struct vo_w32_state *w32 = vo->w32;
    // this defaults to the last screen if specified number does not exist
    vo->xinerama_x = r->left;
    vo->xinerama_y = r->top;
    vo->opts->screenwidth = r->right - r->left;
    vo->opts->screenheight = r->bottom - r->top;

    if (w32->mon_cnt == w32->mon_id)
        return FALSE;

    w32->mon_cnt++;
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
static void w32_update_xinerama_info(struct vo *vo)
{
    struct vo_w32_state *w32 = vo->w32;
    struct mp_vo_opts *opts = vo->opts;
    int screen = opts->fullscreen ? opts->fsscreen_id : opts->screen_id;
    vo->xinerama_x = vo->xinerama_y = 0;

    if (opts->fullscreen && screen == -2) {
        int tmp;
        vo->xinerama_x = GetSystemMetrics(SM_XVIRTUALSCREEN);
        vo->xinerama_y = GetSystemMetrics(SM_YVIRTUALSCREEN);
        tmp = GetSystemMetrics(SM_CXVIRTUALSCREEN);

        if (tmp)
            vo->opts->screenwidth = tmp;

        tmp = GetSystemMetrics(SM_CYVIRTUALSCREEN);

        if (tmp)
            vo->opts->screenheight = tmp;
    } else if (screen == -1) {
        MONITORINFO mi;
        HMONITOR m = MonitorFromWindow(w32->window, MONITOR_DEFAULTTOPRIMARY);
        mi.cbSize = sizeof(mi);
        GetMonitorInfoW(m, &mi);
        vo->xinerama_x = mi.rcMonitor.left;
        vo->xinerama_y = mi.rcMonitor.top;
        vo->opts->screenwidth = mi.rcMonitor.right - mi.rcMonitor.left;
        vo->opts->screenheight = mi.rcMonitor.bottom - mi.rcMonitor.top;
    } else if (screen >= 0) {
        w32->mon_cnt = 0;
        w32->mon_id = screen;
        EnumDisplayMonitors(NULL, NULL, mon_enum, (LONG_PTR)vo);
    }

    aspect_save_screenres(vo, vo->opts->screenwidth, vo->opts->screenheight);
}

static void updateScreenProperties(struct vo *vo)
{
    DEVMODE dm;
    dm.dmSize = sizeof dm;
    dm.dmDriverExtra = 0;
    dm.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;

    if (!EnumDisplaySettings(0, ENUM_CURRENT_SETTINGS, &dm)) {
        MP_ERR(vo, "win32: unable to enumerate display settings!\n");
        return;
    }

    vo->opts->screenwidth = dm.dmPelsWidth;
    vo->opts->screenheight = dm.dmPelsHeight;
    w32_update_xinerama_info(vo);
}

static DWORD update_style(struct vo *vo, DWORD style)
{
    const DWORD NO_FRAME = WS_POPUP;
    const DWORD FRAME = WS_OVERLAPPEDWINDOW | WS_SIZEBOX;
    style &= ~(NO_FRAME | FRAME);
    style |= (vo->opts->border && !vo->opts->fullscreen) ? FRAME : NO_FRAME;
    return style;
}

// Update the window title, position, size, and border style from vo_* values.
static int reinit_window_state(struct vo *vo)
{
    struct vo_w32_state *w32 = vo->w32;
    HWND layer = HWND_NOTOPMOST;
    RECT r;

    if (vo->opts->WinID >= 0)
        return 1;

    bool toggle_fs = w32->current_fs != vo->opts->fullscreen;
    w32->current_fs = vo->opts->fullscreen;

    DWORD style = update_style(vo, GetWindowLong(w32->window, GWL_STYLE));

    if (vo->opts->ontop)
        layer = HWND_TOPMOST;

    // xxx not sure if this can trigger any unwanted messages (WM_MOVE/WM_SIZE)
    updateScreenProperties(vo);

    if (vo->opts->fullscreen) {
        // Save window position and size when switching to fullscreen.
        if (toggle_fs) {
            w32->prev_width = vo->dwidth;
            w32->prev_height = vo->dheight;
            w32->prev_x = w32->window_x;
            w32->prev_y = w32->window_y;
            MP_VERBOSE(vo, "save window bounds: %d:%d:%d:%d\n",
                   w32->prev_x, w32->prev_y, w32->prev_width, w32->prev_height);
        }

        vo->dwidth = vo->opts->screenwidth;
        vo->dheight = vo->opts->screenheight;
        w32->window_x = vo->xinerama_x;
        w32->window_y = vo->xinerama_y;
        style &= ~WS_OVERLAPPEDWINDOW;
    } else {
        if (toggle_fs) {
            // Restore window position and size when switching from fullscreen.
            MP_VERBOSE(vo, "restore window bounds: %d:%d:%d:%d\n",
                   w32->prev_x, w32->prev_y, w32->prev_width, w32->prev_height);
            vo->dwidth = w32->prev_width;
            vo->dheight = w32->prev_height;
            w32->window_x = w32->prev_x;
            w32->window_y = w32->prev_y;
        }
    }

    r.left = w32->window_x;
    r.right = r.left + vo->dwidth;
    r.top = w32->window_y;
    r.bottom = r.top + vo->dheight;

    SetWindowLong(w32->window, GWL_STYLE, style);
    add_window_borders(w32->window, &r);

    MP_VERBOSE(vo, "reset window bounds: %d:%d:%d:%d\n",
           (int) r.left, (int) r.top, (int)(r.right - r.left),
           (int)(r.bottom - r.top));

    SetWindowPos(w32->window, layer, r.left, r.top, r.right - r.left,
                 r.bottom - r.top, SWP_FRAMECHANGED);
    // For some reason, moving SWP_SHOWWINDOW to a second call works better
    // with wine: returning from fullscreen doesn't cause a bogus resize to
    // screen size.
    // It's not needed on Windows XP or wine with a virtual desktop.
    // It doesn't seem to have any negative effects.
    SetWindowPos(w32->window, NULL, 0, 0, 0, 0,
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
int vo_w32_config(struct vo *vo, uint32_t width, uint32_t height,
                  uint32_t flags)
{
    struct vo_w32_state *w32 = vo->w32;
    PIXELFORMATDESCRIPTOR pfd;
    int pf;
    HDC vo_hdc = GetDC(w32->window);

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
        MP_ERR(vo, "win32: unable to select a valid pixel format!\n");
        ReleaseDC(w32->window, vo_hdc);
        return 0;
    }

    SetPixelFormat(vo_hdc, pf, &pfd);
    ReleaseDC(w32->window, vo_hdc);

    // we already have a fully initialized window, so nothing needs to be done
    if (flags & VOFLAG_HIDDEN)
        return 1;

    bool reset_size = !(w32->o_dwidth == width && w32->o_dheight == height);

    w32->o_dwidth = width;
    w32->o_dheight = height;

    // the desired size is ignored in wid mode, it always matches the window size.
    if (vo->opts->WinID < 0) {
        if (w32->window_bounds_initialized) {
            // restore vo_dwidth/vo_dheight, which are reset against our will
            // in vo_config()
            RECT r;
            GetClientRect(w32->window, &r);
            vo->dwidth = r.right;
            vo->dheight = r.bottom;
        } else {
            // first vo_config call; vo_config() will always set vo_dx/dy so
            // that the window is centered on the screen, and this is the only
            // time we actually want to use vo_dy/dy (this is not sane, and
            // vo.h should provide a function to query the initial
            // window position instead)
            w32->window_bounds_initialized = true;
            reset_size = true;
            w32->window_x = w32->prev_x = vo->dx;
            w32->window_y = w32->prev_y = vo->dy;
        }

        if (reset_size) {
            w32->prev_width = vo->dwidth = width;
            w32->prev_height = vo->dheight = height;
        }
    } else {
        RECT r;
        GetClientRect(w32->window, &r);
        vo->dwidth = r.right;
        vo->dheight = r.bottom;
    }

    return reinit_window_state(vo);
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
int vo_w32_init(struct vo *vo)
{
    assert(!vo->w32);

    struct vo_w32_state *w32 = talloc_zero(vo, struct vo_w32_state);
    vo->w32 = w32;

    HINSTANCE hInstance = GetModuleHandleW(NULL);

    HICON mplayerIcon = LoadIconW(hInstance, L"IDI_ICON1");

    WNDCLASSEXW wcex = {
        .cbSize = sizeof wcex,
        .style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW,
        .lpfnWndProc = WndProc,
        .hInstance = hInstance,
        .hIcon = mplayerIcon,
        .hCursor = LoadCursor(NULL, IDC_ARROW),
        .lpszClassName = classname,
        .hIconSm = mplayerIcon,
    };

    if (!RegisterClassExW(&wcex)) {
        MP_ERR(vo, "win32: unable to register window class!\n");
        return 0;
    }

    if (vo->opts->WinID >= 0) {
        RECT r;
        GetClientRect(WIN_ID_TO_HWND(vo->opts->WinID), &r);
        vo->dwidth = r.right; vo->dheight = r.bottom;
        w32->window = CreateWindowExW(WS_EX_NOPARENTNOTIFY, classname,
                                      classname,
                                      WS_CHILD | WS_VISIBLE,
                                      0, 0, vo->dwidth, vo->dheight,
                                      WIN_ID_TO_HWND(vo->opts->WinID),
                                      0, hInstance, vo);
    } else {
        w32->window = CreateWindowExW(0, classname,
                                      classname,
                                      update_style(vo, 0),
                                      CW_USEDEFAULT, 0, 100, 100,
                                      0, 0, hInstance, vo);
    }

    if (!w32->window) {
        MP_ERR(vo, "win32: unable to create window!\n");
        return 0;
    }

    if (OleInitialize(NULL) == S_OK) {
        fmtetc_url.cfFormat = (CLIPFORMAT)RegisterClipboardFormat(TEXT("UniformResourceLocator"));
        DropTarget* dropTarget = talloc(NULL, DropTarget);
        DropTarget_Init(dropTarget, vo);
        RegisterDragDrop(w32->window, &dropTarget->iface);
    }

    w32->tracking   = FALSE;
    w32->trackEvent = (TRACKMOUSEEVENT){
        .cbSize    = sizeof(TRACKMOUSEEVENT),
        .dwFlags   = TME_LEAVE,
        .hwndTrack = w32->window,
    };

    if (vo->opts->WinID >= 0)
        EnableWindow(w32->window, 0);

    w32->cursor_visible = true;

    // we don't have proper event handling
    vo->wakeup_period = 0.02;

    updateScreenProperties(vo);

    MP_VERBOSE(vo, "win32: running at %dx%d\n",
           vo->opts->screenwidth, vo->opts->screenheight);

    return 1;
}

/**
 * \brief Toogle fullscreen / windowed mode.
 *
 * Should be called on VOCTRL_FULLSCREEN event. The window is
 * always resized during this call, so the rendering context
 * should be reinitialized with the new dimensions.
 * It is unspecified if vo_check_events will create a resize
 * event in addition or not.
 */

static void vo_w32_fullscreen(struct vo *vo)
{
    if (vo->opts->fullscreen != vo->w32->current_fs)
        reinit_window_state(vo);
}

/**
 * \brief Toogle window border attribute.
 *
 * Should be called on VOCTRL_BORDER event.
 */
static void vo_w32_border(struct vo *vo)
{
    vo->opts->border = !vo->opts->border;
    reinit_window_state(vo);
}

/**
 * \brief Toogle window ontop attribute.
 *
 * Should be called on VOCTRL_ONTOP event.
 */
static void vo_w32_ontop(struct vo *vo)
{
    vo->opts->ontop = !vo->opts->ontop;
    reinit_window_state(vo);
}

static bool vo_w32_is_cursor_in_client(struct vo *vo)
{
    DWORD pos = GetMessagePos();
    return SendMessage(vo->w32->window, WM_NCHITTEST, 0, pos) == HTCLIENT;
}

int vo_w32_control(struct vo *vo, int *events, int request, void *arg)
{
    struct vo_w32_state *w32 = vo->w32;
    switch (request) {
    case VOCTRL_CHECK_EVENTS:
        *events |= vo_w32_check_events(vo);
        return VO_TRUE;
    case VOCTRL_FULLSCREEN:
        vo_w32_fullscreen(vo);
        *events |= VO_EVENT_RESIZE;
        return VO_TRUE;
    case VOCTRL_ONTOP:
        vo_w32_ontop(vo);
        return VO_TRUE;
    case VOCTRL_BORDER:
        vo_w32_border(vo);
        *events |= VO_EVENT_RESIZE;
        return VO_TRUE;
    case VOCTRL_UPDATE_SCREENINFO:
        w32_update_xinerama_info(vo);
        return VO_TRUE;
    case VOCTRL_GET_WINDOW_SIZE: {
        int *s = arg;

        if (!w32->window_bounds_initialized)
            return VO_FALSE;

        s[0] = w32->current_fs ? w32->prev_width : vo->dwidth;
        s[1] = w32->current_fs ? w32->prev_height : vo->dheight;
        return VO_TRUE;
    }
    case VOCTRL_SET_WINDOW_SIZE: {
        int *s = arg;

        if (!w32->window_bounds_initialized)
            return VO_FALSE;
        if (w32->current_fs) {
            w32->prev_width = s[0];
            w32->prev_height = s[1];
        } else {
            vo->dwidth = s[0];
            vo->dheight = s[1];
        }

        reinit_window_state(vo);
        *events |= VO_EVENT_RESIZE;
        return VO_TRUE;
    }
    case VOCTRL_SET_CURSOR_VISIBILITY:
        w32->cursor_visible = *(bool *)arg;

        if (vo_w32_is_cursor_in_client(vo)) {
            if (w32->cursor_visible)
                SetCursor(LoadCursor(NULL, IDC_ARROW));
            else
                SetCursor(NULL);
        }
        return VO_TRUE;
    case VOCTRL_KILL_SCREENSAVER:
        w32->disable_screensaver = true;
        SetThreadExecutionState(ES_CONTINUOUS | ES_DISPLAY_REQUIRED);
        return VO_TRUE;
    case VOCTRL_RESTORE_SCREENSAVER:
        w32->disable_screensaver = false;
        SetThreadExecutionState(ES_CONTINUOUS);
        return VO_TRUE;
    case VOCTRL_UPDATE_WINDOW_TITLE: {
        wchar_t *title = mp_from_utf8(NULL, (char *)arg);
        SetWindowTextW(w32->window, title);
        talloc_free(title);
        return VO_TRUE;
    }
    }
    return VO_NOTIMPL;
}

/**
 * \brief Uninitialize w32_common framework.
 *
 * Should be called last in video driver's uninit function. First release
 * anything built on top of the created window e.g. rendering context inside
 * and call vo_w32_uninit at the end.
 */
void vo_w32_uninit(struct vo *vo)
{
    struct vo_w32_state *w32 = vo->w32;
    MP_VERBOSE(vo, "win32: uninit\n");

    if (!w32)
        return;

    RevokeDragDrop(w32->window);
    OleUninitialize();
    SetThreadExecutionState(ES_CONTINUOUS);
    DestroyWindow(w32->window);
    UnregisterClassW(classname, 0);
    talloc_free(w32);
    vo->w32 = NULL;
}
