#include <limits.h>
#include <windows.h>

#include "osdep/keycodes.h"
#include "input/input.h"
#include "input/mouse.h"
#include "mp_msg.h"
#include "video_out.h"
#include "aspect.h"
#include "w32_common.h"

extern void mplayer_put_key(int code);

static const char* classname = "MPlayer - Media player for Win32";
int vo_vm = 0;
HDC vo_hdc = 0;

uint32_t o_dwidth;
uint32_t o_dheight;

static HINSTANCE hInstance;
HWND vo_window = 0;
static int cursor = 1;

static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
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
	    switch (wParam) {
		case VK_LEFT:    mplayer_put_key(KEY_LEFT);      break;
		case VK_UP:      mplayer_put_key(KEY_UP);        break;
		case VK_RIGHT:   mplayer_put_key(KEY_RIGHT);     break;
		case VK_DOWN:    mplayer_put_key(KEY_DOWN);      break;
		case VK_TAB:     mplayer_put_key(KEY_TAB);       break;
		case VK_CONTROL: mplayer_put_key(KEY_CTRL);      break;
		case VK_DELETE:  mplayer_put_key(KEY_DELETE);    break;
		case VK_INSERT:  mplayer_put_key(KEY_INSERT);    break;
		case VK_HOME:    mplayer_put_key(KEY_HOME);      break;
		case VK_END:     mplayer_put_key(KEY_END);       break;
		case VK_PRIOR:   mplayer_put_key(KEY_PAGE_UP);   break;
		case VK_NEXT:    mplayer_put_key(KEY_PAGE_DOWN); break;
		case VK_ESCAPE:  mplayer_put_key(KEY_ESC);       break;
	    }
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
	case WM_MOUSEWHEEL:
	    if (!vo_nomouse_input) {
		int x = GET_WHEEL_DELTA_WPARAM(wParam);
		if (x > 0)
		    mplayer_put_key(MOUSE_BTN3);
		else
		    mplayer_put_key(MOUSE_BTN4);
		break;
	    }
    }
    
    return DefWindowProc(hWnd, message, wParam, lParam);
}

int vo_w32_check_events(void) {
    MSG msg;
    int r = 0;
    while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
	TranslateMessage(&msg);
	DispatchMessage(&msg);
	switch (msg.message) {
	    case WM_ACTIVATE:
		r |= VO_EVENT_EXPOSE;
		break;
	}
    }
    
    return r;
}

static void updateScreenProperties() {
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
    vo_depthonscreen = dm.dmBitsPerPel;
    aspect_save_screenres(vo_screenwidth, vo_screenheight);
}

static void changeMode(void) {
    DEVMODE dm;
    dm.dmSize = sizeof dm;
    dm.dmDriverExtra = 0;

    dm.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
    dm.dmBitsPerPel = vo_depthonscreen;
    dm.dmPelsWidth = vo_screenwidth;
    dm.dmPelsHeight = vo_screenheight;

    if (vo_vm) {
	int bestMode = -1;
	int bestScore = INT_MAX;
	int i;
	for (i = 0; EnumDisplaySettings(0, i, &dm); ++i) {
	    int score = (dm.dmPelsWidth - o_dwidth) * (dm.dmPelsHeight - o_dheight);
	    if (dm.dmBitsPerPel != vo_depthonscreen) continue;
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

int createRenderingContext(void) {
    HWND layer = HWND_NOTOPMOST;
    PIXELFORMATDESCRIPTOR pfd;
    RECT r;
    int pf;
    int style = (vo_border && !vo_fs) ?
                (WS_OVERLAPPEDWINDOW | WS_SIZEBOX) : WS_POPUP;

    if (vo_fs || vo_ontop) layer = HWND_TOPMOST;
    if (vo_fs) {
	changeMode();
	if (cursor) {
	    ShowCursor(0);
	    cursor = 0;
	}
    } else {
	resetMode();
	if (!cursor) {
	    ShowCursor(1);
	    cursor = 1;
	}
    }
    updateScreenProperties();
    ShowWindow(vo_window, SW_HIDE);
    SetWindowLong(vo_window, GWL_STYLE, style);
    vo_dwidth = vo_fs ? vo_screenwidth : o_dwidth;
    vo_dheight = vo_fs ? vo_screenheight : o_dheight;
    r.left = vo_fs ? 0 : vo_dy;
    r.right = r.left + vo_dwidth;
    r.top = vo_fs ? 0 : vo_dx;
    r.bottom = r.top + vo_dheight;
    AdjustWindowRect(&r, style, 0);
    SetWindowPos(vo_window, layer, r.left, r.top, r.right - r.left, r.bottom - r.top, SWP_SHOWWINDOW);

    memset(&pfd, 0, sizeof pfd);
    pfd.nSize = sizeof pfd;
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 24;
    pfd.iLayerType = PFD_MAIN_PLANE;
    pf = ChoosePixelFormat(vo_hdc, &pfd);
    if (!pf) {
    	mp_msg(MSGT_VO, MSGL_ERR, "vo: win32: unable to select a valid pixel format!\n");
	return 0;
    }

    SetPixelFormat(vo_hdc, pf, &pfd);
    
    mp_msg(MSGT_VO, MSGL_V, "vo: win32: running at %dx%d with depth %d\n", vo_screenwidth, vo_screenheight, vo_depthonscreen);

    return 1;
}

int vo_init(void) {
    HICON 	mplayerIcon = 0;
    char 	exedir[MAX_PATH];

    if (vo_window)
	return 1;

    hInstance = GetModuleHandle(0);
    
    if (GetModuleFileName(0, exedir, MAX_PATH))
	mplayerIcon = ExtractIcon(hInstance, exedir, 0);
    if (!mplayerIcon)
	mplayerIcon = LoadIcon(0, IDI_APPLICATION);

  {
    WNDCLASSEX wcex = { sizeof wcex, CS_OWNDC, WndProc, 0, 0, hInstance, mplayerIcon, LoadCursor(0, IDC_ARROW), (HBRUSH)GetStockObject(BLACK_BRUSH), 0, classname, mplayerIcon };

    if (!RegisterClassEx(&wcex)) {
	mp_msg(MSGT_VO, MSGL_ERR, "vo: win32: unable to register window class!\n");
	return 0;
    }
  }

    if (WinID >= 0)
      vo_window = WinID;
    else {
    vo_window = CreateWindowEx(0, classname, classname,
                  vo_border ? (WS_OVERLAPPEDWINDOW | WS_SIZEBOX) : WS_POPUP,
                  CW_USEDEFAULT, 0, 100, 100, 0, 0, hInstance, 0);
    if (!vo_window) {
	mp_msg(MSGT_VO, MSGL_ERR, "vo: win32: unable to create window!\n");
	return 0;
    }
    }

    vo_hdc = GetDC(vo_window);

    updateScreenProperties();

    return 1;
}

void vo_w32_fullscreen(void) {
    vo_fs = !vo_fs;

    createRenderingContext();
}

void vo_w32_border() {
    vo_border = !vo_border;
    createRenderingContext();
}

void vo_w32_ontop( void )
{
    vo_ontop = !vo_ontop;
    if (!vo_fs) {
	HWND layer = HWND_NOTOPMOST;
	if (vo_ontop) layer = HWND_TOPMOST;
	SetWindowPos(vo_window, layer, (vo_screenwidth - vo_dwidth) / 2, (vo_screenheight - vo_dheight) / 2, vo_dwidth, vo_dheight, SWP_SHOWWINDOW);
    }
}

void vo_w32_uninit() {
    mp_msg(MSGT_VO, MSGL_V, "vo: win32: uninit\n");
    resetMode();
    ShowCursor(1);
    vo_depthonscreen = 0;
    if (WinID < 0)
    DestroyWindow(vo_window);
    vo_window = 0;
    UnregisterClass(classname, 0);
}
