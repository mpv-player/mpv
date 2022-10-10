#include "stdafx.h" 
 

#include "ltmm.h" 
 
#include "resource.h" 
#include <tchar.h> 
#include <stdio.h> 
#include <math.h> 
 
#define SZ_WNDCLASS_PLAY _T("PLAY WNDCLASS") 
#define WM_PLAYNOTIFY (WM_USER + 1000)  
 
HINSTANCE g_hInstance;    // application instance handle 
HWND g_hwndPlay;      // video frame window 
IltmmPlay* g_pPlay;      // play object interface pointer 
int g_nPositionView;   // current position indicator view mode 
enum 
{ 
   POSITIONVIEW_TIME,  
   POSITIONVIEW_FRAME,  
   POSITIONVIEW_TRACKING 
}; 
 
// 
// SnapFrameToVideo 
// resizes the frame window to match the video width and height 
// 
void SnapFrameToVideo(void)  
{ 
   HWND hwnd;  
   RECT rcWindow, rcClient;  
   long cx, cy;  
 
   // get the frame window 
   g_pPlay->get_VideoWindowFrame ((long*) &hwnd);  
 
   // get the video dimensions 
   g_pPlay->get_VideoWidth (&cx);  
   g_pPlay->get_VideoHeight (&cy);  
 
   // adjust by the border dimensions 
   GetWindowRect(hwnd, &rcWindow);  
   GetClientRect(hwnd, &rcClient);  
   cx += ((rcWindow.right - rcWindow.left) - (rcClient.right - rcClient.left));  
   cy += ((rcWindow.bottom - rcWindow.top) - (rcClient.bottom - rcClient.top));  
 
   // resize the window 
   SetWindowPos(hwnd, NULL, 0, 0, cx, cy, SWP_NOMOVE | SWP_NOZORDER);  
} 
