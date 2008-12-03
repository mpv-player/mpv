/*
 * MPlayer GUI for Win32
 * Copyright (C) 2003 Sascha Sommer <saschasommer@freenet.de>
 * Copyright (C) 2006 Erik Augustson <erik_27can@yahoo.com>
 * Copyright (C) 2006 Gianluigi Tiesi <sherpya@netfarm.it>
 *
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
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>
#include <windows.h>
#include <windowsx.h>
#include <shlobj.h>

#include "version.h"
#include "mplayer.h"
#include "mp_fifo.h"
#include "mp_msg.h"
#include "help_mp.h"
#include "input/input.h"
#include "input/mouse.h"
#include "osdep/keycodes.h"
#include "stream/stream.h"
#include "libvo/video_out.h"
#include "gui/interface.h"
#include "gui.h"
#include "dialogs.h"

// HACK around bug in old mingw
#undef INVALID_FILE_ATTRIBUTES
#define INVALID_FILE_ATTRIBUTES ((DWORD)-1)

#ifndef WM_XBUTTONDOWN
# define WM_XBUTTONDOWN    0x020B
# define WM_XBUTTONUP      0x020C
# define WM_XBUTTONDBLCLK  0x020D
#endif

/* Globals / Externs */
void renderinfobox(skin_t *skin, window_priv_t *priv);
void renderwidget(skin_t *skin, image *dest, widget *item, int state);
void print_version(void);
float sub_aspect;

DWORD oldtime;
NOTIFYICONDATA nid;
int console_state = 0;
play_tree_t *playtree = NULL;

static HBRUSH    colorbrush = NULL;           //Handle to colorkey brush
static COLORREF windowcolor = RGB(255,0,255); //Windowcolor == colorkey

void console_toggle(void)
{
    if (console_state)
    {
        FreeConsole();
        console = 0;
        console_state = 0;
    }
    else
    {
        /* This code comes from: http://dslweb.nwnexus.com/~ast/dload/guicon.htm */
        CONSOLE_SCREEN_BUFFER_INFO coninfo;
        FILE *fp;
        HWND hwnd = NULL;
        console = 1;
        AllocConsole();
        SetConsoleTitle(MP_TITLE);

        /* disable the close button for now */
        while (!hwnd)
        {
            hwnd = FindWindow(NULL, MP_TITLE);
            Sleep(100);
        }
        DeleteMenu(GetSystemMenu(hwnd, 0), SC_CLOSE, MF_BYCOMMAND);

        GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &coninfo);
        coninfo.dwSize.Y = 1000;
        SetConsoleScreenBufferSize(GetStdHandle(STD_OUTPUT_HANDLE), coninfo.dwSize);
        fp = freopen("con", "w", stdout);
        *stdout = *fp;
        setvbuf(stdout, NULL, _IONBF, 0);
        fp = freopen("con", "r", stdin);
        *stdin = *fp;
        setvbuf(stdin, NULL, _IONBF, 0);
        fp = freopen("con", "w", stdout);
        *stderr = *fp;
        setvbuf(stderr, NULL, _IONBF, 0);
        print_version();
        console_state = 1;
    }
}

void capitalize(char *filename)
{
    unsigned int i;
    BOOL cap = TRUE;
    for (i=0; i < strlen(filename); i++)
    {
        if (cap)
        {
            cap = FALSE;
            filename[i] = toupper(filename[i]);
        }
        else if (filename[i] == ' ')
            cap = TRUE;
        else
            filename[i] = tolower(filename[i]);
    }
}

static image *get_drawground(HWND hwnd)
{
    gui_t * gui = (gui_t *) GetWindowLongPtr(hwnd, GWLP_USERDATA);
    unsigned int i;
    if(!gui) return NULL;
    for(i=0; i<gui->window_priv_count; i++)
        if(gui->window_priv[i]->hwnd==hwnd)
            return &gui->window_priv[i]->img;
    return NULL;
}

static HBITMAP get_bitmap(HWND hwnd)
{
    gui_t *gui = (gui_t *) GetWindowLongPtr(hwnd, GWLP_USERDATA);
    unsigned int i;
    if(!gui) return NULL;
    for(i=0; i<gui->window_priv_count; i++)
        if(gui->window_priv[i]->hwnd == hwnd)
            return gui->window_priv[i]->bitmap;
    return NULL;
}

static int get_windowtype(HWND hwnd)
{
    gui_t *gui = (gui_t *) GetWindowLongPtr(hwnd, GWLP_USERDATA);
    unsigned int i;
    if(!gui) return -1;
    for(i=0; i<gui->window_priv_count; i++)
        if(gui->window_priv[i]->hwnd == hwnd)
            return gui->window_priv[i]->type;
    return -1;
}

static void uninit(gui_t *gui)
{
    if(gui->skin) destroy_window(gui);
    if(gui->playlist) gui->playlist->free_playlist(gui->playlist);
    gui->playlist = NULL;
}

/*
    the gui message handler
    tries to handle the incoming messages
    and passes them to the player's message handler if it can't handle them
*/
static void handlemsg(HWND hWnd, int msg)
{
    gui_t *gui = (gui_t *) GetWindowLongPtr(hWnd, GWLP_USERDATA);
    if(msg == evNone) return;

    switch(msg)
    {
        case evLoadPlay:
        case evLoad:
            if(display_openfilewindow(gui, 0) && (msg == evLoadPlay))
                handlemsg(hWnd, evDropFile);
            return;
        case evLoadSubtitle:
            display_opensubtitlewindow(gui);
            break;
        case evPreferences:
            display_prefswindow(gui);
            return;
        case evPlayList:
            display_playlistwindow(gui);
            return;
        case evSkinBrowser:
            display_skinbrowser(gui);
            break;
        case evEqualizer:
            display_eqwindow(gui);
            break;
        case evAbout:
            MessageBox(hWnd, COPYRIGHT, "About", MB_OK);
            break;
        case evIconify:
            ShowWindow(hWnd, SW_MINIMIZE);
            break;
        case evIncVolume:
            mplayer_put_key(KEY_VOLUME_UP);
            break;
        case evDecVolume:
            mplayer_put_key(KEY_VOLUME_DOWN);
            break;
        default:
            mp_msg(MSGT_GPLAYER, MSGL_V, "[GUI] received msg %s (%i)\n", gui->skin->geteventname(msg), msg);
            break;
    }
    gui->playercontrol(msg);
}

static widget *clickedinsidewidget(gui_t *gui, int window, int x, int y)
{
    unsigned int i;
    widget *item;
    for(i=0; i<gui->skin->widgetcount; i++)
    {
        item = gui->skin->widgets[i];
        if((item->window == window) && (item->x <= x) && (item->x + item->width >= x) &&
           (item->y <= y) && (item->y + item->height >= y))
            return item;
    }
    return NULL;
}

/* updates sliders and the display */
static void updatedisplay(gui_t *gui, HWND hwnd)
{
    unsigned int i;
    window_priv_t *priv = NULL;
    DWORD time = timeGetTime();

    if(!hwnd) return;

    /* load all potmeters hpotmeters */
    for(i=0; i<gui->skin->widgetcount; i++)
    {
        if(gui->skin->widgets[i]->type == tyHpotmeter || gui->skin->widgets[i]->type == tyPotmeter)
        {
            if(gui->skin->widgets[i]->msg == evSetVolume)
                gui->skin->widgets[i]->value = guiIntfStruct.Volume;
            else if(gui->skin->widgets[i]->msg == evSetMoviePosition)
                gui->skin->widgets[i]->value = guiIntfStruct.Position;
            else if(gui->skin->widgets[i]->msg == evSetBalance)
                gui->skin->widgets[i]->value = guiIntfStruct.Balance;
            if(gui->skin->widgets[i]->window == get_windowtype(hwnd))
                renderwidget(gui->skin, get_drawground(hwnd), gui->skin->widgets[i],
                             gui->skin->widgets[i]->pressed ? 0 : 1);
        }
        /* update some buttons */
        if(gui->skin->widgets[i]->type == tyButton && gui->skin->widgets[i]->window == get_windowtype(hwnd))
        {
            if(gui->skin->widgets[i]->msg == evPlaySwitchToPause)
            {
                gui->skin->widgets[i]->value = guiIntfStruct.Playing;
                    renderwidget(gui->skin, get_drawground(hwnd), gui->skin->widgets[i],
                                 guiIntfStruct.Playing == 1 ? 0 : 1);
            }
            if(gui->skin->widgets[i]->msg == evMute)
            {
                gui->skin->widgets[i]->value = guiIntfStruct.Volume;
                    renderwidget(gui->skin, get_drawground(hwnd), gui->skin->widgets[i],
                                 guiIntfStruct.Volume == 0.0f ? 0 : 1);
            }
        }
    }

    /* updating the display once a 100.second is enough imo */
    if((time - oldtime) < 100) return;
    oldtime=time;

    /* suppress directx's fullscreen window when using the sub window */
    if(sub_window && &video_driver_list[0] && strstr("directx", video_driver_list[0]))
    {
        HWND hWndFS = NULL; //handle to directx's fullscreen window
        if(hWndFS == NULL)
        {
            hWndFS = FindWindow(NULL, "MPlayer Fullscreen");
            if(hWndFS != NULL) DestroyWindow(hWndFS); //sub window handles fullscreen
        }
    }

    for (i=0; i<gui->window_priv_count; i++)
    {
        if(gui->window_priv[i]->hwnd == hwnd)
            priv=gui->window_priv[i];
    }// Sherpya
    /* display the status msgs */
    renderinfobox(gui->skin, priv);
    RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE);
}

static LRESULT CALLBACK SubProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    gui_t *gui = (gui_t *) GetWindowLongPtr(hWnd, GWLP_USERDATA);
    if (gui && (gui->subwindow != hWnd)) return FALSE;

    switch (message)
    {
        case WM_CLOSE:
            handlemsg(hWnd, evExit);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_KEYDOWN:
        {
            switch(wParam)
            {
                case VK_LEFT:
                    mplayer_put_key(KEY_LEFT);
                    break;
                case VK_UP:
                    mplayer_put_key(KEY_UP);
                    break;
                case VK_RIGHT:
                    mplayer_put_key(KEY_RIGHT);
                    break;
                case VK_DOWN:
                    mplayer_put_key(KEY_DOWN);
                    break;
                case VK_TAB:
                    mplayer_put_key(KEY_TAB);
                    break;
                case VK_BACK:
                    mplayer_put_key(KEY_BS);
                    break;
                case VK_DELETE:
                    mplayer_put_key(KEY_DELETE);
                    break;
                case VK_INSERT:
                    mplayer_put_key(KEY_INSERT);
                    break;
                case VK_HOME:
                    mplayer_put_key(KEY_HOME);
                    break;
                case VK_END:
                    mplayer_put_key(KEY_END);
                    break;
                case VK_PRIOR:
                    mplayer_put_key(KEY_PAGE_UP);
                    break;
                case VK_NEXT:
                    mplayer_put_key(KEY_PAGE_DOWN);
                    break;
                case VK_ESCAPE:
                    mplayer_put_key(KEY_ESC);
                    break;
            }
            break;
        }
        case WM_COMMAND:
        {
            switch(LOWORD(wParam))
            {
                case IDEXIT:
                    PostQuitMessage(0);
                    handlemsg(hWnd, evExit);
                    break;
                case IDFILE_OPEN:
                    handlemsg(hWnd, evLoadPlay);
                    break;
                case IDURL_OPEN:
                    display_openurlwindow(gui, 0);
                    break;
                case IDDIR_OPEN:
                {
                    static char path[MAX_PATH];
                    BROWSEINFO bi;
                    LPITEMIDLIST pidl;
                    memset(&bi, 0, sizeof(BROWSEINFO));
                    bi.lpszTitle = "Choose a Directory...";
                    pidl = SHBrowseForFolder(&bi);
                    if (SHGetPathFromIDList(pidl, path))
                    {
                        gui->playlist->clear_playlist(gui->playlist);
                        adddirtoplaylist(gui->playlist, path, TRUE);
                        gui->startplay(gui);
                    }
                    break;
                }
                case ID_PTRACK:
                    handlemsg(hWnd, evPrev);
                    break;
                case ID_SEEKB:
                    handlemsg(hWnd, evBackward10sec);
                    break;
                case ID_PLAY:
                    handlemsg(hWnd, evPlaySwitchToPause);
                    break;
                case ID_STOP:
                    handlemsg(hWnd, evStop);
                    break;
                case ID_SEEKF:
                    handlemsg(hWnd, evForward10sec);
                    break;
                case ID_NTRACK:
                    handlemsg(hWnd, evNext);
                    break;
#ifdef CONFIG_DVDREAD
                case ID_CHAPTERSEL:
                    display_chapterselwindow(gui);
                    break;
#endif
                case ID_FULLSCREEN:
                    mp_input_queue_cmd(mp_input_parse_cmd("vo_fullscreen"));
                    break;
                case ID_MUTE:
                    mp_input_queue_cmd(mp_input_parse_cmd("mute"));
                    break;
                case ID_ASPECT1:
                    mp_input_queue_cmd(mp_input_parse_cmd("switch_ratio 1.777777"));
                    break;
                case ID_ASPECT2:
                    mp_input_queue_cmd(mp_input_parse_cmd("switch_ratio 1.333333"));
                    break;
                case ID_ASPECT3:
                    mp_input_queue_cmd(mp_input_parse_cmd("switch_ratio 2.35"));
                    break;
                case ID_ASPECT4:
                    mp_input_queue_cmd(mp_input_parse_cmd("switch_ratio 0"));
                    break;
                case IDSUB_TOGGLE:
                    mp_input_queue_cmd(mp_input_parse_cmd("sub_visibility"));
                    break;
                case IDSUB_CYCLE:
                    mp_input_queue_cmd(mp_input_parse_cmd("sub_select"));
                    break;
            }
            return 0;
        }
        case WM_CHAR:
            mplayer_put_key(wParam);
            break;
        case WM_DROPFILES:
        {
            if(!lParam)
            {
                char file[MAX_PATH];
                int filecount = DragQueryFile((HDROP) wParam, -1, file, MAX_PATH);
                int i;
                for(i=0; i<filecount; i++)
                {
                    DragQueryFile((HDROP) wParam, i, file, MAX_PATH);
                    mplSetFileName(NULL, file, STREAMTYPE_FILE);
                    if(!parse_filename(file, playtree, mconfig, 1))
                        gui->playlist->add_track(gui->playlist, file, NULL, NULL, 0);
                }
                DragFinish((HDROP) wParam);
                handlemsg(hWnd, evDropFile);
            }
            else
            {
                gui->playlist->clear_playlist(gui->playlist);
                gui->playlist->add_track(gui->playlist, (const char *) wParam, NULL, NULL, 0);
                handlemsg(hWnd, evDropFile);
            }
            SetForegroundWindow(gui->subwindow);
            return 0;
        }
        case WM_LBUTTONDOWN:
        {
            if(!vo_nomouse_input)
               mplayer_put_key(MOUSE_BTN0);
            break;
        }
        case WM_MBUTTONDOWN:
        {
            if(!vo_nomouse_input)
               mplayer_put_key(MOUSE_BTN1);
            break;
        }
        case WM_RBUTTONDOWN:
        {
            POINT point;
            point.x = GET_X_LPARAM(lParam);
            point.y = GET_Y_LPARAM(lParam);
            ClientToScreen(hWnd, &point);
            if(guiIntfStruct.StreamType == STREAMTYPE_DVD)
                EnableMenuItem(gui->dvdmenu, ID_CHAPTERSEL, MF_BYCOMMAND | MF_ENABLED);
            TrackPopupMenu(gui->submenu, 0, point.x, point.y, 0, hWnd, NULL);
            return 0;
        }
        case WM_LBUTTONDBLCLK:
        {
            if(!vo_nomouse_input)
                mplayer_put_key(MOUSE_BTN0_DBL);
            break;
        }
        case WM_MBUTTONDBLCLK:
        {
            if(!vo_nomouse_input)
                mplayer_put_key(MOUSE_BTN1_DBL);
            break;
        }
        case WM_RBUTTONDBLCLK:
        {
            if(!vo_nomouse_input)
                mplayer_put_key(MOUSE_BTN2_DBL);
            break;
        }
        case WM_MOUSEWHEEL:
        {
            int x = GET_WHEEL_DELTA_WPARAM(wParam);
            if(vo_nomouse_input)
                break;
            if (x > 0)
                mplayer_put_key(MOUSE_BTN3);
            else
                mplayer_put_key(MOUSE_BTN4);
            break;
        }
        case WM_XBUTTONDOWN:
        {
            if(vo_nomouse_input)
                break;
            if(HIWORD(wParam) == 1)
                mplayer_put_key(MOUSE_BTN5);
            else
                mplayer_put_key(MOUSE_BTN6);
            break;
        }
        case WM_XBUTTONDBLCLK:
        {
            if(vo_nomouse_input)
                break;
            if(HIWORD(wParam) == 1)
                mplayer_put_key(MOUSE_BTN5_DBL);
            else
                mplayer_put_key(MOUSE_BTN6_DBL);
            break;
        }
        case WM_TIMER:
        {
            if(fullscreen) while(ShowCursor(FALSE) >= 0){}
            KillTimer(hWnd, ID_TIMER);
            return 0;
        }
        case WM_MOUSEMOVE:
        {
            ShowCursor(TRUE);
            SetTimer(hWnd, ID_TIMER, 3000, (TIMERPROC) NULL);
            break;
        }
        case WM_WINDOWPOSCHANGED:
        {
            int tmpheight=0;
            static uint32_t rect_width;
            static uint32_t rect_height;
            RECT rd;
            POINT pt;
            pt.x = 0;
            pt.y = 0;
            GetClientRect(hWnd, &rd);
            ClientToScreen(hWnd, &pt);

            rect_width = rd.right - rd.left;
            rect_height = rd.bottom - rd.top;

            /* maintain our aspect ratio */
            tmpheight = ((float)rect_width/sub_aspect);
            tmpheight += tmpheight % 2;
            if(tmpheight > rect_height)
            {
                rect_width = ((float)rect_height*sub_aspect);
                rect_width += rect_width % 2;
            }
            else rect_height = tmpheight;

            rd.right = rd.left + rect_width;
            rd.bottom = rd.top + rect_height;

            AdjustWindowRect(&rd, WS_OVERLAPPEDWINDOW | WS_SIZEBOX, 0);
            SetWindowPos(hWnd, 0, fullscreen?0:pt.x+rd.left, fullscreen?0:pt.y+rd.top,
                         fullscreen?vo_screenwidth:rd.right-rd.left, fullscreen?vo_screenheight:rd.bottom-rd.top, SWP_NOOWNERZORDER);
            SetForegroundWindow(hWnd);
            return 0;
        }
        case WM_SYSCOMMAND:
        {
            switch(wParam)
            {
                case SC_SCREENSAVE:
                case SC_MONITORPOWER:
                    mp_msg(MSGT_VO, MSGL_V ,"<vo_directx><INFO>killing screensaver\n" );
                    return 0;
            }
            break;
        }
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            RECT rect;
            HDC hdc = BeginPaint(hWnd, &ps);
            HDC hMemDC = CreateCompatibleDC(hdc);
            HBRUSH blackbrush = (HBRUSH)GetStockObject(BLACK_BRUSH);
            int width, height;
            GetClientRect(hWnd, &rect);
            width = rect.right - rect.left;
            height = rect.bottom - rect.top;
            if(guiIntfStruct.Playing == 0)
            {
                int i;
                window *desc = NULL;

                for (i=0; i<gui->skin->windowcount; i++)
                    if(gui->skin->windows[i]->type == wiSub)
                        desc = gui->skin->windows[i];

                SelectObject(hMemDC, get_bitmap(hWnd));
                StretchBlt(hdc, 0, 0, width, height, hMemDC, 0, 0, desc->base->bitmap[0]->width,
                           desc->base->bitmap[0]->height, SRCCOPY);
            } else {
                FillRect(GetDC(hWnd), &rect, fullscreen?blackbrush:colorbrush);
            }
            DeleteDC(hMemDC);
            EndPaint(hWnd, &ps);
            return 0;
        }
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

/* Window Proc for the gui Window */
static LRESULT CALLBACK EventProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    gui_t *gui = (gui_t *) GetWindowLongPtr(hWnd, GWLP_USERDATA);

    /* Avoid processing when then window doesn't match gui mainwindow */
    if (gui && (gui->mainwindow != hWnd)) return FALSE;

    switch (message)
    {
        case WM_CLOSE:
            handlemsg(hWnd, evExit);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_SYSTRAY:
        {
            switch(lParam)
            {
                POINT cursor;
                case WM_RBUTTONDOWN:
                {
                    GetCursorPos(&cursor);
                    SetForegroundWindow(hWnd);
                    TrackPopupMenu(gui->traymenu, 0, cursor.x, cursor.y, 0, hWnd, NULL);
                    break;
                }
                case WM_MBUTTONDBLCLK:
                case WM_LBUTTONDBLCLK:
                {
                    if(IsWindowVisible(hWnd)) ShowWindow(hWnd, SW_HIDE);
                    else { ShowWindow(hWnd, SW_SHOW); SetForegroundWindow(hWnd); }
                    break;
                }
            }
            break;
        }
        case WM_KEYDOWN:
        {
            switch(wParam)
            {
                case VK_LEFT:
                    mplayer_put_key(KEY_LEFT);
                    break;
                case VK_UP:
                    mplayer_put_key(KEY_UP);
                    break;
                case VK_RIGHT:
                    mplayer_put_key(KEY_RIGHT);
                    break;
                case VK_DOWN:
                    mplayer_put_key(KEY_DOWN);
                    break;
                case VK_TAB:
                    mplayer_put_key(KEY_TAB);
                    break;
                case VK_BACK:
                    mplayer_put_key(KEY_BS);
                    break;
                case VK_DELETE:
                    mplayer_put_key(KEY_DELETE);
                    break;
                case VK_INSERT:
                    mplayer_put_key(KEY_INSERT);
                    break;
                case VK_HOME:
                    mplayer_put_key(KEY_HOME);
                    break;
                case VK_END:
                    mplayer_put_key(KEY_END);
                    break;
                case VK_PRIOR:
                    mplayer_put_key(KEY_PAGE_UP);
                    break;
                case VK_NEXT:
                    mplayer_put_key(KEY_PAGE_DOWN);
                    break;
                case VK_ESCAPE:
                    mplayer_put_key(KEY_ESC);
                    break;
            }
            break;
        }
        case WM_CHAR:
            mplayer_put_key(wParam);
            break;
        case WM_COPYDATA:
        {
            if(lParam)
            {
                PCOPYDATASTRUCT cdData;
                cdData = (PCOPYDATASTRUCT) lParam;
                mplSetFileName(NULL, cdData->lpData, STREAMTYPE_FILE);
                if(!parse_filename(cdData->lpData, playtree, mconfig, 1))
                    gui->playlist->add_track(gui->playlist, cdData->lpData, NULL, NULL, 0);
                gui->startplay(gui);
            }
            break;
        }
        case WM_DROPFILES:
        {
            if(!lParam)
            {
                char file[MAX_PATH];
                int filecount = DragQueryFile((HDROP) wParam, -1, file, MAX_PATH);
                int i;
                for(i=0; i<filecount; i++)
                {
                    DragQueryFile((HDROP) wParam, i, file, MAX_PATH);
                    mplSetFileName(NULL, file, STREAMTYPE_FILE);
                    if(!parse_filename(file, playtree, mconfig, 1))
                        gui->playlist->add_track(gui->playlist, file, NULL, NULL, 0);
                }
                DragFinish((HDROP) wParam);
                handlemsg(hWnd, evDropFile);
            }
            else
            {
                gui->playlist->clear_playlist(gui->playlist);
                gui->playlist->add_track(gui->playlist, (const char *) wParam, NULL, NULL, 0);
                handlemsg(hWnd, evDropFile);
            }
            SetForegroundWindow(gui->mainwindow);
            return 0;
        }
        case WM_LBUTTONDOWN:
        {
            SetCapture(hWnd);
            gui->mousex = GET_X_LPARAM(lParam);
            gui->mousey = GET_Y_LPARAM(lParam);
            /* inside a widget */
            gui->activewidget = clickedinsidewidget(gui, get_windowtype(hWnd), gui->mousex, gui->mousey);
            if(gui->activewidget)
            {
                gui->activewidget->pressed = 1;
                gui->mousewx = gui->mousex - gui->activewidget->x;
                gui->mousewy = gui->mousey - gui->activewidget->y;
                renderwidget(gui->skin, get_drawground(hWnd), gui->activewidget, 0);
                RedrawWindow(hWnd, NULL, NULL, RDW_INVALIDATE);
                handlemsg(hWnd, gui->activewidget->msg);
            }
            break;
        }
        case WM_CAPTURECHANGED:
        {
            if(gui->activewidget)
            {
                gui->activewidget->pressed = 0;
                renderwidget(gui->skin, get_drawground(hWnd), gui->activewidget, 1);
                RedrawWindow(hWnd, NULL, NULL, RDW_INVALIDATE);
                gui->activewidget = NULL;
            }
            break;
        }
        case WM_LBUTTONUP:
        {
            ReleaseCapture();
            if(gui->activewidget)
            {
                gui->activewidget->pressed = 0;
                renderwidget(gui->skin, get_drawground(hWnd), gui->activewidget, 1);
                RedrawWindow(hWnd, NULL, NULL, RDW_INVALIDATE);
                gui->activewidget = NULL;
            }
            break;
        }
        case WM_RBUTTONDOWN:
        {
            POINT point;
            char device[MAX_PATH];
            char searchpath[MAX_PATH];
            char searchpath2[MAX_PATH];
#ifdef CONFIG_LIBCDIO
            char searchpath3[MAX_PATH];
#endif
            int len, pos = 0, cdromdrive = 0;
            UINT errmode;
            point.x = GET_X_LPARAM(lParam);
            point.y = GET_Y_LPARAM(lParam);
            ClientToScreen(hWnd, &point);
            errmode = SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);
            while (GetMenuItemCount(gui->diskmenu) > 0)
                DeleteMenu(gui->diskmenu, 0, MF_BYPOSITION);
            len = GetLogicalDriveStrings(MAX_PATH, device);
            while(pos < len)
            {
                if(GetDriveType(device + pos) == DRIVE_CDROM)
                {
                    char volname[MAX_PATH];
                    char menuitem[MAX_PATH];
                    int flags = MF_STRING;
                    mp_msg(MSGT_GPLAYER, MSGL_V, "[GUI] checking %s for CD/VCD/SVCD/DVDs\n", device + pos);
                    sprintf(searchpath, "%sVIDEO_TS", device + pos);
                    sprintf(searchpath2, "%sMpegav", device + pos);
#ifdef CONFIG_LIBCDIO
                    sprintf(searchpath3, "%sTrack01.cda", device + pos);
#endif
                    if(GetFileAttributes(searchpath) != INVALID_FILE_ATTRIBUTES)
                        flags |= MF_ENABLED;
                    else if(GetFileAttributes(searchpath2) != INVALID_FILE_ATTRIBUTES)
                        flags |= MF_ENABLED;
#ifdef CONFIG_LIBCDIO
                    else if(GetFileAttributes(searchpath3) != INVALID_FILE_ATTRIBUTES)
                        flags |= MF_ENABLED;
#endif
                    else
                        flags |= MF_GRAYED;
                    volname[0] = 0;
                    strcpy(menuitem, device + pos);
                    menuitem[strlen(menuitem) - 1]=0;
                    GetVolumeInformation(device + pos, volname, MAX_PATH, NULL, NULL, NULL, NULL, 0);
                    if (strlen(volname))
                    {
                        capitalize(volname);
                        strcat(menuitem, " - ");
                        strcat(menuitem, volname);
                    }
                    AppendMenu(gui->diskmenu, flags, IDPLAYDISK + cdromdrive, menuitem);
                        cdromdrive++;
                }
                pos += strlen(device + pos) + 1;
            }
            SetErrorMode(errmode);
            TrackPopupMenu(gui->menu, 0, point.x, point.y, 0, hWnd, NULL);
            return 0;
        }
        case WM_MOUSEMOVE:
        {
            if(wParam & MK_LBUTTON)
            {
                POINT point;
                RECT rect;
                if(gui->activewidget)
                {
                    widget *item = gui->activewidget;

                    if(item->type == tyHpotmeter)
                    {
                        item->x = GET_X_LPARAM(lParam) - gui->mousewx;
                        item->value = (float)((float)((item->x - item->wx) * 100.0f) / (float)(item->wwidth - item->width));
                    }
                    if(item->type == tyPotmeter)
                    {
                        gui->mousewx = GET_X_LPARAM(lParam) - gui->activewidget->x;
                        item->value = (float) (gui->mousewx * 100.0f) / (float) item->wwidth;
                    }

                    if((item->type == tyPotmeter) || (item->type == tyHpotmeter) || (item->type == tyVpotmeter))
                    {
                        /* Bound checks */
                        if(item->value > 100.0f)
                            item->value = 100.0f;
                        else if(item->value < 0.0f)
                            item->value = 0.0f;

                        if(item->msg == evSetVolume)
                            guiIntfStruct.Volume = (float) item->value;
                        else if(item->msg == evSetMoviePosition)
                            guiIntfStruct.Position = (float) item->value;
                        else if(item->msg == evSetBalance)
                        {
                            /* make the range for 50% a bit bigger, because the sliders for balance usually suck */
                            if((item->value - 50.0f < 1.5f) && (item->value - 50.0f > -1.5f))
                                item->value = 50.0f;
                            guiIntfStruct.Balance = (float) item->value;
                        }
                        updatedisplay(gui, hWnd);
                        handlemsg(hWnd, item->msg);
                    }
                    break;
                }
                point.x = GET_X_LPARAM(lParam);
                point.y = GET_Y_LPARAM(lParam);
                ClientToScreen(hWnd, &point);
                GetWindowRect(hWnd, &rect);
                MoveWindow(hWnd, point.x - gui->mousex, point.y - gui->mousey,
                           rect.right-rect.left,rect.bottom-rect.top,TRUE);
                break;
            }
            break;
        }
        case WM_COMMAND:
        {
            switch(LOWORD(wParam))
            {
                case IDEXIT:
                    PostQuitMessage(0);
                    handlemsg(hWnd, evExit);
                    break;
                case IDFILE_OPEN:
                    handlemsg(hWnd, evLoadPlay);
                    break;
                case IDDIR_OPEN:
                {
                    static char path[MAX_PATH];
                    BROWSEINFO bi;
                    LPITEMIDLIST pidl;
                    memset(&bi, 0, sizeof(BROWSEINFO));
                    bi.lpszTitle = "Choose a Directory...";
                    pidl = SHBrowseForFolder(&bi);
                    if (SHGetPathFromIDList(pidl, path))
                    {
                        gui->playlist->clear_playlist(gui->playlist);
                        adddirtoplaylist(gui->playlist, path, TRUE);
                        gui->startplay(gui);
                    }
                    break;
                }
                case ID_SKINBROWSER:
                    handlemsg(hWnd, evSkinBrowser);
                    break;
                case IDURL_OPEN:
                    display_openurlwindow(gui, 0);
                    break;
                case ID_MUTE:
                    mp_input_queue_cmd(mp_input_parse_cmd("mute"));
                    break;
                case IDSUBTITLE_OPEN:
                    display_opensubtitlewindow(gui);
                    break;
                case ID_PTRACK:
                    handlemsg(hWnd, evPrev);
                    break;
                case ID_SEEKB:
                    handlemsg(hWnd, evBackward10sec);
                    break;
                case ID_PLAY:
                    handlemsg(hWnd, evPlaySwitchToPause);
                    break;
                case ID_STOP:
                    handlemsg(hWnd, evStop);
                    break;
                case ID_SEEKF:
                    handlemsg(hWnd, evForward10sec);
                    break;
                case ID_NTRACK:
                    handlemsg(hWnd, evNext);
                    break;
                case ID_SHOWHIDE:
                {
                    if(IsWindowVisible(hWnd)) ShowWindow(hWnd, SW_HIDE);
                    else ShowWindow(hWnd, SW_SHOW);
                    break;
                }
                case ID_PLAYLIST:
                    handlemsg(hWnd, evPlayList);
                    break;
                case ID_PREFS:
                    handlemsg(hWnd, evPreferences);
                    break;
                case ID_CONSOLE:
                    console_toggle();
                    break;
                case ID_ONLINEHELP:
                    ShellExecute(NULL, "open", ONLINE_HELP_URL, NULL, NULL, SW_SHOWNORMAL);
                    break;
            }
            if((IDPLAYDISK <= LOWORD(wParam)) && (LOWORD(wParam) < (IDPLAYDISK + 100)))
            {
                char device[MAX_PATH];
                char searchpath[MAX_PATH];
                char filename[MAX_PATH];
                int len, pos = 0, cdromdrive = 0;
                len = GetLogicalDriveStrings(MAX_PATH, device);
                while(pos < len)
                {
                    if(GetDriveType(device + pos)==DRIVE_CDROM)
                    {
                        if(LOWORD(wParam) - IDPLAYDISK == cdromdrive)
                        {
                            sprintf(searchpath, "%sVIDEO_TS", device + pos);
                            if(GetFileAttributes(searchpath) != INVALID_FILE_ATTRIBUTES)
                            {
#ifdef CONFIG_DVDREAD
                                if (dvd_device) free(dvd_device);
                                dvd_device = strdup(device + pos);
                                dvd_title = dvd_chapter = dvd_angle = 1;
                                handlemsg(hWnd, evPlayDVD);
#endif
                            }
                            sprintf(searchpath, "%sTrack01.cda", device + pos);
                            if(GetFileAttributes(searchpath) != INVALID_FILE_ATTRIBUTES)
                            {
#ifdef CONFIG_LIBCDIO
                                if (cdrom_device) free(cdrom_device);
                                cdrom_device = strdup(device + pos);
                                /* mplayer doesn't seem to like the trailing \ after the device name */
                                cdrom_device[2]=0;
                                handlemsg(hWnd, evPlayCD);
#endif
                            } else {
                                HANDLE searchhndl;
                                WIN32_FIND_DATA finddata;
                                sprintf(searchpath, "%smpegav\\*.dat", device + pos);
                                if((searchhndl=FindFirstFile(searchpath, &finddata)) != INVALID_HANDLE_VALUE)
                                {
                                    mp_msg(MSGT_GPLAYER,MSGL_V, "Opening VCD/SVCD\n");
                                    gui->playlist->clear_playlist(gui->playlist);
                                    do
                                    {
                                        sprintf(filename, "%smpegav\\%s", device + pos, finddata.cFileName);
                                        gui->playlist->add_track(gui->playlist, filename, NULL, NULL, 0);
                                    }
                                    while(FindNextFile(searchhndl, &finddata));
                                    FindClose(searchhndl);
                                }
                                gui->startplay(gui);
                            }
                        }
                        cdromdrive++;
                    }
                    pos += strlen(device + pos) + 1;
                }
            }
            break;
        }
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            RECT rd;
            HDC hdc = BeginPaint(hWnd, &ps);
            HDC hMemDC = CreateCompatibleDC(hdc);
            int width, height;
            GetClientRect(hWnd, &rd);
            width = rd.right - rd.left;
            height = rd.bottom - rd.top;
            SelectObject(hMemDC, get_bitmap(hWnd));
            BitBlt(hdc, 0, 0, width, height, hMemDC, 0, 0, SRCCOPY);
            DeleteDC(hMemDC);
            EndPaint(hWnd, &ps);
            return 0;
        }
        return 0;
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

static void startplay(gui_t *gui)
{
    handlemsg(gui->mainwindow, evDropFile);
}

/* returns the bits per pixel of the desktop */
/* the format is always in BGR byte order */
static int GetDesktopBitsPerPixel(void)
{
    HWND desktop=GetDesktopWindow();
    HDC dc=GetDC(desktop);
    int bpp=GetDeviceCaps(dc, BITSPIXEL);
    ReleaseDC(desktop, dc);
    return bpp;
}

/* unloads a skin and destroys its windows */
int destroy_window(gui_t *gui)
{
    RECT rd;
    unsigned int i;

    /* Save position: MSDN says don't pass workspace coordinates
     * to CreateWindow() or SetWindowPos(), as both of which expect
     * screen coordinates; resulting in the window appearing in the
     * wrong location.
     * -Erik
     */

    /* main window position */
    if(IsIconic(gui->mainwindow))
        ShowWindow(gui->mainwindow, SW_SHOWNORMAL);
    GetWindowRect(gui->mainwindow, &rd);
    gui_main_pos_x = rd.left;
    gui_main_pos_y = rd.top;

    /* sub window position */
    if(IsIconic(gui->subwindow))
        ShowWindow(gui->subwindow, SW_SHOWNORMAL);
    GetWindowRect(gui->subwindow, &rd);
    gui_sub_pos_x = rd.left;
    gui_sub_pos_y = rd.top;

    for(i=0; i<gui->window_priv_count; i++)
    {
        if(gui->window_priv[i]->bitmap)
            DeleteObject(gui->window_priv[i]->bitmap);
        free(gui->window_priv[i]);
    }
    free(gui->window_priv);
    gui->window_priv = NULL;
    gui->window_priv_count = 0;

    /* destroy the main window */
    if(gui->mainwindow)
        DestroyWindow(gui->mainwindow);
    gui->mainwindow = NULL;

    /* destroy the sub window */
    if(gui->subwindow)
        DestroyWindow(gui->subwindow);
    gui->subwindow = NULL;

    UnregisterClass(gui->classname, 0);
    DestroyIcon(gui->icon);

    gui->skin->freeskin(gui->skin);
    gui->skin = NULL;
    return 0;
}

static void create_menu(gui_t *gui)
{
    gui->diskmenu = CreatePopupMenu();
    gui->menu=CreatePopupMenu();
    gui->trayplaymenu = CreatePopupMenu();
    AppendMenu(gui->menu, MF_STRING | MF_POPUP, (UINT) gui->trayplaymenu, "Open...");
    AppendMenu(gui->trayplaymenu, MF_STRING, IDFILE_OPEN, "File...");
    AppendMenu(gui->trayplaymenu, MF_STRING, IDURL_OPEN, "Url...");
    AppendMenu(gui->trayplaymenu, MF_STRING, IDDIR_OPEN, "Directory...");
    AppendMenu(gui->menu, MF_SEPARATOR, 0, 0);
    AppendMenu(gui->menu, MF_STRING | MF_POPUP, (UINT) gui->diskmenu, "Play &CD/DVD/VCD/SVCD");
    AppendMenu(gui->menu, MF_SEPARATOR, 0, 0);
    AppendMenu(gui->menu, MF_STRING, IDSUBTITLE_OPEN, "Open Subtitle");
    AppendMenu(gui->menu, MF_STRING, ID_SKINBROWSER, "Skin Browser");
    AppendMenu(gui->menu, MF_SEPARATOR, 0, 0);
    AppendMenu(gui->menu, MF_STRING, ID_PREFS, "Preferences");
    AppendMenu(gui->menu, MF_STRING, ID_CONSOLE, "Debug Console");
    AppendMenu(gui->menu, MF_STRING, ID_ONLINEHELP, "Online Help");
    AppendMenu(gui->menu, MF_SEPARATOR, 0, 0);
    AppendMenu(gui->menu, MF_STRING, IDEXIT, "&Exit");
}

static void create_traymenu(gui_t *gui)
{
    gui->traymenu = CreatePopupMenu();
    gui->trayplaybackmenu = CreatePopupMenu();
    AppendMenu(gui->traymenu, MF_STRING | MF_POPUP, (UINT) gui->trayplaymenu, "Open...");
    AppendMenu(gui->traymenu, MF_SEPARATOR, 0, 0);
    AppendMenu(gui->traymenu, MF_STRING | MF_POPUP, (UINT) gui->trayplaybackmenu, "Playback");
    AppendMenu(gui->trayplaybackmenu, MF_STRING, ID_SEEKB, "Seek Backwards");
    AppendMenu(gui->trayplaybackmenu, MF_STRING, ID_PTRACK, "Previous Track");
    AppendMenu(gui->trayplaybackmenu, MF_STRING, ID_PLAY, "Play/Pause");
    AppendMenu(gui->trayplaybackmenu, MF_STRING, ID_STOP, "Stop");
    AppendMenu(gui->trayplaybackmenu, MF_STRING, ID_NTRACK, "Next Track");
    AppendMenu(gui->trayplaybackmenu, MF_STRING, ID_SEEKF, "Seek Forwards");
    AppendMenu(gui->traymenu, MF_SEPARATOR, 0, 0);
    AppendMenu(gui->traymenu, MF_STRING, ID_MUTE, "Toggle Mute");
    AppendMenu(gui->traymenu, MF_SEPARATOR, 0, 0);
    AppendMenu(gui->traymenu, MF_STRING, IDSUBTITLE_OPEN, "Open Subtitle");
    AppendMenu(gui->traymenu, MF_STRING, ID_PLAYLIST, "Playlist");
    AppendMenu(gui->traymenu, MF_SEPARATOR, 0, 0);
    AppendMenu(gui->traymenu, MF_STRING, ID_SHOWHIDE, "Show/Hide");
    AppendMenu(gui->traymenu, MF_SEPARATOR, 0, 0);
    AppendMenu(gui->traymenu, MF_STRING, ID_PREFS, "Preferences");
    AppendMenu(gui->traymenu, MF_STRING, ID_CONSOLE, "Debug Console");
    AppendMenu(gui->traymenu, MF_STRING, ID_ONLINEHELP, "Online Help");
    AppendMenu(gui->traymenu, MF_SEPARATOR, 0, 0);
    AppendMenu(gui->traymenu, MF_STRING, IDEXIT, "&Exit");
}

static void create_submenu(gui_t *gui)
{
    gui->submenu = CreatePopupMenu();
    gui->dvdmenu = CreatePopupMenu();
    gui->aspectmenu = CreatePopupMenu();
    gui->subtitlemenu = CreatePopupMenu();
    AppendMenu(gui->submenu, MF_STRING | MF_POPUP, (UINT) gui->trayplaymenu, "Open...");
    AppendMenu(gui->submenu, MF_SEPARATOR, 0, 0);
    AppendMenu(gui->submenu, MF_STRING, ID_SEEKB, "Seek Backwards");
    AppendMenu(gui->submenu, MF_STRING, ID_PTRACK, "Previous Track");
    AppendMenu(gui->submenu, MF_STRING, ID_PLAY, "Play/Pause");
    AppendMenu(gui->submenu, MF_STRING, ID_STOP, "Stop");
    AppendMenu(gui->submenu, MF_STRING, ID_NTRACK, "Next Track");
    AppendMenu(gui->submenu, MF_STRING, ID_SEEKF, "Seek Forwards");
    AppendMenu(gui->submenu, MF_SEPARATOR, 0, 0);
    AppendMenu(gui->submenu, MF_STRING, ID_FULLSCREEN, "Toggle Fullscreen");
    AppendMenu(gui->submenu, MF_STRING, ID_MUTE, "Toggle Mute");
    AppendMenu(gui->submenu, MF_SEPARATOR, 0, 0);
    AppendMenu(gui->submenu, MF_STRING | MF_POPUP, (UINT) gui->aspectmenu, "Aspect Ratio");
    AppendMenu(gui->submenu, MF_STRING | MF_POPUP, (UINT) gui->subtitlemenu, "Subtitle Options");
    AppendMenu(gui->submenu, MF_STRING | MF_POPUP, (UINT) gui->dvdmenu, "DVD Options");
#ifdef CONFIG_DVDREAD
    AppendMenu(gui->dvdmenu, MF_STRING | MF_GRAYED, ID_CHAPTERSEL, "Select Title/Chapter...");
#endif
    AppendMenu(gui->subtitlemenu, MF_STRING, IDSUB_TOGGLE, "Subtitle Visibility On/Off");
    AppendMenu(gui->subtitlemenu, MF_STRING, IDSUB_CYCLE, "Cycle Subtitle Languages");
    AppendMenu(gui->aspectmenu, MF_STRING, ID_ASPECT1, "Set 16:9");
    AppendMenu(gui->aspectmenu, MF_STRING, ID_ASPECT2, "Set 4:3");
    AppendMenu(gui->aspectmenu, MF_STRING, ID_ASPECT3, "Set 2.35");
    AppendMenu(gui->aspectmenu, MF_SEPARATOR, 0, 0);
    AppendMenu(gui->aspectmenu, MF_STRING, ID_ASPECT4, "Original Aspect");
    AppendMenu(gui->submenu, MF_SEPARATOR, 0, 0);
    AppendMenu(gui->submenu, MF_STRING, IDEXIT, "&Exit");
}

static void maketransparent(HWND hwnd, COLORREF crTransparent)
{
    HDC mdc = GetDC(hwnd);
    RECT rd;
    HRGN crRgnres, crRgn, crRgnTmp;
    int iX = 0, iY = 0, iLeftX = 0;
    int width, height;
    GetWindowRect(hwnd, &rd);
    width = rd.right - rd.left;
    height = rd.bottom - rd.top;

    /* create an empty region */
    crRgn = CreateRectRgn(0, 0, 0, 0);

    /* Create a region from a bitmap with transparency colour of Purple */
    for (iY = -1; iY < height; iY++)
    {
        do
        {
            /* skip over transparent pixels at start of lines */
            while (iX <= width && GetPixel(mdc,iX, iY) == crTransparent) iX++;

            /* remember this pixel */
            iLeftX = iX;

            /* now find first non transparent pixel */
            while (iX <= width && GetPixel(mdc,iX, iY) != crTransparent) ++iX;

            /* create a temp region on this info */
            crRgnTmp = CreateRectRgn(iLeftX, iY, iX, iY+1);

            /* combine into main region */
            crRgnres = crRgn;
            CombineRgn(crRgnres, crRgn, crRgnTmp, RGN_OR);
            crRgn = crRgnres;

            /* delete the temp region for next pass (otherwise you'll get an ASSERT) */
            DeleteObject(crRgnTmp);
        } while (iX < width);
        iX = 0;
    }
    SetWindowRgn(hwnd, crRgn, TRUE);
    DeleteObject(crRgn);
    ReleaseDC(hwnd,mdc);
}

static int window_render(gui_t *gui, HWND hWnd, HDC hdc, window_priv_t *priv, window *desc, BITMAPINFO binfo)
{
    int i;
    SetWindowLongPtr(hWnd, GWLP_USERDATA, (DWORD) gui);
    (gui->window_priv_count)++;
    gui->window_priv = realloc(gui->window_priv, sizeof(window_priv_t *) * gui->window_priv_count);
    priv = gui->window_priv[gui->window_priv_count - 1] = calloc(1, sizeof(window_priv_t));
    priv->hwnd = hWnd;
    priv->type = desc->type;
    priv->background = desc->base->bitmap[0];
    memcpy(&priv->img, desc->base->bitmap[0], sizeof(image));
    hdc = GetDC(hWnd);
    binfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    binfo.bmiHeader.biWidth = priv->img.width;
    binfo.bmiHeader.biHeight = -priv->img.height;
    binfo.bmiHeader.biPlanes = 1;
    binfo.bmiHeader.biSizeImage = priv->img.width * priv->img.height * (gui->screenbpp / 8);
    binfo.bmiHeader.biXPelsPerMeter = 0;
    binfo.bmiHeader.biYPelsPerMeter = 0;
    binfo.bmiHeader.biClrUsed = 0;
    binfo.bmiHeader.biClrImportant = 0;
    binfo.bmiHeader.biBitCount = gui->screenbpp;
    binfo.bmiHeader.biCompression = BI_RGB;
    priv->bitmap = CreateDIBSection(hdc, &binfo, DIB_RGB_COLORS, (void **) &priv->img.data, NULL, 0);
    if(!priv->bitmap)
    {
        mp_msg(MSGT_GPLAYER, MSGL_FATAL, "[GUI] unable to create bitmap for skinned window\n");
        return 0;
    }
    memcpy(priv->img.data, desc->base->bitmap[0]->data, binfo.bmiHeader.biSizeImage);
    ReleaseDC(hWnd,hdc);

    for (i=0; i<gui->skin->widgetcount; i++)
        if(gui->skin->widgets[i]->window == desc->type)
            renderwidget(gui->skin, &priv->img, gui->skin->widgets[i], 1);

    return 0;
}

/* creates the sub (AKA video) window,*/
int create_subwindow(gui_t *gui, char *skindir)
{
    HINSTANCE instance = GetModuleHandle(NULL);
    WNDCLASS wc;
    RECT rect;
    HWND hWnd;
    DWORD style = 0;
    HDC hdc = NULL;
    BITMAPINFO binfo;
    window_priv_t *priv = NULL;
    window *desc = NULL;
    int i, x = -1, y = -1;
    vo_colorkey = 0xff00ff;

    for (i=0; i<gui->skin->windowcount; i++)
        if(gui->skin->windows[i]->type == wiSub)
            desc = gui->skin->windows[i];

    if(!desc)
    {
        mp_msg(MSGT_GPLAYER, MSGL_FATAL, "[GUI] Invalid skin description\n");
        return 1;
    }

    windowcolor = vo_colorkey;
    colorbrush = CreateSolidBrush(windowcolor);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = SubProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = gui->icon;
    wc.hbrBackground = NULL; //WM_PAINT will handle background color switching;
    wc.lpszClassName = "MPlayer Sub for Windows";
    wc.lpszMenuName = NULL;
    RegisterClass(&wc);

    /* create the sub window menu */
    create_submenu(gui);

    rect.top = rect.left = 100;
    rect.bottom = rect.top+desc->base->bitmap[0]->height;
    rect.right = rect.left+desc->base->bitmap[0]->width;

    /* our window aspect */
    sub_aspect = (float)(rect.right-rect.left)/(rect.bottom-rect.top);

    style = fullscreen?WS_VISIBLE | WS_POPUP:WS_OVERLAPPEDWINDOW | WS_SYSMENU | WS_MINIMIZEBOX;
    AdjustWindowRect(&rect, style, 0);

    if (gui_sub_pos_x >= 0)
        x = gui_sub_pos_x;
    if (gui_sub_pos_y >= 0)
        y = gui_sub_pos_y;

    /* out of bounds check */
    if (x <= -1 || (x+(rect.right-rect.left) > GetSystemMetrics(SM_CXSCREEN)))
        x = CW_USEDEFAULT;
    if (y <= -1 || (y+(rect.bottom-rect.top) > GetSystemMetrics(SM_CYSCREEN)))
        y = x;

    hWnd = CreateWindowEx(0, "MPlayer Sub for Windows", "MPlayer for Windows", style,
                          x, y, rect.right-rect.left, rect.bottom-rect.top,
                          gui->subwindow, NULL, instance, NULL);

    /* load all the window images */
    window_render(gui, hWnd, hdc, priv, desc, binfo);

    /* enable drag and drop support */
    DragAcceptFiles(hWnd, TRUE);

    gui->subwindow = hWnd;
    if(sub_window)
        WinID = gui->subwindow;
    ShowWindow(gui->subwindow, SW_SHOW);
    UpdateWindow(gui->subwindow);
    return 0;
}

/* loads/updates a skin and creates windows for it */
int create_window(gui_t *gui, char *skindir)
{
    HINSTANCE instance = GetModuleHandle(NULL);
    WNDCLASS wc;
    RECT rect;
    DWORD style = 0;
    HWND hwnd;
    HDC hdc = NULL;
    BITMAPINFO binfo;
    window_priv_t *priv = NULL;
    window *desc = NULL;
    char dir[MAX_PATH];
    unsigned int i;
    int x = -1, y = -1;

    /* destroy the current main window */
    if(gui->skin) destroy_window(gui);

    /* get screenproperties */
    gui->screenbpp = GetDesktopBitsPerPixel();
    gui->screenw = GetSystemMetrics(SM_CXSCREEN);
    gui->screenh = GetSystemMetrics(SM_CYSCREEN);

    /* load the new skin */
    gui->skin = loadskin(skindir, gui->screenbpp);
    if(!gui->skin)
    {
        mp_msg(MSGT_GPLAYER, MSGL_FATAL, "[GUI] fatal error during skinload\n");
        /* Set default Skin */
        if (skinName) free(skinName);
        skinName = strdup("Blue");
        /* then force write conf */
        cfg_write();
        return 1;
    }

    /* find the description of the mainwindow */
    for (i=0; i<gui->skin->windowcount; i++)
        if(gui->skin->windows[i]->type == wiMain)
            desc = gui->skin->windows[i];

    if(!desc)
    {
        mp_msg(MSGT_GPLAYER, MSGL_FATAL, "[GUI] Invalid skin description\n");
        return 1;
    }

    /* load the icon from the executable */
    GetModuleFileName(NULL, dir, MAX_PATH);
    gui->icon = ExtractIcon(instance, dir, 0);

    /* create the window class */
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = EventProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = gui->icon;
    wc.hbrBackground = CreateSolidBrush(RGB(0, 0, 0));
    wc.lpszClassName = gui->classname = "MPlayer GUI for Windows";
    wc.lpszMenuName = NULL;
    RegisterClass(&wc);

    /* create a context menu */
    create_menu(gui);
    /* create the systray menu */
    create_traymenu(gui);

    /* create the mainwindow */
    /* TODO implement aligning as described in skin.html */
    rect.top = rect.left = 100;
    rect.bottom = rect.top+desc->base->bitmap[0]->height;
    rect.right = rect.left+desc->base->bitmap[0]->width;
    if(desc->decoration) style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    else style = WS_POPUP | WS_SYSMENU;

    AdjustWindowRect(&rect, style, 0);

    /* Check if out of screen */
    if (gui_main_pos_x >= 0)
        x = gui_main_pos_x;
    if (gui_main_pos_y >= 0)
        y = gui_main_pos_y;

    if (x <= -1 || (x+(rect.right-rect.left) > GetSystemMetrics(SM_CXFULLSCREEN)))
    {
        x = (GetSystemMetrics(SM_CXSCREEN) / 2) - ((rect.right-rect.left) / 2);
        gui_main_pos_x = x;
    }
    if (y <= -1 || (y+(rect.bottom-rect.top) > GetSystemMetrics(SM_CYFULLSCREEN)))
    {
        y = ((GetSystemMetrics(SM_CYSCREEN)-40) - (rect.bottom-rect.top));
        gui_main_pos_y = y;
    }

    hwnd = CreateWindowEx(0, gui->classname, "MPlayer for Windows", style,
                          x, y, rect.right-rect.left, rect.bottom-rect.top,
                          gui->mainwindow, NULL, instance, NULL);

    /* set the systray icon properties */
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid.uCallbackMessage = WM_SYSTRAY;
    nid.hIcon = gui->icon;
    strcpy(nid.szTip, "MPlayer for Windows");

    /* register the systray icon */
    Shell_NotifyIcon(NIM_ADD, &nid);

    /* load all the window images */
    window_render(gui, hwnd, hdc, priv, desc, binfo);

    /* enable drag and drop support */
    DragAcceptFiles(hwnd, TRUE);

    updatedisplay(gui, hwnd);
    gui->mainwindow = hwnd;

    /* display */
    ShowWindow(gui->mainwindow, SW_SHOW);
    UpdateWindow(gui->mainwindow);
    maketransparent(gui->mainwindow, RGB(255, 0, 255));
    return 0;
}

gui_t *create_gui(char *skindir, char *skinName, void (*playercontrol)(int event))
{
    gui_t *gui = calloc(1, sizeof(gui_t));
    char temp[MAX_PATH];
    HWND runningmplayer = FindWindow("MPlayer GUI for Windows", "MPlayer for Windows");

    if(runningmplayer)
    {
        free(gui);
        return NULL;
    }

    gui->startplay = startplay;
    gui->playercontrol = playercontrol;
    gui->uninit = uninit;
    gui->updatedisplay = updatedisplay;

    /* create playlist */
    gui->playlist = create_playlist();

    if(!skinName) skinName = strdup("Blue");
    sprintf(temp, "%s\\%s", skindir, skinName);
    if(create_window(gui, temp)) return NULL;
    if(create_subwindow(gui, temp)) return NULL;
    if(console) console_toggle();
    return gui;
}
