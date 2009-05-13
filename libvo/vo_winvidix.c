/*
 * VIDIX-accelerated overlay in a Win32 window
 *
 * copyright (C) 2003 Sascha Sommer
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
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>

#include "config.h"
#include "video_out.h"
#include "video_out_internal.h"

#include <windows.h>
#include "osdep/keycodes.h"
#include "input/input.h"

#include "aspect.h"
#include "mp_msg.h"
#include "mp_fifo.h"

#include "vosub_vidix.h"
#include "vidix/vidix.h"


static const vo_info_t info =
{
    "WIN32 (VIDIX)",
    "winvidix",
    "Sascha Sommer",
    ""
};

LIBVO_EXTERN(winvidix)

#define UNUSED(x) ((void)(x)) /* Removes warning about unused arguments */

/* VIDIX related */
static char *vidix_name;

/* Image parameters */
static uint32_t image_width;
static uint32_t image_height;
static uint32_t image_format;
/* Window parameters */
static HWND hWnd=NULL,hWndFS=NULL;
static float window_aspect;

static vidix_grkey_t gr_key;


void set_video_eq(int cap);


static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message){
	    case WM_DESTROY:
			PostQuitMessage(0);
			return 0;
	    case WM_CLOSE:
			mplayer_put_key(KEY_CLOSE_WIN);
			break;
		case WM_WINDOWPOSCHANGED:
           {
                int tmpheight=0;
                /*calculate new window rect*/
                 if(!vo_fs){
                 RECT rd;
                 POINT point_window;
                 if(!hWnd)hWnd=hwnd;
                 ShowCursor(TRUE);
                 point_window.x = 0;
                 point_window.y = 0;
                 ClientToScreen(hWnd,&point_window);
                 GetClientRect(hWnd,&rd);

                 vo_dwidth=rd.right - rd.left;
                 vo_dheight=rd.bottom - rd.top;
                 vo_dx =point_window.x;
                 vo_dy =point_window.y;
          //       aspect(&vo_dwidth, &vo_dheight, A_NOZOOM);

                 /* keep aspect on resize, borrowed from vo_directx.c */
                 tmpheight = ((float)vo_dwidth/window_aspect);
                 tmpheight += tmpheight % 2;
                 if(tmpheight > vo_dheight)
                 {
                     vo_dwidth = ((float)vo_dheight*window_aspect);
                     vo_dwidth += vo_dwidth % 2;
                 }
                 else vo_dheight = tmpheight;
                 rd.right = rd.left + vo_dwidth;
                 rd.bottom = rd.top + vo_dheight;

                 if(rd.left < 0) rd.left = 0;
                 if(rd.right > vo_screenwidth) rd.right = vo_screenwidth;
                 if(rd.top < 0) rd.top = 0;
                 if(rd.bottom > vo_screenheight) rd.bottom = vo_screenheight;

                 AdjustWindowRect(&rd, WS_OVERLAPPEDWINDOW | WS_SIZEBOX, 0);
                 SetWindowPos(hWnd, HWND_TOPMOST, vo_dx+rd.left, vo_dy+rd.top, rd.right-rd.left, rd.bottom-rd.top, SWP_NOOWNERZORDER);
               }
               else {
                 if(ShowCursor(FALSE)>=0)while(ShowCursor(FALSE)>=0){}
                 aspect(&vo_dwidth, &vo_dheight, A_ZOOM);
                 vo_dx = (vo_screenwidth - vo_dwidth)/2;
                 vo_dy = (vo_screenheight - vo_dheight)/2;
               }
               /*update vidix*/
               /* FIXME: implement runtime resize/move if possible, this way is very ugly! */
	           vidix_stop();
	           if(vidix_init(image_width, image_height, vo_dx, vo_dy, vo_dwidth, vo_dheight, image_format, vo_depthonscreen, vo_screenwidth, vo_screenheight) != 0)
	               mp_msg(MSGT_VO, MSGL_FATAL, "Can't initialize VIDIX driver: %s\n", strerror(errno));
               /*set colorkey*/
               vidix_start();
               mp_msg(MSGT_VO, MSGL_V, "[winvidix] window properties: pos: %dx%d, size: %dx%d\n",vo_dx, vo_dy, vo_dwidth, vo_dheight);
               if(vidix_grkey_support()){
                 vidix_grkey_get(&gr_key);
	             gr_key.key_op = KEYS_PUT;
	             gr_key.ckey.op = CKEY_TRUE;
	             if(vo_fs)gr_key.ckey.red = gr_key.ckey.green = gr_key.ckey.blue = 0;
                 else {
                   gr_key.ckey.red = gr_key.ckey.blue = 255;
                   gr_key.ckey.green = 0;
	             }
                 vidix_grkey_set(&gr_key);
               }

           }
           break;
        case WM_SYSCOMMAND:
	        switch (wParam){
                case SC_SCREENSAVE:
		        case SC_MONITORPOWER:
                    return 0;
			 }
             break;
        case WM_KEYDOWN:
			switch (wParam){
				case VK_LEFT:
					{mplayer_put_key(KEY_LEFT);break;}
                case VK_UP:
					{mplayer_put_key(KEY_UP);break;}
                case VK_RIGHT:
					{mplayer_put_key(KEY_RIGHT);break;}
	            case VK_DOWN:
					{mplayer_put_key(KEY_DOWN);break;}
	            case VK_TAB:
					{mplayer_put_key(KEY_TAB);break;}
		        case VK_CONTROL:
					{mplayer_put_key(KEY_CTRL);break;}
		        case VK_DELETE:
					{mplayer_put_key(KEY_DELETE);break;}
		        case VK_INSERT:
					{mplayer_put_key(KEY_INSERT);break;}
		        case VK_HOME:
					{mplayer_put_key(KEY_HOME);break;}
		        case VK_END:
					{mplayer_put_key(KEY_END);break;}
		        case VK_PRIOR:
			        {mplayer_put_key(KEY_PAGE_UP);break;}
		        case VK_NEXT:
			        {mplayer_put_key(KEY_PAGE_DOWN);break;}
		        case VK_ESCAPE:
					{mplayer_put_key(KEY_ESC);break;}
			}
            break;
        case WM_CHAR:
			mplayer_put_key(wParam);
			break;
    }
	return DefWindowProc(hwnd, message, wParam, lParam);
}


static int config(uint32_t width, uint32_t height, uint32_t d_width,uint32_t d_height, uint32_t flags, char *title, uint32_t format){
    title = "MPlayer VIDIX WIN32 Overlay";

    panscan_init();

    image_height = height;
    image_width = width;
    image_format = format;
    vo_screenwidth = GetSystemMetrics(SM_CXSCREEN);
    vo_screenheight = GetSystemMetrics(SM_CYSCREEN);
    vo_depthonscreen = GetDeviceCaps(GetDC(GetDesktopWindow()),BITSPIXEL);


    aspect_save_orig(width, height);
    aspect_save_prescale(d_width, d_height);
    aspect_save_screenres(vo_screenwidth, vo_screenheight);

    vo_dx = 0;
    vo_dy = 0;

    vo_dx=( vo_screenwidth - d_width ) / 2; vo_dy=( vo_screenheight - d_height ) / 2;
    geometry(&vo_dx, &vo_dy, &d_width, &d_height, vo_screenwidth, vo_screenheight);

    vo_fs = flags&VOFLAG_FULLSCREEN;


    aspect(&d_width, &d_height, A_NOZOOM);
    vo_dwidth=d_width; vo_dheight=d_height;
    window_aspect = (float)d_width / (float)d_height;


    if(!vo_config_count){
    HINSTANCE hInstance = GetModuleHandle(NULL);
    WNDCLASS   wc;
    RECT rd;
    rd.left = vo_dx;
    rd.top = vo_dy;
    rd.right = rd.left + vo_dwidth;
    rd.bottom = rd.top + vo_dheight;
    AdjustWindowRect(&rd,WS_OVERLAPPEDWINDOW| WS_SIZEBOX,0);
    wc.style =  CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL,IDC_ARROW);
    wc.hIcon =ExtractIcon(hInstance,"mplayer.exe",0);
//LoadIcon(NULL,IDI_APPLICATION);
    wc.hbrBackground = CreateSolidBrush(RGB(255,0,255));
    wc.lpszClassName = "MPlayer - The Movie Player";
    wc.lpszMenuName = NULL;
    RegisterClass(&wc);
    hWnd = CreateWindow("MPlayer - The Movie Player",
                        title,
                        WS_OVERLAPPEDWINDOW| WS_SIZEBOX,
                        rd.left,
                        rd.top,
                        rd.right - rd.left,
                        rd.bottom - rd.top,
                        NULL,
                        NULL,
                        hInstance,
                        NULL);
    wc.hbrBackground = CreateSolidBrush(RGB(0,0,0));
    wc.lpszClassName = "MPlayer - Fullscreen";
    RegisterClass(&wc);
    hWndFS = CreateWindow("MPlayer - Fullscreen","MPlayer VIDIX Fullscreen",WS_POPUP,0,0,vo_screenwidth,vo_screenheight,hWnd,NULL,hInstance,NULL);





    }
    ShowWindow(hWnd,SW_SHOW);
    if(vo_fs)ShowWindow(hWndFS,SW_SHOW);

    return 0;
}

static void check_events(void){
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0,PM_REMOVE))
    {
		TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

/* draw_osd, flip_page, draw_slice, draw_frame should be
   overwritten with vidix functions (vosub_vidix.c) */
static void draw_osd(void){
    mp_msg(MSGT_VO, MSGL_FATAL, "[winvidix] error: didn't use vidix draw_osd!\n");
    return;
}

static void flip_page(void){
    mp_msg(MSGT_VO, MSGL_FATAL, "[winvidix] error: didn't use vidix flip_page!\n");
    return;
}

static int draw_slice(uint8_t *src[], int stride[],int w, int h, int x, int y){
    UNUSED(src);
    UNUSED(stride);
    UNUSED(w);
    UNUSED(h);
    UNUSED(x);
    UNUSED(y);
    mp_msg(MSGT_VO, MSGL_FATAL, "[winvidix] error: didn't use vidix draw_slice!\n");
    return -1;
}

static int draw_frame(uint8_t *src[]){
    UNUSED(src);
    mp_msg(MSGT_VO, MSGL_FATAL, "[winvidix] error: didn't use vidix draw_frame!\n");
    return -1;
}

static int query_format(uint32_t format){
  return vidix_query_fourcc(format);
}

static void uninit(void){
    DestroyWindow(hWndFS);
    DestroyWindow(hWnd);
    if ( !vo_config_count ) return;
    vidix_term();

    if (vidix_name){
	free(vidix_name);
	vidix_name = NULL;
    }
    //
}

static int preinit(const char *arg){
    if (arg)
        vidix_name = strdup(arg);
    else
    {
	mp_msg(MSGT_VO, MSGL_INFO, "No vidix driver name provided, probing available ones (-v option for details)!\n");
	vidix_name = NULL;
    }

    if (vidix_preinit(vidix_name, &video_out_winvidix) != 0)
	return 1;

    return 0;
}

static int control(uint32_t request, void *data, ...){
  switch (request) {
  case VOCTRL_FULLSCREEN:
    if(!vo_fs){vo_fs=1;ShowWindow(hWndFS,SW_SHOW);SetForegroundWindow(hWndFS);}
    else {vo_fs=0; ShowWindow(hWndFS,SW_HIDE);}
    break;
  case VOCTRL_QUERY_FORMAT:
    return query_format(*((uint32_t*)data));
  case VOCTRL_SET_EQUALIZER:
  {
    va_list ap;
    int value;

    va_start(ap, data);
    value = va_arg(ap, int);
    va_end(ap);

    return vidix_control(request, data, (int *)value);
  }
  case VOCTRL_GET_EQUALIZER:
  {
    va_list ap;
    int *value;

    va_start(ap, data);
    value = va_arg(ap, int*);
    va_end(ap);

    return vidix_control(request, data, value);
  }
  }
  return vidix_control(request, data);
//  return VO_NOTIMPL;
}
