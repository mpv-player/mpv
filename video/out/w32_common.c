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

#include <assert.h>
#include <limits.h>
#include <stdatomic.h>
#include <stdio.h>

#define _DECL_DLLMAIN
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <ole2.h>
#include <process.h>
#include <shellscalingapi.h>
#include <shobjidl.h>
#include <avrt.h>

#include "options/m_config.h"
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
#include "win32/menu.h"
#include "osdep/io.h"
#include "osdep/threads.h"
#include "osdep/w32_keyboard.h"
#include "misc/dispatch.h"
#include "misc/rendezvous.h"
#include "mpv_talloc.h"

#define MPV_WINDOW_CLASS_NAME L"mpv"

EXTERN_C IMAGE_DOS_HEADER __ImageBase;
#define HINST_THISCOMPONENT ((HINSTANCE)&__ImageBase)

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

#ifndef DWMWA_VISIBLE_FRAME_BORDER_THICKNESS
#define DWMWA_VISIBLE_FRAME_BORDER_THICKNESS 37
#endif

#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif

#ifndef DWMWA_SYSTEMBACKDROP_TYPE
#define DWMWA_SYSTEMBACKDROP_TYPE 38
#endif

#define rect_w(r) ((r).right - (r).left)
#define rect_h(r) ((r).bottom - (r).top)

#define WM_SHOWMENU (WM_USER + 1)

struct w32_api {
    BOOLEAN (WINAPI *pShouldAppsUseDarkMode)(void);
    DWORD (WINAPI *pSetPreferredAppMode)(DWORD mode);
};

struct vo_w32_state {
    struct mp_log *log;
    struct vo *vo;
    struct mp_vo_opts *opts;
    struct m_config_cache *opts_cache;
    struct input_ctx *input_ctx;

    mp_thread thread;
    bool terminate;
    struct mp_dispatch_queue *dispatch; // used to run stuff on the GUI thread
    bool in_dispatch;

    struct w32_api api; // stores functions from dynamically loaded DLLs

    HWND window;
    HWND parent; // 0 normally, set in embedding mode
    HHOOK parent_win_hook;
    HWINEVENTHOOK parent_evt_hook;

    struct menu_ctx *menu_ctx;

    HMONITOR monitor; // Handle of the current screen
    char *color_profile; // Path of the current screen's color profile

    // Has the window seen a WM_DESTROY? If so, don't call DestroyWindow again.
    bool destroyed;

    bool focused;

    // whether the window position and size were initialized
    bool window_bounds_initialized;

    bool current_fs;
    bool toggle_fs; // whether the current fullscreen state needs to be switched

    // Note: maximized state doesn't involve nor modify windowrc
    RECT windowrc; // currently known normal/fullscreen window client rect
    RECT prev_windowrc; // saved normal window client rect while in fullscreen

    // video size
    uint32_t o_dwidth;
    uint32_t o_dheight;

    int dpi;
    double dpi_scale;

    bool disable_screensaver;
    bool cursor_visible;
    atomic_uint event_flags;

    BOOL tracking;
    TRACKMOUSEEVENT track_event;

    int mouse_x;
    int mouse_y;

    // Should SetCursor be called when handling VOCTRL_SET_CURSOR_VISIBILITY?
    bool can_set_cursor;

    // UTF-16 decoding state for WM_CHAR and VK_PACKET
    int high_surrogate;

    // Fit the window to one monitor working area next time it's not fullscreen
    // and not maximized. Used once after every new "untrusted" size comes from
    // mpv, else we assume that the last known size is valid and don't fit.
    // FIXME: on a multi-monitor setup one bit is not enough, because the first
    // fit (autofit etc) should be to one monitor, but later size changes from
    // mpv like window-scale (VOCTRL_SET_UNFS_WINDOW_SIZE) should allow the
    // entire virtual desktop area - but we still limit to one monitor size.
    bool fit_on_screen;

    bool win_force_pos;

    ITaskbarList2 *taskbar_list;
    ITaskbarList3 *taskbar_list3;
    UINT tbtn_created_msg;
    bool tbtn_created;

    struct voctrl_playback_state current_pstate;

    // updates on move/resize/displaychange
    double display_fps;

    bool moving;

    union {
        uint8_t snapped;
        struct {
            uint8_t snapped_left : 1;
            uint8_t snapped_right : 1;
            uint8_t snapped_top : 1;
            uint8_t snapped_bottom : 1;
        };
    };
    int snap_dx;
    int snap_dy;

    HANDLE avrt_handle;

    bool cleared;
    bool dragging;
    bool start_dragging;
    BOOL win_arranging;

    bool conversion_mode_init;
    bool unmaximize;

    HIMC imc;
};

static inline int get_system_metrics(struct vo_w32_state *w32, int metric)
{
    return GetSystemMetricsForDpi(metric, w32->dpi);
}

static void adjust_window_rect(struct vo_w32_state *w32, HWND hwnd, RECT *rc)
{
    if (!w32->opts->border && !IsMaximized(w32->window))
        return;

    AdjustWindowRectExForDpi(rc, GetWindowLongPtrW(hwnd, GWL_STYLE), 0,
                             GetWindowLongPtrW(hwnd, GWL_EXSTYLE), w32->dpi);
}

static bool check_windows10_build(DWORD build)
{
    OSVERSIONINFOEXW osvi = {
        .dwOSVersionInfoSize = sizeof(osvi),
        .dwMajorVersion = HIBYTE(_WIN32_WINNT_WIN10),
        .dwMinorVersion = LOBYTE(_WIN32_WINNT_WIN10),
        .dwBuildNumber = build,
    };

    DWORD type = VER_MAJORVERSION | VER_MINORVERSION | VER_BUILDNUMBER;

    ULONGLONG mask = 0;
    mask = VerSetConditionMask(mask, VER_MAJORVERSION, VER_GREATER_EQUAL);
    mask = VerSetConditionMask(mask, VER_MINORVERSION, VER_GREATER_EQUAL);
    mask = VerSetConditionMask(mask, VER_BUILDNUMBER, VER_GREATER_EQUAL);

    return VerifyVersionInfoW(&osvi, type, mask);
}

// Get adjusted title bar height, only relevant for --title-bar=no
static int get_title_bar_height(struct vo_w32_state *w32)
{
    assert(w32->opts->border ? !w32->opts->title_bar : IsMaximized(w32->window));
    UINT visible_border = 0;
    // Only available on Windows 11, check in case it's backported and breaks
    // WM_NCCALCSIZE exception for Windows 10.
    if (check_windows10_build(22000)) {
        DwmGetWindowAttribute(w32->window, DWMWA_VISIBLE_FRAME_BORDER_THICKNESS,
                              &visible_border, sizeof(visible_border));
    }
    int top_bar = IsMaximized(w32->window)
                      ? get_system_metrics(w32, SM_CYFRAME) +
                        get_system_metrics(w32, SM_CXPADDEDBORDER)
                      : visible_border;
    return top_bar;
}

static void add_window_borders(struct vo_w32_state *w32, HWND hwnd, RECT *rc)
{
    RECT win = *rc;
    adjust_window_rect(w32, hwnd, rc);
    // Adjust for title bar height that will be hidden in WM_NCCALCSIZE
    // Keep the frame border. On Windows 10 the top border is not retained.
    // It appears that DWM draws the title bar with its full height, extending
    // outside the window area. Essentially, there is a bug in DWM, preventing
    // the adjustment of the title bar height. This issue occurs when both the
    // top and left client areas are non-zero in WM_NCCALCSIZE. If the left NC
    // area is set to 0, the title bar is drawn correctly with the adjusted
    // height. To mitigate this problem, set the top NC area to zero. The issue
    // doesn't happen on Windows 11 or when DWM NC drawing is disabled with
    // DWMWA_NCRENDERING_POLICY. We aim to avoid the manual drawing the border
    // and want the DWM look and feel, so skip the top border on Windows 10.
    // Also DWMWA_VISIBLE_FRAME_BORDER_THICKNESS is available only on Windows 11,
    // so it would be hard to guess this size correctly on Windows 10 anyway.
    if (w32->opts->border && !w32->opts->title_bar && !w32->current_fs &&
       (GetWindowLongPtrW(w32->window, GWL_STYLE) & WS_CAPTION))
    {
        if (!check_windows10_build(22000) && !IsMaximized(w32->window))
            *rc = win;
        rc->top = win.top - get_title_bar_height(w32);
    }
}

// basically a reverse AdjustWindowRect (win32 doesn't appear to have this)
static void subtract_window_borders(struct vo_w32_state *w32, HWND hwnd, RECT *rc)
{
    RECT b = { 0, 0, 0, 0 };
    add_window_borders(w32, hwnd, &b);
    rc->left -= b.left;
    rc->top -= b.top;
    rc->right -= b.right;
    rc->bottom -= b.bottom;
}

static LRESULT borderless_nchittest(struct vo_w32_state *w32, int x, int y)
{
    if (IsMaximized(w32->window))
        return HTCLIENT;

    RECT rc;
    if (!GetWindowRect(w32->window, &rc))
        return HTNOWHERE;

    POINT frame = {get_system_metrics(w32, SM_CXSIZEFRAME),
                   get_system_metrics(w32, SM_CYSIZEFRAME)};
    if (w32->opts->border) {
        frame.x += get_system_metrics(w32, SM_CXPADDEDBORDER);
        frame.y += get_system_metrics(w32, SM_CXPADDEDBORDER);
        if (!w32->opts->title_bar)
            rc.top -= get_system_metrics(w32, SM_CXPADDEDBORDER);
    }
    InflateRect(&rc, -frame.x, -frame.y);

    // Hit-test top border
    if (y < rc.top) {
        if (x < rc.left)
            return HTTOPLEFT;
        if (x > rc.right)
            return HTTOPRIGHT;
        return HTTOP;
    }

    // Hit-test bottom border
    if (y > rc.bottom) {
        if (x < rc.left)
            return HTBOTTOMLEFT;
        if (x > rc.right)
            return HTBOTTOMRIGHT;
        return HTBOTTOM;
    }

    // Hit-test side borders
    if (x < rc.left)
        return HTLEFT;
    if (x > rc.right)
        return HTRIGHT;
    return HTCLIENT;
}

// turn a WMSZ_* input value in v into the border that should be resized
// take into consideration which borders are snapped to avoid detaching
// returns: 0=left, 1=top, 2=right, 3=bottom, -1=undefined
static int get_resize_border(struct vo_w32_state *w32, int v)
{
    switch (v) {
    case WMSZ_LEFT:
    case WMSZ_RIGHT:
        return w32->snapped_bottom ? 1 : 3;
    case WMSZ_TOP:
    case WMSZ_BOTTOM:
        return w32->snapped_right ? 0 : 2;
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
    // <https://web.archive.org/web/20101004154432/http://blogs.msdn.com/b/michkap/archive/2006/04/06/569632.aspx>
    // <https://web.archive.org/web/20100820152419/http://blogs.msdn.com/b/michkap/archive/2007/10/27/5717859.aspx>
    do {
        ret = ToUnicode(vkey, scancode, keys, buf, MP_ARRAY_SIZE(buf), 0);
    } while (ret < 0);
}

static int to_unicode(UINT vkey, UINT scancode, const BYTE keys[256])
{
    // This wraps ToUnicode to be stateless and to return only one character

    // Make the buffer 10 code units long to be safe, same as here:
    // <https://web.archive.org/web/20101013215215/http://blogs.msdn.com/b/michkap/archive/2006/03/24/559169.aspx>
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
    int mpkey = mp_w32_vkey_to_mpkey(vkey, scancode & KF_EXTENDED);
    if (!mpkey) {
        mpkey = decode_key(w32, vkey, scancode & (0xff | KF_EXTENDED));
        if (!mpkey)
            return;
    }

    int state = w32->opts->native_keyrepeat ? 0 : MP_KEY_STATE_DOWN;
    mp_input_put_key(w32->input_ctx, mpkey | mod_state(w32) | state);
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

static bool handle_char(struct vo_w32_state *w32, WPARAM wc, bool decode)
{
    int c = decode ? decode_utf16(w32, wc) : wc;

    if (c == 0)
        return true;
    if (c < 0x20)
        return false;

    mp_input_put_key(w32->input_ctx, c | mod_state(w32));
    return true;
}

static void begin_dragging(struct vo_w32_state *w32)
{
    if (w32->current_fs ||
        mp_input_test_dragging(w32->input_ctx, w32->mouse_x, w32->mouse_y))
        return;
    // Window dragging hack
    ReleaseCapture();
    // The dragging model loop is entered at SendMessage() here.
    // Unfortunately, the w32->current_fs value is stale because the
    // input is handled in a different thread, and we cannot wait for
    // an up-to-date value before entering the model loop if dragging
    // needs to be kept responsive.
    // Workaround this by intercepting the loop in the WM_MOVING message,
    // where the up-to-date value is available.
    SystemParametersInfoW(SPI_GETWINARRANGING, 0, &w32->win_arranging, 0);
    w32->dragging = true;
    SendMessage(w32->window, WM_NCLBUTTONDOWN, HTCAPTION, 0);
    w32->dragging = false;
    SystemParametersInfoW(SPI_SETWINARRANGING, w32->win_arranging, 0, 0);

    mp_input_put_key(w32->input_ctx, MP_INPUT_RELEASE_ALL);
}

// If native touch is enabled and the mouse event is emulated, ignore it.
// See: <https://learn.microsoft.com/en-us/windows/win32/tablet/
//       system-events-and-mouse-messages#distinguishing-pen-input-from-mouse-and-touch>
static bool should_ignore_mouse_event(const struct vo_w32_state *w32)
{
    return w32->opts->native_touch && ((GetMessageExtraInfo() & 0xFFFFFF00) == 0xFF515700);
}

static void handle_mouse_down(struct vo_w32_state *w32, int btn, int x, int y)
{
    if (should_ignore_mouse_event(w32))
        return;
    btn |= mod_state(w32);
    mp_input_put_key(w32->input_ctx, btn | MP_KEY_STATE_DOWN);
    SetCapture(w32->window);
}

static void handle_mouse_up(struct vo_w32_state *w32, int btn)
{
    if (should_ignore_mouse_event(w32))
        return;
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
        case 164:
        case 239:
        case 359:
        case 479:
            rv = (rv + 1) / 1.001;
    }

    return rv;
}

static char *get_color_profile(void *ctx, const wchar_t *device)
{
    char *name = NULL;
    wchar_t *wname = NULL;

    HDC ic = CreateICW(device, NULL, NULL, NULL);
    if (!ic)
        goto done;
    wname = talloc_array(NULL, wchar_t, MP_PATH_MAX);
    if (!GetICMProfileW(ic, &(DWORD){ MP_PATH_MAX - 1 }, wname))
        goto done;

    name = mp_to_utf8(ctx, wname);
done:
    if (ic)
        DeleteDC(ic);
    talloc_free(wname);
    return name;
}

static void update_dpi(struct vo_w32_state *w32)
{
    UINT dpiX, dpiY;
    HDC hdc = NULL;
    int dpi = 0;

    if (GetDpiForMonitor(w32->monitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY) == S_OK) {
        dpi = (int)dpiX;
        MP_VERBOSE(w32, "DPI detected from the new API: %d\n", dpi);
    } else if ((hdc = GetDC(NULL))) {
        dpi = GetDeviceCaps(hdc, LOGPIXELSX);
        ReleaseDC(NULL, hdc);
        MP_VERBOSE(w32, "DPI detected from the old API: %d\n", dpi);
    }

    if (dpi <= 0) {
        dpi = 96;
        MP_VERBOSE(w32, "Couldn't determine DPI, falling back to %d\n", dpi);
    }

    w32->dpi = dpi;
    w32->dpi_scale = w32->dpi / 96.0;
    signal_events(w32, VO_EVENT_DPI);
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

    if (!w32->taskbar_list3 || !w32->tbtn_created)
        return;

    if (!pstate->playing || !pstate->taskbar_progress) {
        ITaskbarList3_SetProgressState(w32->taskbar_list3, w32->window,
                                       TBPF_NOPROGRESS);
        return;
    }

    ULONGLONG completed = pstate->position;
    ULONGLONG total = UINT8_MAX;
    if (!pstate->position) {
        completed = 1;
        total = MAXULONGLONG;
    }

    ITaskbarList3_SetProgressValue(w32->taskbar_list3, w32->window,
                                   completed, total);
    ITaskbarList3_SetProgressState(w32->taskbar_list3, w32->window,
                                   pstate->paused ? TBPF_PAUSED :
                                                    TBPF_NORMAL);
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

static HMONITOR get_default_monitor(struct vo_w32_state *w32)
{
    const int id = w32->current_fs ? w32->opts->fsscreen_id :
                                     w32->opts->screen_id;

    // Handle --fs-screen=<all|default> and --screen=default
    if (id < 0) {
        if (w32->win_force_pos && !w32->current_fs) {
            // Get window from forced position
            return MonitorFromRect(&w32->windowrc, MONITOR_DEFAULTTOPRIMARY);
        } else {
            // Let compositor decide
            return MonitorFromWindow(w32->window, MONITOR_DEFAULTTOPRIMARY);
        }
    }

    HMONITOR mon = get_monitor(id);
    if (mon)
        return mon;
    MP_VERBOSE(w32, "Screen %d does not exist, falling back to primary\n", id);
    return MonitorFromPoint((POINT){0, 0}, MONITOR_DEFAULTTOPRIMARY);
}

static MONITORINFO get_monitor_info(struct vo_w32_state *w32)
{
    HMONITOR mon;
    if (IsWindowVisible(w32->window) && !w32->current_fs) {
        mon = MonitorFromWindow(w32->window, MONITOR_DEFAULTTOPRIMARY);
    } else {
        // The window is not visible during initialization, so get the
        // monitor by --screen or --fs-screen id, or fallback to primary.
        mon = get_default_monitor(w32);
    }
    MONITORINFO mi = { .cbSize = sizeof(mi) };
    GetMonitorInfoW(mon, &mi);
    return mi;
}

static RECT get_screen_area(struct vo_w32_state *w32)
{
    // Handle --fs-screen=all
    if (w32->current_fs && w32->opts->fsscreen_id == -2) {
        const int x = get_system_metrics(w32, SM_XVIRTUALSCREEN);
        const int y = get_system_metrics(w32, SM_YVIRTUALSCREEN);
        return (RECT) { x, y, x + get_system_metrics(w32, SM_CXVIRTUALSCREEN),
                              y + get_system_metrics(w32, SM_CYVIRTUALSCREEN) };
    }
    return get_monitor_info(w32).rcMonitor;
}

static RECT get_working_area(struct vo_w32_state *w32)
{
    return w32->current_fs ? get_screen_area(w32) :
                             get_monitor_info(w32).rcWork;
}

// Adjust working area boundaries to compensate for invisible borders.
static void adjust_working_area_for_extended_frame(RECT *wa_rect, RECT *wnd_rect, HWND wnd)
{
    RECT frame = {0};

    if (DwmGetWindowAttribute(wnd, DWMWA_EXTENDED_FRAME_BOUNDS,
                              &frame, sizeof(RECT)) == S_OK) {
        wa_rect->left -= frame.left - wnd_rect->left;
        wa_rect->top -= frame.top - wnd_rect->top;
        wa_rect->right += wnd_rect->right - frame.right;
        wa_rect->bottom += wnd_rect->bottom - frame.bottom;
    }
}

static bool snap_to_screen_edges(struct vo_w32_state *w32, RECT *rc)
{
    if (w32->parent || w32->current_fs || IsMaximized(w32->window))
        return false;

    if (!w32->opts->snap_window) {
        w32->snapped = 0;
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

    // Get the work area to let the window snap to taskbar
    wr = get_working_area(w32);

    adjust_working_area_for_extended_frame(&wr, &rect, w32->window);

    // Let the window to unsnap by changing its position,
    // otherwise it will stick to the screen edges forever
    rect = *rc;
    if (w32->snapped) {
        OffsetRect(&rect, cursor.x - rect.left - w32->snap_dx,
                          cursor.y - rect.top - w32->snap_dy);
    }

    int threshold = (w32->dpi * 16) / 96;
    bool was_snapped = !!w32->snapped;
    w32->snapped = 0;
    // Adjust X position
    // snapped_left & snapped_right are mutually exclusive
    if (abs(rect.left - wr.left) < threshold) {
        w32->snapped_left = 1;
        OffsetRect(&rect, wr.left - rect.left, 0);
    } else if (abs(rect.right - wr.right) < threshold) {
        w32->snapped_right = 1;
        OffsetRect(&rect, wr.right - rect.right, 0);
    }
    // Adjust Y position
    // snapped_top & snapped_bottom are mutually exclusive
    if (abs(rect.top - wr.top) < threshold) {
        w32->snapped_top = 1;
        OffsetRect(&rect, 0, wr.top - rect.top);
    } else if (abs(rect.bottom - wr.bottom) < threshold) {
        w32->snapped_bottom = 1;
        OffsetRect(&rect, 0, wr.bottom - rect.bottom);
    }

    if (!was_snapped && w32->snapped != 0) {
        w32->snap_dx = cursor.x - rc->left;
        w32->snap_dy = cursor.y - rc->top;
    }

    *rc = rect;
    return true;
}

static bool is_high_contrast(void)
{
    HIGHCONTRAST hc = {sizeof(hc)};
    SystemParametersInfo(SPI_GETHIGHCONTRAST, sizeof(hc), &hc, 0);
    return hc.dwFlags & HCF_HIGHCONTRASTON;
}

static DWORD update_style(struct vo_w32_state *w32, DWORD style)
{
    const DWORD NO_FRAME = WS_OVERLAPPED | WS_MINIMIZEBOX | WS_THICKFRAME;
    const DWORD FRAME = WS_OVERLAPPEDWINDOW;
    const DWORD FULLSCREEN = NO_FRAME & ~WS_THICKFRAME;
    style &= ~(NO_FRAME | FRAME | FULLSCREEN);
    style |= WS_SYSMENU;
    if (w32->current_fs) {
        style |= FULLSCREEN;
    } else {
        style |= (w32->opts->border || w32->opts->window_maximized) ? FRAME : NO_FRAME;
        if (!w32->opts->title_bar && is_high_contrast())
            style &= ~WS_CAPTION;
    }
    return style;
}

static DWORD update_exstyle(struct vo_w32_state *w32, DWORD exstyle)
{
    exstyle &= ~(WS_EX_TOOLWINDOW);
    if (!w32->opts->show_in_taskbar)
        exstyle |= WS_EX_TOOLWINDOW;
    return exstyle;
}

static void update_window_style(struct vo_w32_state *w32)
{
    if (w32->parent)
        return;

    // SetWindowLongPtr can trigger a WM_SIZE event, so window rect
    // has to be saved now and restored after setting the new style.
    const RECT wr = w32->windowrc;
    const DWORD style = GetWindowLongPtrW(w32->window, GWL_STYLE);
    const DWORD exstyle = GetWindowLongPtrW(w32->window, GWL_EXSTYLE);
    SetWindowLongPtrW(w32->window, GWL_STYLE, update_style(w32, style));
    SetWindowLongPtrW(w32->window, GWL_EXSTYLE, update_exstyle(w32, exstyle));
    w32->windowrc = wr;
}

// Resize window rect to width = w and height = h. If window is snapped,
// don't let it detach from snapped borders. Otherwise resize around the center.
static void resize_and_move_rect(struct vo_w32_state *w32, RECT *rc, int w, int h)
{
    int x, y;

    if (w32->snapped_left)
        x = rc->left;
    else if (w32->snapped_right)
        x = rc->right - w;
    else
        x = rc->left + rect_w(*rc) / 2 - w / 2;

    if (w32->snapped_top)
        y = rc->top;
    else if (w32->snapped_bottom)
        y = rc->bottom - h;
    else
        y = rc->top + rect_h(*rc) / 2 - h / 2;

    SetRect(rc, x, y, x + w, y + h);
}

// If rc is wider/taller than n_w/n_h, shrink rc size while keeping the center.
// returns true if the rectangle was modified.
static bool fit_rect_size(struct vo_w32_state *w32, RECT *rc, long n_w, long n_h)
{
    // nothing to do if we already fit.
    int o_w = rect_w(*rc), o_h = rect_h(*rc);
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

    resize_and_move_rect(w32, rc, n_w, n_h);

    return true;
}

// If the window is bigger than the desktop, shrink to fit with same center.
// Also, if the top edge is above the working area, move down to align.
static void fit_window_on_screen(struct vo_w32_state *w32)
{
    RECT screen = get_working_area(w32);
    if (w32->opts->border)
        subtract_window_borders(w32, w32->window, &screen);

    RECT window_rect;
    if (GetWindowRect(w32->window, &window_rect))
        adjust_working_area_for_extended_frame(&screen, &window_rect, w32->window);

    bool adjusted = fit_rect_size(w32, &w32->windowrc, rect_w(screen), rect_h(screen));

    if (w32->windowrc.top < screen.top) {
        // if the top-edge of client area is above the target area (mainly
        // because the client-area is centered but the title bar is taller
        // than the bottom border), then move it down to align the edges.
        // Windows itself applies the same constraint during manual move.
        w32->windowrc.bottom += screen.top - w32->windowrc.top;
        w32->windowrc.top = screen.top;
        adjusted = true;
    }

    if (adjusted) {
        MP_VERBOSE(w32, "adjusted window bounds: %d:%d:%d:%d\n",
                   (int)w32->windowrc.left, (int)w32->windowrc.top,
                   (int)rect_w(w32->windowrc), (int)rect_h(w32->windowrc));
    }
}

// Calculate new fullscreen state and change window size and position.
static void update_fullscreen_state(struct vo_w32_state *w32)
{
    if (w32->parent)
        return;

    bool new_fs = w32->opts->fullscreen;
    if (w32->toggle_fs) {
        new_fs = !w32->current_fs;
        w32->toggle_fs = false;
    }

    bool toggle_fs = w32->current_fs != new_fs;
    w32->opts->fullscreen = w32->current_fs = new_fs;
    m_config_cache_write_opt(w32->opts_cache,
                             &w32->opts->fullscreen);

    if (toggle_fs && (!w32->opts->window_maximized || w32->unmaximize)) {
        if (w32->current_fs) {
            // Save window rect when switching to fullscreen.
            w32->prev_windowrc = w32->windowrc;
            MP_VERBOSE(w32, "save window bounds: %d:%d:%d:%d\n",
                       (int)w32->windowrc.left, (int)w32->windowrc.top,
                       (int)rect_w(w32->windowrc), (int)rect_h(w32->windowrc));
        } else {
            // Restore window rect when switching from fullscreen.
            w32->windowrc = w32->prev_windowrc;
        }
    }

    if (w32->current_fs)
        w32->windowrc = get_screen_area(w32);

    MP_VERBOSE(w32, "reset window bounds: %d:%d:%d:%d\n",
               (int)w32->windowrc.left, (int)w32->windowrc.top,
               (int)rect_w(w32->windowrc), (int)rect_h(w32->windowrc));
}

static void update_minimized_state(struct vo_w32_state *w32)
{
    if (w32->parent)
        return;

    if (!!IsMinimized(w32->window) != w32->opts->window_minimized) {
        if (w32->opts->window_minimized) {
            ShowWindow(w32->window, SW_SHOWMINNOACTIVE);
        } else {
            ShowWindow(w32->window, SW_RESTORE);
        }
    }
}

static void update_window_state(struct vo_w32_state *w32);

static void update_maximized_state(struct vo_w32_state *w32, bool leaving_fullscreen)
{
    if (w32->parent)
        return;

    // Apply the maximized state on leaving fullscreen.
    if (w32->current_fs && !leaving_fullscreen)
        return;

    bool toggle = w32->opts->window_maximized ^ IsMaximized(w32->window);

    if (toggle && !w32->current_fs && w32->opts->window_maximized)
        w32->prev_windowrc = w32->windowrc;

    WINDOWPLACEMENT wp = { .length = sizeof wp };
    GetWindowPlacement(w32->window, &wp);

    if (wp.showCmd == SW_SHOWMINIMIZED) {
        // When the window is minimized, setting this property just changes
        // whether it will be maximized when it's restored
        if (w32->opts->window_maximized) {
            wp.flags |= WPF_RESTORETOMAXIMIZED;
        } else {
            wp.flags &= ~WPF_RESTORETOMAXIMIZED;
        }
        SetWindowPlacement(w32->window, &wp);
    } else if ((wp.showCmd == SW_SHOWMAXIMIZED) != w32->opts->window_maximized) {
        if (w32->opts->window_maximized) {
            ShowWindow(w32->window, SW_SHOWMAXIMIZED);
        } else {
            ShowWindow(w32->window, SW_SHOWNOACTIVATE);
        }
    }

    update_window_style(w32);

    if (toggle && !w32->current_fs && !w32->opts->window_maximized) {
        w32->windowrc = w32->prev_windowrc;
        update_window_state(w32);
    }
}

static bool is_visible(HWND window)
{
    // Unlike IsWindowVisible, this doesn't check the window's parents
    return GetWindowLongPtrW(window, GWL_STYLE) & WS_VISIBLE;
}

//Set the mpv window's affinity.
//This will affect how it's displayed on the desktop and in system-level operations like taking screenshots.
static void update_affinity(struct vo_w32_state *w32)
{
    if (!w32 || w32->parent) {
        return;
    }
    SetWindowDisplayAffinity(w32->window, w32->opts->window_affinity);
}

static void update_window_state(struct vo_w32_state *w32)
{
    if (w32->parent)
        return;

    RECT wr = w32->windowrc;
    add_window_borders(w32, w32->window, &wr);

    SetWindowPos(w32->window, w32->opts->ontop ? HWND_TOPMOST : HWND_NOTOPMOST,
                 wr.left, wr.top, rect_w(wr), rect_h(wr),
                 SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOOWNERZORDER);

    // Unmaximize the window if a size change is requested because SetWindowPos
    // doesn't change the window maximized state.
    // ShowWindow(SW_SHOWNOACTIVATE) can't be used here because it tries to
    // "restore" the window to its size before it's maximized.
    if (w32->unmaximize && !w32->current_fs) {
        WINDOWPLACEMENT wp = { .length = sizeof wp };
        GetWindowPlacement(w32->window, &wp);
        wp.showCmd = SW_SHOWNOACTIVATE;
        wp.rcNormalPosition = wr;
        SetWindowPlacement(w32->window, &wp);
        w32->unmaximize = false;
    }

    // Show the window if it's not yet visible
    if (!is_visible(w32->window)) {
        if (w32->opts->window_minimized) {
            ShowWindow(w32->window, SW_SHOWMINNOACTIVE);
            update_maximized_state(w32, false); // Set the WPF_RESTORETOMAXIMIZED flag
        } else if (w32->opts->window_maximized) {
            ShowWindow(w32->window, SW_SHOWMAXIMIZED);
        } else {
            ShowWindow(w32->window, SW_SHOW);
        }
    }

    // Notify the taskbar about the fullscreen state only after the window
    // is visible, to make sure the taskbar item has already been created
    if (w32->taskbar_list) {
        ITaskbarList2_MarkFullscreenWindow(w32->taskbar_list,
                                           w32->window, w32->current_fs);
    }

    // Update snapping status if needed
    if (w32->opts->snap_window && !w32->parent &&
        !w32->current_fs && !IsMaximized(w32->window)) {
        RECT wa = get_working_area(w32);

        adjust_working_area_for_extended_frame(&wa, &wr, w32->window);

        // snapped_left & snapped_right are mutually exclusive
        if (wa.left == wr.left && wa.right == wr.right) {
            // Leave as is.
        } else if (wa.left == wr.left) {
            w32->snapped_left = 1;
            w32->snapped_right = 0;
        } else if (wa.right == wr.right) {
            w32->snapped_right = 1;
            w32->snapped_left = 0;
        } else {
            w32->snapped_left = w32->snapped_right = 0;
        }

        // snapped_top & snapped_bottom are mutually exclusive
        if (wa.top == wr.top && wa.bottom == wr.bottom) {
            // Leave as is.
        } else if (wa.top == wr.top) {
            w32->snapped_top = 1;
            w32->snapped_bottom = 0;
        } else if (wa.bottom == wr.bottom) {
            w32->snapped_bottom = 1;
            w32->snapped_top = 0;
        } else {
            w32->snapped_top = w32->snapped_bottom = 0;
        }
    }

    signal_events(w32, VO_EVENT_RESIZE);
}

static void update_corners_pref(const struct vo_w32_state *w32) {
    if (w32->parent)
        return;

    int pref = w32->current_fs ? 0 : w32->opts->window_corners;
    DwmSetWindowAttribute(w32->window, DWMWA_WINDOW_CORNER_PREFERENCE,
                          &pref, sizeof(pref));
}

static void reinit_window_state(struct vo_w32_state *w32)
{
    if (w32->parent)
        return;

    // The order matters: fs state should be updated prior to changing styles
    update_fullscreen_state(w32);
    update_corners_pref(w32);
    update_window_style(w32);

    // fit_on_screen is applied at most once when/if applicable (normal win).
    if (w32->fit_on_screen && !w32->current_fs && !IsMaximized(w32->window)) {
        fit_window_on_screen(w32);
        w32->fit_on_screen = false;
    }

    // Show and activate the window after all window state parameters were set
    update_window_state(w32);
}

// Follow Windows settings and update dark mode state
// Microsoft documented how to enable dark mode for title bar:
// https://learn.microsoft.com/windows/apps/desktop/modernize/apply-windows-themes
// https://learn.microsoft.com/windows/win32/api/dwmapi/ne-dwmapi-dwmwindowattribute
// Documentation says to set the DWMWA_USE_IMMERSIVE_DARK_MODE attribute to
// TRUE to honor dark mode for the window, FALSE to always use light mode. While
// in fact setting it to TRUE causes dark mode to be always enabled, regardless
// of the settings. Since it is quite unlikely that it will be fixed, just use
// UxTheme API to check if dark mode should be applied and while at it enable it
// fully. Ideally this function should only call the DwmSetWindowAttribute(),
// but it just doesn't work as documented.
static void update_dark_mode(const struct vo_w32_state *w32)
{
    if (w32->api.pSetPreferredAppMode)
        w32->api.pSetPreferredAppMode(1); // allow dark mode

    // if pShouldAppsUseDarkMode is not available, just assume it to be true
    const BOOL use_dark_mode = !is_high_contrast() && (!w32->api.pShouldAppsUseDarkMode ||
                                                       w32->api.pShouldAppsUseDarkMode());

    SetWindowTheme(w32->window, use_dark_mode ? L"DarkMode_Explorer" : L"", NULL);

    DwmSetWindowAttribute(w32->window, DWMWA_USE_IMMERSIVE_DARK_MODE,
                          &use_dark_mode, sizeof(use_dark_mode));
}

static void update_backdrop(const struct vo_w32_state *w32)
{
    if (w32->parent)
        return;

    int backdropType = w32->opts->backdrop_type;
    DwmSetWindowAttribute(w32->window, DWMWA_SYSTEMBACKDROP_TYPE,
                          &backdropType, sizeof(backdropType));
}

static void update_cursor_passthrough(const struct vo_w32_state *w32)
{
    if (w32->parent)
        return;

    LONG_PTR exstyle = GetWindowLongPtrW(w32->window, GWL_EXSTYLE);
    if (exstyle) {
        if (w32->opts->cursor_passthrough) {
            SetWindowLongPtrW(w32->window, GWL_EXSTYLE, exstyle | WS_EX_LAYERED | WS_EX_TRANSPARENT);
            // This is required, otherwise the titlebar disappears.
            SetLayeredWindowAttributes(w32->window, 0, 255, LWA_ALPHA);
        } else {
            SetWindowLongPtrW(w32->window, GWL_EXSTYLE, exstyle & ~(WS_EX_LAYERED | WS_EX_TRANSPARENT));
        }
    }
}

static void update_native_touch(const struct vo_w32_state *w32)
{
    if (w32->parent)
        return;

    if (w32->opts->native_touch) {
        RegisterTouchWindow(w32->window, 0);
    } else {
        UnregisterTouchWindow(w32->window);
        mp_input_put_key(w32->input_ctx, MP_TOUCH_RELEASE_ALL);
    }
}

static void set_ime_conversion_mode(const struct vo_w32_state *w32, DWORD mode)
{
    if (w32->parent)
        return;

    HIMC imc = ImmGetContext(w32->window);
    if (imc) {
        DWORD sentence_mode;
        if (ImmGetConversionStatus(imc, NULL, &sentence_mode))
            ImmSetConversionStatus(imc, mode, sentence_mode);
        ImmReleaseContext(w32->window, imc);
    }
}

static void update_ime_enabled(struct vo_w32_state *w32, bool enable)
{
    if (w32->parent)
        return;

    if (enable && w32->imc) {
        ImmAssociateContext(w32->window, w32->imc);
        w32->imc = NULL;
    } else if (!enable && !w32->imc) {
        w32->imc = ImmAssociateContext(w32->window, NULL);
    }
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
    // Start window dragging if the flag is set by the voctrl.
    // This is processed here to avoid blocking the dispatch queue.
    if (w32->start_dragging) {
        w32->start_dragging = false;
        begin_dragging(w32);
    }

    switch (message) {
    case WM_ERASEBKGND:
        if (w32->cleared || !w32->opts->border || w32->current_fs)
            return TRUE;
        break;
    case WM_PAINT:
        w32->cleared = true;
        signal_events(w32, VO_EVENT_EXPOSE);
        break;
    case WM_MOVE: {
        w32->moving = false;
        const int x = GET_X_LPARAM(lParam), y = GET_Y_LPARAM(lParam);
        OffsetRect(&w32->windowrc, x - w32->windowrc.left,
                                   y - w32->windowrc.top);

        // Window may intersect with new monitors (see VOCTRL_GET_DISPLAY_NAMES)
        signal_events(w32, VO_EVENT_WIN_STATE);

        update_display_info(w32);  // if we moved between monitors
        break;
    }
    case WM_MOVING: {
        w32->moving = true;
        RECT *rc = (RECT*)lParam;
        // Prevent the window from being moved if the window dragging hack
        // is active, and the window is currently in fullscreen.
        if (w32->dragging && w32->current_fs) {
            // Temporarily disable window arrangement to prevent aero shake
            // from being activated. The original system setting will be restored
            // after the dragging hack ends.
            if (w32->win_arranging) {
                SystemParametersInfoW(SPI_SETWINARRANGING, FALSE, 0, 0);
            }
            *rc = w32->windowrc;
            return TRUE;
        }
        if (snap_to_screen_edges(w32, rc))
            return TRUE;
        break;
    }
    case WM_ENTERSIZEMOVE:
        w32->moving = true;
        if (w32->snapped != 0) {
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
        const int w = LOWORD(lParam), h = HIWORD(lParam);
        if (w > 0 && h > 0) {
            w32->windowrc.right = w32->windowrc.left + w;
            w32->windowrc.bottom = w32->windowrc.top + h;
            signal_events(w32, VO_EVENT_RESIZE);
            MP_VERBOSE(w32, "resize window: %d:%d\n", w, h);
        }

        // Window may have been minimized, maximized or restored
        if (is_visible(w32->window)) {
            WINDOWPLACEMENT wp = { .length = sizeof wp };
            GetWindowPlacement(w32->window, &wp);

            bool is_minimized = wp.showCmd == SW_SHOWMINIMIZED;
            if (w32->opts->window_minimized != is_minimized) {
                w32->opts->window_minimized = is_minimized;
                m_config_cache_write_opt(w32->opts_cache,
                                         &w32->opts->window_minimized);
            }

            bool is_maximized = wp.showCmd == SW_SHOWMAXIMIZED ||
                (wp.showCmd == SW_SHOWMINIMIZED &&
                    (wp.flags & WPF_RESTORETOMAXIMIZED));
            if (w32->opts->window_maximized != is_maximized) {
                w32->opts->window_maximized = is_maximized;
                update_window_style(w32);
                m_config_cache_write_opt(w32->opts_cache,
                                         &w32->opts->window_maximized);
            }
        }

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
            subtract_window_borders(w32, w32->window, &r);
            int c_w = rect_w(r), c_h = rect_h(r);
            float aspect = w32->o_dwidth / (float) MPMAX(w32->o_dheight, 1);
            int d_w = c_h * aspect - c_w;
            int d_h = c_w / aspect - c_h;
            int d_corners[4] = { d_w, d_h, -d_w, -d_h };
            int corners[4] = { rc->left, rc->top, rc->right, rc->bottom };
            int corner = get_resize_border(w32, wParam);
            if (corner >= 0)
                corners[corner] -= d_corners[corner];
            *rc = (RECT) { corners[0], corners[1], corners[2], corners[3] };
            return TRUE;
        }
        break;
    case WM_DPICHANGED:
        update_display_info(w32);

        RECT *rc = (RECT*)lParam;
        w32->windowrc = *rc;
        subtract_window_borders(w32, w32->window, &w32->windowrc);
        update_window_state(w32);
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
    case WM_COMMAND: {
        const char *cmd = mp_win32_menu_get_cmd(w32->menu_ctx, LOWORD(wParam));
        if (cmd) {
            mp_cmd_t *cmdt = mp_input_parse_cmd(w32->input_ctx, bstr0(cmd), "");
            mp_input_queue_cmd(w32->input_ctx, cmdt);
        }
        break;
    }
    case WM_SYSCOMMAND: {
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

                return 0;
            }
            break;
        }
        // All custom items must use ids of less than 0xF000. The context menu items are
        // also larger than WM_USER, which excludes SCF_ISSECURE.
        if (wParam > WM_USER && wParam < 0xF000) {
            const char *cmd = mp_win32_menu_get_cmd(w32->menu_ctx, LOWORD(wParam));
            if (cmd) {
                mp_cmd_t *cmdt = mp_input_parse_cmd(w32->input_ctx, bstr0(cmd), "");
                mp_input_queue_cmd(w32->input_ctx, cmdt);
                return 0;
            }
        }
        break;
    }
    case WM_NCACTIVATE:
        // Cosmetic to remove blinking window border when initializing window
        if (!w32->opts->border)
            lParam = -1;
        break;
    case WM_NCHITTEST:
        // Provide sizing handles for borderless windows
        if ((!w32->opts->border || !w32->opts->title_bar) && !w32->current_fs) {
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

        handle_key_down(w32, wParam, HIWORD(lParam));
        if (wParam == VK_F10)
            return 0;
        break;
    case WM_KEYDOWN:
        handle_key_down(w32, wParam, HIWORD(lParam));
        break;
    case WM_SYSKEYUP:
    case WM_KEYUP:
        handle_key_up(w32, wParam, HIWORD(lParam));
        if (wParam == VK_F10)
            return 0;
        break;
    case WM_CHAR:
    case WM_SYSCHAR:
        if (handle_char(w32, wParam, true))
            return 0;
        break;
    case WM_UNICHAR:
        if (wParam == UNICODE_NOCHAR) {
            return TRUE;
        } else if (handle_char(w32, wParam, false)) {
            return 0;
        }
        break;
    case WM_KILLFOCUS:
        mp_input_put_key(w32->input_ctx, MP_INPUT_RELEASE_ALL);
        w32->focused = false;
        signal_events(w32, VO_EVENT_FOCUS);
        return 0;
    case WM_SETFOCUS:
        w32->focused = true;
        signal_events(w32, VO_EVENT_FOCUS);
        return 0;
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
            w32->tracking = TrackMouseEvent(&w32->track_event);
            mp_input_put_key(w32->input_ctx, MP_KEY_MOUSE_ENTER);
        }
        // Windows can send spurious mouse events, which would make the mpv
        // core unhide the mouse cursor on completely unrelated events. See:
        // <https://web.archive.org/web/20100821161603/
        // https://blogs.msdn.com/b/oldnewthing/archive/2003/10/01/55108.aspx>
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);
        if (x != w32->mouse_x || y != w32->mouse_y) {
            w32->mouse_x = x;
            w32->mouse_y = y;
            if (!should_ignore_mouse_event(w32))
                mp_input_set_mouse_pos(w32->input_ctx, x, y);
        }
        break;
    }
    case WM_LBUTTONDOWN:
        handle_mouse_down(w32, MP_MBTN_LEFT, GET_X_LPARAM(lParam),
                                             GET_Y_LPARAM(lParam));
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
        return TRUE;
    case WM_XBUTTONUP:
        handle_mouse_up(w32,
            HIWORD(wParam) == 1 ? MP_MBTN_BACK : MP_MBTN_FORWARD);
        return TRUE;
    case WM_DISPLAYCHANGE:
        force_update_display_info(w32);
        break;
    case WM_SETTINGCHANGE:
        update_dark_mode(w32);
        update_window_style(w32);
        update_window_state(w32);
        break;
    case WM_NCCALCSIZE:
        if (!w32->opts->border && !IsMaximized(w32->window))
            return 0;

        // Apparently removing WS_CAPTION disables some window animation, instead
        // just reduce non-client size to remove title bar.
        if (wParam && lParam && !w32->current_fs && !w32->parent &&
            (w32->opts->border ? !w32->opts->title_bar : IsMaximized(w32->window)) &&
            (GetWindowLongPtrW(w32->window, GWL_STYLE) & WS_CAPTION))
        {
            // Remove all NC area on Windows 10 due to inability to control the
            // top bar height before Windows 11.
            if (!check_windows10_build(22000) && !IsMaximized(w32->window))
                return 0;
            RECT r = {0};
            adjust_window_rect(w32, w32->window, &r);
            NCCALCSIZE_PARAMS *p = (LPNCCALCSIZE_PARAMS)lParam;
            p->rgrc[0].top += r.top + get_title_bar_height(w32);
        }
        break;
    case WM_IME_STARTCOMPOSITION: {
        HIMC imc = ImmGetContext(w32->window);
        if (imc) {
            COMPOSITIONFORM cf = {.dwStyle = CFS_POINT, .ptCurrentPos = {0, 0}};
            ImmSetCompositionWindow(imc, &cf);
            ImmReleaseContext(w32->window, imc);
        }
        break;
    }
    case WM_CREATE:
        // The IME can only be changed to alphanumeric input after it's initialized.
        // Unfortunately, there is no way to know when this happens, as
        // none of the WM_CREATE, WM_INPUTLANGCHANGE, or WM_IME_* messages work.
        // This works if the IME is initialized within a short time after
        // the window is created. Otherwise, fallback to setting alphanumeric mode on
        // the first keypress.
        SetTimer(w32->window, (UINT_PTR)WM_CREATE, 250, NULL);
        break;
    case WM_TIMER:
        if (wParam == WM_CREATE) {
            // Default to alphanumeric input when the IME is first initialized.
            set_ime_conversion_mode(w32, IME_CMODE_ALPHANUMERIC);
            KillTimer(w32->window, (UINT_PTR)WM_CREATE);
            return 0;
        }
        break;
    case WM_SHOWMENU:
        mp_win32_menu_show(w32->menu_ctx, w32->window);
        break;
    case WM_TOUCH: {
        UINT count = LOWORD(wParam);
        TOUCHINPUT *inputs = talloc_array_ptrtype(NULL, inputs, count);
        if (GetTouchInputInfo((HTOUCHINPUT)lParam, count, inputs, sizeof(TOUCHINPUT))) {
            for (UINT i = 0; i < count; i++) {
                TOUCHINPUT *ti = &inputs[i];
                POINT pt = {TOUCH_COORD_TO_PIXEL(ti->x), TOUCH_COORD_TO_PIXEL(ti->y)};
                ScreenToClient(w32->window, &pt);
                if (ti->dwFlags & TOUCHEVENTF_DOWN)
                    mp_input_add_touch_point(w32->input_ctx, ti->dwID, pt.x, pt.y);
                if (ti->dwFlags & TOUCHEVENTF_MOVE)
                    mp_input_update_touch_point(w32->input_ctx, ti->dwID, pt.x, pt.y);
                if (ti->dwFlags & TOUCHEVENTF_UP)
                    mp_input_remove_touch_point(w32->input_ctx, ti->dwID);
            }
        }
        CloseTouchInputHandle((HTOUCHINPUT)lParam);
        talloc_free(inputs);
        return 0;
    }
    }

    if (message == w32->tbtn_created_msg) {
        w32->tbtn_created = true;
        update_playback_state(w32);
        return 0;
    }

    return DefWindowProcW(hWnd, message, wParam, lParam);
}

static mp_once window_class_init_once = MP_STATIC_ONCE_INITIALIZER;
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
        .hbrBackground = (HBRUSH) GetStockObject(BLACK_BRUSH),
        .lpszClassName = MPV_WINDOW_CLASS_NAME,
    });
}

static ATOM get_window_class(void)
{
    mp_exec_once(&window_class_init_once, register_window_class);
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

static bool is_key_message(UINT msg)
{
    return msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN ||
           msg == WM_KEYUP || msg == WM_SYSKEYUP;
}

// Dispatch incoming window events and handle them.
// This returns only when the thread is asked to terminate.
static void run_message_loop(struct vo_w32_state *w32)
{
    MSG msg;
    while (!w32->destroyed && GetMessageW(&msg, 0, 0, 0) > 0) {
        // Change the conversion mode on the first keypress, in case the timer
        // solution fails. Note that this leaves the mode indicator in the language
        // bar showing the original mode until a key is pressed.
        if (is_key_message(msg.message) && !w32->conversion_mode_init) {
            set_ime_conversion_mode(w32, IME_CMODE_ALPHANUMERIC);
            w32->conversion_mode_init = true;
            KillTimer(w32->window, (UINT_PTR)WM_CREATE);
        }
        // Only send IME messages to TranslateMessage
        if (is_key_message(msg.message) && msg.wParam == VK_PROCESSKEY)
            TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    // Even if the message loop somehow exits, we still have to respond to
    // external requests until termination is requested.
    while (!w32->terminate) {
        assert(!w32->in_dispatch);
        w32->in_dispatch = true;
        mp_dispatch_queue_process(w32->dispatch, 1000);
        w32->in_dispatch = false;
    }
}

static void window_reconfig(struct vo_w32_state *w32, bool force)
{
    struct vo *vo = w32->vo;

    RECT r = get_working_area(w32);
    // for normal window which is auto-positioned (centered), center the window
    // rather than the content (by subtracting the borders from the work area)
    if (!w32->current_fs && !IsMaximized(w32->window) && w32->opts->border &&
        !w32->opts->geometry.xy_valid /* specific position not requested */)
    {
        subtract_window_borders(w32, w32->window, &r);
    }
    struct mp_rect screen = { r.left, r.top, r.right, r.bottom };
    struct vo_win_geometry geo;

    RECT monrc = get_monitor_info(w32).rcMonitor;
    struct mp_rect mon = { monrc.left, monrc.top, monrc.right, monrc.bottom };

    if (w32->dpi_scale == 0)
        force_update_display_info(w32);

    vo_calc_window_geometry(vo, &screen, &mon, w32->dpi_scale,
                            !w32->window_bounds_initialized, &geo);
    vo_apply_window_geometry(vo, &geo);

    bool reset_size = ((w32->o_dwidth != vo->dwidth ||
                       w32->o_dheight != vo->dheight) &&
                       w32->opts->auto_window_resize) || force;

    w32->o_dwidth = vo->dwidth;
    w32->o_dheight = vo->dheight;

    if (!w32->parent && (!w32->window_bounds_initialized || force)) {
        int x0 = geo.win.x0;
        int y0 = geo.win.y0;
        if (!w32->opts->geometry.xy_valid && w32->window_bounds_initialized) {
            x0 = w32->windowrc.left;
            y0 = w32->windowrc.top;
        }
        SetRect(&w32->windowrc, x0, y0, x0 + vo->dwidth, y0 + vo->dheight);
        w32->prev_windowrc = w32->windowrc;
        w32->window_bounds_initialized = true;
        w32->win_force_pos = geo.flags & VO_WIN_FORCE_POS;
        w32->fit_on_screen = true;
        goto finish;
    }

    // The rect which size is going to be modified.
    RECT *rc = &w32->windowrc;

    // The desired size always matches the window size in wid mode.
    if (!reset_size || w32->parent) {
        GetClientRect(w32->window, &r);
        // Restore vo_dwidth and vo_dheight, which were reset in vo_config()
        vo->dwidth = r.right;
        vo->dheight = r.bottom;
    } else {
        if (w32->current_fs || w32->opts->window_maximized)
            rc = &w32->prev_windowrc;
        w32->fit_on_screen = true;
    }

    resize_and_move_rect(w32, rc, vo->dwidth, vo->dheight);

finish:
    reinit_window_state(w32);
}

static void gui_thread_reconfig(void *ptr)
{
    window_reconfig(ptr, false);
}

// Resize the window. On the first call, it's also made visible.
void vo_w32_config(struct vo *vo)
{
    struct vo_w32_state *w32 = vo->w32;
    mp_dispatch_run(w32->dispatch, gui_thread_reconfig, w32);
}

static void w32_api_load(struct vo_w32_state *w32)
{
    // Dark mode related functions, available since the 1809 Windows 10 update
    // Check the Windows build version as on previous versions used ordinals
    // may point to unexpected code/data. Alternatively could check uxtheme.dll
    // version directly, but it is little bit more boilerplate code, and build
    // number is good enough check.
    HMODULE uxtheme_dll = !check_windows10_build(17763) ? NULL :
                GetModuleHandle(L"uxtheme.dll");
    w32->api.pShouldAppsUseDarkMode = !uxtheme_dll ? NULL :
                (void *)GetProcAddress(uxtheme_dll, MAKEINTRESOURCEA(132));
    w32->api.pSetPreferredAppMode = !uxtheme_dll ? NULL :
                (void *)GetProcAddress(uxtheme_dll, MAKEINTRESOURCEA(135));
}

static MP_THREAD_VOID gui_thread(void *ptr)
{
    struct vo_w32_state *w32 = ptr;
    bool ole_ok = false;
    int res = 0;

    mp_thread_set_name("window");

    w32_api_load(w32);

    if (w32->opts->WinID >= 0)
        w32->parent = (HWND)(intptr_t)(w32->opts->WinID);

    ATOM cls = get_window_class();
    if (w32->parent) {
        RECT r;
        GetClientRect(w32->parent, &r);
        CreateWindowExW(WS_EX_NOPARENTNOTIFY, (LPWSTR)MAKEINTATOM(cls), MPV_WINDOW_CLASS_NAME,
                        WS_CHILD | WS_VISIBLE, 0, 0, r.right, r.bottom,
                        w32->parent, 0, HINST_THISCOMPONENT, w32);

        // Install a hook to get notifications when the parent changes size
        if (w32->window)
            install_parent_hook(w32);
    } else {
        CreateWindowExW(0, (LPWSTR)MAKEINTATOM(cls), MPV_WINDOW_CLASS_NAME,
                        update_style(w32, 0), CW_USEDEFAULT, SW_HIDE, 100, 100,
                        0, 0, HINST_THISCOMPONENT, w32);
    }

    if (!w32->window) {
        MP_ERR(w32, "unable to create window!\n");
        goto done;
    }

    w32->menu_ctx = mp_win32_menu_init(w32->window);
    update_dark_mode(w32);
    update_corners_pref(w32);
    if (w32->opts->window_affinity)
        update_affinity(w32);
    if (w32->opts->backdrop_type)
        update_backdrop(w32);
    if (w32->opts->cursor_passthrough)
        update_cursor_passthrough(w32);
    if (w32->opts->native_touch)
        update_native_touch(w32);
    if (!w32->opts->input_ime)
        update_ime_enabled(w32, false);

    if (SUCCEEDED(OleInitialize(NULL))) {
        ole_ok = true;

        IDropTarget *dt = mp_w32_droptarget_create(w32->log, w32->opts, w32->input_ctx);
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
                w32->tbtn_created_msg = RegisterWindowMessage(L"TaskbarButtonCreated");
            }
        }
    } else {
        MP_ERR(w32, "Failed to initialize OLE/COM\n");
    }

    w32->tracking   = FALSE;
    w32->track_event = (TRACKMOUSEEVENT){
        .cbSize    = sizeof(TRACKMOUSEEVENT),
        .dwFlags   = TME_LEAVE,
        .hwndTrack = w32->window,
    };

    if (w32->parent)
        EnableWindow(w32->window, 0);

    w32->cursor_visible = true;
    w32->moving = false;
    w32->snapped = 0;
    w32->snap_dx = w32->snap_dy = 0;

    mp_dispatch_set_wakeup_fn(w32->dispatch, wakeup_gui_thread, w32);

    res = 1;
done:

    mp_rendezvous(w32, res); // init barrier

    // This blocks until the GUI thread is to be exited.
    if (res)
        run_message_loop(w32);

    MP_VERBOSE(w32, "uninit\n");

    remove_parent_hook(w32);
    if (w32->menu_ctx)
        mp_win32_menu_uninit(w32->menu_ctx);
    if (w32->window && !w32->destroyed)
        DestroyWindow(w32->window);
    if (w32->taskbar_list)
        ITaskbarList2_Release(w32->taskbar_list);
    if (w32->taskbar_list3)
        ITaskbarList3_Release(w32->taskbar_list3);
    if (ole_ok)
        OleUninitialize();
    SetThreadExecutionState(ES_CONTINUOUS);
    MP_THREAD_RETURN();
}

bool vo_w32_init(struct vo *vo)
{
    assert(!vo->w32);

    struct vo_w32_state *w32 = talloc_ptrtype(vo, w32);
    *w32 = (struct vo_w32_state){
        .log = mp_log_new(w32, vo->log, "win32"),
        .vo = vo,
        .opts_cache = m_config_cache_alloc(w32, vo->global, &vo_sub_opts),
        .input_ctx = vo->input_ctx,
        .dispatch = mp_dispatch_create(w32),
    };
    w32->opts = w32->opts_cache->opts;
    vo->w32 = w32;

    if (mp_thread_create(&w32->thread, gui_thread, w32))
        goto fail;

    if (!mp_rendezvous(w32, 0)) { // init barrier
        mp_thread_join(w32->thread);
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

    return true;
fail:
    talloc_free(w32);
    vo->w32 = NULL;
    return false;
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
    case VOCTRL_VO_OPTS_CHANGED: {
        void *changed_option;

        while (m_config_cache_get_next_changed(w32->opts_cache,
                                               &changed_option))
        {
            struct mp_vo_opts *vo_opts = w32->opts_cache->opts;

            if (changed_option == &vo_opts->fullscreen) {
                if (!vo_opts->fullscreen)
                    update_maximized_state(w32, true);
                reinit_window_state(w32);
            } else if (changed_option == &vo_opts->window_affinity) {
                update_affinity(w32);
            } else if (changed_option == &vo_opts->ontop) {
                update_window_state(w32);
            } else if (changed_option == &vo_opts->backdrop_type) {
                update_backdrop(w32);
            } else if (changed_option == &vo_opts->cursor_passthrough) {
                update_cursor_passthrough(w32);
            } else if (changed_option == &vo_opts->border ||
                       changed_option == &vo_opts->title_bar)
            {
                update_window_style(w32);
                update_window_state(w32);
            } else if (changed_option == &vo_opts->show_in_taskbar) {
                // This hide and show is apparently required according to the documentation:
                // https://learn.microsoft.com/en-us/windows/win32/shell/taskbar#managing-taskbar-buttons
                ShowWindow(w32->window, SW_HIDE);
                update_window_style(w32);
                ShowWindow(w32->window, SW_SHOW);
                update_window_state(w32);
            } else if (changed_option == &vo_opts->window_minimized) {
                update_minimized_state(w32);
            } else if (changed_option == &vo_opts->window_maximized) {
                update_maximized_state(w32, false);
            } else if (changed_option == &vo_opts->window_corners) {
                update_corners_pref(w32);
            } else if (changed_option == &vo_opts->native_touch) {
                update_native_touch(w32);
            } else if (changed_option == &vo_opts->input_ime) {
                update_ime_enabled(w32, vo_opts->input_ime);
            } else if (changed_option == &vo_opts->geometry || changed_option == &vo_opts->autofit ||
                changed_option == &vo_opts->autofit_smaller || changed_option == &vo_opts->autofit_larger)
            {
                if (w32->opts->window_maximized) {
                    w32->unmaximize = true;
                }
                window_reconfig(w32, true);
            }
        }

        return VO_TRUE;
    }
    case VOCTRL_GET_WINDOW_ID: {
        if (!w32->window)
            return VO_NOTAVAIL;
        *(int64_t *)arg = (intptr_t)w32->window;
        return VO_TRUE;
    }
    case VOCTRL_GET_HIDPI_SCALE: {
        *(double *)arg = w32->dpi_scale;
        return VO_TRUE;
    }
    case VOCTRL_GET_UNFS_WINDOW_SIZE: {
        int *s = arg;

        if (!w32->window_bounds_initialized)
            return VO_FALSE;

        RECT *rc = (w32->current_fs || w32->opts->window_maximized)
                        ? &w32->prev_windowrc : &w32->windowrc;
        s[0] = rect_w(*rc);
        s[1] = rect_h(*rc);
        return VO_TRUE;
    }
    case VOCTRL_SET_UNFS_WINDOW_SIZE: {
        int *s = arg;

        if (!w32->window_bounds_initialized)
            return VO_FALSE;

        RECT *rc = w32->current_fs ? &w32->prev_windowrc : &w32->windowrc;
        resize_and_move_rect(w32, rc, s[0], s[1]);

        if (w32->opts->window_maximized && !w32->current_fs) {
            w32->unmaximize = true;
        }
        w32->fit_on_screen = true;
        reinit_window_state(w32);
        return VO_TRUE;
    }
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
    case VOCTRL_GET_DISPLAY_RES: ;
        RECT monrc = get_monitor_info(w32).rcMonitor;
        ((int *)arg)[0] = monrc.right - monrc.left;
        ((int *)arg)[1] = monrc.bottom - monrc.top;
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
    case VOCTRL_GET_FOCUSED:
        *(bool *)arg = w32->focused;
        return VO_TRUE;
    case VOCTRL_BEGIN_DRAGGING:
        w32->start_dragging = true;
        return VO_TRUE;
    case VOCTRL_SHOW_MENU:
        PostMessageW(w32->window, WM_SHOWMENU, 0, 0);
        return VO_TRUE;
    case VOCTRL_UPDATE_MENU:
        mp_win32_menu_update(w32->menu_ctx, (struct mpv_node *)arg);
        return VO_TRUE;
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
    mp_thread_join(w32->thread);

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

void vo_w32_set_transparency(struct vo *vo, bool enable)
{
    struct vo_w32_state *w32 = vo->w32;
    if (w32->parent)
        return;

    DWM_BLURBEHIND dbb = {0};
    if (enable) {
        HRGN rgn = CreateRectRgn(0, 0, -1, -1);
        dbb.dwFlags = DWM_BB_ENABLE | DWM_BB_BLURREGION;
        dbb.hRgnBlur = rgn;
        dbb.fEnable = TRUE;
        DwmEnableBlurBehindWindow(w32->window, &dbb);
        DeleteObject(rgn);
    } else {
        dbb.dwFlags = DWM_BB_ENABLE;
        dbb.fEnable = FALSE;
        DwmEnableBlurBehindWindow(w32->window, &dbb);
    }
}

BOOL WINAPI DllMain(HANDLE dll, DWORD reason, LPVOID reserved)
{
    if (reason == DLL_PROCESS_DETACH && window_class)
        UnregisterClassW(MPV_WINDOW_CLASS_NAME, HINST_THISCOMPONENT);
    return TRUE;
}
