/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <limits.h>
#include <pthread.h>
#include <assert.h>
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <ole2.h>
#include <shobjidl.h>
#include <avrt.h>

#include "options/options.h"
#include "input/keycodes.h"
#include "input/input.h"
#include "input/event.h"
#include "stream/stream.h"
#include "common/msg.h"
#include "common/common.h"
#include "vo.h"
#include "win_state.h"
#include "w32_common.h"
#include "win32/displayconfig.h"
#include "win32/droptarget.h"
#include "osdep/io.h"
#include "osdep/threads.h"
#include "osdep/w32_keyboard.h"
#include "osdep/atomic.h"
#include "misc/dispatch.h"
#include "misc/rendezvous.h"
#include "mpv_talloc.h"

EXTERN_C IMAGE_DOS_HEADER __ImageBase;
#define HINST_THISCOMPONENT ((HINSTANCE)&__ImageBase)

#ifndef WM_DPICHANGED
#define WM_DPICHANGED (0x02E0)
#endif

#ifndef DPI_ENUMS_DECLARED
typedef enum MONITOR_DPI_TYPE {
    MDT_EFFECTIVE_DPI = 0,
    MDT_ANGULAR_DPI = 1,
    MDT_RAW_DPI = 2,
    MDT_DEFAULT = MDT_EFFECTIVE_DPI
} MONITOR_DPI_TYPE;
#endif

#define rect_w(r) ((r).right - (r).left)
#define rect_h(r) ((r).bottom - (r).top)

struct w32_api {
    HRESULT (WINAPI *pGetDpiForMonitor)(HMONITOR, MONITOR_DPI_TYPE, UINT*, UINT*);
    BOOL (WINAPI *pImmDisableIME)(DWORD);
};

struct vo_w32_state {
    struct mp_log *log;
    struct vo *vo;
    struct mp_vo_opts *opts;
    struct input_ctx *input_ctx;

    pthread_t thread;
    bool terminate;
    struct mp_dispatch_queue *dispatch; // used to run stuff on the GUI thread
    bool in_dispatch;

    struct w32_api api; // stores functions from dynamically loaded DLLs

    HWND window;
    HWND parent; // 0 normally, set in embedding mode
    HHOOK parent_win_hook;
    HWINEVENTHOOK parent_evt_hook;

    HMONITOR monitor; // Handle of the current screen
    char *color_profile; // Path of the current screen's color profile

    // Has the window seen a WM_DESTROY? If so, don't call DestroyWindow again.
    bool destroyed;

    // whether the window position and size were intialized
    bool window_bounds_initialized;

    bool current_fs;
    bool toggle_fs; // whether the current fullscreen state needs to be switched

    RECT windowrc; // currently known window rect
    RECT screenrc; // current screen rect
    // last non-fullscreen rect, updated only on fullscreen or on initialization
    RECT prev_windowrc;

    // video size
    uint32_t o_dwidth;
    uint32_t o_dheight;

    int dpi;

    bool disable_screensaver;
    bool cursor_visible;
    atomic_uint event_flags;

    BOOL tracking;
    TRACKMOUSEEVENT trackEvent;

    int mouse_x;
    int mouse_y;

    // Should SetCursor be called when handling VOCTRL_SET_CURSOR_VISIBILITY?
    bool can_set_cursor;

    // UTF-16 decoding state for WM_CHAR and VK_PACKET
    int high_surrogate;

    // Whether to fit the window on screen on next window state updating
    bool fit_on_screen;

    ITaskbarList2 *taskbar_list;
    ITaskbarList3 *taskbar_list3;
    UINT tbtnCreatedMsg;
    bool tbtnCreated;

    struct voctrl_playback_state current_pstate;

    // updates on move/resize/displaychange
    double display_fps;

    bool moving;
    bool snapped;
    int snap_dx;
    int snap_dy;

    HANDLE avrt_handle;
};

static void add_window_borders(HWND hwnd, RECT *rc)
{
    AdjustWindowRect(rc, GetWindowLongPtrW(hwnd, GWL_STYLE), 0);
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

static LRESULT borderless_nchittest(struct vo_w32_state *w32, int x, int y)
{
    if (IsMaximized(w32->window))
        return HTCLIENT;

    POINT mouse = { x, y };
    ScreenToClient(w32->window, &mouse);

    // The horizontal frame should be the same size as the vertical frame,
    // since the NONCLIENTMETRICS structure does not distinguish between them
    int frame_size = GetSystemMetrics(SM_CXFRAME) +
                     GetSystemMetrics(SM_CXPADDEDBORDER);
    // The diagonal size handles are slightly wider than the side borders
    int diagonal_width = frame_size * 2 + GetSystemMetrics(SM_CXBORDER);

    // Hit-test top border
    if (mouse.y < frame_size) {
        if (mouse.x < diagonal_width)
            return HTTOPLEFT;
        if (mouse.x >= rect_w(w32->windowrc) - diagonal_width)
            return HTTOPRIGHT;
        return HTTOP;
    }

    // Hit-test bottom border
    if (mouse.y >= rect_h(w32->windowrc) - frame_size) {
        if (mouse.x < diagonal_width)
            return HTBOTTOMLEFT;
        if (mouse.x >= rect_w(w32->windowrc) - diagonal_width)
            return HTBOTTOMRIGHT;
        return HTBOTTOM;
    }

    // Hit-test side borders
    if (mouse.x < frame_size)
        return HTLEFT;
    if (mouse.x >= rect_w(w32->windowrc) - frame_size)
        return HTRIGHT;
    return HTCLIENT;
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
    return 0x10000 + (((lead & 0x3ff) << 10) | (trail & 0x3ff));
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

static bool handle_appcommand(struct vo_w32_state *w32, UINT cmd)
{
    if (!mp_input_use_media_keys(w32->input_ctx))
        return false;
    int mpkey = mp_w32_appcmd_to_mpkey(cmd);
    if (!mpkey)
        return false;
    mp_input_put_key(w32->input_ctx, mpkey | mod_state(w32));
    return true;
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

static bool handle_mouse_down(struct vo_w32_state *w32, int btn, int x, int y)
{
    btn |= mod_state(w32);
    mp_input_put_key(w32->input_ctx, btn | MP_KEY_STATE_DOWN);

    if (btn == MP_MBTN_LEFT && !w32->current_fs &&
        !mp_input_test_dragging(w32->input_ctx, x, y))
    {
        // Window dragging hack
        ReleaseCapture();
        SendMessage(w32->window, WM_NCLBUTTONDOWN, HTCAPTION, 0);
        mp_input_put_key(w32->input_ctx, MP_MBTN_LEFT | MP_KEY_STATE_UP);

        // Indicate the message was handled, so DefWindowProc won't be called
        return true;
    }

    SetCapture(w32->window);
    return false;
}

static void handle_mouse_up(struct vo_w32_state *w32, int btn)
{
    btn |= mod_state(w32);
    mp_input_put_key(w32->input_ctx, btn | MP_KEY_STATE_UP);

    ReleaseCapture();
}

static void handle_mouse_wheel(struct vo_w32_state *w32, bool horiz, int val)
{
    int code;
    if (horiz)
        code = val > 0 ? MP_WHEEL_RIGHT : MP_WHEEL_LEFT;
    else
        code = val > 0 ? MP_WHEEL_UP : MP_WHEEL_DOWN;
    mp_input_put_wheel(w32->input_ctx, code | mod_state(w32), abs(val) / 120.);
}

static void signal_events(struct vo_w32_state *w32, int events)
{
    atomic_fetch_or(&w32->event_flags, events);
    vo_wakeup(w32->vo);
}

static void wakeup_gui_thread(void *ctx)
{
    struct vo_w32_state *w32 = ctx;
    // Wake up the window procedure (which processes the dispatch queue)
    if (GetWindowThreadProcessId(w32->window, NULL) == GetCurrentThreadId()) {
        PostMessageW(w32->window, WM_NULL, 0, 0);
    } else {
        // Use a sent message when cross-thread, since the queue of sent
        // messages is processed in some cases when posted messages are blocked
        SendNotifyMessageW(w32->window, WM_NULL, 0, 0);
    }
}

static double get_refresh_rate_from_gdi(const wchar_t *device)
{
    DEVMODEW dm = { .dmSize = sizeof dm };
    if (!EnumDisplaySettingsW(device, ENUM_CURRENT_SETTINGS, &dm))
        return 0.0;

    // May return 0 or 1 which "represent the display hardware's default refresh rate"
    // https://msdn.microsoft.com/en-us/library/windows/desktop/dd183565%28v=vs.85%29.aspx
    // mpv validates this value with a threshold of 1, so don't return exactly 1
    if (dm.dmDisplayFrequency == 1)
        return 0.0;

    // dm.dmDisplayFrequency is an integer which is rounded down, so it's
    // highly likely that 23 represents 24/1.001, 59 represents 60/1.001, etc.
    // A caller can always reproduce the original value by using floor.
    double rv = dm.dmDisplayFrequency;
    switch (dm.dmDisplayFrequency) {
        case  23:
        case  29:
        case  47:
        case  59:
        case  71:
        case  89:
        case  95:
        case 119:
        case 143:
            rv = (rv + 1) / 1.001;
    }

    return rv;
}

static char *get_color_profile(void *ctx, const wchar_t *device)
{
    char *name = NULL;

    HDC ic = CreateICW(device, NULL, NULL, NULL);
    if (!ic)
        goto done;
    wchar_t wname[MAX_PATH + 1];
    if (!GetICMProfileW(ic, &(DWORD){ MAX_PATH }, wname))
        goto done;

    name = mp_to_utf8(ctx, wname);
done:
    if (ic)
        DeleteDC(ic);
    return name;
}

static void update_dpi(struct vo_w32_state *w32)
{
    UINT dpiX, dpiY;
    if (w32->api.pGetDpiForMonitor && w32->api.pGetDpiForMonitor(w32->monitor,
                                     MDT_EFFECTIVE_DPI, &dpiX, &dpiY) == S_OK) {
        w32->dpi = (int)dpiX;
        MP_VERBOSE(w32, "DPI detected from the new API: %d\n", w32->dpi);
        return;
    }
    HDC hdc = GetDC(NULL);
    if (hdc) {
        w32->dpi = GetDeviceCaps(hdc, LOGPIXELSX);
        ReleaseDC(NULL, hdc);
        MP_VERBOSE(w32, "DPI detected from the old API: %d\n", w32->dpi);
    } else {
        w32->dpi = 96;
        MP_VERBOSE(w32, "Couldn't determine DPI, falling back to %d\n", w32->dpi);
    }
}

static void update_display_info(struct vo_w32_state *w32)
{
    HMONITOR monitor = MonitorFromWindow(w32->window, MONITOR_DEFAULTTOPRIMARY);
    if (w32->monitor == monitor)
        return;
    w32->monitor = monitor;

    update_dpi(w32);

    MONITORINFOEXW mi = { .cbSize = sizeof mi };
    GetMonitorInfoW(monitor, (MONITORINFO*)&mi);

    // Try to get the monitor refresh rate.
    double freq = 0.0;

    if (freq == 0.0)
        freq = mp_w32_displayconfig_get_refresh_rate(mi.szDevice);
    if (freq == 0.0)
        freq = get_refresh_rate_from_gdi(mi.szDevice);

    if (freq != w32->display_fps) {
        MP_VERBOSE(w32, "display-fps: %f\n", freq);
        if (freq == 0.0)
            MP_WARN(w32, "Couldn't determine monitor refresh rate\n");
        w32->display_fps = freq;
        signal_events(w32, VO_EVENT_WIN_STATE);
    }

    char *color_profile = get_color_profile(w32, mi.szDevice);
    if ((color_profile == NULL) != (w32->color_profile == NULL) ||
        (color_profile && strcmp(color_profile, w32->color_profile)))
    {
        if (color_profile)
            MP_VERBOSE(w32, "color-profile: %s\n", color_profile);
        talloc_free(w32->color_profile);
        w32->color_profile = color_profile;
        color_profile = NULL;
        signal_events(w32, VO_EVENT_ICC_PROFILE_CHANGED);
    }

    talloc_free(color_profile);
}

static void force_update_display_info(struct vo_w32_state *w32)
{
    w32->monitor = 0;
    update_display_info(w32);
}

static void update_playback_state(struct vo_w32_state *w32)
{
    struct voctrl_playback_state *pstate = &w32->current_pstate;

    if (!w32->taskbar_list3 || !w32->tbtnCreated)
        return;

    if (!pstate->playing || !pstate->taskbar_progress) {
        ITaskbarList3_SetProgressState(w32->taskbar_list3, w32->window,
                                       TBPF_NOPROGRESS);
        return;
    }

    ITaskbarList3_SetProgressValue(w32->taskbar_list3, w32->window,
                                   pstate->percent_pos, 100);
    ITaskbarList3_SetProgressState(w32->taskbar_list3, w32->window,
                                   pstate->paused ? TBPF_PAUSED :
                                                    TBPF_NORMAL);
}

static bool snap_to_screen_edges(struct vo_w32_state *w32, RECT *rc)
{
    if (w32->parent || w32->current_fs || IsMaximized(w32->window))
        return false;

    if (!w32->opts->snap_window) {
        w32->snapped = false;
        return false;
    }

    RECT rect;
    POINT cursor;
    if (!GetWindowRect(w32->window, &rect) || !GetCursorPos(&cursor))
        return false;
    // Check if window is going to be aero-snapped
    if (rect_w(*rc) != rect_w(rect) || rect_h(*rc) != rect_h(rect))
        return false;

    // Check if window has already been aero-snapped
    WINDOWPLACEMENT wp = {0};
    wp.length = sizeof(wp);
    if (!GetWindowPlacement(w32->window, &wp))
        return false;
    RECT wr = wp.rcNormalPosition;
    if (rect_w(*rc) != rect_w(wr) || rect_h(*rc) != rect_h(wr))
        return false;

    MONITORINFO mi = { .cbSize = sizeof(mi) };
    if (!GetMonitorInfoW(w32->monitor, &mi))
        return false;
    // Get the work area to let the window snap to taskbar
    wr = mi.rcWork;

    // Check for invisible borders and adjust the work area size
    RECT frame = {0};
    if (DwmGetWindowAttribute(w32->window, DWMWA_EXTENDED_FRAME_BOUNDS,
                              &frame, sizeof(RECT)) == S_OK) {
        wr.left -= frame.left - rect.left;
        wr.top -= frame.top - rect.top;
        wr.right += rect.right - frame.right;
        wr.bottom += rect.bottom - frame.bottom;
    }

    // Let the window to unsnap by changing its position,
    // otherwise it will stick to the screen edges forever
    rect = *rc;
    if (w32->snapped) {
        OffsetRect(&rect, cursor.x - rect.left - w32->snap_dx,
                          cursor.y - rect.top - w32->snap_dy);
    }

    int threshold = (w32->dpi * 16) / 96;
    bool snapped = false;
    // Adjust X position
    if (abs(rect.left - wr.left) < threshold) {
        snapped = true;
        OffsetRect(&rect, wr.left - rect.left, 0);
    } else if (abs(rect.right - wr.right) < threshold) {
        snapped = true;
        OffsetRect(&rect, wr.right - rect.right, 0);
    }
    // Adjust Y position
    if (abs(rect.top - wr.top) < threshold) {
        snapped = true;
        OffsetRect(&rect, 0, wr.top - rect.top);
    } else if (abs(rect.bottom - wr.bottom) < threshold) {
        snapped = true;
        OffsetRect(&rect, 0, wr.bottom - rect.bottom);
    }

    if (!w32->snapped && snapped) {
        w32->snap_dx = cursor.x - rc->left;
        w32->snap_dy = cursor.y - rc->top;
    }

    w32->snapped = snapped;
    *rc = rect;
    return true;
}

struct get_monitor_data {
    int i;
    int target;
    HMONITOR mon;
};

static BOOL CALLBACK get_monitor_proc(HMONITOR mon, HDC dc, LPRECT r, LPARAM p)
{
    struct get_monitor_data *data = (struct get_monitor_data*)p;

    if (data->i == data->target) {
        data->mon = mon;
        return FALSE;
    }
    data->i++;
    return TRUE;
}

static HMONITOR get_monitor(int id)
{
    struct get_monitor_data data = { .target = id };
    EnumDisplayMonitors(NULL, NULL, get_monitor_proc, (LPARAM)&data);
    return data.mon;
}

static void update_screen_rect(struct vo_w32_state *w32)
{
    struct mp_vo_opts *opts = w32->opts;
    int screen = w32->current_fs ? opts->fsscreen_id : opts->screen_id;

    // Handle --fs-screen=all
    if (w32->current_fs && screen == -2) {
        const int x = GetSystemMetrics(SM_XVIRTUALSCREEN);
        const int y = GetSystemMetrics(SM_YVIRTUALSCREEN);
        SetRect(&w32->screenrc, x, y, x + GetSystemMetrics(SM_CXVIRTUALSCREEN),
                                      y + GetSystemMetrics(SM_CYVIRTUALSCREEN));
        return;
    }

    // When not using --fs-screen=all, mpv belongs to a specific HMONITOR
    HMONITOR mon;
    if (screen == -1) {
        // Handle --fs-screen=current and --screen=default
        mon = MonitorFromWindow(w32->window, MONITOR_DEFAULTTOPRIMARY);
    } else {
        mon = get_monitor(screen);
        if (!mon) {
            MP_INFO(w32, "Screen %d does not exist, falling back to primary\n",
                    screen);
            mon = MonitorFromPoint((POINT){0, 0}, MONITOR_DEFAULTTOPRIMARY);
        }
    }

    MONITORINFO mi = { .cbSize = sizeof(mi) };
    GetMonitorInfoW(mon, &mi);
    w32->screenrc = mi.rcMonitor;
}

static DWORD update_style(struct vo_w32_state *w32, DWORD style)
{
    const DWORD NO_FRAME = WS_OVERLAPPED | WS_MINIMIZEBOX;
    const DWORD FRAME = WS_OVERLAPPEDWINDOW;
    const DWORD FULLSCREEN = NO_FRAME | WS_SYSMENU;
    style &= ~(NO_FRAME | FRAME | FULLSCREEN);
    if (w32->current_fs) {
        style |= FULLSCREEN;
    } else {
        style |= w32->opts->border ? FRAME : NO_FRAME;
    }
    return style;
}

static void update_window_style(struct vo_w32_state *w32)
{
    if (w32->parent)
        return;

    // SetWindowLongPtr can trigger a WM_SIZE event, so window rect
    // has to be saved now and restored after setting the new style.
    const RECT wr = w32->windowrc;
    const DWORD style = GetWindowLongPtrW(w32->window, GWL_STYLE);
    SetWindowLongPtrW(w32->window, GWL_STYLE, update_style(w32, style));
    w32->windowrc = wr;
}

// Adjust rc size and position if its size is larger than rc2.
// returns true if the rectangle was modified.
static bool fit_rect(RECT *rc, RECT *rc2)
{
    // Calculate old size and maximum new size
    int o_w = rect_w(*rc), o_h = rect_h(*rc);
    int n_w = rect_w(*rc2), n_h = rect_h(*rc2);
    if (o_w <= n_w && o_h <= n_h)
        return false;

    // Apply letterboxing
    const float o_asp = o_w / (float)MPMAX(o_h, 1);
    const float n_asp = n_w / (float)MPMAX(n_h, 1);
    if (o_asp > n_asp) {
        n_h = n_w / o_asp;
    } else {
        n_w = n_h * o_asp;
    }

    // Calculate new position and save the rect
    const int x = rc->left + o_w / 2 - n_w / 2;
    const int y = rc->top + o_h / 2 - n_h / 2;
    SetRect(rc, x, y, x + n_w, y + n_h);
    return true;
}

// Adjust window size and position if its size is larger than the screen size.
static void fit_window_on_screen(struct vo_w32_state *w32)
{
    if (w32->parent || w32->current_fs || IsMaximized(w32->window))
        return;

    RECT screen = w32->screenrc;
    if (w32->opts->border && w32->opts->fit_border)
        subtract_window_borders(w32->window, &screen);

    if (fit_rect(&w32->windowrc, &screen)) {
        MP_VERBOSE(w32, "adjusted window bounds: %d:%d:%d:%d\n",
                   (int)w32->windowrc.left, (int)w32->windowrc.top,
                   (int)rect_w(w32->windowrc), (int)rect_h(w32->windowrc));
    }
}

// Calculate new fullscreen state and change window size and position.
// returns true if fullscreen state was changed.
static bool update_fullscreen_state(struct vo_w32_state *w32)
{
    if (w32->parent)
        return false;

    bool new_fs = w32->opts->fullscreen;
    if (w32->toggle_fs) {
        new_fs = !w32->current_fs;
        w32->toggle_fs = false;
    }

    bool toggle_fs = w32->current_fs != new_fs;
    w32->current_fs = new_fs;

    update_screen_rect(w32);

    if (toggle_fs) {
        RECT rc;
        char msg[50];
        if (w32->current_fs) {
            // Save window rect when switching to fullscreen.
            rc = w32->prev_windowrc = w32->windowrc;
            sprintf(msg, "save window bounds");
        } else {
            // Restore window rect when switching from fullscreen.
            rc = w32->windowrc = w32->prev_windowrc;
            sprintf(msg, "restore window bounds");
        }
        MP_VERBOSE(w32, "%s: %d:%d:%d:%d\n", msg,
                   (int)rc.left, (int)rc.top, (int)rect_w(rc), (int)rect_h(rc));
    }

    if (w32->current_fs)
        w32->windowrc = w32->screenrc;

    MP_VERBOSE(w32, "reset window bounds: %d:%d:%d:%d\n",
               (int)w32->windowrc.left, (int)w32->windowrc.top,
               (int)rect_w(w32->windowrc), (int)rect_h(w32->windowrc));
    return toggle_fs;
}

static void update_window_state(struct vo_w32_state *w32)
{
    if (w32->parent)
        return;

    RECT wr = w32->windowrc;
    add_window_borders(w32->window, &wr);

    SetWindowPos(w32->window, w32->opts->ontop ? HWND_TOPMOST : HWND_NOTOPMOST,
                 wr.left, wr.top, rect_w(wr), rect_h(wr),
                 SWP_FRAMECHANGED | SWP_SHOWWINDOW);

    // Notify the taskbar about the fullscreen state only after the window
    // is visible, to make sure the taskbar item has already been created
    if (w32->taskbar_list) {
        ITaskbarList2_MarkFullscreenWindow(w32->taskbar_list,
                                           w32->window, w32->current_fs);
    }

    signal_events(w32, VO_EVENT_RESIZE);
}

static void reinit_window_state(struct vo_w32_state *w32)
{
    if (w32->parent)
        return;

    // The order matters: fs state should be updated prior to changing styles
    bool toggle_fs = update_fullscreen_state(w32);
    update_window_style(w32);

    // Assume that the window has already been fit on screen before switching fs
    if (!toggle_fs || w32->fit_on_screen) {
        fit_window_on_screen(w32);
        // The fullscreen state might still be active, so set the flag
        // to fit on screen next time the window leaves the fullscreen.
        w32->fit_on_screen = w32->current_fs;
    }

    // Show and activate the window after all window state parameters were set
    update_window_state(w32);
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam,
                                LPARAM lParam)
{
    struct vo_w32_state *w32 = (void*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    if (!w32) {
        // WM_NCCREATE is supposed to be the first message that a window
        // receives. It allows struct vo_w32_state to be passed from
        // CreateWindow's lpParam to the window procedure. However, as a
        // longstanding Windows bug, overlapped top-level windows will get a
        // WM_GETMINMAXINFO before WM_NCCREATE. This can be ignored.
        if (message != WM_NCCREATE)
            return DefWindowProcW(hWnd, message, wParam, lParam);

        CREATESTRUCTW *cs = (CREATESTRUCTW *)lParam;
        w32 = cs->lpCreateParams;
        w32->window = hWnd;
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)w32);
    }

    // The dispatch queue should be processed as soon as possible to prevent
    // playback glitches, since it is likely blocking the VO thread
    if (!w32->in_dispatch) {
        w32->in_dispatch = true;
        mp_dispatch_queue_process(w32->dispatch, 0);
        w32->in_dispatch = false;
    }

    switch (message) {
    case WM_ERASEBKGND: // no need to erase background separately
        return 1;
    case WM_PAINT:
        signal_events(w32, VO_EVENT_EXPOSE);
        break;
    case WM_MOVE: {
        const int x = GET_X_LPARAM(lParam), y = GET_Y_LPARAM(lParam);
        OffsetRect(&w32->windowrc, x - w32->windowrc.left,
                                   y - w32->windowrc.top);

        // Window may intersect with new monitors (see VOCTRL_GET_DISPLAY_NAMES)
        signal_events(w32, VO_EVENT_WIN_STATE);

        update_display_info(w32);  // if we moved between monitors
        MP_DBG(w32, "move window: %d:%d\n", x, y);
        break;
    }
    case WM_MOVING: {
        w32->moving = true;
        RECT *rc = (RECT*)lParam;
        if (snap_to_screen_edges(w32, rc))
            return TRUE;
        break;
    }
    case WM_ENTERSIZEMOVE:
        w32->moving = true;
        if (w32->snapped) {
            // Save the cursor offset from the window borders,
            // so the player window can be unsnapped later
            RECT rc;
            POINT cursor;
            if (GetWindowRect(w32->window, &rc) && GetCursorPos(&cursor)) {
                w32->snap_dx = cursor.x - rc.left;
                w32->snap_dy = cursor.y - rc.top;
            }
        }
        break;
    case WM_EXITSIZEMOVE:
        w32->moving = false;
        break;
    case WM_SIZE: {
        if (w32->moving)
            w32->snapped = false;

        const int w = LOWORD(lParam), h = HIWORD(lParam);
        if (w > 0 && h > 0) {
            w32->windowrc.right = w32->windowrc.left + w;
            w32->windowrc.bottom = w32->windowrc.top + h;
            signal_events(w32, VO_EVENT_RESIZE);
            MP_VERBOSE(w32, "resize window: %d:%d\n", w, h);
        }

        // Window may have been minimized or restored
        signal_events(w32, VO_EVENT_WIN_STATE);

        update_display_info(w32);
        break;
    }
    case WM_SIZING:
        if (w32->opts->keepaspect && w32->opts->keepaspect_window &&
            !w32->current_fs && !w32->parent)
        {
            RECT *rc = (RECT*)lParam;
            // get client area of the windows if it had the rect rc
            // (subtracting the window borders)
            RECT r = *rc;
            subtract_window_borders(w32->window, &r);
            int c_w = rect_w(r), c_h = rect_h(r);
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
    case WM_DPICHANGED:
        update_display_info(w32);
        break;
    case WM_CLOSE:
        // Don't destroy the window yet to not lose wakeup events.
        mp_input_put_key(w32->input_ctx, MP_KEY_CLOSE_WIN);
        return 0;
    case WM_NCDESTROY: // Sometimes only WM_NCDESTROY is received in --wid mode
    case WM_DESTROY:
        if (w32->destroyed)
            break;
        // If terminate is not set, something else destroyed the window. This
        // can also happen in --wid mode when the parent window is destroyed.
        if (!w32->terminate)
            mp_input_put_key(w32->input_ctx, MP_KEY_CLOSE_WIN);
        RevokeDragDrop(w32->window);
        w32->destroyed = true;
        w32->window = NULL;
        PostQuitMessage(0);
        break;
    case WM_SYSCOMMAND:
        switch (wParam & 0xFFF0) {
        case SC_SCREENSAVE:
        case SC_MONITORPOWER:
            if (w32->disable_screensaver) {
                MP_VERBOSE(w32, "killing screensaver\n");
                return 0;
            }
            break;
        case SC_RESTORE:
            if (IsMaximized(w32->window) && w32->current_fs) {
                w32->toggle_fs = true;
                reinit_window_state(w32);
                signal_events(w32, VO_EVENT_FULLSCREEN_STATE);
                return 0;
            }
            break;
        }
        break;
    case WM_NCHITTEST:
        // Provide sizing handles for borderless windows
        if (!w32->opts->border && !w32->current_fs) {
            return borderless_nchittest(w32, GET_X_LPARAM(lParam),
                                        GET_Y_LPARAM(lParam));
        }
        break;
    case WM_APPCOMMAND:
        if (handle_appcommand(w32, GET_APPCOMMAND_LPARAM(lParam)))
            return TRUE;
        break;
    case WM_SYSKEYDOWN:
        // Open the window menu on Alt+Space. Normally DefWindowProc opens the
        // window menu in response to WM_SYSCHAR, but since mpv translates its
        // own keyboard input, WM_SYSCHAR isn't generated, so the window menu
        // must be opened manually.
        if (wParam == VK_SPACE) {
            SendMessage(w32->window, WM_SYSCOMMAND, SC_KEYMENU, ' ');
            return 0;
        }

        // Handle all other WM_SYSKEYDOWN messages as WM_KEYDOWN
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
        // The cursor should only be hidden if the mouse is in the client area
        // and if the window isn't in menu mode (HIWORD(lParam) is non-zero)
        w32->can_set_cursor = LOWORD(lParam) == HTCLIENT && HIWORD(lParam);
        if (w32->can_set_cursor && !w32->cursor_visible) {
            SetCursor(NULL);
            return TRUE;
        }
        break;
    case WM_MOUSELEAVE:
        w32->tracking = FALSE;
        mp_input_put_key(w32->input_ctx, MP_KEY_MOUSE_LEAVE);
        break;
    case WM_MOUSEMOVE: {
        if (!w32->tracking) {
            w32->tracking = TrackMouseEvent(&w32->trackEvent);
            mp_input_put_key(w32->input_ctx, MP_KEY_MOUSE_ENTER);
        }
        // Windows can send spurious mouse events, which would make the mpv
        // core unhide the mouse cursor on completely unrelated events. See:
        //  https://blogs.msdn.com/b/oldnewthing/archive/2003/10/01/55108.aspx
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);
        if (x != w32->mouse_x || y != w32->mouse_y) {
            w32->mouse_x = x;
            w32->mouse_y = y;
            mp_input_set_mouse_pos(w32->input_ctx, x, y);
        }
        break;
    }
    case WM_LBUTTONDOWN:
        if (handle_mouse_down(w32, MP_MBTN_LEFT, GET_X_LPARAM(lParam),
                                                 GET_Y_LPARAM(lParam)))
            return 0;
        break;
    case WM_LBUTTONUP:
        handle_mouse_up(w32, MP_MBTN_LEFT);
        break;
    case WM_MBUTTONDOWN:
        handle_mouse_down(w32, MP_MBTN_MID, GET_X_LPARAM(lParam),
                                            GET_Y_LPARAM(lParam));
        break;
    case WM_MBUTTONUP:
        handle_mouse_up(w32, MP_MBTN_MID);
        break;
    case WM_RBUTTONDOWN:
        handle_mouse_down(w32, MP_MBTN_RIGHT, GET_X_LPARAM(lParam),
                                              GET_Y_LPARAM(lParam));
        break;
    case WM_RBUTTONUP:
        handle_mouse_up(w32, MP_MBTN_RIGHT);
        break;
    case WM_MOUSEWHEEL:
        handle_mouse_wheel(w32, false, GET_WHEEL_DELTA_WPARAM(wParam));
        return 0;
    case WM_MOUSEHWHEEL:
        handle_mouse_wheel(w32, true, GET_WHEEL_DELTA_WPARAM(wParam));
        // Some buggy mouse drivers (SetPoint) stop delivering WM_MOUSEHWHEEL
        // events when the message loop doesn't return TRUE (even on Windows 7)
        return TRUE;
    case WM_XBUTTONDOWN:
        handle_mouse_down(w32,
            HIWORD(wParam) == 1 ? MP_MBTN_BACK : MP_MBTN_FORWARD,
            GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        break;
    case WM_XBUTTONUP:
        handle_mouse_up(w32,
            HIWORD(wParam) == 1 ? MP_MBTN_BACK : MP_MBTN_FORWARD);
        break;
    case WM_DISPLAYCHANGE:
        force_update_display_info(w32);
        break;
    }

    if (message == w32->tbtnCreatedMsg) {
        w32->tbtnCreated = true;
        update_playback_state(w32);
        return 0;
    }

    return DefWindowProcW(hWnd, message, wParam, lParam);
}

static pthread_once_t window_class_init_once = PTHREAD_ONCE_INIT;
static ATOM window_class;
static void register_window_class(void)
{
    window_class = RegisterClassExW(&(WNDCLASSEXW) {
        .cbSize = sizeof(WNDCLASSEXW),
        .style = CS_HREDRAW | CS_VREDRAW,
        .lpfnWndProc = WndProc,
        .hInstance = HINST_THISCOMPONENT,
        .hIcon = LoadIconW(HINST_THISCOMPONENT, L"IDI_ICON1"),
        .hCursor = LoadCursor(NULL, IDC_ARROW),
        .lpszClassName = L"mpv",
    });
}

static ATOM get_window_class(void)
{
    pthread_once(&window_class_init_once, register_window_class);
    return window_class;
}

static void resize_child_win(HWND parent)
{
    // Check if an mpv window is a child of this window. This will not
    // necessarily be the case because the hook functions will run for all
    // windows on the parent window's thread.
    ATOM cls = get_window_class();
    HWND child = FindWindowExW(parent, NULL, (LPWSTR)MAKEINTATOM(cls), NULL);
    if (!child)
        return;
    // Make sure the window was created by this instance
    if (GetWindowLongPtrW(child, GWLP_HINSTANCE) != (LONG_PTR)HINST_THISCOMPONENT)
        return;

    // Resize the mpv window to match its parent window's size
    RECT rm, rp;
    if (!GetClientRect(child, &rm))
        return;
    if (!GetClientRect(parent, &rp))
        return;
    if (EqualRect(&rm, &rp))
        return;
    SetWindowPos(child, NULL, 0, 0, rp.right, rp.bottom, SWP_ASYNCWINDOWPOS |
        SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOSENDCHANGING);
}

static LRESULT CALLBACK parent_win_hook(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode != HC_ACTION)
        goto done;
    CWPSTRUCT *cwp = (CWPSTRUCT*)lParam;
    if (cwp->message != WM_WINDOWPOSCHANGED)
        goto done;
    resize_child_win(cwp->hwnd);
done:
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

static void CALLBACK parent_evt_hook(HWINEVENTHOOK hWinEventHook, DWORD event,
    HWND hwnd, LONG idObject, LONG idChild, DWORD dwEventThread,
    DWORD dwmsEventTime)
{
    if (event != EVENT_OBJECT_LOCATIONCHANGE)
        return;
    if (!hwnd || idObject != OBJID_WINDOW || idChild != CHILDID_SELF)
        return;
    resize_child_win(hwnd);
}

static void install_parent_hook(struct vo_w32_state *w32)
{
    DWORD pid;
    DWORD tid = GetWindowThreadProcessId(w32->parent, &pid);

    // If the parent lives inside the current process, install a Windows hook
    if (pid == GetCurrentProcessId()) {
        w32->parent_win_hook = SetWindowsHookExW(WH_CALLWNDPROC,
            parent_win_hook, NULL, tid);
    } else {
        // Otherwise, use a WinEvent hook. These don't seem to be as smooth as
        // Windows hooks, but they can be delivered across process boundaries.
        w32->parent_evt_hook = SetWinEventHook(
            EVENT_OBJECT_LOCATIONCHANGE, EVENT_OBJECT_LOCATIONCHANGE,
            NULL, parent_evt_hook, pid, tid, WINEVENT_OUTOFCONTEXT);
    }
}

static void remove_parent_hook(struct vo_w32_state *w32)
{
    if (w32->parent_win_hook)
        UnhookWindowsHookEx(w32->parent_win_hook);
    if (w32->parent_evt_hook)
        UnhookWinEvent(w32->parent_evt_hook);
}

// Dispatch incoming window events and handle them.
// This returns only when the thread is asked to terminate.
static void run_message_loop(struct vo_w32_state *w32)
{
    MSG msg;
    while (GetMessageW(&msg, 0, 0, 0) > 0)
        DispatchMessageW(&msg);

    // Even if the message loop somehow exits, we still have to respond to
    // external requests until termination is requested.
    while (!w32->terminate)
        mp_dispatch_queue_process(w32->dispatch, 1000);
}

static void gui_thread_reconfig(void *ptr)
{
    struct vo_w32_state *w32 = ptr;
    struct vo *vo = w32->vo;

    struct vo_win_geometry geo;
    struct mp_rect screen = { w32->screenrc.left, w32->screenrc.top,
                              w32->screenrc.right, w32->screenrc.bottom };
    vo_calc_window_geometry(vo, &screen, &geo);
    vo_apply_window_geometry(vo, &geo);

    bool reset_size = w32->o_dwidth != vo->dwidth ||
                      w32->o_dheight != vo->dheight;

    w32->o_dwidth = vo->dwidth;
    w32->o_dheight = vo->dheight;

    if (!w32->parent && !w32->window_bounds_initialized) {
        SetRect(&w32->windowrc, geo.win.x0, geo.win.y0,
                geo.win.x0 + vo->dwidth, geo.win.y0 + vo->dheight);
        w32->prev_windowrc = w32->windowrc;
        w32->window_bounds_initialized = true;
        w32->fit_on_screen = true;
        goto finish;
    }

    // The rect which size is going to be modified.
    RECT *rc = &w32->windowrc;

    // The desired size always matches the window size in wid mode.
    if (!reset_size || w32->parent) {
        RECT r;
        GetClientRect(w32->window, &r);
        // Restore vo_dwidth and vo_dheight, which were reset in vo_config()
        vo->dwidth = r.right;
        vo->dheight = r.bottom;
    } else {
        if (w32->current_fs)
            rc = &w32->prev_windowrc;
        w32->fit_on_screen = true;
    }

    // Save new window size and position.
    const int x = rc->left + rect_w(*rc) / 2 - vo->dwidth / 2;
    const int y = rc->top + rect_h(*rc) / 2 - vo->dheight / 2;
    SetRect(rc, x, y, x + vo->dwidth, y + vo->dheight);

finish:
    reinit_window_state(w32);
}

// Resize the window. On the first call, it's also made visible.
void vo_w32_config(struct vo *vo)
{
    struct vo_w32_state *w32 = vo->w32;
    mp_dispatch_run(w32->dispatch, gui_thread_reconfig, w32);
}

static void w32_api_load(struct vo_w32_state *w32)
{
    HMODULE shcore_dll = LoadLibraryW(L"shcore.dll");
    // Available since Win8.1
    w32->api.pGetDpiForMonitor = !shcore_dll ? NULL :
                (void *)GetProcAddress(shcore_dll, "GetDpiForMonitor");

    // imm32.dll must be loaded dynamically
    // to account for machines without East Asian language support
    HMODULE imm32_dll = LoadLibraryW(L"imm32.dll");
    w32->api.pImmDisableIME = !imm32_dll ? NULL :
                (void *)GetProcAddress(imm32_dll, "ImmDisableIME");
}

static void *gui_thread(void *ptr)
{
    struct vo_w32_state *w32 = ptr;
    bool ole_ok = false;
    int res = 0;

    mpthread_set_name("win32 window");

    w32_api_load(w32);

    // Disables the IME for windows on this thread
    if (w32->api.pImmDisableIME)
        w32->api.pImmDisableIME(0);

    if (w32->opts->WinID >= 0)
        w32->parent = (HWND)(intptr_t)(w32->opts->WinID);

    ATOM cls = get_window_class();
    if (w32->parent) {
        RECT r;
        GetClientRect(w32->parent, &r);
        CreateWindowExW(WS_EX_NOPARENTNOTIFY, (LPWSTR)MAKEINTATOM(cls), L"mpv",
                        WS_CHILD | WS_VISIBLE, 0, 0, r.right, r.bottom,
                        w32->parent, 0, HINST_THISCOMPONENT, w32);

        // Install a hook to get notifications when the parent changes size
        if (w32->window)
            install_parent_hook(w32);
    } else {
        CreateWindowExW(0, (LPWSTR)MAKEINTATOM(cls), L"mpv",
                        update_style(w32, 0), CW_USEDEFAULT, SW_HIDE, 100, 100,
                        0, 0, HINST_THISCOMPONENT, w32);
    }

    if (!w32->window) {
        MP_ERR(w32, "unable to create window!\n");
        goto done;
    }

    if (SUCCEEDED(OleInitialize(NULL))) {
        ole_ok = true;

        IDropTarget *dt = mp_w32_droptarget_create(w32->log, w32->input_ctx);
        RegisterDragDrop(w32->window, dt);

        // ITaskbarList2 has the MarkFullscreenWindow method, which is used to
        // make sure the taskbar is hidden when mpv goes fullscreen
        if (SUCCEEDED(CoCreateInstance(&CLSID_TaskbarList, NULL,
                                       CLSCTX_INPROC_SERVER, &IID_ITaskbarList2,
                                       (void**)&w32->taskbar_list)))
        {
            if (FAILED(ITaskbarList2_HrInit(w32->taskbar_list))) {
                ITaskbarList2_Release(w32->taskbar_list);
                w32->taskbar_list = NULL;
            }
        }

        // ITaskbarList3 has methods for status indication on taskbar buttons,
        // however that interface is only available on Win7/2008 R2 or newer
        if (SUCCEEDED(CoCreateInstance(&CLSID_TaskbarList, NULL,
                                       CLSCTX_INPROC_SERVER, &IID_ITaskbarList3,
                                       (void**)&w32->taskbar_list3)))
        {
            if (FAILED(ITaskbarList3_HrInit(w32->taskbar_list3))) {
                ITaskbarList3_Release(w32->taskbar_list3);
                w32->taskbar_list3 = NULL;
            } else {
                w32->tbtnCreatedMsg = RegisterWindowMessage(L"TaskbarButtonCreated");
            }
        }
    } else {
        MP_ERR(w32, "Failed to initialize OLE/COM\n");
    }

    w32->tracking   = FALSE;
    w32->trackEvent = (TRACKMOUSEEVENT){
        .cbSize    = sizeof(TRACKMOUSEEVENT),
        .dwFlags   = TME_LEAVE,
        .hwndTrack = w32->window,
    };

    if (w32->parent)
        EnableWindow(w32->window, 0);

    w32->cursor_visible = true;
    w32->moving = false;
    w32->snapped = false;
    w32->snap_dx = w32->snap_dy = 0;

    update_screen_rect(w32);

    mp_dispatch_set_wakeup_fn(w32->dispatch, wakeup_gui_thread, w32);

    res = 1;
done:

    mp_rendezvous(w32, res); // init barrier

    // This blocks until the GUI thread is to be exited.
    if (res)
        run_message_loop(w32);

    MP_VERBOSE(w32, "uninit\n");

    remove_parent_hook(w32);
    if (w32->window && !w32->destroyed)
        DestroyWindow(w32->window);
    if (w32->taskbar_list)
        ITaskbarList2_Release(w32->taskbar_list);
    if (w32->taskbar_list3)
        ITaskbarList3_Release(w32->taskbar_list3);
    if (ole_ok)
        OleUninitialize();
    SetThreadExecutionState(ES_CONTINUOUS);
    return NULL;
}

// Returns: 1 = Success, 0 = Failure
int vo_w32_init(struct vo *vo)
{
    assert(!vo->w32);

    struct vo_w32_state *w32 = talloc_ptrtype(vo, w32);
    *w32 = (struct vo_w32_state){
        .log = mp_log_new(w32, vo->log, "win32"),
        .vo = vo,
        .opts = vo->opts,
        .input_ctx = vo->input_ctx,
        .dispatch = mp_dispatch_create(w32),
    };
    vo->w32 = w32;

    if (pthread_create(&w32->thread, NULL, gui_thread, w32))
        goto fail;

    if (!mp_rendezvous(w32, 0)) { // init barrier
        pthread_join(w32->thread, NULL);
        goto fail;
    }

    // While the UI runs in its own thread, the thread in which this function
    // runs in will be the renderer thread. Apply magic MMCSS cargo-cult,
    // which might stop Windows from throttling clock rate and so on.
    if (vo->opts->mmcss_profile[0]) {
        wchar_t *profile = mp_from_utf8(NULL, vo->opts->mmcss_profile);
        w32->avrt_handle = AvSetMmThreadCharacteristicsW(profile, &(DWORD){0});
        talloc_free(profile);
    }

    return 1;
fail:
    talloc_free(w32);
    vo->w32 = NULL;
    return 0;
}

struct disp_names_data {
    HMONITOR assoc;
    int count;
    char **names;
};

static BOOL CALLBACK disp_names_proc(HMONITOR mon, HDC dc, LPRECT r, LPARAM p)
{
    struct disp_names_data *data = (struct disp_names_data*)p;

    // get_disp_names() adds data->assoc to the list, so skip it here
    if (mon == data->assoc)
        return TRUE;

    MONITORINFOEXW mi = { .cbSize = sizeof mi };
    if (GetMonitorInfoW(mon, (MONITORINFO*)&mi)) {
        MP_TARRAY_APPEND(NULL, data->names, data->count,
                         mp_to_utf8(NULL, mi.szDevice));
    }
    return TRUE;
}

static char **get_disp_names(struct vo_w32_state *w32)
{
    // Get the client area of the window in screen space
    RECT rect = { 0 };
    GetClientRect(w32->window, &rect);
    MapWindowPoints(w32->window, NULL, (POINT*)&rect, 2);

    struct disp_names_data data = { .assoc = w32->monitor };

    // Make sure the monitor that Windows considers to be associated with the
    // window is first in the list
    MONITORINFOEXW mi = { .cbSize = sizeof mi };
    if (GetMonitorInfoW(data.assoc, (MONITORINFO*)&mi)) {
        MP_TARRAY_APPEND(NULL, data.names, data.count,
                         mp_to_utf8(NULL, mi.szDevice));
    }

    // Get the names of the other monitors that intersect the client rect
    EnumDisplayMonitors(NULL, &rect, disp_names_proc, (LPARAM)&data);
    MP_TARRAY_APPEND(NULL, data.names, data.count, NULL);
    return data.names;
}

static int gui_thread_control(struct vo_w32_state *w32, int request, void *arg)
{
    switch (request) {
    case VOCTRL_FULLSCREEN:
        if (w32->opts->fullscreen != w32->current_fs)
            reinit_window_state(w32);
        return VO_TRUE;
    case VOCTRL_ONTOP:
        update_window_state(w32);
        return VO_TRUE;
    case VOCTRL_BORDER:
        update_window_style(w32);
        update_window_state(w32);
        return VO_TRUE;
    case VOCTRL_GET_FULLSCREEN:
        *(bool *)arg = w32->current_fs;
        return VO_TRUE;
    case VOCTRL_GET_UNFS_WINDOW_SIZE: {
        int *s = arg;

        if (!w32->window_bounds_initialized)
            return VO_FALSE;

        RECT *rc = w32->current_fs ? &w32->prev_windowrc : &w32->windowrc;
        s[0] = rect_w(*rc);
        s[1] = rect_h(*rc);
        return VO_TRUE;
    }
    case VOCTRL_SET_UNFS_WINDOW_SIZE: {
        int *s = arg;

        if (!w32->window_bounds_initialized)
            return VO_FALSE;

        RECT *rc = w32->current_fs ? &w32->prev_windowrc : &w32->windowrc;
        const int x = rc->left + rect_w(*rc) / 2 - s[0] / 2;
        const int y = rc->top + rect_h(*rc) / 2 - s[1] / 2;
        SetRect(rc, x, y, x + s[0], y + s[1]);

        w32->fit_on_screen = true;
        reinit_window_state(w32);
        return VO_TRUE;
    }
    case VOCTRL_GET_WIN_STATE:
        *(int *)arg = IsIconic(w32->window) ? VO_WIN_STATE_MINIMIZED : 0;
        return VO_TRUE;
    case VOCTRL_SET_CURSOR_VISIBILITY:
        w32->cursor_visible = *(bool *)arg;

        if (w32->can_set_cursor && w32->tracking) {
            if (w32->cursor_visible)
                SetCursor(LoadCursor(NULL, IDC_ARROW));
            else
                SetCursor(NULL);
        }
        return VO_TRUE;
    case VOCTRL_KILL_SCREENSAVER:
        w32->disable_screensaver = true;
        SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED |
                                ES_DISPLAY_REQUIRED);
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
    case VOCTRL_UPDATE_PLAYBACK_STATE: {
        w32->current_pstate = *(struct voctrl_playback_state *)arg;

        update_playback_state(w32);
        return VO_TRUE;
    }
    case VOCTRL_GET_DISPLAY_FPS:
        update_display_info(w32);
        *(double*) arg = w32->display_fps;
        return VO_TRUE;
    case VOCTRL_GET_DISPLAY_NAMES:
        *(char ***)arg = get_disp_names(w32);
        return VO_TRUE;
    case VOCTRL_GET_ICC_PROFILE:
        update_display_info(w32);
        if (w32->color_profile) {
            bstr *p = arg;
            *p = stream_read_file(w32->color_profile, NULL,
                w32->vo->global, 100000000); // 100 MB
            return p->len ? VO_TRUE : VO_FALSE;
        }
        return VO_FALSE;
    }
    return VO_NOTIMPL;
}

static void do_control(void *ptr)
{
    void **p = ptr;
    struct vo_w32_state *w32 = p[0];
    int *events = p[1];
    int request = *(int *)p[2];
    void *arg = p[3];
    int *ret = p[4];
    *ret = gui_thread_control(w32, request, arg);
    *events |= atomic_fetch_and(&w32->event_flags, 0);
    // Safe access, since caller (owner of vo) is blocked.
    if (*events & VO_EVENT_RESIZE) {
        w32->vo->dwidth = rect_w(w32->windowrc);
        w32->vo->dheight = rect_h(w32->windowrc);
    }
}

int vo_w32_control(struct vo *vo, int *events, int request, void *arg)
{
    struct vo_w32_state *w32 = vo->w32;
    if (request == VOCTRL_CHECK_EVENTS) {
        *events |= atomic_fetch_and(&w32->event_flags, 0);
        if (*events & VO_EVENT_RESIZE) {
            mp_dispatch_lock(w32->dispatch);
            vo->dwidth = rect_w(w32->windowrc);
            vo->dheight = rect_h(w32->windowrc);
            mp_dispatch_unlock(w32->dispatch);
        }
        return VO_TRUE;
    } else {
        int r;
        void *p[] = {w32, events, &request, arg, &r};
        mp_dispatch_run(w32->dispatch, do_control, p);
        return r;
    }
}

static void do_terminate(void *ptr)
{
    struct vo_w32_state *w32 = ptr;
    w32->terminate = true;

    if (!w32->destroyed)
        DestroyWindow(w32->window);

    mp_dispatch_interrupt(w32->dispatch);
}

void vo_w32_uninit(struct vo *vo)
{
    struct vo_w32_state *w32 = vo->w32;
    if (!w32)
        return;

    mp_dispatch_run(w32->dispatch, do_terminate, w32);
    pthread_join(w32->thread, NULL);

    AvRevertMmThreadCharacteristics(w32->avrt_handle);

    talloc_free(w32);
    vo->w32 = NULL;
}

HWND vo_w32_hwnd(struct vo *vo)
{
    struct vo_w32_state *w32 = vo->w32;
    return w32->window; // immutable, so no synchronization needed
}

void vo_w32_run_on_thread(struct vo *vo, void (*cb)(void *ctx), void *ctx)
{
    struct vo_w32_state *w32 = vo->w32;
    mp_dispatch_run(w32->dispatch, cb, ctx);
}
