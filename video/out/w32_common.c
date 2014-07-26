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
#include "win_state.h"
#include "w32_common.h"
#include "osdep/io.h"
#include "osdep/w32_keyboard.h"
#include "talloc.h"

#define WIN_ID_TO_HWND(x) ((HWND)(intptr_t)(x))

static const wchar_t classname[] = L"mpv";

struct vo_w32_state {
    struct mp_log *log;
    struct vo *vo;
    struct mp_vo_opts *opts;
    struct input_ctx *input_ctx;

    HWND window;

    // Size and virtual position of the current screen.
    struct mp_rect screenrc;

    // last non-fullscreen extends (updated only on fullscreen or on initialization)
    int prev_width;
    int prev_height;
    int prev_x;
    int prev_y;

    // whether the window position and size were intialized
    bool window_bounds_initialized;

    bool current_fs;

    // currently known window state
    int window_x;
    int window_y;
    int dw;
    int dh;

    // video size
    uint32_t o_dwidth;
    uint32_t o_dheight;

    bool disable_screensaver;
    bool cursor_visible;
    int event_flags;
    int mon_cnt;
    int mon_id;

    BOOL tracking;
    TRACKMOUSEEVENT trackEvent;

    int mouse_x;
    int mouse_y;

    // UTF-16 decoding state for WM_CHAR and VK_PACKET
    int high_surrogate;
};

typedef struct tagDropTarget {
    IDropTarget iface;
    ULONG refCnt;
    DWORD lastEffect;
    IDataObject* dataObj;
    struct vo_w32_state *w32;
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

                    MP_VERBOSE(t->w32, "received dropped file: %s\n",
                               fname);
                } else {
                    MP_ERR(t->w32, "error getting dropped file name\n");
                }

                talloc_free(buf);
            }

            GlobalUnlock(medium.hGlobal);
            mp_event_drop_files(t->w32->input_ctx, nrecvd_files, files);

            talloc_free(files);
        }

        ReleaseStgMedium(&medium);
    } else if (pDataObj->lpVtbl->GetData(pDataObj,
                                         &fmtetc_url, &medium) == S_OK) {
        // get the URL encoded in US-ASCII
        char* url = (char*)GlobalLock(medium.hGlobal);
        if (url != NULL) {
            if (mp_event_drop_mime_data(t->w32->input_ctx, "text/uri-list",
                                        bstr0(url)) > 0) {
                MP_VERBOSE(t->w32, "received dropped URL: %s\n", url);
            } else {
                MP_ERR(t->w32, "error getting dropped URL\n");
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


static void DropTarget_Init(DropTarget* This, struct vo_w32_state *w32)
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
    This->w32 = w32;
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

static bool key_state(int vk)
{
    return GetKeyState(vk) & 0x8000;
}

static int mod_state(struct vo_w32_state *w32)
{
    int res = 0;

    // AltGr is represented as LCONTROL+RMENU on Windows
    bool alt_gr = mp_input_use_alt_gr(w32->input_ctx) &&
        key_state(VK_RMENU) && key_state(VK_LCONTROL);

    if (key_state(VK_RCONTROL) || (key_state(VK_LCONTROL) && !alt_gr))
        res |= MP_KEY_MODIFIER_CTRL;
    if (key_state(VK_SHIFT))
        res |= MP_KEY_MODIFIER_SHIFT;
    if (key_state(VK_LMENU) || (key_state(VK_RMENU) && !alt_gr))
        res |= MP_KEY_MODIFIER_ALT;
    return res;
}

static int decode_surrogate_pair(wchar_t lead, wchar_t trail)
{
    return 0x10000 + ((lead & 0x3ff) << 10) | (trail & 0x3ff);
}

static int decode_utf16(struct vo_w32_state *w32, wchar_t c)
{
    // Decode UTF-16, keeping state in w32->high_surrogate
    if (IS_HIGH_SURROGATE(c)) {
        w32->high_surrogate = c;
        return 0;
    }
    if (IS_LOW_SURROGATE(c)) {
        if (!w32->high_surrogate) {
            MP_ERR(w32, "Invalid UTF-16 input\n");
            return 0;
        }
        int codepoint = decode_surrogate_pair(w32->high_surrogate, c);
        w32->high_surrogate = 0;
        return codepoint;
    }
    if (w32->high_surrogate != 0) {
        w32->high_surrogate = 0;
        MP_ERR(w32, "Invalid UTF-16 input\n");
        return 0;
    }

    return c;
}

static void clear_keyboard_buffer(void)
{
    static const UINT vkey = VK_DECIMAL;
    static const BYTE keys[256] = { 0 };
    UINT scancode = MapVirtualKey(vkey, MAPVK_VK_TO_VSC);
    wchar_t buf[10];
    int ret = 0;

    // Use the method suggested by Michael Kaplan to clear any pending dead
    // keys from the current keyboard layout. See:
    // https://web.archive.org/web/20101004154432/http://blogs.msdn.com/b/michkap/archive/2006/04/06/569632.aspx
    // https://web.archive.org/web/20100820152419/http://blogs.msdn.com/b/michkap/archive/2007/10/27/5717859.aspx
    do {
        ret = ToUnicode(vkey, scancode, keys, buf, MP_ARRAY_SIZE(buf), 0);
    } while (ret < 0);
}

static int to_unicode(UINT vkey, UINT scancode, const BYTE keys[256])
{
    // This wraps ToUnicode to be stateless and to return only one character

    // Make the buffer 10 code units long to be safe, same as here:
    // https://web.archive.org/web/20101013215215/http://blogs.msdn.com/b/michkap/archive/2006/03/24/559169.aspx
    wchar_t buf[10] = { 0 };

    // Dead keys aren't useful for key shortcuts, so clear the keyboard state
    clear_keyboard_buffer();

    int len = ToUnicode(vkey, scancode, keys, buf, MP_ARRAY_SIZE(buf), 0);

    // Return the last complete UTF-16 code point. A negative return value
    // indicates a dead key, however there should still be a non-combining
    // version of the key in the buffer.
    if (len < 0)
        len = -len;
    if (len >= 2 && IS_SURROGATE_PAIR(buf[len - 2], buf[len - 1]))
        return decode_surrogate_pair(buf[len - 2], buf[len - 1]);
    if (len >= 1)
        return buf[len - 1];

    return 0;
}

static int decode_key(struct vo_w32_state *w32, UINT vkey, UINT scancode)
{
    BYTE keys[256];
    GetKeyboardState(keys);

    // If mp_input_use_alt_gr is false, detect and remove AltGr so normal
    // characters are generated. Note that AltGr is represented as
    // LCONTROL+RMENU on Windows.
    if ((keys[VK_RMENU] & 0x80) && (keys[VK_LCONTROL] & 0x80) &&
        !mp_input_use_alt_gr(w32->input_ctx))
    {
        keys[VK_RMENU] = keys[VK_LCONTROL] = 0;
        keys[VK_MENU] = keys[VK_LMENU];
        keys[VK_CONTROL] = keys[VK_RCONTROL];
    }

    int c = to_unicode(vkey, scancode, keys);

    // Some shift states prevent ToUnicode from working or cause it to produce
    // control characters. If this is detected, remove modifiers until it
    // starts producing normal characters.
    if (c < 0x20 && (keys[VK_MENU] & 0x80)) {
        keys[VK_LMENU] = keys[VK_RMENU] = keys[VK_MENU] = 0;
        c = to_unicode(vkey, scancode, keys);
    }
    if (c < 0x20 && (keys[VK_CONTROL] & 0x80)) {
        keys[VK_LCONTROL] = keys[VK_RCONTROL] = keys[VK_CONTROL] = 0;
        c = to_unicode(vkey, scancode, keys);
    }
    if (c < 0x20)
        return 0;

    // Decode lone UTF-16 surrogates (VK_PACKET can generate these)
    if (c < 0x10000)
        return decode_utf16(w32, c);
    return c;
}

static void handle_key_down(struct vo_w32_state *w32, UINT vkey, UINT scancode)
{
    // Ignore key repeat
    if (scancode & KF_REPEAT)
        return;

    int mpkey = mp_w32_vkey_to_mpkey(vkey, scancode & KF_EXTENDED);
    if (!mpkey) {
        mpkey = decode_key(w32, vkey, scancode & (0xff | KF_EXTENDED));
        if (!mpkey)
            return;
    }

    mp_input_put_key(w32->input_ctx, mpkey | mod_state(w32) | MP_KEY_STATE_DOWN);
}

static void handle_key_up(struct vo_w32_state *w32, UINT vkey, UINT scancode)
{
    switch (vkey) {
    case VK_MENU:
    case VK_CONTROL:
    case VK_SHIFT:
        break;
    default:
        // Releasing all keys on key-up is simpler and ensures no keys can be
        // get "stuck." This matches the behaviour of other VOs.
        mp_input_put_key(w32->input_ctx, MP_INPUT_RELEASE_ALL);
    }
}

static bool handle_char(struct vo_w32_state *w32, wchar_t wc)
{
    int c = decode_utf16(w32, wc);

    if (c == 0)
        return true;
    if (c < 0x20)
        return false;

    mp_input_put_key(w32->input_ctx, c | mod_state(w32));
    return true;
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam,
                                LPARAM lParam)
{
    if (message == WM_NCCREATE) {
        CREATESTRUCT *cs = (void*)lParam;
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
    }
    struct vo_w32_state *w32 = (void*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    // message before WM_NCCREATE, pray to Raymond Chen that it's not important
    if (!w32)
        return DefWindowProcW(hWnd, message, wParam, lParam);
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
        MP_VERBOSE(w32, "move window: %d:%d\n", w32->window_x, w32->window_y);
        break;
    }
    case WM_SIZE: {
        w32->event_flags |= VO_EVENT_RESIZE;
        RECT r;
        GetClientRect(w32->window, &r);
        w32->dw = r.right;
        w32->dh = r.bottom;
        w32->vo->dwidth = w32->dw;
        w32->vo->dheight = w32->dh;
        MP_VERBOSE(w32, "resize window: %d:%d\n", w32->dw, w32->dh);
        break;
    }
    case WM_SIZING:
        if (w32->opts->keepaspect && !w32->opts->fullscreen &&
            w32->opts->WinID < 0)
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
        mp_input_put_key(w32->input_ctx, MP_KEY_CLOSE_WIN);
        break;
    case WM_SYSCOMMAND:
        switch (wParam) {
        case SC_SCREENSAVE:
        case SC_MONITORPOWER:
            if (w32->disable_screensaver) {
                MP_VERBOSE(w32, "killing screensaver\n");
                return 0;
            }
            break;
        }
        break;
    case WM_SYSKEYDOWN:
    case WM_KEYDOWN:
        handle_key_down(w32, wParam, HIWORD(lParam));
        if (wParam == VK_F10)
            return 0;
        break;
    case WM_SYSKEYUP:
    case WM_KEYUP:
        handle_key_up(w32, wParam, HIWORD(lParam));
        if (wParam == VK_F10)
            return 0;
        break;
    case WM_CHAR:
    case WM_SYSCHAR:
        if (handle_char(w32, wParam))
            return 0;
        break;
    case WM_KILLFOCUS:
        mp_input_put_key(w32->input_ctx, MP_INPUT_RELEASE_ALL);
        break;
    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT && !w32->cursor_visible) {
            SetCursor(NULL);
            return TRUE;
        }
        break;
    case WM_MOUSELEAVE:
        w32->tracking = FALSE;
        mp_input_put_key(w32->input_ctx, MP_KEY_MOUSE_LEAVE);
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
            vo_mouse_movement(w32->vo, x, y);
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
        mouse_button |= mod_state(w32);
        mp_input_put_key(w32->input_ctx, mouse_button);

        if (w32->opts->enable_mouse_movements) {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);

            if (mouse_button == (MP_MOUSE_BTN0 | MP_KEY_STATE_DOWN) &&
                !w32->opts->fullscreen &&
                !mp_input_test_dragging(w32->input_ctx, x, y))
            {
                // Window dragging hack
                ReleaseCapture();
                SendMessage(hWnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
                mp_input_put_key(w32->input_ctx, MP_MOUSE_BTN0 |
                                                 MP_KEY_STATE_UP);
                return 0;
            }
        }

        if (mouse_button & MP_KEY_STATE_DOWN)
            SetCapture(w32->window);
        else
            ReleaseCapture();
    }

    return DefWindowProcW(hWnd, message, wParam, lParam);
}

static bool is_key_message(UINT msg)
{
    return msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN ||
           msg == WM_KEYUP || msg == WM_SYSKEYUP;
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
static int vo_w32_check_events(struct vo_w32_state *w32)
{
    MSG msg;
    w32->event_flags = 0;

    while (PeekMessageW(&msg, 0, 0, 0, PM_REMOVE)) {
        // Only send IME messages to TranslateMessage
        if (is_key_message(msg.message) && msg.wParam == VK_PROCESSKEY)
            TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (w32->opts->WinID >= 0) {
        BOOL res;
        RECT r;
        POINT p;
        res = GetClientRect(w32->window, &r);

        if (res && (r.right != w32->dw || r.bottom != w32->dh)) {
            w32->dw = r.right; w32->dh = r.bottom;
            w32->vo->dwidth = w32->dw; w32->vo->dheight = w32->dh;
            w32->event_flags |= VO_EVENT_RESIZE;
        }

        p.x = 0; p.y = 0;
        ClientToScreen(w32->window, &p);

        if (p.x != w32->window_x || p.y != w32->window_y) {
            w32->window_x = p.x; w32->window_y = p.y;
        }

        res = GetClientRect(WIN_ID_TO_HWND(w32->opts->WinID), &r);

        if (res && (r.right != w32->dw || r.bottom != w32->dh))
            MoveWindow(w32->window, 0, 0, r.right, r.bottom, FALSE);

        if (!IsWindow(WIN_ID_TO_HWND(w32->opts->WinID))) {
            // Window has probably been closed, e.g. due to program crash
            mp_input_put_key(w32->input_ctx, MP_KEY_CLOSE_WIN);
        }
    }

    return w32->event_flags;
}

static BOOL CALLBACK mon_enum(HMONITOR hmon, HDC hdc, LPRECT r, LPARAM p)
{
    struct vo_w32_state *w32 = (void *)p;
    // this defaults to the last screen if specified number does not exist
    w32->screenrc = (struct mp_rect){r->left, r->top, r->right, r->bottom};

    if (w32->mon_cnt == w32->mon_id)
        return FALSE;

    w32->mon_cnt++;
    return TRUE;
}

static void w32_update_xinerama_info(struct vo_w32_state *w32)
{
    struct mp_vo_opts *opts = w32->opts;
    int screen = opts->fullscreen ? opts->fsscreen_id : opts->screen_id;

    if (opts->fullscreen && screen == -2) {
        struct mp_rect rc = {
            GetSystemMetrics(SM_XVIRTUALSCREEN),
            GetSystemMetrics(SM_YVIRTUALSCREEN),
            GetSystemMetrics(SM_CXVIRTUALSCREEN),
            GetSystemMetrics(SM_CYVIRTUALSCREEN),
        };
        if (!rc.x1 || !rc.y1) {
            rc.x0 = rc.y0 = 0;
            rc.x1 = w32->screenrc.x1;
            rc.y1 = w32->screenrc.y1;
        }
        rc.x1 += rc.x0;
        rc.y1 += rc.y0;
        w32->screenrc = rc;
    } else if (screen == -1) {
        MONITORINFO mi;
        HMONITOR m = MonitorFromWindow(w32->window, MONITOR_DEFAULTTOPRIMARY);
        mi.cbSize = sizeof(mi);
        GetMonitorInfoW(m, &mi);
        w32->screenrc = (struct mp_rect){
            mi.rcMonitor.left, mi.rcMonitor.top,
            mi.rcMonitor.right, mi.rcMonitor.bottom,
        };
    } else if (screen >= 0) {
        w32->mon_cnt = 0;
        w32->mon_id = screen;
        EnumDisplayMonitors(NULL, NULL, mon_enum, (LONG_PTR)w32);
    }
}

static void updateScreenProperties(struct vo_w32_state *w32)
{
    DEVMODE dm;
    dm.dmSize = sizeof dm;
    dm.dmDriverExtra = 0;
    dm.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;

    if (!EnumDisplaySettings(0, ENUM_CURRENT_SETTINGS, &dm)) {
        MP_ERR(w32, "unable to enumerate display settings!\n");
        return;
    }

    w32->screenrc = (struct mp_rect){0, 0, dm.dmPelsWidth, dm.dmPelsHeight};
    w32_update_xinerama_info(w32);
}

static DWORD update_style(struct vo_w32_state *w32, DWORD style)
{
    const DWORD NO_FRAME = WS_POPUP;
    const DWORD FRAME = WS_OVERLAPPEDWINDOW | WS_SIZEBOX;
    style &= ~(NO_FRAME | FRAME);
    style |= (w32->opts->border && !w32->opts->fullscreen) ? FRAME : NO_FRAME;
    return style;
}

// Update the window title, position, size, and border style.
static int reinit_window_state(struct vo_w32_state *w32)
{
    HWND layer = HWND_NOTOPMOST;
    RECT r;

    if (w32->opts->WinID >= 0)
        return 1;

    bool toggle_fs = w32->current_fs != w32->opts->fullscreen;
    w32->current_fs = w32->opts->fullscreen;

    DWORD style = update_style(w32, GetWindowLong(w32->window, GWL_STYLE));

    if (w32->opts->ontop)
        layer = HWND_TOPMOST;

    // xxx not sure if this can trigger any unwanted messages (WM_MOVE/WM_SIZE)
    updateScreenProperties(w32);

    if (w32->opts->fullscreen) {
        // Save window position and size when switching to fullscreen.
        if (toggle_fs) {
            w32->prev_width = w32->dw;
            w32->prev_height = w32->dh;
            w32->prev_x = w32->window_x;
            w32->prev_y = w32->window_y;
            MP_VERBOSE(w32, "save window bounds: %d:%d:%d:%d\n",
                   w32->prev_x, w32->prev_y, w32->prev_width, w32->prev_height);
        }

        w32->window_x = w32->screenrc.x0;
        w32->window_y = w32->screenrc.y0;
        w32->dw = w32->screenrc.x1 - w32->screenrc.x0;
        w32->dh = w32->screenrc.y1 - w32->screenrc.y0;
        style &= ~WS_OVERLAPPEDWINDOW;
    } else {
        if (toggle_fs) {
            // Restore window position and size when switching from fullscreen.
            MP_VERBOSE(w32, "restore window bounds: %d:%d:%d:%d\n",
                   w32->prev_x, w32->prev_y, w32->prev_width, w32->prev_height);
            w32->dw = w32->prev_width;
            w32->dh = w32->prev_height;
            w32->window_x = w32->prev_x;
            w32->window_y = w32->prev_y;
        }
    }
    w32->vo->dwidth = w32->dw;
    w32->vo->dheight = w32->dh;

    r.left = w32->window_x;
    r.right = r.left + w32->dw;
    r.top = w32->window_y;
    r.bottom = r.top + w32->dh;

    SetWindowLong(w32->window, GWL_STYLE, style);
    add_window_borders(w32->window, &r);

    MP_VERBOSE(w32, "reset window bounds: %d:%d:%d:%d\n",
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
int vo_w32_config(struct vo *vo, uint32_t flags)
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
        MP_ERR(w32, "unable to select a valid pixel format!\n");
        ReleaseDC(w32->window, vo_hdc);
        return 0;
    }

    SetPixelFormat(vo_hdc, pf, &pfd);
    ReleaseDC(w32->window, vo_hdc);

    // we already have a fully initialized window, so nothing needs to be done
    if (flags & VOFLAG_HIDDEN)
        return 1;

    struct vo_win_geometry geo;
    vo_calc_window_geometry(vo, &w32->screenrc, &geo);
    vo_apply_window_geometry(vo, &geo);
    w32->dw = vo->dwidth;
    w32->dh = vo->dheight;

    bool reset_size = w32->o_dwidth != vo->dwidth || w32->o_dheight != vo->dheight;

    w32->o_dwidth = vo->dwidth;
    w32->o_dheight = vo->dheight;

    // the desired size is ignored in wid mode, it always matches the window size.
    if (w32->opts->WinID < 0) {
        if (w32->window_bounds_initialized) {
            // restore vo_dwidth/vo_dheight, which are reset against our will
            // in vo_config()
            RECT r;
            GetClientRect(w32->window, &r);
            vo->dwidth = r.right;
            vo->dheight = r.bottom;
        } else {
            w32->window_bounds_initialized = true;
            reset_size = true;
            w32->window_x = w32->prev_x = geo.win.x0;
            w32->window_y = w32->prev_y = geo.win.y0;
        }

        if (reset_size) {
            w32->prev_width = vo->dwidth = w32->o_dwidth;
            w32->prev_height = vo->dheight = w32->o_dheight;
        }
    } else {
        RECT r;
        GetClientRect(w32->window, &r);
        vo->dwidth = r.right;
        vo->dheight = r.bottom;
    }

    return reinit_window_state(w32);
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
 * \return 1 = Success, 0 = Failure
 */
int vo_w32_init(struct vo *vo)
{
    assert(!vo->w32);

    struct vo_w32_state *w32 = talloc_ptrtype(vo, w32);
    vo->w32 = w32;
    *w32 = (struct vo_w32_state){
        .log = mp_log_new(w32, vo->log, "win32"),
        .vo = vo,
        .opts = vo->opts,
        .input_ctx = vo->input_ctx,
    };

    HINSTANCE hInstance = GetModuleHandleW(NULL);

    WNDCLASSEXW wcex = {
        .cbSize = sizeof wcex,
        .style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW,
        .lpfnWndProc = WndProc,
        .hInstance = hInstance,
        .hIcon = LoadIconW(hInstance, L"IDI_ICON1"),
        .hCursor = LoadCursor(NULL, IDC_ARROW),
        .lpszClassName = classname,
    };

    if (!RegisterClassExW(&wcex)) {
        MP_ERR(w32, "unable to register window class!\n");
        return 0;
    }

    if (w32->opts->WinID >= 0) {
        RECT r;
        GetClientRect(WIN_ID_TO_HWND(w32->opts->WinID), &r);
        w32->window = CreateWindowExW(WS_EX_NOPARENTNOTIFY, classname,
                                      classname,
                                      WS_CHILD | WS_VISIBLE,
                                      0, 0, r.right, r.bottom,
                                      WIN_ID_TO_HWND(w32->opts->WinID),
                                      0, hInstance, w32);
    } else {
        w32->window = CreateWindowExW(0, classname,
                                      classname,
                                      update_style(w32, 0),
                                      CW_USEDEFAULT, 0, 100, 100,
                                      0, 0, hInstance, w32);
    }

    if (!w32->window) {
        MP_ERR(w32, "unable to create window!\n");
        return 0;
    }

    if (OleInitialize(NULL) == S_OK) {
        fmtetc_url.cfFormat = (CLIPFORMAT)RegisterClipboardFormat(TEXT("UniformResourceLocator"));
        DropTarget* dropTarget = talloc(NULL, DropTarget);
        DropTarget_Init(dropTarget, w32);
        RegisterDragDrop(w32->window, &dropTarget->iface);
    }

    w32->tracking   = FALSE;
    w32->trackEvent = (TRACKMOUSEEVENT){
        .cbSize    = sizeof(TRACKMOUSEEVENT),
        .dwFlags   = TME_LEAVE,
        .hwndTrack = w32->window,
    };

    if (w32->opts->WinID >= 0)
        EnableWindow(w32->window, 0);

    w32->cursor_visible = true;

    // we don't have proper event handling
    vo->wakeup_period = 0.02;

    updateScreenProperties(w32);

    return 1;
}

static bool vo_w32_is_cursor_in_client(struct vo_w32_state *w32)
{
    DWORD pos = GetMessagePos();
    return SendMessage(w32->window, WM_NCHITTEST, 0, pos) == HTCLIENT;
}

int vo_w32_control(struct vo *vo, int *events, int request, void *arg)
{
    struct vo_w32_state *w32 = vo->w32;
    switch (request) {
    case VOCTRL_CHECK_EVENTS:
        *events |= vo_w32_check_events(w32);
        return VO_TRUE;
    case VOCTRL_FULLSCREEN:
        if (w32->opts->fullscreen != w32->current_fs)
            reinit_window_state(w32);
        *events |= VO_EVENT_RESIZE;
        return VO_TRUE;
    case VOCTRL_ONTOP:
        w32->opts->ontop = !w32->opts->ontop;
        reinit_window_state(w32);
        return VO_TRUE;
    case VOCTRL_BORDER:
        w32->opts->border = !w32->opts->border;
        reinit_window_state(vo->w32);
        *events |= VO_EVENT_RESIZE;
        return VO_TRUE;
    case VOCTRL_GET_WINDOW_SIZE: {
        int *s = arg;

        if (!w32->window_bounds_initialized)
            return VO_FALSE;

        s[0] = w32->current_fs ? w32->prev_width : w32->dw;
        s[1] = w32->current_fs ? w32->prev_height : w32->dh;
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
            w32->dw = s[0];
            w32->dh = s[1];
        }

        reinit_window_state(w32);
        *events |= VO_EVENT_RESIZE;
        return VO_TRUE;
    }
    case VOCTRL_SET_CURSOR_VISIBILITY:
        w32->cursor_visible = *(bool *)arg;

        if (vo_w32_is_cursor_in_client(w32)) {
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
    if (!w32)
        return;

    MP_VERBOSE(w32, "uninit\n");

    RevokeDragDrop(w32->window);
    OleUninitialize();
    SetThreadExecutionState(ES_CONTINUOUS);
    DestroyWindow(w32->window);
    UnregisterClassW(classname, 0);
    talloc_free(w32);
    vo->w32 = NULL;
}

HWND vo_w32_hwnd(struct vo *vo)
{
    struct vo_w32_state *w32 = vo->w32;
    return w32->window;
}
