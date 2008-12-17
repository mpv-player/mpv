/*
 * Directx v2 or later DirectDraw interface
 *
 * Copyright (c) 2002 - 2005 Sascha Sommer <saschasommer@freenet.de>
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

#include <windows.h>
#include <windowsx.h>
#include <ddraw.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "config.h"
#include "video_out.h"
#include "video_out_internal.h"
#include "fastmemcpy.h"
#include "input/input.h"
#include "osdep/keycodes.h"
#include "input/mouse.h"
#include "mp_msg.h"
#include "aspect.h"
#include "geometry.h"
#include "mp_fifo.h"
#include "sub.h"

#ifdef CONFIG_GUI
#include "gui/interface.h"
#endif

#ifndef WM_XBUTTONDOWN
# define WM_XBUTTONDOWN    0x020B
# define WM_XBUTTONUP      0x020C
# define WM_XBUTTONDBLCLK  0x020D
#endif

#define WNDCLASSNAME_WINDOWED	"MPlayer - The Movie Player"
#define WNDCLASSNAME_FULLSCREEN	"MPlayer - Fullscreen"
#define WNDSTYLE WS_OVERLAPPEDWINDOW|WS_SIZEBOX

static LPDIRECTDRAWCOLORCONTROL	g_cc = NULL;		//color control interface
static LPDIRECTDRAW7        g_lpdd = NULL;          //DirectDraw Object
static LPDIRECTDRAWSURFACE7  g_lpddsPrimary = NULL;  //Primary Surface: viewport through the Desktop
static LPDIRECTDRAWSURFACE7  g_lpddsOverlay = NULL;  //Overlay Surface
static LPDIRECTDRAWSURFACE7  g_lpddsBack = NULL;     //Back surface
static LPDIRECTDRAWCLIPPER  g_lpddclipper;          //clipper object, can only be used without overlay
static DDSURFACEDESC2		ddsdsf;                 //surface descripiton needed for locking
static HINSTANCE            hddraw_dll;             //handle to ddraw.dll
static RECT                 rd;                     //rect of our stretched image
static RECT                 rs;                     //rect of our source image
static HWND                 hWnd=NULL;              //handle to the window
static HWND                 hWndFS=NULL;           //fullscreen window
static HBRUSH               colorbrush = NULL;      // Handle to colorkey brush
static HBRUSH               blackbrush = NULL;      // Handle to black brush
static HICON                mplayericon = NULL;     // Handle to mplayer icon
static HCURSOR              mplayercursor = NULL;   // Handle to mplayer cursor
static uint32_t image_width, image_height;          //image width and height
static uint32_t d_image_width, d_image_height;      //image width and height zoomed 
static uint8_t  *image=NULL;                        //image data
static void* tmp_image = NULL;
static uint32_t image_format=0;                       //image format
static uint32_t primary_image_format;
static uint32_t vm_height=0;
static uint32_t vm_width=0;
static uint32_t vm_bpp=0;
static uint32_t dstride;                            //surface stride
static uint32_t nooverlay = 0;                      //NonOverlay mode
static DWORD    destcolorkey;                       //colorkey for our surface
static COLORREF windowcolor = RGB(0,0,16);          //windowcolor == colorkey
static int adapter_count=0;
static GUID selected_guid;
static GUID *selected_guid_ptr = NULL;
static RECT monitor_rect;	                        //monitor coordinates 
static float window_aspect;
static BOOL (WINAPI* myGetMonitorInfo)(HMONITOR, LPMONITORINFO) = NULL;
static RECT last_rect = {0xDEADC0DE, 0xDEADC0DE, 0xDEADC0DE, 0xDEADC0DE};

extern int vidmode;

/*****************************************************************************
 * DirectDraw GUIDs.
 * Defining them here allows us to get rid of the dxguid library during
 * the linking stage.
 *****************************************************************************/
const GUID IID_IDirectDraw7 =
{
	0x15e65ec0,0x3b9c,0x11d2,{0xb9,0x2f,0x00,0x60,0x97,0x97,0xea,0x5b}
};

const GUID IID_IDirectDrawColorControl =
{
	0x4b9f0ee0,0x0d7e,0x11d0,{0x9b,0x06,0x00,0xa0,0xc9,0x03,0xa3,0xb8}
}; 


typedef struct directx_fourcc_caps
{
   char*  img_format_name;      //human readable name
   uint32_t img_format;         //as MPlayer image format
   uint32_t drv_caps;           //what hw supports with this format
   DDPIXELFORMAT g_ddpfOverlay; //as Directx Sourface description
} directx_fourcc_caps;


static directx_fourcc_caps g_ddpf[] =
{                                                                         
	{"YV12 ",IMGFMT_YV12 ,0,{sizeof(DDPIXELFORMAT), DDPF_FOURCC,MAKEFOURCC('Y','V','1','2'),0,0,0,0,0}},
	{"I420 ",IMGFMT_I420 ,0,{sizeof(DDPIXELFORMAT), DDPF_FOURCC,MAKEFOURCC('I','4','2','0'),0,0,0,0,0}},   //yv12 with swapped uv 
	{"IYUV ",IMGFMT_IYUV ,0,{sizeof(DDPIXELFORMAT), DDPF_FOURCC,MAKEFOURCC('I','Y','U','V'),0,0,0,0,0}},   //same as i420
	{"YVU9 ",IMGFMT_YVU9 ,0,{sizeof(DDPIXELFORMAT), DDPF_FOURCC,MAKEFOURCC('Y','V','U','9'),0,0,0,0,0}},	
	{"YUY2 ",IMGFMT_YUY2 ,0,{sizeof(DDPIXELFORMAT), DDPF_FOURCC,MAKEFOURCC('Y','U','Y','2'),0,0,0,0,0}},
	{"UYVY ",IMGFMT_UYVY ,0,{sizeof(DDPIXELFORMAT), DDPF_FOURCC,MAKEFOURCC('U','Y','V','Y'),0,0,0,0,0}},
 	{"BGR8 ",IMGFMT_BGR8 ,0,{sizeof(DDPIXELFORMAT), DDPF_RGB, 0, 8,  0x00000000, 0x00000000, 0x00000000, 0}},   
	{"RGB15",IMGFMT_RGB15,0,{sizeof(DDPIXELFORMAT), DDPF_RGB, 0, 16,  0x0000001F, 0x000003E0, 0x00007C00, 0}},   //RGB 5:5:5
	{"BGR15",IMGFMT_BGR15,0,{sizeof(DDPIXELFORMAT), DDPF_RGB, 0, 16,  0x00007C00, 0x000003E0, 0x0000001F, 0}},   
	{"RGB16",IMGFMT_RGB16,0,{sizeof(DDPIXELFORMAT), DDPF_RGB, 0, 16,  0x0000001F, 0x000007E0, 0x0000F800, 0}},   //RGB 5:6:5
    {"BGR16",IMGFMT_BGR16,0,{sizeof(DDPIXELFORMAT), DDPF_RGB, 0, 16,  0x0000F800, 0x000007E0, 0x0000001F, 0}},   
	{"RGB24",IMGFMT_RGB24,0,{sizeof(DDPIXELFORMAT), DDPF_RGB, 0, 24,  0x000000FF, 0x0000FF00, 0x00FF0000, 0}},   
    {"BGR24",IMGFMT_BGR24,0,{sizeof(DDPIXELFORMAT), DDPF_RGB, 0, 24,  0x00FF0000, 0x0000FF00, 0x000000FF, 0}},  
    {"RGB32",IMGFMT_RGB32,0,{sizeof(DDPIXELFORMAT), DDPF_RGB, 0, 32,  0x000000FF, 0x0000FF00, 0x00FF0000, 0}},  
    {"BGR32",IMGFMT_BGR32,0,{sizeof(DDPIXELFORMAT), DDPF_RGB, 0, 32,  0x00FF0000, 0x0000FF00, 0x000000FF, 0}}   
};
#define NUM_FORMATS (sizeof(g_ddpf) / sizeof(g_ddpf[0]))

static const vo_info_t info =
{
	"Directx DDraw YUV/RGB/BGR renderer",
	"directx",
	"Sascha Sommer <saschasommer@freenet.de>",
	""
};

const LIBVO_EXTERN(directx)

static void draw_alpha(int x0, int y0, int w, int h, unsigned char *src,
		unsigned char *srca, int stride)
{
    switch(image_format) {
    case IMGFMT_YV12 :
    case IMGFMT_I420 :
	case IMGFMT_IYUV :
	case IMGFMT_YVU9 :
    	vo_draw_alpha_yv12(w,h,src,srca,stride,((uint8_t *) image) + dstride*y0 + x0,dstride);
	break;
	case IMGFMT_YUY2 :
	    vo_draw_alpha_yuy2(w,h,src,srca,stride,((uint8_t *) image)+ dstride*y0 + 2*x0 ,dstride);
    break;
    case IMGFMT_UYVY :
        vo_draw_alpha_yuy2(w,h,src,srca,stride,((uint8_t *) image) + dstride*y0 + 2*x0 + 1,dstride);
    break;
	case IMGFMT_RGB15:	
    case IMGFMT_BGR15:
		vo_draw_alpha_rgb15(w,h,src,srca,stride,((uint8_t *) image)+dstride*y0+2*x0,dstride);
    break;
    case IMGFMT_RGB16:
	case IMGFMT_BGR16:
        vo_draw_alpha_rgb16(w,h,src,srca,stride,((uint8_t *) image)+dstride*y0+2*x0,dstride);
    break;
    case IMGFMT_RGB24:
	case IMGFMT_BGR24:
        vo_draw_alpha_rgb24(w,h,src,srca,stride,((uint8_t *) image)+dstride*y0+4*x0,dstride);
    break;
    case IMGFMT_RGB32:
	case IMGFMT_BGR32:
        vo_draw_alpha_rgb32(w,h,src,srca,stride,((uint8_t *) image)+dstride*y0+4*x0,dstride);
    break;
    }
}

static void draw_osd(void)
{
    vo_draw_text(image_width,image_height,draw_alpha);
}

static int
query_format(uint32_t format)
{
    uint32_t i=0;
    while ( i < NUM_FORMATS )
    {
		if (g_ddpf[i].img_format == format)
		    return g_ddpf[i].drv_caps;
	    i++;
    }
    return 0;
}

static uint32_t Directx_CreatePrimarySurface()
{
    DDSURFACEDESC2   ddsd;
    //cleanup
	if(g_lpddsPrimary)g_lpddsPrimary->lpVtbl->Release(g_lpddsPrimary);
	g_lpddsPrimary=NULL;
	
    if(vidmode)g_lpdd->lpVtbl->SetDisplayMode(g_lpdd,vm_width,vm_height,vm_bpp,vo_refresh_rate,0);
    ZeroMemory(&ddsd, sizeof(ddsd));
    ddsd.dwSize = sizeof(ddsd);
    //set flags and create a primary surface.
    ddsd.dwFlags = DDSD_CAPS;
    ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
    if(g_lpdd->lpVtbl->CreateSurface(g_lpdd,&ddsd, &g_lpddsPrimary, NULL )== DD_OK)
		mp_msg(MSGT_VO, MSGL_DBG3,"<vo_directx><INFO>primary surface created\n");
	else
	{
		mp_msg(MSGT_VO, MSGL_FATAL,"<vo_directx><FATAL ERROR>could not create primary surface\n");
		return 1;
	}
	return 0;
}

static uint32_t Directx_CreateOverlay(uint32_t imgfmt)
{
    HRESULT ddrval;
    DDSURFACEDESC2   ddsdOverlay;
    uint32_t        i=0;
	while ( i < NUM_FORMATS +1 && imgfmt != g_ddpf[i].img_format)
	{
		i++;
	}
	if (!g_lpdd || !g_lpddsPrimary)
        return 1;
    //cleanup
	if (g_lpddsOverlay)g_lpddsOverlay->lpVtbl->Release(g_lpddsOverlay);
	if (g_lpddsBack)g_lpddsBack->lpVtbl->Release(g_lpddsBack);
	g_lpddsOverlay= NULL;
	g_lpddsBack = NULL;
	//create our overlay
    ZeroMemory(&ddsdOverlay, sizeof(ddsdOverlay));
    ddsdOverlay.dwSize = sizeof(ddsdOverlay);
    ddsdOverlay.ddsCaps.dwCaps=DDSCAPS_OVERLAY | DDSCAPS_FLIP | DDSCAPS_COMPLEX | DDSCAPS_VIDEOMEMORY;
    ddsdOverlay.dwFlags= DDSD_CAPS|DDSD_HEIGHT|DDSD_WIDTH|DDSD_BACKBUFFERCOUNT| DDSD_PIXELFORMAT;
    ddsdOverlay.dwWidth=image_width;
    ddsdOverlay.dwHeight=image_height;
    ddsdOverlay.dwBackBufferCount=2;
	ddsdOverlay.ddpfPixelFormat=g_ddpf[i].g_ddpfOverlay; 
	if(vo_doublebuffering)   //tribblebuffering
	{
		if (g_lpdd->lpVtbl->CreateSurface(g_lpdd,&ddsdOverlay, &g_lpddsOverlay, NULL)== DD_OK)
		{
			mp_msg(MSGT_VO, MSGL_V,"<vo_directx><INFO>overlay with format %s created\n",g_ddpf[i].img_format_name);
            //get the surface directly attached to the primary (the back buffer)
            ddsdOverlay.ddsCaps.dwCaps = DDSCAPS_BACKBUFFER; 
            if(g_lpddsOverlay->lpVtbl->GetAttachedSurface(g_lpddsOverlay,&ddsdOverlay.ddsCaps, &g_lpddsBack) != DD_OK)
			{
				mp_msg(MSGT_VO, MSGL_FATAL,"<vo_directx><FATAL ERROR>can't get attached surface\n");
				return 1;
			}
			return 0;
		}
		vo_doublebuffering=0; //disable tribblebuffering
		mp_msg(MSGT_VO, MSGL_V,"<vo_directx><WARN>cannot create tribblebuffer overlay with format %s\n",g_ddpf[i].img_format_name);
	} 
	//single buffer
	mp_msg(MSGT_VO, MSGL_V,"<vo_directx><INFO>using singlebuffer overlay\n");
    ddsdOverlay.dwBackBufferCount=0;
    ddsdOverlay.ddsCaps.dwCaps=DDSCAPS_OVERLAY | DDSCAPS_VIDEOMEMORY;
    ddsdOverlay.dwFlags= DDSD_CAPS|DDSD_HEIGHT|DDSD_WIDTH|DDSD_PIXELFORMAT;
    ddsdOverlay.ddpfPixelFormat=g_ddpf[i].g_ddpfOverlay;
	// try to create the overlay surface
	ddrval = g_lpdd->lpVtbl->CreateSurface(g_lpdd,&ddsdOverlay, &g_lpddsOverlay, NULL);
	if(ddrval != DD_OK)
	{
	   if(ddrval == DDERR_INVALIDPIXELFORMAT)mp_msg(MSGT_VO,MSGL_V,"<vo_directx><ERROR> invalid pixelformat: %s\n",g_ddpf[i].img_format_name);
       else mp_msg(MSGT_VO, MSGL_ERR,"<vo_directx><ERROR>");
       switch(ddrval)
	   {
	      case DDERR_INCOMPATIBLEPRIMARY:
			 {mp_msg(MSGT_VO, MSGL_ERR,"incompatible primary surface\n");break;}
		  case DDERR_INVALIDCAPS:
			 {mp_msg(MSGT_VO, MSGL_ERR,"invalid caps\n");break;}
	      case DDERR_INVALIDOBJECT:
		     {mp_msg(MSGT_VO, MSGL_ERR,"invalid object\n");break;}
	      case DDERR_INVALIDPARAMS:
		     {mp_msg(MSGT_VO, MSGL_ERR,"invalid parameters\n");break;}
	      case DDERR_NODIRECTDRAWHW:
		     {mp_msg(MSGT_VO, MSGL_ERR,"no directdraw hardware\n");break;}
	      case DDERR_NOEMULATION:
		     {mp_msg(MSGT_VO, MSGL_ERR,"can't emulate\n");break;}
	      case DDERR_NOFLIPHW:
		     {mp_msg(MSGT_VO, MSGL_ERR,"hardware can't do flip\n");break;}
	      case DDERR_NOOVERLAYHW:
		     {mp_msg(MSGT_VO, MSGL_ERR,"hardware can't do overlay\n");break;}
	      case DDERR_OUTOFMEMORY:
		     {mp_msg(MSGT_VO, MSGL_ERR,"not enough system memory\n");break;}
	      case DDERR_UNSUPPORTEDMODE:
			 {mp_msg(MSGT_VO, MSGL_ERR,"unsupported mode\n");break;}  
		  case DDERR_OUTOFVIDEOMEMORY:
			 {mp_msg(MSGT_VO, MSGL_ERR,"not enough video memory\n");break;}
          default:
             mp_msg(MSGT_VO, MSGL_ERR,"create surface failed with 0x%x\n",ddrval);       
	   }
	   return 1;
	}
    g_lpddsBack = g_lpddsOverlay;
	return 0;
}

static uint32_t Directx_CreateBackpuffer()
{
    DDSURFACEDESC2   ddsd;
	//cleanup
	if (g_lpddsBack)g_lpddsBack->lpVtbl->Release(g_lpddsBack); 
	g_lpddsBack=NULL;
	ZeroMemory(&ddsd, sizeof(ddsd));
    ddsd.dwSize = sizeof(ddsd);
    ddsd.ddsCaps.dwCaps= DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
    ddsd.dwFlags= DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
    ddsd.dwWidth=image_width;
    ddsd.dwHeight=image_height;
    if(g_lpdd->lpVtbl->CreateSurface( g_lpdd, &ddsd, &g_lpddsBack, 0 ) != DD_OK )
	{
		mp_msg(MSGT_VO, MSGL_FATAL,"<vo_directx><FATAL ERROR>can't create backpuffer\n");
		return 1;
	}
    mp_msg(MSGT_VO, MSGL_DBG3,"<vo_directx><INFO>backbuffer created\n");
	return 0;
}

static void uninit(void)
{
	if (g_cc != NULL)
	{
		g_cc->lpVtbl->Release(g_cc);
	}
	g_cc=NULL;
	if (g_lpddclipper != NULL) g_lpddclipper->lpVtbl->Release(g_lpddclipper);
	g_lpddclipper = NULL;
	mp_msg(MSGT_VO, MSGL_DBG3,"<vo_directx><INFO>clipper released\n");
	if (g_lpddsBack != NULL) g_lpddsBack->lpVtbl->Release(g_lpddsBack);
	g_lpddsBack = NULL;
	mp_msg(MSGT_VO, MSGL_DBG3,"<vo_directx><INFO>back surface released\n");
	if(vo_doublebuffering && !nooverlay)
	{
		if (g_lpddsOverlay != NULL)g_lpddsOverlay->lpVtbl->Release(g_lpddsOverlay);
        g_lpddsOverlay = NULL;
		mp_msg(MSGT_VO, MSGL_DBG3,"<vo_directx><INFO>overlay surface released\n");
	}
	if (g_lpddsPrimary != NULL) g_lpddsPrimary->lpVtbl->Release(g_lpddsPrimary);
    g_lpddsPrimary = NULL;
	mp_msg(MSGT_VO, MSGL_DBG3,"<vo_directx><INFO>primary released\n");
	if(hWndFS)DestroyWindow(hWndFS);
	hWndFS = NULL;
	if((WinID == -1) && hWnd) DestroyWindow(hWnd);
	hWnd = NULL;
	mp_msg(MSGT_VO, MSGL_DBG3,"<vo_directx><INFO>window destroyed\n");
	UnregisterClass(WNDCLASSNAME_WINDOWED, GetModuleHandle(NULL));
	UnregisterClass(WNDCLASSNAME_FULLSCREEN, GetModuleHandle(NULL));
	if (mplayericon) DestroyIcon(mplayericon);
	mplayericon = NULL;
	if (mplayercursor) DestroyCursor(mplayercursor);
	mplayercursor = NULL;
	if (blackbrush) DeleteObject(blackbrush);
	blackbrush = NULL;
	if (colorbrush) DeleteObject(colorbrush);
	colorbrush = NULL;
	mp_msg(MSGT_VO, MSGL_DBG3,"<vo_directx><INFO>GDI resources deleted\n");
	if (g_lpdd != NULL){
	    if(vidmode)g_lpdd->lpVtbl->RestoreDisplayMode(g_lpdd);
	    g_lpdd->lpVtbl->Release(g_lpdd);
	}  
	mp_msg(MSGT_VO, MSGL_DBG3,"<vo_directx><INFO>directdrawobject released\n");
	FreeLibrary( hddraw_dll);
	hddraw_dll= NULL;
	mp_msg(MSGT_VO, MSGL_DBG3,"<vo_directx><INFO>ddraw.dll freed\n");
	mp_msg(MSGT_VO, MSGL_DBG3,"<vo_directx><INFO>uninitialized\n");    
}

static BOOL WINAPI EnumCallbackEx(GUID FAR *lpGUID, LPSTR lpDriverDescription, LPSTR lpDriverName, LPVOID lpContext, HMONITOR  hm)
{   
    mp_msg(MSGT_VO, MSGL_INFO ,"<vo_directx> adapter %d: ", adapter_count);

    if (!lpGUID)
    {
        mp_msg(MSGT_VO, MSGL_INFO ,"%s", "Primary Display Adapter");
    }
    else
    {
        mp_msg(MSGT_VO, MSGL_INFO ,"%s", lpDriverDescription);
    }
    
    if(adapter_count == vo_adapter_num){
        MONITORINFO mi;
        if (!lpGUID)
            selected_guid_ptr = NULL;
        else
        {
            selected_guid = *lpGUID;
            selected_guid_ptr = &selected_guid;
        }
        mi.cbSize = sizeof(mi);

        if (myGetMonitorInfo(hm, &mi)) {
			monitor_rect = mi.rcMonitor;
        }
        mp_msg(MSGT_VO, MSGL_INFO ,"\t\t<--");
    }
    mp_msg(MSGT_VO, MSGL_INFO ,"\n");
    
    adapter_count++;
    
    return 1; // list all adapters
}

static uint32_t Directx_InitDirectDraw()
{
	HRESULT    (WINAPI *OurDirectDrawCreateEx)(GUID *,LPVOID *, REFIID,IUnknown FAR *);
	DDSURFACEDESC2 ddsd;
	LPDIRECTDRAWENUMERATEEX OurDirectDrawEnumerateEx;
	HINSTANCE user32dll=LoadLibrary("user32.dll");
	
	adapter_count = 0;
	if(user32dll){
		myGetMonitorInfo=GetProcAddress(user32dll,"GetMonitorInfoA");
		if(!myGetMonitorInfo && vo_adapter_num){
			mp_msg(MSGT_VO, MSGL_ERR, "<vo_directx> -adapter is not supported on Win95\n");
			vo_adapter_num = 0;
		}
	}
	
	mp_msg(MSGT_VO, MSGL_DBG3,"<vo_directx><INFO>Initing DirectDraw\n" );

	//load direct draw DLL: based on videolans code
	hddraw_dll = LoadLibrary("DDRAW.DLL");
	if( hddraw_dll == NULL )
    {
        mp_msg(MSGT_VO, MSGL_FATAL,"<vo_directx><FATAL ERROR>failed loading ddraw.dll\n" );
		return 1;
    }
	
    last_rect.left = 0xDEADC0DE;   // reset window position cache

	if(vo_adapter_num){ //display other than default
        OurDirectDrawEnumerateEx = (LPDIRECTDRAWENUMERATEEX) GetProcAddress(hddraw_dll,"DirectDrawEnumerateExA");
        if (!OurDirectDrawEnumerateEx){
            FreeLibrary( hddraw_dll );
            hddraw_dll = NULL;
            mp_msg(MSGT_VO, MSGL_FATAL,"<vo_directx><FATAL ERROR>failed geting proc address: DirectDrawEnumerateEx\n");
            mp_msg(MSGT_VO, MSGL_FATAL,"<vo_directx><FATAL ERROR>no directx 7 or higher installed\n");
            return 1;
        }

        // enumerate all display devices attached to the desktop
        OurDirectDrawEnumerateEx(EnumCallbackEx, NULL, DDENUM_ATTACHEDSECONDARYDEVICES );

        if(vo_adapter_num >= adapter_count)
            mp_msg(MSGT_VO, MSGL_ERR,"Selected adapter (%d) doesn't exist: Default Display Adapter selected\n",vo_adapter_num);
    }
    FreeLibrary(user32dll);

	OurDirectDrawCreateEx = (void *)GetProcAddress(hddraw_dll, "DirectDrawCreateEx");
    if ( OurDirectDrawCreateEx == NULL )
     {
         FreeLibrary( hddraw_dll );
         hddraw_dll = NULL;
        mp_msg(MSGT_VO, MSGL_FATAL,"<vo_directx><FATAL ERROR>failed geting proc address: DirectDrawCreateEx\n");
 		return 1;
     }

	// initialize DirectDraw and create directx v7 object
    if (OurDirectDrawCreateEx(selected_guid_ptr, (VOID**)&g_lpdd, &IID_IDirectDraw7, NULL ) != DD_OK )
    {
        FreeLibrary( hddraw_dll );
        hddraw_dll = NULL;
        mp_msg(MSGT_VO, MSGL_FATAL,"<vo_directx><FATAL ERROR>can't initialize ddraw\n");
		return 1;
    }

	//get current screen siz for selected monitor ...
	ddsd.dwSize=sizeof(ddsd);
	ddsd.dwFlags=DDSD_WIDTH|DDSD_HEIGHT|DDSD_PIXELFORMAT;
	g_lpdd->lpVtbl->GetDisplayMode(g_lpdd, &ddsd);		
	if(vo_screenwidth && vo_screenheight)
	{
	    vm_height=vo_screenheight;
	    vm_width=vo_screenwidth;
	}
    else 
    {
	    vm_height=ddsd.dwHeight;
	    vm_width=ddsd.dwWidth;
	}


	if(vo_dbpp)vm_bpp=vo_dbpp;
	else vm_bpp=ddsd.ddpfPixelFormat.dwRGBBitCount;

	if(vidmode){
		if (g_lpdd->lpVtbl->SetCooperativeLevel(g_lpdd, hWnd, DDSCL_EXCLUSIVE|DDSCL_FULLSCREEN) != DD_OK)
		{
	        mp_msg(MSGT_VO, MSGL_FATAL,"<vo_directx><FATAL ERROR>can't set cooperativelevel for exclusive mode\n");
            return 1;
		}                                
		/*SetDisplayMode(ddobject,width,height,bpp,refreshrate,aditionalflags)*/
		if(g_lpdd->lpVtbl->SetDisplayMode(g_lpdd,vm_width, vm_height, vm_bpp,0,0) != DD_OK)
		{
	        mp_msg(MSGT_VO, MSGL_FATAL,"<vo_directx><FATAL ERROR>can't set displaymode\n");
	        return 1;
		}
	    mp_msg(MSGT_VO, MSGL_V,"<vo_directx><INFO>Initialized adapter %i for %i x %i @ %i \n",vo_adapter_num,vm_width,vm_height,vm_bpp);	
	    return 0;	
	}
	if (g_lpdd->lpVtbl->SetCooperativeLevel(g_lpdd, hWnd, DDSCL_NORMAL) != DD_OK) // or DDSCL_SETFOCUSWINDOW
     {
        mp_msg(MSGT_VO, MSGL_FATAL,"<vo_directx><FATAL ERROR>could not set cooperativelevel for hardwarecheck\n");
		return 1;
    }
    mp_msg(MSGT_VO, MSGL_DBG3,"<vo_directx><INFO>DirectDraw Initialized\n");
	return 0;
}	

static void check_events(void)
{
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0,PM_REMOVE))
    {
		TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

static uint32_t Directx_ManageDisplay()
{   
    HRESULT         ddrval;
    DDCAPS          capsDrv;
    DDOVERLAYFX     ovfx;
    DWORD           dwUpdateFlags=0;
    int width,height;
   
    if(!vidmode && !vo_fs && WinID!=-1) {
      RECT current_rect = {0, 0, 0, 0};
      GetWindowRect(hWnd, &current_rect);
      if ((current_rect.left   == last_rect.left)
      &&  (current_rect.top    == last_rect.top)
      &&  (current_rect.right  == last_rect.right)
      &&  (current_rect.bottom == last_rect.bottom))
        return 0;
      last_rect = current_rect;
    }

    if(vo_fs || vidmode){
      aspect(&width,&height,A_ZOOM);
      rd.left=(vo_screenwidth-width)/2;
      rd.top=(vo_screenheight-height)/2;
      if (WinID == -1)
        if(ShowCursor(FALSE)>=0)while(ShowCursor(FALSE)>=0){}
    }
    else if (WinID != -1 && vo_geometry) {
      POINT pt;
      pt.x = vo_dx;
      pt.y = vo_dy;
      ClientToScreen(hWnd,&pt);  
      width=d_image_width;
      height=d_image_height;
      rd.left = pt.x;
      rd.top = pt.y;
      while(ShowCursor(TRUE)<=0){}
    }
    else {
      POINT pt;
      pt.x = 0;  //overlayposition relative to the window
      pt.y = 0;
      ClientToScreen(hWnd,&pt);  
      GetClientRect(hWnd, &rd);
	  width=rd.right - rd.left;
	  height=rd.bottom - rd.top;
      pt.x -= monitor_rect.left;    /* move coordinates from global to local monitor space */
      pt.y -= monitor_rect.top;
      rd.right -= monitor_rect.left;
      rd.bottom -= monitor_rect.top;
	  rd.left = pt.x;
      rd.top = pt.y; 
      if(!nooverlay && (!width || !height)){
	    /*window is minimized*/
	    ddrval = g_lpddsOverlay->lpVtbl->UpdateOverlay(g_lpddsOverlay,NULL, g_lpddsPrimary, NULL, DDOVER_HIDE, NULL);
	    return 0;
	  }
      if(vo_keepaspect){
          int tmpheight=((float)width/window_aspect);
          tmpheight+=tmpheight%2;       
          if(tmpheight > height){
            width=((float)height*window_aspect);
            width+=width%2;       
          }
          else height=tmpheight;
      }    
      if (WinID == -1)
          while(ShowCursor(TRUE)<=0){}
    }
    rd.right=rd.left+width;
    rd.bottom=rd.top+height;

	/*ok, let's workaround some overlay limitations*/
	if(!nooverlay)
	{
		uint32_t        uStretchFactor1000;  //minimum stretch 
        uint32_t        xstretch1000,ystretch1000; 
		/*get driver capabilities*/
        ZeroMemory(&capsDrv, sizeof(capsDrv));
        capsDrv.dwSize = sizeof(capsDrv);
        if(g_lpdd->lpVtbl->GetCaps(g_lpdd,&capsDrv, NULL) != DD_OK)return 1;
		/*get minimum stretch, depends on display adaptor and mode (refresh rate!) */
        uStretchFactor1000 = capsDrv.dwMinOverlayStretch>1000 ? capsDrv.dwMinOverlayStretch : 1000;
        rd.right = ((width+rd.left)*uStretchFactor1000+999)/1000;
        rd.bottom = (height+rd.top)*uStretchFactor1000/1000;
        /*calculate xstretch1000 and ystretch1000*/
        xstretch1000 = ((rd.right - rd.left)* 1000)/image_width ;
        ystretch1000 = ((rd.bottom - rd.top)* 1000)/image_height;
		rs.left=0;
		rs.right=image_width;
		rs.top=0;
		rs.bottom=image_height;
        if(rd.left < 0)rs.left=(-rd.left*1000)/xstretch1000;
        if(rd.top < 0)rs.top=(-rd.top*1000)/ystretch1000;
        if(rd.right > vo_screenwidth)rs.right=((vo_screenwidth-rd.left)*1000)/xstretch1000;
        if(rd.bottom > vo_screenheight)rs.bottom=((vo_screenheight-rd.top)*1000)/ystretch1000;
		/*do not allow to zoom or shrink if hardware isn't able to do so*/
		if((width < image_width)&& !(capsDrv.dwFXCaps & DDFXCAPS_OVERLAYSHRINKX))
		{
			if(capsDrv.dwFXCaps & DDFXCAPS_OVERLAYSHRINKXN)mp_msg(MSGT_VO, MSGL_ERR,"<vo_directx><ERROR>can only shrinkN\n");
	        else mp_msg(MSGT_VO, MSGL_ERR,"<vo_directx><ERROR>can't shrink x\n");
	        rd.right=rd.left+image_width;
		}
		else if((width > image_width)&& !(capsDrv.dwFXCaps & DDFXCAPS_OVERLAYSTRETCHX))
		{
			if(capsDrv.dwFXCaps & DDFXCAPS_OVERLAYSTRETCHXN)mp_msg(MSGT_VO, MSGL_ERR,"<vo_directx><ERROR>can only stretchN\n"); 
	        else mp_msg(MSGT_VO, MSGL_ERR,"<vo_directx><ERROR>can't stretch x\n");
	        rd.right = rd.left+image_width;
		}
		if((height < image_height) && !(capsDrv.dwFXCaps & DDFXCAPS_OVERLAYSHRINKY))
		{
			if(capsDrv.dwFXCaps & DDFXCAPS_OVERLAYSHRINKYN)mp_msg(MSGT_VO, MSGL_ERR,"<vo_directx><ERROR>can only shrinkN\n");
	        else mp_msg(MSGT_VO, MSGL_ERR,"<vo_directx><ERROR>can't shrink y\n");
	        rd.bottom = rd.top + image_height;
		}
		else if((height > image_height ) && !(capsDrv.dwFXCaps & DDFXCAPS_OVERLAYSTRETCHY))
		{
			if(capsDrv.dwFXCaps & DDFXCAPS_OVERLAYSTRETCHYN)mp_msg(MSGT_VO, MSGL_ERR,"<vo_directx><ERROR>can only stretchN\n");
	        else mp_msg(MSGT_VO, MSGL_ERR,"<vo_directx><ERROR>can't stretch y\n");
	        rd.bottom = rd.top + image_height;
		} 
		/*the last thing to check are alignment restrictions
          these expressions (x & -y) just do alignment by dropping low order bits...
          so to round up, we add first, then truncate*/
		if((capsDrv.dwCaps & DDCAPS_ALIGNBOUNDARYSRC) && capsDrv.dwAlignBoundarySrc)
		  rs.left = (rs.left + capsDrv.dwAlignBoundarySrc / 2) & -(signed)(capsDrv.dwAlignBoundarySrc);
        if((capsDrv.dwCaps & DDCAPS_ALIGNSIZESRC) && capsDrv.dwAlignSizeSrc)
		  rs.right = rs.left + ((rs.right - rs.left + capsDrv.dwAlignSizeSrc / 2) & -(signed) (capsDrv.dwAlignSizeSrc));
        if((capsDrv.dwCaps & DDCAPS_ALIGNBOUNDARYDEST) && capsDrv.dwAlignBoundaryDest)
		  rd.left = (rd.left + capsDrv.dwAlignBoundaryDest / 2) & -(signed)(capsDrv.dwAlignBoundaryDest);
        if((capsDrv.dwCaps & DDCAPS_ALIGNSIZEDEST) && capsDrv.dwAlignSizeDest)
		  rd.right = rd.left + ((rd.right - rd.left) & -(signed) (capsDrv.dwAlignSizeDest));
		/*create an overlay FX structure to specify a destination color key*/
		ZeroMemory(&ovfx, sizeof(ovfx));
        ovfx.dwSize = sizeof(ovfx);
        if(vo_fs||vidmode)
		{
			ovfx.dckDestColorkey.dwColorSpaceLowValue = 0; 
            ovfx.dckDestColorkey.dwColorSpaceHighValue = 0;
		}
		else
		{
			ovfx.dckDestColorkey.dwColorSpaceLowValue = destcolorkey; 
            ovfx.dckDestColorkey.dwColorSpaceHighValue = destcolorkey;
		}
        // set the flags we'll send to UpdateOverlay      //DDOVER_AUTOFLIP|DDOVERFX_MIRRORLEFTRIGHT|DDOVERFX_MIRRORUPDOWN could be useful?;
        dwUpdateFlags = DDOVER_SHOW | DDOVER_DDFX;
        /*if hardware can't do colorkeying set the window on top*/
		if(capsDrv.dwCKeyCaps & DDCKEYCAPS_DESTOVERLAY) dwUpdateFlags |= DDOVER_KEYDESTOVERRIDE;
        else if (!tmp_image) vo_ontop = 1;
	}
    else
    {
        g_lpddclipper->lpVtbl->SetHWnd(g_lpddclipper, 0,(vo_fs && !vidmode)?hWndFS: hWnd);
    }       
	
    if(!vidmode && !vo_fs){
      if(WinID == -1) {
          RECT rdw=rd;
          if (vo_border)
          AdjustWindowRect(&rdw,WNDSTYLE,FALSE);
//          printf("window: %i %i %ix%i\n",rdw.left,rdw.top,rdw.right - rdw.left,rdw.bottom - rdw.top);      
		  rdw.left += monitor_rect.left; /* move to global coordinate space */
          rdw.top += monitor_rect.top;
		  rdw.right += monitor_rect.left;
		  rdw.bottom += monitor_rect.top;
          SetWindowPos(hWnd,(vo_ontop)?HWND_TOPMOST:(vo_rootwin?HWND_BOTTOM:HWND_NOTOPMOST),rdw.left,rdw.top,rdw.right-rdw.left,rdw.bottom-rdw.top,SWP_NOOWNERZORDER); 
      }
    }
    else SetWindowPos(vidmode?hWnd:hWndFS,vo_rootwin?HWND_BOTTOM:HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOOWNERZORDER|SWP_NOCOPYBITS);

    /*make sure the overlay is inside the screen*/
    if(rd.left<0)rd.left=0;
    if(rd.right>vo_screenwidth)rd.right=vo_screenwidth;
    if(rd.top<0)rd.top=0;
    if(rd.bottom>vo_screenheight)rd.bottom=vo_screenheight;
    
  	/*for nonoverlay mode we are finished, for overlay mode we have to display the overlay first*/
	if(nooverlay)return 0;
	
//    printf("overlay: %i %i %ix%i\n",rd.left,rd.top,rd.right - rd.left,rd.bottom - rd.top);
	ddrval = g_lpddsOverlay->lpVtbl->UpdateOverlay(g_lpddsOverlay,&rs, g_lpddsPrimary, &rd, dwUpdateFlags, &ovfx);
    if(FAILED(ddrval))
    {
        // one cause might be the driver lied about minimum stretch 
        // we should try upping the destination size a bit, or
        // perhaps shrinking the source size
	   	mp_msg(MSGT_VO, MSGL_ERR ,"<vo_directx><ERROR>UpdateOverlay failed\n" );
	  	mp_msg(MSGT_VO, MSGL_ERR ,"<vo_directx><ERROR>Overlay:x1:%i,y1:%i,x2:%i,y2:%i,w:%i,h:%i\n",rd.left,rd.top,rd.right,rd.bottom,rd.right - rd.left,rd.bottom - rd.top );
	  	mp_msg(MSGT_VO, MSGL_ERR ,"<vo_directx><ERROR>");
		switch (ddrval)
		{
			case DDERR_NOSTRETCHHW:
				{mp_msg(MSGT_VO, MSGL_ERR ,"hardware can't stretch: try to size the window back\n");break;}
            case DDERR_INVALIDRECT:
				{mp_msg(MSGT_VO, MSGL_ERR ,"invalid rectangle\n");break;}
			case DDERR_INVALIDPARAMS:
				{mp_msg(MSGT_VO, MSGL_ERR ,"invalid parameters\n");break;}
			case DDERR_HEIGHTALIGN:
				{mp_msg(MSGT_VO, MSGL_ERR ,"height align\n");break;}
			case DDERR_XALIGN:
				{mp_msg(MSGT_VO, MSGL_ERR ,"x align\n");break;}
			case DDERR_UNSUPPORTED:
				{mp_msg(MSGT_VO, MSGL_ERR ,"unsupported\n");break;}
			case DDERR_INVALIDSURFACETYPE:
				{mp_msg(MSGT_VO, MSGL_ERR ,"invalid surfacetype\n");break;}
			case DDERR_INVALIDOBJECT:
				{mp_msg(MSGT_VO, MSGL_ERR ,"invalid object\n");break;}
			case DDERR_SURFACELOST:
				{
					mp_msg(MSGT_VO, MSGL_ERR ,"surfaces lost\n");
					g_lpddsOverlay->lpVtbl->Restore( g_lpddsOverlay ); //restore and try again
			        g_lpddsPrimary->lpVtbl->Restore( g_lpddsPrimary );
			        ddrval = g_lpddsOverlay->lpVtbl->UpdateOverlay(g_lpddsOverlay,&rs, g_lpddsPrimary, &rd, dwUpdateFlags, &ovfx);   
					if(ddrval !=DD_OK)mp_msg(MSGT_VO, MSGL_FATAL ,"<vo_directx><FATAL ERROR>UpdateOverlay failed again\n" );
					break;
				}
            default:
                mp_msg(MSGT_VO, MSGL_ERR ," 0x%x\n",ddrval);      
		}
	    /*ok we can't do anything about it -> hide overlay*/
		if(ddrval != DD_OK)
		{
			ddrval = g_lpddsOverlay->lpVtbl->UpdateOverlay(g_lpddsOverlay,NULL, g_lpddsPrimary, NULL, DDOVER_HIDE, NULL);
            return 1;
		}
	}
    return 0;
}

//find out supported overlay pixelformats
static uint32_t Directx_CheckOverlayPixelformats()
{
    DDCAPS          capsDrv;
    HRESULT         ddrval;
    DDSURFACEDESC2   ddsdOverlay;
	uint32_t        i;
    uint32_t        formatcount = 0;
	//get driver caps to determine overlay support
    ZeroMemory(&capsDrv, sizeof(capsDrv));
    capsDrv.dwSize = sizeof(capsDrv);
	ddrval = g_lpdd->lpVtbl->GetCaps(g_lpdd,&capsDrv, NULL);
    if (FAILED(ddrval))
	{
        mp_msg(MSGT_VO, MSGL_ERR ,"<vo_directx><ERROR>failed getting ddrawcaps\n");
		return 1;
	}
    if (!(capsDrv.dwCaps & DDCAPS_OVERLAY))
    {
		mp_msg(MSGT_VO, MSGL_ERR ,"<vo_directx><ERROR>Your card doesn't support overlay\n");
		return 1;
	}
    mp_msg(MSGT_VO, MSGL_V ,"<vo_directx><INFO>testing supported overlay pixelformats\n");
    //it is not possible to query for pixel formats supported by the
    //overlay hardware: try out various formats till one works  
    ZeroMemory(&ddsdOverlay, sizeof(ddsdOverlay));
    ddsdOverlay.dwSize = sizeof(ddsdOverlay);
    ddsdOverlay.ddsCaps.dwCaps=DDSCAPS_OVERLAY | DDSCAPS_VIDEOMEMORY;
    ddsdOverlay.dwFlags= DDSD_CAPS|DDSD_HEIGHT|DDSD_WIDTH| DDSD_PIXELFORMAT;
    ddsdOverlay.dwWidth=300;
    ddsdOverlay.dwHeight=280;
    ddsdOverlay.dwBackBufferCount=0;
    //try to create an overlay surface using one of the pixel formats in our global list
	i=0;
    do 
    {
   		ddsdOverlay.ddpfPixelFormat=g_ddpf[i].g_ddpfOverlay;
        ddrval = g_lpdd->lpVtbl->CreateSurface(g_lpdd,&ddsdOverlay, &g_lpddsOverlay, NULL);
        if (ddrval == DD_OK)
		{
			 mp_msg(MSGT_VO, MSGL_V ,"<vo_directx><FORMAT OVERLAY>%i %s supported\n",i,g_ddpf[i].img_format_name);
			 g_ddpf[i].drv_caps = VFCAP_CSP_SUPPORTED |VFCAP_OSD |VFCAP_CSP_SUPPORTED_BY_HW | VFCAP_HWSCALE_UP;
			 formatcount++;}
	    else mp_msg(MSGT_VO, MSGL_V ,"<vo_directx><FORMAT OVERLAY>%i %s not supported\n",i,g_ddpf[i].img_format_name);
		if (g_lpddsOverlay != NULL) {g_lpddsOverlay->lpVtbl->Release(g_lpddsOverlay);g_lpddsOverlay = NULL;}
	} while( ++i < NUM_FORMATS );
    mp_msg(MSGT_VO, MSGL_V ,"<vo_directx><INFO>Your card supports %i of %i overlayformats\n",formatcount, NUM_FORMATS);
	if (formatcount == 0)
	{
		mp_msg(MSGT_VO, MSGL_V ,"<vo_directx><WARN>Your card supports overlay, but we couldn't create one\n");
		mp_msg(MSGT_VO, MSGL_V ,"<vo_directx><INFO>This can have the following reasons:\n");
		mp_msg(MSGT_VO, MSGL_V ,"<vo_directx><INFO>- you are already using an overlay with another app\n");
		mp_msg(MSGT_VO, MSGL_V ,"<vo_directx><INFO>- you don't have enough videomemory\n");
		mp_msg(MSGT_VO, MSGL_V ,"<vo_directx><INFO>- vo_directx doesn't support the cards overlay pixelformat\n");
		return 1;
	}
    if(capsDrv.dwFXCaps & DDFXCAPS_OVERLAYMIRRORLEFTRIGHT)mp_msg(MSGT_VO, MSGL_V ,"<vo_directx><INFO>can mirror left right\n"); //I don't have hardware which
    if(capsDrv.dwFXCaps & DDFXCAPS_OVERLAYMIRRORUPDOWN )mp_msg(MSGT_VO, MSGL_V ,"<vo_directx><INFO>can mirror up down\n");      //supports those send me one and I'll implement ;)
	return 0;		
}

//find out the Pixelformat of the Primary Surface
static uint32_t Directx_CheckPrimaryPixelformat()
{	
	uint32_t i=0;
    uint32_t formatcount = 0;
	DDPIXELFORMAT	ddpf;
	DDSURFACEDESC2   ddsd;
    HDC             hdc;
    HRESULT         hres;
	COLORREF        rgbT=RGB(0,0,0);
	mp_msg(MSGT_VO, MSGL_V ,"<vo_directx><INFO>checking primary surface\n");
	memset( &ddpf, 0, sizeof( DDPIXELFORMAT ));
    ddpf.dwSize = sizeof( DDPIXELFORMAT );
    //we have to create a primary surface first
	if(Directx_CreatePrimarySurface()!=0)return 1;
	if(g_lpddsPrimary->lpVtbl->GetPixelFormat( g_lpddsPrimary, &ddpf ) != DD_OK )
	{
		mp_msg(MSGT_VO, MSGL_FATAL ,"<vo_directx><FATAL ERROR>can't get pixelformat\n");
		return 1;
	}
    while ( i < NUM_FORMATS )
    {
	   if (g_ddpf[i].g_ddpfOverlay.dwRGBBitCount == ddpf.dwRGBBitCount)
	   {
           if (g_ddpf[i].g_ddpfOverlay.dwRBitMask == ddpf.dwRBitMask)
		   {
			   mp_msg(MSGT_VO, MSGL_V ,"<vo_directx><FORMAT PRIMARY>%i %s supported\n",i,g_ddpf[i].img_format_name);
			   g_ddpf[i].drv_caps = VFCAP_CSP_SUPPORTED |VFCAP_OSD;
			   formatcount++;
               primary_image_format=g_ddpf[i].img_format;     
		   }
	   }
	   i++;
    }
    //get the colorkey for overlay mode
	destcolorkey = CLR_INVALID;
    if (windowcolor != CLR_INVALID && g_lpddsPrimary->lpVtbl->GetDC(g_lpddsPrimary,&hdc) == DD_OK)
    {
        rgbT = GetPixel(hdc, 0, 0);     
        SetPixel(hdc, 0, 0, windowcolor);  
        g_lpddsPrimary->lpVtbl->ReleaseDC(g_lpddsPrimary,hdc);
    }
    // read back the converted color
    ddsd.dwSize = sizeof(ddsd);
    while ((hres = g_lpddsPrimary->lpVtbl->Lock(g_lpddsPrimary,NULL, &ddsd, 0, NULL)) == DDERR_WASSTILLDRAWING)
        ;
    if (hres == DD_OK)
    {
        destcolorkey = *(DWORD *) ddsd.lpSurface;                
        if (ddsd.ddpfPixelFormat.dwRGBBitCount < 32)
            destcolorkey &= (1 << ddsd.ddpfPixelFormat.dwRGBBitCount) - 1;  
        g_lpddsPrimary->lpVtbl->Unlock(g_lpddsPrimary,NULL);
    }
    if (windowcolor != CLR_INVALID && g_lpddsPrimary->lpVtbl->GetDC(g_lpddsPrimary,&hdc) == DD_OK)
    {
        SetPixel(hdc, 0, 0, rgbT);
        g_lpddsPrimary->lpVtbl->ReleaseDC(g_lpddsPrimary,hdc);
    }
	//release primary
	g_lpddsPrimary->lpVtbl->Release(g_lpddsPrimary);
	g_lpddsPrimary = NULL;
	if(formatcount==0)
	{
		mp_msg(MSGT_VO, MSGL_FATAL ,"<vo_directx><FATAL ERROR>Unknown Pixelformat\n");
		return 1;
	}
	return 0;
}

//function handles input
static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
    {
        case WM_MOUSEACTIVATE:
            return MA_ACTIVATEANDEAT;       
	    case WM_NCACTIVATE:
        {
            if(vidmode && adapter_count > 2) //only disable if more than one adapter.
			    return 0;
			break;
		}
		case WM_DESTROY:
		{
			PostQuitMessage(0);
			return 0;
		}
        case WM_CLOSE:
		{
			mplayer_put_key(KEY_CLOSE_WIN);
			return 0;
		}
        case WM_WINDOWPOSCHANGED:
		{
			//printf("Windowposchange\n");
			if(g_lpddsBack != NULL)  //or it will crash with -vm
			{
				Directx_ManageDisplay();
			}
		    break;
		}
        case WM_SYSCOMMAND:
		{
			switch (wParam)
			{   //kill screensaver etc. 
				//note: works only when the window is active
				//you can workaround this by disabling the allow screensaver option in
				//the link to the app
				case SC_SCREENSAVE:                
				case SC_MONITORPOWER:
                mp_msg(MSGT_VO, MSGL_V ,"<vo_directx><INFO>killing screensaver\n" );
                return 0;                      
				case SC_MAXIMIZE:
					if (!vo_fs) control(VOCTRL_FULLSCREEN, NULL);
                return 0;                      
			}
			break;
		}
        case WM_KEYDOWN:
		{
			switch (wParam)
			{
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
		        case VK_BACK:
					{mplayer_put_key(KEY_BS);break;}
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
		}
        case WM_CHAR:
		{
			mplayer_put_key(wParam);
			break;
		}
        case WM_LBUTTONDOWN:
		{
			if (!vo_nomouse_input)
				mplayer_put_key(MOUSE_BTN0);
			break;
		}
        case WM_MBUTTONDOWN:
		{
			if (!vo_nomouse_input)
				mplayer_put_key(MOUSE_BTN1);
			break;
		}
        case WM_RBUTTONDOWN:
		{
			if (!vo_nomouse_input)
				mplayer_put_key(MOUSE_BTN2);
			break;
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
			int x;
			if (vo_nomouse_input)
				break;
			x = GET_WHEEL_DELTA_WPARAM(wParam);
			if (x > 0)
				mplayer_put_key(MOUSE_BTN3);
			else
				mplayer_put_key(MOUSE_BTN4);
			break;
		}
        case WM_XBUTTONDOWN:
		{
			if (vo_nomouse_input)
				break;
			if (HIWORD(wParam) == 1)
				mplayer_put_key(MOUSE_BTN5);
			else
				mplayer_put_key(MOUSE_BTN6);
			break;
		}
        case WM_XBUTTONDBLCLK:
		{
			if (vo_nomouse_input)
				break;
			if (HIWORD(wParam) == 1)
				mplayer_put_key(MOUSE_BTN5_DBL);
			else
				mplayer_put_key(MOUSE_BTN6_DBL);
			break;
		}
		
    }
	return DefWindowProc(hWnd, message, wParam, lParam);
}


static int preinit(const char *arg)
{
    HINSTANCE hInstance = GetModuleHandle(NULL);
    char exedir[MAX_PATH];
    WNDCLASS   wc;
	if(arg)
	{
		if(strstr(arg,"noaccel"))
		{
			mp_msg(MSGT_VO,MSGL_V,"<vo_directx><INFO>disabled overlay\n");
		    nooverlay = 1;
		}
	}
	/*load icon from the main app*/
    if(GetModuleFileName(NULL,exedir,MAX_PATH))
    {
        mplayericon = ExtractIcon( hInstance, exedir, 0 );
  	}
    if(!mplayericon)mplayericon=LoadIcon(NULL,IDI_APPLICATION);
    mplayercursor = LoadCursor(NULL, IDC_ARROW);
    monitor_rect.right=GetSystemMetrics(SM_CXSCREEN);
    monitor_rect.bottom=GetSystemMetrics(SM_CYSCREEN);
	
    windowcolor = vo_colorkey;
    colorbrush = CreateSolidBrush(windowcolor);
    blackbrush = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.style         =  CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc   =  WndProc;
    wc.cbClsExtra    =  0;
    wc.cbWndExtra    =  0;
    wc.hInstance     =  hInstance;
    wc.hCursor       =  mplayercursor;
    wc.hIcon         =  mplayericon;
    wc.hbrBackground =  vidmode ? blackbrush : colorbrush;
    wc.lpszClassName =  WNDCLASSNAME_WINDOWED;
    wc.lpszMenuName  =  NULL;
    RegisterClass(&wc);
    if (WinID != -1) hWnd = WinID;
    else
    hWnd = CreateWindowEx(vidmode?WS_EX_TOPMOST:0,
        WNDCLASSNAME_WINDOWED,"",(vidmode || !vo_border)?WS_POPUP:WNDSTYLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 100, 100,NULL,NULL,hInstance,NULL);
    wc.hbrBackground = blackbrush;
    wc.lpszClassName = WNDCLASSNAME_FULLSCREEN;
    RegisterClass(&wc);
	
	if (Directx_InitDirectDraw()!= 0)return 1;          //init DirectDraw
	
    if(!vidmode)hWndFS = CreateWindow(WNDCLASSNAME_FULLSCREEN,"MPlayer Fullscreen",WS_POPUP,monitor_rect.left,monitor_rect.top,monitor_rect.right-monitor_rect.left,monitor_rect.bottom-monitor_rect.top,hWnd,NULL,hInstance,NULL);			
    mp_msg(MSGT_VO, MSGL_DBG3 ,"<vo_directx><INFO>initial mplayer windows created\n");
    
    if (Directx_CheckPrimaryPixelformat()!=0)return 1;
	if (!nooverlay && Directx_CheckOverlayPixelformats() == 0)        //check for supported hardware
	{
		mp_msg(MSGT_VO, MSGL_V ,"<vo_directx><INFO>hardware supports overlay\n");
		nooverlay = 0;
 	} 
	else   //if we can't have overlay we create a backpuffer with the same imageformat as the primary surface
	{
       	mp_msg(MSGT_VO, MSGL_V ,"<vo_directx><INFO>using backpuffer\n");
		nooverlay = 1;
	}
 	mp_msg(MSGT_VO, MSGL_DBG3 ,"<vo_directx><INFO>preinit succesfully finished\n");
	return 0;
}

static int draw_slice(uint8_t *src[], int stride[], int w,int h,int x,int y )
{
	uint8_t *s;
    uint8_t *d;
    uint32_t uvstride=dstride/2;
	// copy Y
    d=image+dstride*y+x;                
    s=src[0];                           
    mem2agpcpy_pic(d,s,w,h,dstride,stride[0]);
    
	w/=2;h/=2;x/=2;y/=2;
	
	// copy U
    d=image+dstride*image_height + uvstride*y+x;
    if(image_format == IMGFMT_YV12)s=src[2];
	else s=src[1];
    mem2agpcpy_pic(d,s,w,h,uvstride,stride[1]);
	
	// copy V
    d=image+dstride*image_height +uvstride*(image_height/2) + uvstride*y+x;
    if(image_format == IMGFMT_YV12)s=src[1];
	else s=src[2];
    mem2agpcpy_pic(d,s,w,h,uvstride,stride[2]);
    return 0;
}

static void flip_page(void)
{
   	HRESULT dxresult;
	g_lpddsBack->lpVtbl->Unlock (g_lpddsBack,NULL);
	if (vo_doublebuffering) 
    {
		// flip to the next image in the sequence  
		dxresult = g_lpddsOverlay->lpVtbl->Flip( g_lpddsOverlay,NULL, DDFLIP_WAIT);
		if(dxresult == DDERR_SURFACELOST)
		{
			mp_msg(MSGT_VO, MSGL_ERR,"<vo_directx><ERROR><vo_directx><INFO>Restoring Surface\n");
			g_lpddsBack->lpVtbl->Restore( g_lpddsBack );
			// restore overlay and primary before calling
			// Directx_ManageDisplay() to avoid error messages
			g_lpddsOverlay->lpVtbl->Restore( g_lpddsOverlay );
			g_lpddsPrimary->lpVtbl->Restore( g_lpddsPrimary );
			// update overlay in case we return from screensaver
			Directx_ManageDisplay();
		    dxresult = g_lpddsOverlay->lpVtbl->Flip( g_lpddsOverlay,NULL, DDFLIP_WAIT);
		}
		if(dxresult != DD_OK)mp_msg(MSGT_VO, MSGL_ERR,"<vo_directx><ERROR>can't flip page\n");
    }
	if(nooverlay) 
	{
		DDBLTFX  ddbltfx;
        // ask for the "NOTEARING" option
	    memset( &ddbltfx, 0, sizeof(DDBLTFX) );
        ddbltfx.dwSize = sizeof(DDBLTFX);
        ddbltfx.dwDDFX = DDBLTFX_NOTEARING;
        g_lpddsPrimary->lpVtbl->Blt(g_lpddsPrimary, &rd, g_lpddsBack, NULL, DDBLT_WAIT, &ddbltfx);
	}
	if (g_lpddsBack->lpVtbl->Lock(g_lpddsBack,NULL,&ddsdsf, DDLOCK_NOSYSLOCK | DDLOCK_WAIT , NULL) == DD_OK) {
	    if(vo_directrendering && (dstride != ddsdsf.lPitch)){
	        mp_msg(MSGT_VO,MSGL_WARN,"<vo_directx><WARN>stride changed !!!! disabling direct rendering\n");
	        vo_directrendering=0;
	    }
	    if (tmp_image)
		    free(tmp_image);
	    tmp_image = NULL;
	    dstride = ddsdsf.lPitch;
	    image = ddsdsf.lpSurface;
	} else if (!tmp_image) {
		mp_msg(MSGT_VO, MSGL_WARN, "<vo_directx><WARN>Locking the surface failed, rendering to a hidden surface!\n");
		tmp_image = image = calloc(1, image_height * dstride * 2);
	}
}

static int draw_frame(uint8_t *src[])
{
  	fast_memcpy( image, *src, dstride * image_height );
	return 0;
}

static uint32_t get_image(mp_image_t *mpi)
{
    if(mpi->flags&MP_IMGFLAG_READABLE) {mp_msg(MSGT_VO, MSGL_V,"<vo_directx><ERROR>slow video ram\n");return VO_FALSE;} 
    if(mpi->type==MP_IMGTYPE_STATIC) {mp_msg(MSGT_VO, MSGL_V,"<vo_directx><ERROR>not static\n");return VO_FALSE;}
    if((mpi->width==dstride) || (mpi->flags&(MP_IMGFLAG_ACCEPT_STRIDE|MP_IMGFLAG_ACCEPT_WIDTH)))
	{
		if(mpi->flags&MP_IMGFLAG_PLANAR)
		{
		    if(image_format == IMGFMT_YV12)
			{
				mpi->planes[2]= image + dstride*image_height;
	            mpi->planes[1]= image + dstride*image_height+ dstride*image_height/4;
                mpi->stride[1]=mpi->stride[2]=dstride/2;
			}
			else if(image_format == IMGFMT_IYUV || image_format == IMGFMT_I420)
			{
				mpi->planes[1]= image + dstride*image_height;
	            mpi->planes[2]= image + dstride*image_height+ dstride*image_height/4;
			    mpi->stride[1]=mpi->stride[2]=dstride/2;
			}
			else if(image_format == IMGFMT_YVU9)
			{
				mpi->planes[2] = image + dstride*image_height;
				mpi->planes[1] = image + dstride*image_height+ dstride*image_height/16;
			    mpi->stride[1]=mpi->stride[2]=dstride/4;
			}
		}
		mpi->planes[0]=image;
        mpi->stride[0]=dstride;
   		mpi->width=image_width;
		mpi->height=image_height;
        mpi->flags|=MP_IMGFLAG_DIRECT;
        mp_msg(MSGT_VO, MSGL_DBG3, "<vo_directx><INFO>Direct Rendering ENABLED\n");
        return VO_TRUE;
    } 
    return VO_FALSE;
}
  
static uint32_t put_image(mp_image_t *mpi){

    uint8_t   *d;
	uint8_t   *s;
    uint32_t x = mpi->x;
	uint32_t y = mpi->y;
	uint32_t w = mpi->w;
	uint32_t h = mpi->h;

    if (WinID != -1) Directx_ManageDisplay();
   
    if((mpi->flags&MP_IMGFLAG_DIRECT)||(mpi->flags&MP_IMGFLAG_DRAW_CALLBACK)) 
	{
		mp_msg(MSGT_VO, MSGL_DBG3 ,"<vo_directx><INFO>put_image: nothing to do: drawslices\n");
		return VO_TRUE;
    }
   
    if (mpi->flags&MP_IMGFLAG_PLANAR)
	{
		
		if(image_format!=IMGFMT_YVU9)draw_slice(mpi->planes,mpi->stride,mpi->w,mpi->h,0,0);
		else
		{
			// copy Y
            d=image+dstride*y+x;
            s=mpi->planes[0];
            mem2agpcpy_pic(d,s,w,h,dstride,mpi->stride[0]);
            w/=4;h/=4;x/=4;y/=4;
    	    // copy V
            d=image+dstride*image_height + dstride*y/4+x;
	        s=mpi->planes[2];
            mem2agpcpy_pic(d,s,w,h,dstride/4,mpi->stride[1]);
  	        // copy U
            d=image+dstride*image_height + dstride*image_height/16 + dstride/4*y+x;
		    s=mpi->planes[1];
            mem2agpcpy_pic(d,s,w,h,dstride/4,mpi->stride[2]);
		}
	}
	else //packed
	{
		mem2agpcpy_pic(image, mpi->planes[0], w * (mpi->bpp / 8), h, dstride, mpi->stride[0]);
	}
	return VO_TRUE;
}

static int
config(uint32_t width, uint32_t height, uint32_t d_width, uint32_t d_height, uint32_t options, char *title, uint32_t format)
{
    RECT rd;
    vo_fs = options & 0x01;
	image_format =  format;
	image_width = width;
	image_height = height;
	d_image_width = d_width;
	d_image_height = d_height;
    if(format != primary_image_format)nooverlay = 0;
    window_aspect= (float)d_image_width / (float)d_image_height;

#ifdef CONFIG_GUI
    if(use_gui){
        guiGetEvent(guiSetShVideo, 0);
    }
#endif
    /*release all directx objects*/
    if (g_cc != NULL)g_cc->lpVtbl->Release(g_cc);
    g_cc=NULL;
    if(g_lpddclipper)g_lpddclipper->lpVtbl->Release(g_lpddclipper);
        g_lpddclipper=NULL;   
    if (g_lpddsBack != NULL) g_lpddsBack->lpVtbl->Release(g_lpddsBack);
    g_lpddsBack = NULL;
    if(vo_doublebuffering)
        if (g_lpddsOverlay != NULL)g_lpddsOverlay->lpVtbl->Release(g_lpddsOverlay);
    g_lpddsOverlay = NULL;
    if (g_lpddsPrimary != NULL) g_lpddsPrimary->lpVtbl->Release(g_lpddsPrimary);
    g_lpddsPrimary = NULL;
    mp_msg(MSGT_VO, MSGL_DBG3,"<vo_directx><INFO>overlay surfaces released\n");

    if(!vidmode){
        if(!vo_geometry){
            GetWindowRect(hWnd,&rd);
            vo_dx=rd.left;
            vo_dy=rd.top;
        }
        vo_dx += monitor_rect.left; /* move position to global window space */
        vo_dy += monitor_rect.top;
        rd.left = vo_dx;
        rd.top = vo_dy;
        rd.right = rd.left + d_image_width;
        rd.bottom = rd.top + d_image_height;
        if (WinID == -1) {
        if (vo_border)
        AdjustWindowRect(&rd,WNDSTYLE,FALSE);  
        SetWindowPos(hWnd,NULL, vo_dx, vo_dy,rd.right-rd.left,rd.bottom-rd.top,SWP_SHOWWINDOW|SWP_NOOWNERZORDER); 
        }
    }
    else ShowWindow(hWnd,SW_SHOW); 
     
    if(vo_fs && !vidmode)ShowWindow(hWndFS,SW_SHOW);   
	if (WinID == -1)
	SetWindowText(hWnd,title);
    
    
    if(vidmode)vo_fs=0;    


	/*create the surfaces*/
    if(Directx_CreatePrimarySurface())return 1;
 
	//create palette for 256 color mode  
	if(image_format==IMGFMT_BGR8){
		LPDIRECTDRAWPALETTE ddpalette=NULL;
		char* palette=malloc(4*256);
		int i; 
		for(i=0; i<256; i++){
			palette[4*i+0] = ((i >> 5) & 0x07) * 255 / 7;
			palette[4*i+1] = ((i >> 2) & 0x07) * 255 / 7;
			palette[4*i+2] = ((i >> 0) & 0x03) * 255 / 3;
			palette[4*i+3] = PC_NOCOLLAPSE;	
		}
		g_lpdd->lpVtbl->CreatePalette(g_lpdd,DDPCAPS_8BIT|DDPCAPS_INITIALIZE,palette,&ddpalette,NULL);  
		g_lpddsPrimary->lpVtbl->SetPalette(g_lpddsPrimary,ddpalette);
		free(palette);       
		ddpalette->lpVtbl->Release(ddpalette);      
	}

	if (!nooverlay && Directx_CreateOverlay(image_format))
	{
			if(format == primary_image_format)nooverlay=1; /*overlay creation failed*/
			else {
              mp_msg(MSGT_VO, MSGL_FATAL,"<vo_directx><FATAL ERROR>can't use overlay mode: please use -vo directx:noaccel\n");
			  return 1;
            }       
	}
	if(nooverlay)
	{
		if(Directx_CreateBackpuffer())
		{
			mp_msg(MSGT_VO, MSGL_FATAL,"<vo_directx><FATAL ERROR>can't get the driver to work on your system :(\n");
			return 1;
		}
		mp_msg(MSGT_VO, MSGL_V,"<vo_directx><INFO>back surface created\n");
		vo_doublebuffering = 0;
		/*create clipper for nonoverlay mode*/
	    if(g_lpdd->lpVtbl->CreateClipper(g_lpdd, 0, &g_lpddclipper,NULL)!= DD_OK){mp_msg(MSGT_VO, MSGL_FATAL,"<vo_directx><FATAL ERROR>can't create clipper\n");return 1;}
        if(g_lpddclipper->lpVtbl->SetHWnd (g_lpddclipper, 0, hWnd)!= DD_OK){mp_msg(MSGT_VO, MSGL_FATAL,"<vo_directx><FATAL ERROR>can't associate clipper with window\n");return 1;}
        if(g_lpddsPrimary->lpVtbl->SetClipper (g_lpddsPrimary,g_lpddclipper)!=DD_OK){mp_msg(MSGT_VO, MSGL_FATAL,"<vo_directx><FATAL ERROR>can't associate primary surface with clipper\n");return 1;}
	    mp_msg(MSGT_VO, MSGL_DBG3,"<vo_directx><INFO>clipper succesfully created\n");
	}else{
		if(DD_OK != g_lpddsOverlay->lpVtbl->QueryInterface(g_lpddsOverlay,&IID_IDirectDrawColorControl,(void**)&g_cc))
			mp_msg(MSGT_VO, MSGL_V,"<vo_directx><WARN>unable to get DirectDraw ColorControl interface\n");
	}
	Directx_ManageDisplay();
	memset(&ddsdsf, 0,sizeof(DDSURFACEDESC2));
	ddsdsf.dwSize = sizeof (DDSURFACEDESC2);
	if (g_lpddsBack->lpVtbl->Lock(g_lpddsBack,NULL,&ddsdsf, DDLOCK_NOSYSLOCK | DDLOCK_WAIT, NULL) == DD_OK) {
        dstride = ddsdsf.lPitch;
        image = ddsdsf.lpSurface;
        return 0;
	}
	mp_msg(MSGT_VO, MSGL_V, "<vo_directx><ERROR>Initial Lock on the Surface failed.\n");
	return 1;
}

//function to set color controls
//  brightness	[0, 10000]
//  contrast	[0, 20000]
//  hue		[-180, 180]
//  saturation	[0, 20000]
static uint32_t color_ctrl_set(char *what, int value)
{
	uint32_t	r = VO_NOTIMPL;
	DDCOLORCONTROL	dcc;
	//printf("\n*** %s = %d\n", what, value);
	if (!g_cc) {
		//printf("\n *** could not get color control interface!!!\n");
		return VO_NOTIMPL;
	}
	ZeroMemory(&dcc, sizeof(dcc));
	dcc.dwSize = sizeof(dcc);

	if (!strcmp(what, "brightness")) {
		dcc.dwFlags = DDCOLOR_BRIGHTNESS;
		dcc.lBrightness = (value + 100) * 10000 / 200;
		r = VO_TRUE;
	} else if (!strcmp(what, "contrast")) {
		dcc.dwFlags = DDCOLOR_CONTRAST;
		dcc.lContrast = (value + 100) * 20000 / 200;
		r = VO_TRUE;
	} else if (!strcmp(what, "hue")) {
		dcc.dwFlags = DDCOLOR_HUE;
		dcc.lHue = value * 180 / 100;
		r = VO_TRUE;
	} else if (!strcmp(what, "saturation")) {
		dcc.dwFlags = DDCOLOR_SATURATION;
		dcc.lSaturation = (value + 100) * 20000 / 200;
		r = VO_TRUE;
	}
	
	if (r == VO_TRUE) {
		g_cc->lpVtbl->SetColorControls(g_cc, &dcc);
	}
	return r;
}

//analoguous to color_ctrl_set
static uint32_t color_ctrl_get(char *what, int *value)
{
	uint32_t	r = VO_NOTIMPL;
	DDCOLORCONTROL	dcc;
	if (!g_cc) {
		//printf("\n *** could not get color control interface!!!\n");
		return VO_NOTIMPL;
	}
	ZeroMemory(&dcc, sizeof(dcc));
	dcc.dwSize = sizeof(dcc);

	if (g_cc->lpVtbl->GetColorControls(g_cc, &dcc) != DD_OK) {
		return r;
	}

	if (!strcmp(what, "brightness") && (dcc.dwFlags & DDCOLOR_BRIGHTNESS)) {
		*value = dcc.lBrightness * 200 / 10000 - 100;
		r = VO_TRUE;
	} else if (!strcmp(what, "contrast") && (dcc.dwFlags & DDCOLOR_CONTRAST)) {
		*value = dcc.lContrast * 200 / 20000 - 100;
		r = VO_TRUE;
	} else if (!strcmp(what, "hue") && (dcc.dwFlags & DDCOLOR_HUE)) {
		*value = dcc.lHue * 100 / 180;
		r = VO_TRUE;
	} else if (!strcmp(what, "saturation") && (dcc.dwFlags & DDCOLOR_SATURATION)) {
		*value = dcc.lSaturation * 200 / 20000 - 100;
		r = VO_TRUE;
	}
//	printf("\n*** %s = %d\n", what, *value);
	
	return r;
}

static int control(uint32_t request, void *data, ...)
{
    switch (request) {
   
	case VOCTRL_GET_IMAGE:
      	return get_image(data);
    case VOCTRL_QUERY_FORMAT:
        last_rect.left = 0xDEADC0DE;   // reset window position cache
        return query_format(*((uint32_t*)data));
	case VOCTRL_DRAW_IMAGE:
        return put_image(data);
    case VOCTRL_BORDER:
			if(WinID != -1) return VO_TRUE;
	        if(vidmode)
			{
				mp_msg(MSGT_VO, MSGL_ERR,"<vo_directx><ERROR>border has no meaning in exclusive mode\n");
			}
	        else
			{
				if(vo_border) {
					vo_border = 0;
					SetWindowLong(hWnd, GWL_STYLE, WS_POPUP);
				} else {
					vo_border = 1;
					SetWindowLong(hWnd, GWL_STYLE, WNDSTYLE);
				}
				// needed AFAICT to force the window to
				// redisplay with the new style.  --Joey
				if (!vo_fs) {
					ShowWindow(hWnd,SW_HIDE);
					ShowWindow(hWnd,SW_SHOW);
				}
				last_rect.left = 0xDEADC0DE;   // reset window position cache
				Directx_ManageDisplay();
			}
		return VO_TRUE;
    case VOCTRL_ONTOP:
			if(WinID != -1) return VO_TRUE;
	        if(vidmode)
			{
				mp_msg(MSGT_VO, MSGL_ERR,"<vo_directx><ERROR>ontop has no meaning in exclusive mode\n");
			}
	        else
			{
				if(vo_ontop) vo_ontop = 0;
				else vo_ontop = 1;
				last_rect.left = 0xDEADC0DE;   // reset window position cache
				Directx_ManageDisplay();
			}
		return VO_TRUE;
    case VOCTRL_ROOTWIN:
			if(WinID != -1) return VO_TRUE;
	        if(vidmode)
			{
				mp_msg(MSGT_VO, MSGL_ERR,"<vo_directx><ERROR>rootwin has no meaning in exclusive mode\n");
			}
	        else
			{
				if(vo_rootwin) vo_rootwin = 0;
				else vo_rootwin = 1;
				last_rect.left = 0xDEADC0DE;   // reset window position cache
				Directx_ManageDisplay();
			}
		return VO_TRUE;
    case VOCTRL_FULLSCREEN:
		{
	        if(vidmode)
			{
				mp_msg(MSGT_VO, MSGL_ERR,"<vo_directx><ERROR>currently we do not allow to switch from exclusive to windowed mode\n");
			}
	        else
			{
			    if(!vo_fs)
				{
					vo_fs=1;
					ShowWindow(hWndFS,SW_SHOW);
					ShowWindow(hWnd,SW_HIDE);
					SetForegroundWindow(hWndFS);
				}
                else
				{
					vo_fs=0;
					ShowWindow(hWndFS,SW_HIDE);
					ShowWindow(hWnd,SW_SHOW);
				}  
	      			last_rect.left = 0xDEADC0DE;   // reset window position cache
				Directx_ManageDisplay();
                break;				
			}
		    return VO_TRUE;
		}
	case VOCTRL_SET_EQUALIZER: {
		va_list	ap;
		int	value;
		
		va_start(ap, data);
		value = va_arg(ap, int);
		va_end(ap);
		return color_ctrl_set(data, value);
	}
	case VOCTRL_GET_EQUALIZER: {
		va_list	ap;
		int	*value;
		
		va_start(ap, data);
		value = va_arg(ap, int*);
		va_end(ap);
		return color_ctrl_get(data, value);
	}
    case VOCTRL_UPDATE_SCREENINFO:
        if (vidmode) {
            vo_screenwidth = vm_width;
            vo_screenheight = vm_height;
        } else {
            vo_screenwidth = monitor_rect.right - monitor_rect.left;
            vo_screenheight = monitor_rect.bottom - monitor_rect.top;
        }
        aspect_save_screenres(vo_screenwidth, vo_screenheight);
        return VO_TRUE;
    case VOCTRL_RESET:
        last_rect.left = 0xDEADC0DE;   // reset window position cache
        // fall-through intended
    };
    return VO_NOTIMPL;
}
