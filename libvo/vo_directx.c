/******************************************************************************
 * vo_directx.c: Directx v2 or later DirectDraw interface for MPlayer
 * Copyright (c) 2002 Sascha Sommer <saschasommer@freenet.de>.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *
 *****************************************************************************/
/******************************************************************************
 * TODO:
 * -fix dr + implement DMA
 * -implement mousehiding
 * -better exclusive mode
 *
 *****************************************************************************/

#include <windows.h>
#include <windowsx.h>
#include <ddraw.h>
#include <initguid.h>
#include <stdlib.h>
#include <errno.h>
#include "config.h"
#include "video_out.h"
#include "video_out_internal.h"
#include "fastmemcpy.h"
#include "../input/input.h"
#include "../linux/keycodes.h"
#include "../mp_msg.h"

static LPDIRECTDRAW2        g_lpdd = NULL;          //DirectDraw Object
static LPDIRECTDRAWSURFACE  g_lpddsPrimary = NULL;  //Primary Surface: viewport through the Desktop
static LPDIRECTDRAWSURFACE  g_lpddsOverlay = NULL;  //Overlay Surface
static LPDIRECTDRAWSURFACE  g_lpddsBack = NULL;     //Back surface
static LPDIRECTDRAWCLIPPER  g_lpddclipper;          //clipper object, can only be used without overlay
static DDSURFACEDESC		ddsdsf;                 //surface descripiton needed for locking
static RECT                 rd;                     //rect of our stretched image
static RECT                 rs;                     //rect of our source image
static HWND                 hWnd=NULL;              //handle to the window
static uint32_t             ontop=0;                //always in foreground
static uint32_t image_width, image_height;          //image width and height
static uint32_t d_image_width, d_image_height;      //image width and height zoomed 
static uint8_t  *image=NULL;                        //image data
static uint32_t image_format;                       //image format
static uint32_t vm = 0;                             //exclusive mode, allows resolution switching (not implemented yet)
static uint32_t fs = 0;                             //display in window or fullscreen 
static uint32_t dstride;                            //surface stride
static uint32_t swap = 1;                           //swap u<->v planes set to 1 if you experience bluish faces 
static uint32_t nooverlay = 0;                      //NonOverlay mode
static DWORD    destcolorkey;                       //colorkey for our surface
static COLORREF windowcolor = RGB(0,0,16);          //windowcolor == colorkey

extern void mplayer_put_key(int code);              //let mplayer handel the keyevents 
extern int vo_doublebuffering;                      //tribblebuffering    

/*****************************************************************************
 * DirectDraw GUIDs.
 * Defining them here allows us to get rid of the dxguid library during
 * the linking stage.
 *****************************************************************************/
DEFINE_GUID( IID_IDirectDraw2,                  0xB3A6F3E0,0x2B43,0x11CF,0xA2,0xDE,0x00,0xAA,0x00,0xB9,0x33,0x56 );

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
//	{"UYVY ",IMGFMT_UYVY ,0,{sizeof(DDPIXELFORMAT), DDPF_FOURCC,MAKEFOURCC('U','Y','V','Y'),0,0,0,0,0}},
	{"RGB15",IMGFMT_RGB15,0,{sizeof(DDPIXELFORMAT), DDPF_RGB, 0, 16,  0x0000001F, 0x000003E0, 0x00007C00, 0}},   //RGB 5:5:5
	{"BGR15",IMGFMT_BGR15,0,{sizeof(DDPIXELFORMAT), DDPF_RGB, 0, 16,  0x00007C00, 0x000003E0, 0x0000001F, 0}},   
	{"RGB16",IMGFMT_RGB16,0,{sizeof(DDPIXELFORMAT), DDPF_RGB, 0, 16,  0x0000F800, 0x000007E0, 0x0000001F, 0}},   //RGB 5:6:5
	{"BGR16",IMGFMT_BGR16,0,{sizeof(DDPIXELFORMAT), DDPF_RGB, 0, 16,  0x0000001F, 0x000007E0, 0x0000F800, 0}},  
	{"RGB24",IMGFMT_RGB24,0,{sizeof(DDPIXELFORMAT), DDPF_RGB, 0, 24,  0x000000FF, 0x0000FF00, 0x00FF0000, 0}},   
    {"BGR24",IMGFMT_BGR24,0,{sizeof(DDPIXELFORMAT), DDPF_RGB, 0, 24,  0x00FF0000, 0x0000FF00, 0x000000FF, 0}},  
    {"RGB32",IMGFMT_RGB32,0,{sizeof(DDPIXELFORMAT), DDPF_RGB, 0, 32,  0x000000FF, 0x0000FF00, 0x00FF0000, 0}},  
    {"BGR32",IMGFMT_BGR32,0,{sizeof(DDPIXELFORMAT), DDPF_RGB, 0, 32,  0x00FF0000, 0x0000FF00, 0x000000FF, 0}}   
};
#define NUM_FORMATS (sizeof(g_ddpf) / sizeof(g_ddpf[0]))


LIBVO_EXTERN(directx)

static vo_info_t vo_info =
{
	"Directx DDraw YUV/RGB/BGR renderer",
	"directx",
	"Sascha Sommer <saschasommer@freenet.de>",
	""
};


static void draw_alpha(int x0, int y0, int w, int h, unsigned char *src,
		unsigned char *srca, int stride)
{
    switch(image_format) {
    case IMGFMT_YV12 :
    case IMGFMT_I420 :
	case IMGFMT_IYUV :
	case IMGFMT_YVU9 :
    	vo_draw_alpha_yv12(w,h,src,srca,stride,((uint8_t *) image) + image_width*y0 + x0,image_width);
	break;
	case IMGFMT_YUY2 :
	    vo_draw_alpha_yuy2(w,h,src,srca,stride,((uint8_t *) image)+ 2*image_width*y0 + 2*x0 ,2*image_width);
    break;
    case IMGFMT_UYVY :
        vo_draw_alpha_yuy2(w,h,src,srca,stride,((uint8_t *) image) + 2*image_width*y0 + 2*x0 + 1,dstride);
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

static uint32_t
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
    DDSURFACEDESC   ddsd;
    //cleanup
	if(g_lpddsPrimary)g_lpddsPrimary->lpVtbl->Release(g_lpddsPrimary);
	g_lpddsPrimary=NULL;
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
	DDSURFACEDESC   ddsdOverlay;
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
	   mp_msg(MSGT_VO, MSGL_ERR,"<vo_directx><ERROR>");
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
	      case DDERR_INVALIDPIXELFORMAT:
		     {mp_msg(MSGT_VO, MSGL_ERR,"invalid pixelformat\n");break;}
	      case DDERR_NODIRECTDRAWHW:
		     {mp_msg(MSGT_VO, MSGL_ERR,"no directdraw hardware\n");break;}
	      case DDERR_NOEMULATION:
		     {mp_msg(MSGT_VO, MSGL_ERR,"cant emulate\n");break;}
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
	   }
	   return 1;
	}
    g_lpddsBack = g_lpddsOverlay;
	return 0;
}

static uint32_t Directx_CreateBackpuffer()
{
	DDSURFACEDESC   ddsd;
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

//create clipper for nonoverlay mode
static uint32_t Directx_CreateClipper()
{
	if(g_lpddclipper)g_lpddclipper->lpVtbl->Release(g_lpddclipper);
    g_lpddclipper=NULL;
	if(g_lpdd->lpVtbl->CreateClipper(g_lpdd, 0, &g_lpddclipper,NULL)!= DD_OK){mp_msg(MSGT_VO, MSGL_FATAL,"<vo_directx><FATAL ERROR>can't create clipper\n");return 1;}
    if(g_lpddclipper->lpVtbl->SetHWnd (g_lpddclipper, 0, hWnd)!= DD_OK){mp_msg(MSGT_VO, MSGL_FATAL,"<vo_directx><FATAL ERROR>can't associate clipper with window\n");return 1;}
    if(g_lpddsPrimary->lpVtbl->SetClipper (g_lpddsPrimary,g_lpddclipper)!=DD_OK){mp_msg(MSGT_VO, MSGL_FATAL,"<vo_directx><FATAL ERROR>can't associate primary surface with clipper\n");return 1;}
	mp_msg(MSGT_VO, MSGL_DBG3,"<vo_directx><INFO>clipper succesfully created\n");
	return 0;
}

static const vo_info_t*
get_info(void)
{
	return &vo_info;
}

static void uninit(void)
{
	if (g_lpddclipper != NULL) g_lpddclipper->lpVtbl->Release(g_lpddclipper);
	mp_msg(MSGT_VO, MSGL_DBG3,"<vo_directx><INFO>clipper released\n");
	if (g_lpddsBack != NULL) g_lpddsBack->lpVtbl->Release(g_lpddsBack);
	g_lpddsBack = NULL;
	mp_msg(MSGT_VO, MSGL_DBG3,"<vo_directx><INFO>back surface released\n");
	if(vo_doublebuffering)
	{
		if (g_lpddsOverlay != NULL)g_lpddsOverlay->lpVtbl->Release(g_lpddsOverlay);
        g_lpddsOverlay = NULL;
		mp_msg(MSGT_VO, MSGL_DBG3,"<vo_directx><INFO>overlay surface released\n");
	}
	if (g_lpddsPrimary != NULL) g_lpddsPrimary->lpVtbl->Release(g_lpddsPrimary);
    g_lpddsPrimary = NULL;
	mp_msg(MSGT_VO, MSGL_DBG3,"<vo_directx><INFO>primary released\n");
	if(hWnd != NULL)DestroyWindow(hWnd);
	mp_msg(MSGT_VO, MSGL_DBG3,"<vo_directx><INFO>window destroyed\n");
	if (g_lpdd != NULL) g_lpdd->lpVtbl->Release(g_lpdd);
	mp_msg(MSGT_VO, MSGL_DBG3,"<vo_directx><INFO>directdrawobject released\n");
	mp_msg(MSGT_VO, MSGL_DBG3,"<vo_directx><INFO>uninited\n");    
}

static uint32_t Directx_InitDirectDraw()
{
    HINSTANCE  hddraw_dll;
	HRESULT    (WINAPI *OurDirectDrawCreate)(GUID *,LPDIRECTDRAW *,IUnknown *);
 	LPDIRECTDRAW lpDDraw;
	mp_msg(MSGT_VO, MSGL_DBG3,"<vo_directx><INFO>Initing DirectDraw\n" );

	//load direct draw DLL: based on videolans code
	hddraw_dll = LoadLibrary("DDRAW.DLL");
	if( hddraw_dll == NULL )
    {
        mp_msg(MSGT_VO, MSGL_FATAL,"<vo_directx><FATAL ERROR>failed loading ddraw.dll\n" );
		return 1;
    }
	OurDirectDrawCreate = (void *)GetProcAddress(hddraw_dll, "DirectDrawCreate");
    if ( OurDirectDrawCreate == NULL )
    {
        FreeLibrary( hddraw_dll );
        hddraw_dll = NULL;
        mp_msg(MSGT_VO, MSGL_FATAL,"<vo_directx><FATAL ERROR>failed geting proc address\n");
		return 1;
    }
	// initialize DirectDraw and create directx v1 object 
    if (OurDirectDrawCreate( NULL, &lpDDraw, NULL ) != DD_OK )
    {
        lpDDraw = NULL;
        FreeLibrary( hddraw_dll );
        hddraw_dll = NULL;
        mp_msg(MSGT_VO, MSGL_FATAL,"<vo_directx><FATAL ERROR>can't initialize ddraw\n");
		return 1;
    }
   	// ask IDirectDraw for IDirectDraw2 
	if (lpDDraw->lpVtbl->QueryInterface(lpDDraw, &IID_IDirectDraw2, (void **)&g_lpdd) != DD_OK)
    {
        mp_msg(MSGT_VO, MSGL_FATAL,"<vo_directx><FATAL ERROR>no directx 2 installed\n");
		return 1;
    }
	//release our old interface and free ddraw.dll
    lpDDraw->lpVtbl->Release(lpDDraw);
 	FreeLibrary( hddraw_dll);
	hddraw_dll= NULL;
	mp_msg(MSGT_VO, MSGL_DBG3,"<vo_directx><INFO>lpDDraw released & hddraw freed\n" );
	//set cooperativelevel: for our tests, no handle to a window is needed
	if (g_lpdd->lpVtbl->SetCooperativeLevel(g_lpdd, NULL, DDSCL_NORMAL) != DD_OK)
    {
        mp_msg(MSGT_VO, MSGL_FATAL,"<vo_directx><FATAL ERROR>could not set cooperativelevel for hardwarecheck\n");
		return 1;
    }
    mp_msg(MSGT_VO, MSGL_DBG3,"<vo_directx><INFO>DirectDraw Inited\n");
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

//fullscreen code: centers and zooms image, returns window rect
static RECT Directx_Fullscreencalc()
{
	uint32_t        xscreen = GetSystemMetrics(SM_CXSCREEN);
    uint32_t        yscreen = GetSystemMetrics(SM_CYSCREEN);
	uint32_t        xstretch1000,ystretch1000; 
	RECT            rd_window;
	rd.top = 0;
	rd.left=0;
	rd.right = xscreen;
	rd.bottom = yscreen;
	rd_window=rd;
    xstretch1000 = (xscreen*1000)/d_image_width;           
    ystretch1000 = (yscreen*1000)/d_image_height;
    if(xstretch1000 > ystretch1000)
	{
		rd.bottom=yscreen;
		rd.right=(d_image_width*ystretch1000)/1000;
	}
	else
	{
		rd.right=xscreen;
		rd.bottom=(d_image_height*xstretch1000)/1000;
	}
	//printf("%i,%i,%i,%i,%i,%i\n",xstretch1000,ystretch1000,d_image_height,d_image_width,rd.bottom,rd.right);
	rd.left = (xscreen-rd.right)/2;
	rd.right= rd.right+rd.left;
	rd.top = (yscreen-rd.bottom)/2;
	rd.bottom = rd.bottom + rd.top;
    return rd_window;
}

//this function hopefully takes into account most of those  alignments, minimum stretch, move outside of screen
//no stretch hw, etc... and displays the overlay
//I seperated non overlay mode because it is already to much stuff in here
static uint32_t Directx_DisplayOverlay()
{   
    WINDOWPLACEMENT window_placement;
    RECT            rd_window;  //Rect of the window
    HRESULT         ddrval;
    DDCAPS          capsDrv;
    DDOVERLAYFX     ovfx;
    DWORD           dwUpdateFlags;
    HWND            hWndafter;
	uint32_t        uStretchFactor1000;  //minimum stretch 
    uint32_t        xstretch1000,ystretch1000; 
    uint32_t        xscreen = GetSystemMetrics(SM_CXSCREEN);
    uint32_t        yscreen = GetSystemMetrics(SM_CYSCREEN);
    POINT           point_window;
    //get driver capabilities
    ZeroMemory(&capsDrv, sizeof(capsDrv));
    capsDrv.dwSize = sizeof(capsDrv);
    if(g_lpdd->lpVtbl->GetCaps(g_lpdd,&capsDrv, NULL) != DD_OK)return 1;
    //check minimum stretch, depends on display adaptor and mode (refresh rate!) 
    uStretchFactor1000 = capsDrv.dwMinOverlayStretch>1000 ? capsDrv.dwMinOverlayStretch : 1000;
    window_placement.length = sizeof(WINDOWPLACEMENT);
    GetWindowPlacement(hWnd, &window_placement );
    if(!fs) //windowed
    {
       if (window_placement.showCmd == SW_SHOWMAXIMIZED)   //is our window maximized?
	   {
		   CloseWindow(hWnd);  //we do this to prevent the screen being filled with garbage
		   window_placement.showCmd = SW_SHOWNORMAL;       //restore 		   
		   SetWindowLong( hWnd,GWL_STYLE , WS_OVERLAPPEDWINDOW| WS_SIZEBOX );       //change Style
		   SetWindowPlacement( hWnd, &window_placement );
	   }
    }
    else       //fullscreen
    {
	   if (window_placement.showCmd == SW_SHOWNORMAL)   //is our window normal?
	   {
		   window_placement.showCmd = SW_SHOWMAXIMIZED;       //maximize
		   SetWindowLong( hWnd, GWL_STYLE, 0 );       //change style
		   SetWindowPlacement( hWnd, &window_placement );
	   }
    }

    if(!fs)
	{
		GetClientRect (hWnd, &rd_window);
        if(rd_window.top == rd_window.bottom)   //window is minimized
		{
			ddrval = g_lpddsOverlay->lpVtbl->UpdateOverlay(g_lpddsOverlay,NULL, g_lpddsPrimary, NULL, DDOVER_HIDE, NULL); //hide the overlay
	        return 0;
		}
        if (GetWindowTextLength(hWnd)==0)
		{
			SetWindowText(hWnd,"MPlayer - DirectX Overlay"); //for the first call: change only title, not the overlay size 
		}
        else
		{
			d_image_width = rd_window.right - rd_window.left; //calculate new overlay from the window size
            d_image_height = rd_window.bottom - rd_window.top;
		}
	}
    else  
	{
		rd.top = 0;rd.left=0;
	    rd.bottom = yscreen; rd.right = xscreen;
	    rs.top = 0;rs.left=0;
	    rs.bottom = image_height; rs.right = image_width;
	}
    //check if hardware can stretch or shrink, if needed
    //do some cards support only nshrinking or is this a special shrink,stretch feature?
    if((d_image_width < image_width)&& !(capsDrv.dwFXCaps & DDFXCAPS_OVERLAYSHRINKX))
    {         
	    if(capsDrv.dwFXCaps & DDFXCAPS_OVERLAYSHRINKXN)mp_msg(MSGT_VO, MSGL_ERR,"<vo_directx><ERROR>can only shrinkN\n");
	    else mp_msg(MSGT_VO, MSGL_ERR,"<vo_directx><ERROR>can't shrink x\n");
	    d_image_width=image_width;
    }
    else if((d_image_width > image_width)&& !(capsDrv.dwFXCaps & DDFXCAPS_OVERLAYSTRETCHX))
    {	   
	    if(capsDrv.dwFXCaps & DDFXCAPS_OVERLAYSTRETCHXN)mp_msg(MSGT_VO, MSGL_ERR,"<vo_directx><ERROR>can only stretchN\n"); 
	    else mp_msg(MSGT_VO, MSGL_ERR,"<vo_directx><ERROR>can't stretch x\n");
	    d_image_width = image_width;
    }
    if((d_image_height < image_height) && !(capsDrv.dwFXCaps & DDFXCAPS_OVERLAYSHRINKY))
    {
	    if(capsDrv.dwFXCaps & DDFXCAPS_OVERLAYSHRINKYN)mp_msg(MSGT_VO, MSGL_ERR,"<vo_directx><ERROR>can only shrinkN\n");
	    else mp_msg(MSGT_VO, MSGL_ERR,"<vo_directx><ERROR>can't shrink y\n");
	    d_image_height = image_height;
    }
    else if((d_image_height > image_height ) && !(capsDrv.dwFXCaps & DDFXCAPS_OVERLAYSTRETCHY))
    {
   	    if(capsDrv.dwFXCaps & DDFXCAPS_OVERLAYSTRETCHYN)mp_msg(MSGT_VO, MSGL_ERR,"<vo_directx><ERROR>can only stretchN\n");
	    else mp_msg(MSGT_VO, MSGL_ERR,"<vo_directx><ERROR>can't stretch y\n");
	    d_image_height = image_height;
    }

    if(!fs) //windowed
	{
		point_window.x = 0;  //overlayposition relative to the window
        point_window.y = 0;
        ClientToScreen(hWnd,&point_window);  
        rd.left = point_window.x;                    //fill the wanted x1&y1 of the Overlay
        rd.top = point_window.y;
        //fix minimum stretch
        rd.right = ((d_image_width+rd.left)*uStretchFactor1000+999)/1000;
        rd.bottom = (d_image_height+rd.top)*uStretchFactor1000/1000;
        //calculate xstretch1000 and ystretch1000
        xstretch1000 = ((rd.right - rd.left)* 1000)/image_width ;
        ystretch1000 = ((rd.bottom - rd.top)* 1000)/image_height;
        //handle move outside of window with cropping
        rd_window = rd;         //don't crop the window !!!
        if(rd.left < 0)         //move out left
		{
           rs.left=(-rd.left*1000)/xstretch1000;
           rd.left = 0; 
		}
        else rs.left=0;
        if(rd.top < 0)          //move out up
		{
	       rs.top=(-rd.top*1000)/ystretch1000;
	       rd.top = 0;
		}
        else rs.top = 0;
        if(rd.right > xscreen)  //move out right
		{
	       rs.right=((xscreen-rd.left)*1000)/xstretch1000;
	       rd.right= xscreen;
		}
        else rs.right = image_width;
        if(rd.bottom > yscreen) //move out down
		{
	       rs.bottom=((yscreen-rd.top)*1000)/ystretch1000;
	       rd.bottom= yscreen;
		}
        else rs.bottom= image_height;
	}
	else
	{
		rd_window = Directx_Fullscreencalc();
	}
    // check alignment restrictions
    // these expressions (x & -y) just do alignment by dropping low order bits...
    // so to round up, we add first, then truncate.
    if ((capsDrv.dwCaps & DDCAPS_ALIGNBOUNDARYSRC) && capsDrv.dwAlignBoundarySrc)
        rs.left = (rs.left + capsDrv.dwAlignBoundarySrc / 2) & -(signed)(capsDrv.dwAlignBoundarySrc);
    if ((capsDrv.dwCaps & DDCAPS_ALIGNSIZESRC) && capsDrv.dwAlignSizeSrc)
        rs.right = rs.left + (rs.right - rs.left + capsDrv.dwAlignSizeSrc / 2) & -(signed) (capsDrv.dwAlignSizeSrc);
    if ((capsDrv.dwCaps & DDCAPS_ALIGNBOUNDARYDEST) && capsDrv.dwAlignBoundaryDest)
	{
        rd.left = (rd.left + capsDrv.dwAlignBoundaryDest / 2) & -(signed)(capsDrv.dwAlignBoundaryDest);
	    if(!fs)rd_window.left = (rd_window.left + capsDrv.dwAlignBoundaryDest / 2) & -(signed)(capsDrv.dwAlignBoundaryDest); //don't forget the window
	}
    if ((capsDrv.dwCaps & DDCAPS_ALIGNSIZEDEST) && capsDrv.dwAlignSizeDest)
	{
        rd.right = rd.left + (rd.right - rd.left) & -(signed) (capsDrv.dwAlignSizeDest);
	    if(!fs)rd_window.right = rd_window.left + (rd_window.right - rd_window.left) & -(signed) (capsDrv.dwAlignSizeDest); //don't forget the window
	}
    if(!fs)AdjustWindowRect(&rd_window,WS_OVERLAPPEDWINDOW|WS_SIZEBOX,0); //calculate window rect

    //printf("Window:x:%i,y:%i,w:%i,h:%i\n",rd_window.left,rd_window.top,rd_window.right - rd_window.left,rd_window.bottom - rd_window.top);
    //printf("Overlay:x1:%i,y1:%i,x2:%i,y2:%i,w:%i,h:%i\n",rd.left,rd.top,rd.right,rd.bottom,rd.right - rd.left,rd.bottom - rd.top);
    //printf("Source:x1:%i,x2:%i,y1:%i,y2:%i\n",rs.left,rs.right,rs.top,rs.bottom);
    //printf("Image:x:%i->%i,y:%i->%i\n",image_width,d_image_width,image_height,d_image_height);

    // create an overlay FX structure so we can specify a destination color key
    ZeroMemory(&ovfx, sizeof(ovfx));
    ovfx.dwSize = sizeof(ovfx);
    ovfx.dckDestColorkey.dwColorSpaceLowValue = destcolorkey; 
    ovfx.dckDestColorkey.dwColorSpaceHighValue = destcolorkey;
    // set the flags we'll send to UpdateOverlay      //DDOVER_AUTOFLIP|DDOVERFX_MIRRORLEFTRIGHT|DDOVERFX_MIRRORUPDOWN could be usefull?;
    dwUpdateFlags = DDOVER_SHOW | DDOVER_DDFX;
    if (capsDrv.dwCKeyCaps & DDCKEYCAPS_DESTOVERLAY) dwUpdateFlags |= DDOVER_KEYDESTOVERRIDE;
    else ontop = 1;  //if hardware can't do colorkeying set the window on top
	//now we have enough information to display the window 
	if((fs) || (!fs && ontop))hWndafter=HWND_TOPMOST;
	else hWndafter=HWND_NOTOPMOST;
	SetWindowPos(hWnd,
                 hWndafter,
                 rd_window.left,
                 rd_window.top,
                 rd_window.right - rd_window.left,
                 rd_window.bottom - rd_window.top,
                 SWP_SHOWWINDOW|SWP_NOOWNERZORDER/*|SWP_NOREDRAW*/);

	ddrval = g_lpddsOverlay->lpVtbl->UpdateOverlay(g_lpddsOverlay,&rs, g_lpddsPrimary, &rd, dwUpdateFlags, &ovfx);
    if(FAILED(ddrval))
    {
        // on cause might be the driver lied about minimum stretch 
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
		}
		//hide overlay
		if(ddrval != DD_OK)
		{
			ddrval = g_lpddsOverlay->lpVtbl->UpdateOverlay(g_lpddsOverlay,NULL, g_lpddsPrimary, NULL, DDOVER_HIDE, NULL);
            return 1;
		}
	}
    return 0;
}

static uint32_t Directx_DisplayNonOverlay()
{
	WINDOWPLACEMENT window_placement;
	RECT            rd_window;  //rect of the window
    HWND            hWndafter;
	POINT point_window;
    window_placement.length = sizeof(WINDOWPLACEMENT);
    GetWindowPlacement(hWnd, &window_placement );
	
	if(!fs) //windowed
    {
        if (window_placement.showCmd == SW_SHOWMAXIMIZED)   //is our window maximized?
		{
		   CloseWindow(hWnd);  //we do this to prevent the screen from being filled with garbage
		   window_placement.showCmd = SW_SHOWNORMAL;       //restore 		   
		   SetWindowLong(hWnd,GWL_STYLE , WS_OVERLAPPEDWINDOW| WS_SIZEBOX );       //change style
		   SetWindowPlacement( hWnd, &window_placement );
		}
    }
    else       //fullscreen
    {
	    if (window_placement.showCmd == SW_SHOWNORMAL)    //is our window normal?
		{
		   window_placement.showCmd = SW_SHOWMAXIMIZED;   //maximize
		   SetWindowLong( hWnd, GWL_STYLE, 0 );           //change Style
		   SetWindowPlacement( hWnd, &window_placement );
		}
    }
	
    if(!fs)  //windowed
	{
		GetClientRect (hWnd, &rd_window);
		if(rd_window.top == rd_window.bottom)return 0;   //window is minimized
		if (GetWindowTextLength(hWnd)==0)
		{
			SetWindowText(hWnd,"MPlayer - DirectX NoN-Overlay"); //for the first call: change only title, not d_image stuff 
		}
        else
		{
            d_image_width = rd_window.right - rd_window.left; //Calculate new Imagedimensions
            d_image_height = rd_window.bottom - rd_window.top;
		}
        point_window.x = 0;                          //imageposition relative to the window
        point_window.y = 0;
        ClientToScreen(hWnd,&point_window);  
        rd.left = point_window.x;                    //fill the wanted x1&y1 of the image
        rd.top = point_window.y;
        rd.right = d_image_width+rd.left;
        rd.bottom =d_image_height+rd.top;
        rd_window = rd;         
        AdjustWindowRect(&rd_window,WS_OVERLAPPEDWINDOW|WS_SIZEBOX,0);
	}
    else    //fullscreen
	{
		rd_window=Directx_Fullscreencalc();
	}
    if((fs) || (!fs && ontop))hWndafter=HWND_TOPMOST;
	else hWndafter=HWND_NOTOPMOST;
	SetWindowPos(hWnd,
                 hWndafter,           
                 rd_window.left,
                 rd_window.top,
                 rd_window.right - rd_window.left,
                 rd_window.bottom - rd_window.top,
                 SWP_SHOWWINDOW|SWP_NOOWNERZORDER);
	return 0;
}

//find out supported overlay pixelformats
static uint32_t Directx_CheckOverlayPixelformats()
{
    DDCAPS          capsDrv;
    HRESULT         ddrval;
    DDSURFACEDESC   ddsdOverlay;
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
	DDSURFACEDESC   ddsd;
    HDC             hdc;
    HRESULT         hres;
	COLORREF        rgbT;
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
			   g_ddpf[i].drv_caps = VFCAP_CSP_SUPPORTED |VFCAP_OSD |VFCAP_CSP_SUPPORTED_BY_HW;
			   formatcount++;
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
		case WM_DESTROY:
		{
			PostQuitMessage(0);
			return 0;
		}
        case WM_CLOSE:
		{
			mplayer_put_key('q');
			return 0;
		}
        case WM_WINDOWPOSCHANGED:
		{
			//printf("Windowposchange\n");
			if(g_lpddsBack != NULL)  //or it will crash with -vm
			{
				if(nooverlay)Directx_DisplayNonOverlay();
		        else Directx_DisplayOverlay();
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
			}
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
		        case VK_CONTROL:
					{mplayer_put_key(KEY_CTRL);break;}
		        case VK_DELETE:
					{mplayer_put_key(KEY_DELETE);break;}
		        case VK_INSERT:
					{mplayer_put_key(KEY_INSERT);break;}
		        case VK_HOME:
					{mplayer_put_key(KEY_HOME);break;}
		        case VK_END:
					{mplayer_put_key(VK_END);break;}
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
		
    }
	return DefWindowProc(hWnd, message, wParam, lParam);
}

static uint32_t Directx_CreateWindow()
{
    HINSTANCE hInstance = GetModuleHandle(NULL);
    WNDCLASS   wc;
    wc.style         =  CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   =  WndProc;
    wc.cbClsExtra    =  0;
    wc.cbWndExtra    =  0;
    wc.hInstance     =  hInstance;
    wc.hCursor       =  LoadCursor(NULL,IDC_ARROW);
    wc.hIcon         =  LoadIcon(NULL,IDI_APPLICATION);
    wc.hbrBackground =  CreateSolidBrush(windowcolor);
    wc.lpszClassName =  "Mplayer - Movieplayer for Linux";
    wc.lpszMenuName  =  NULL;
    RegisterClass(&wc);
    hWnd = CreateWindow("MPlayer - Movieplayer for Linux",
                        "",
                        WS_OVERLAPPEDWINDOW| WS_SIZEBOX,
                        CW_USEDEFAULT,                   //position x 
                        CW_USEDEFAULT,                   //position y
                        100,                             //width
                        100,                             //height 
                        NULL,
                        NULL,
                        hInstance,
                        NULL);
    mp_msg(MSGT_VO, MSGL_DBG3 ,"<vo_directx><INFO>initial mplayer window created\n");
	return 0;
}

static uint32_t preinit(const char *arg)
{
	if(arg)
	{
		if(!strcmp(arg,"noaccel"))
		{
			mp_msg(MSGT_VO,MSGL_V,"<vo_directx><INFO>disabled overlay\n");
		    nooverlay = 1;
		}
        else
		{
			mp_msg(MSGT_VO,MSGL_ERR,"<vo_directx><ERROR>unknown subdevice: %s\n",arg);
	        return ENOSYS;
		}
	}
	if (Directx_InitDirectDraw()!= 0)return 1;          //init DirectDraw
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
	Directx_CreateWindow(); //create initial window
	mp_msg(MSGT_VO, MSGL_DBG3 ,"<vo_directx><INFO>preinit succesfully finished\n");
	return 0;
}

static uint32_t draw_slice(uint8_t *src[], int stride[], int w,int h,int x,int y )
{
	uint8_t *s;
    uint8_t *d;
    uint32_t i=0;
    
	// copy Y
    d=image+dstride*y+x;                
    s=src[0];                           
    for(i=0;i<h;i++){
        memcpy(d,s,w);                  
        s+=stride[0];                  
        d+=stride[0];
    }
    
	w/=2;h/=2;x/=2;y/=2;
	
	// copy U
    d=image+image_width*image_height + dstride*y/2+x;
    if(swap)s=src[2];
	else s=src[1];
    for(i=0;i<h;i++){
        memcpy(d,s,w);
        s+=stride[1];
        d+=stride[1];
    }
	
	// copy V
    d=image+image_width*image_height +image_width*image_height/4 + dstride*y/2+x;
    if(swap)s=src[1];
	else s=src[2];
    for(i=0;i<h;i++){
        memcpy(d,s,w);
        s+=stride[2];
        d+=stride[2];
    }
    return 0;
}

static void flip_page(void)
{
   	HRESULT dxresult;
	//bufferflipping is slow on my system?
	if (vo_doublebuffering) 
    {
		// flip to the next image in the sequence  
		dxresult = g_lpddsOverlay->lpVtbl->Flip( g_lpddsOverlay,NULL, DDFLIP_WAIT);
		if(dxresult == DDERR_SURFACELOST)
		{
			mp_msg(MSGT_VO, MSGL_ERR,"<vo_directx><ERROR><vo_directx><INFO>Restoring Surface\n");
			g_lpddsBack->lpVtbl->Restore( g_lpddsBack );
		    dxresult = g_lpddsOverlay->lpVtbl->Flip( g_lpddsOverlay,NULL, DDFLIP_WAIT);
		}
		if(dxresult != DD_OK)mp_msg(MSGT_VO, MSGL_ERR,"<vo_directx><ERROR>can't flip page\n");
    }
	g_lpddsBack->lpVtbl->Lock(g_lpddsBack,NULL,&ddsdsf, DDLOCK_NOSYSLOCK | DDLOCK_WAIT , NULL);
	g_lpddsBack->lpVtbl->Unlock (g_lpddsBack,NULL);
	dstride = ddsdsf.lPitch;
    image = ddsdsf.lpSurface;
	if(nooverlay == 1) 
	{
		DDBLTFX  ddbltfx;
        // ask for the "NOTEARING" option
	    memset( &ddbltfx, 0, sizeof(DDBLTFX) );
        ddbltfx.dwSize = sizeof(DDBLTFX);
        ddbltfx.dwDDFX = DDBLTFX_NOTEARING;
        g_lpddsPrimary->lpVtbl->Blt(g_lpddsPrimary, &rd, g_lpddsBack, NULL, DDBLT_WAIT, &ddbltfx);
    }	
}

static uint32_t draw_frame(uint8_t *src[])
{
  	memcpy( image, *src, dstride * image_height );
	return 0;
}

static uint32_t get_image(mp_image_t *mpi)
{
    //dr is untested for planar formats
	//we should make a surface in mem and use busmastering!
    if(mpi->flags&MP_IMGFLAG_READABLE) {mp_msg(MSGT_VO, MSGL_V,"<vo_directx><ERROR>slow video ram\n");return VO_FALSE;} 
    if(mpi->type==MP_IMGTYPE_STATIC) {mp_msg(MSGT_VO, MSGL_V,"<vo_directx><ERROR>not static\n");return VO_FALSE;}
    if((mpi->width==dstride) || (mpi->flags&(MP_IMGFLAG_ACCEPT_STRIDE|MP_IMGFLAG_ACCEPT_WIDTH)))
	{
		if(mpi->flags&MP_IMGFLAG_PLANAR && image_format==IMGFMT_YV12)
		{
			mpi->planes[0]=image;
	        if(swap)
			{
				mpi->planes[2]= image + dstride*image_height;
	            mpi->planes[1]= image + dstride*image_height+ dstride*image_height/4;
			}
			else
			{
				mpi->planes[1]= image + dstride*image_height;
	            mpi->planes[2]= image + dstride*image_height+ dstride*image_height/4;
			}
   	        mpi->width=mpi->stride[0]=dstride;
	        mpi->stride[1]=mpi->stride[2]=dstride/2;
		}
    	else //packed
		{
			mpi->planes[0]=image;
	        mpi->width=image_width;
	        mpi->stride[0]=dstride;
		}
        mpi->flags|=MP_IMGFLAG_DIRECT;
        mp_msg(MSGT_VO, MSGL_DBG3, "<vo_directx><INFO>Direct Rendering ENABLED\n");
        return VO_TRUE;
    } 
    return VO_FALSE;
}
  
static uint32_t put_image(mp_image_t *mpi){

    uint32_t  i = 0;
    uint8_t   *d;
	uint8_t   *s;
    uint32_t x = mpi->x;
	uint32_t y = mpi->y;
	uint32_t w = mpi->w;
	uint32_t h = mpi->h;
   
    if((mpi->flags&(MP_IMGFLAG_DIRECT|MP_IMGFLAG_DRAW_CALLBACK))) 
	{
		mp_msg(MSGT_VO, MSGL_DBG3 ,"<vo_directx><INFO>put_image: nothing to do: drawslices\n");
		return VO_TRUE;
    }
   
    if (mpi->flags&MP_IMGFLAG_PLANAR)
	{
		if (image_format == IMGFMT_YV12|| image_format == IMGFMT_I420 || image_format == IMGFMT_IYUV)
		{
			draw_slice(mpi->planes, mpi->stride, mpi->w,mpi->h,mpi->x,mpi->y );
		}
		else if(image_format==IMGFMT_YVU9)
		{
			// copy Y
            d=image+dstride*y+x;
            s=mpi->planes[0];
            for(i=0;i<h;i++){
			  memcpy(d,s,w);
              s+=mpi->stride[0];
              d+=dstride;
			}
            w/=4;h/=4;x/=4;y/=4; dstride/=4;
    	    // copy V
            d=image+image_width*image_height + dstride*y+x;
            if(swap)s=mpi->planes[2];
	        else s=mpi->planes[1];
		    for(i=0;i<h;i++){
			  memcpy(d,s,w);
              s+=mpi->stride[1];
              d+=dstride;
			}
  	        // copy U
            d=image+image_width*image_height +image_width*image_height/16 + dstride*y+x;
            if(swap)s=mpi->planes[1];
		    else s=mpi->planes[2];
            for(i=0;i<h;i++){
			  memcpy(d,s,w);
              s+=mpi->stride[2];
              d+=dstride;
			}
		}
	}
	else //packed
	{
        memcpy( image, mpi->planes[0], image_height * dstride);
	}
	return VO_TRUE;
}

static uint32_t
config(uint32_t width, uint32_t height, uint32_t d_width, uint32_t d_height, uint32_t options, char *title, uint32_t format)
{
	//int zoom = options & 0x04;
	//int flip = options & 0x08;
	fs = options & 0x01;
	vm = options & 0x02;
	if(vm)fs=1;
	image_format =  format;
	d_image_width = d_width;
	d_image_height = d_height;
	image_width = width;
	image_height = height;
	//printf("<vo_directx><INFO>config entered\n");
	//printf("width:%i\nheight:%i\nd_width:%i\nd_height%i\n",width,height,d_width,d_height);
    SetWindowText(hWnd,"");
	if (g_lpddsBack != NULL) g_lpddsBack->lpVtbl->Release(g_lpddsBack);
	g_lpddsBack = NULL;
	if(vo_doublebuffering)
	{
		if (g_lpddsOverlay != NULL)g_lpddsOverlay->lpVtbl->Release(g_lpddsOverlay);
	}
    g_lpddsOverlay = NULL;
	if (g_lpddsPrimary != NULL) g_lpddsPrimary->lpVtbl->Release(g_lpddsPrimary);
    g_lpddsPrimary = NULL;
	mp_msg(MSGT_VO, MSGL_DBG3,"<vo_directx><INFO>overlay surfaces released\n");
	if(vm)
	{      //exclusive mode
	  if (g_lpdd->lpVtbl->SetCooperativeLevel(g_lpdd, hWnd, DDSCL_EXCLUSIVE|DDSCL_FULLSCREEN) != DD_OK)
	  {
        mp_msg(MSGT_VO, MSGL_FATAL,"<vo_directx><FATAL ERROR>can't set cooperativelevel for exclusive mode");
        return 1;
	  }                                     //ddobject,width,height,bpp,refreshrate,aditionalflags
      SetWindowLong( hWnd, GWL_STYLE, 0 );  //changeWindowStyle
	  if(g_lpdd->lpVtbl->SetDisplayMode(g_lpdd,640, 480, 16,0,0) != DD_OK)   
	  {	   
	    mp_msg(MSGT_VO, MSGL_FATAL,"<vo_directx><FATAL ERROR>can't set displaymode\n");
	    return 1;
	  }
	  mp_msg(MSGT_VO, MSGL_V,"<vo_directx><INFO>cooperativelevel for exclusive mode set\n");
	}
	else
	{
	  if (g_lpdd->lpVtbl->SetCooperativeLevel(g_lpdd, hWnd, DDSCL_NORMAL) != DD_OK)
	  {
        mp_msg(MSGT_VO, MSGL_FATAL,"<vo_directx><FATAL ERROR>can't set cooperativelevel for windowed mode");
        return 1;
	  }
	  SetWindowLong( hWnd,WS_OVERLAPPEDWINDOW| WS_SIZEBOX , 0 ); 
	  mp_msg(MSGT_VO, MSGL_V,"<vo_directx><INFO>cooperativelevel for windowed mode set\n");
	} 
    if(Directx_CreatePrimarySurface()!=0)return 1;
	if (nooverlay==0)
	{
		if(Directx_CreateOverlay(image_format)==0)
		{
			Directx_DisplayOverlay();
		    if(!vo_doublebuffering) g_lpddsBack = g_lpddsOverlay;
		}
	    else
		{	
			nooverlay=1; //if failed try with nooverlay
			mp_msg(MSGT_VO, MSGL_ERR,"<vo_directx><ERROR>can't use overlay mode\n");

		}
	}
	if (nooverlay==1)
	{
		if(Directx_CreateBackpuffer()!=0)return 1;
		Directx_DisplayNonOverlay();
	}

	if(Directx_CreateClipper() != 0)return 1;
	memset(&ddsdsf, 0,sizeof(DDSURFACEDESC));
	ddsdsf.dwSize = sizeof (DDSURFACEDESC);
	g_lpddsBack->lpVtbl->Lock(g_lpddsBack,NULL,&ddsdsf, DDLOCK_NOSYSLOCK | DDLOCK_WAIT, NULL);
	g_lpddsBack->lpVtbl->Unlock (g_lpddsBack,NULL);
	dstride = ddsdsf.lPitch;
    image = ddsdsf.lpSurface;
	if(image_format==IMGFMT_I420||image_format==IMGFMT_IYUV)
	{
		if(swap)swap=0;  //swap uv
		else swap=1;
	}
	return 0;
}

static uint32_t control(uint32_t request, void *data, ...)
{
    switch (request) {
   
	case VOCTRL_GET_IMAGE:
      	return get_image(data);
    case VOCTRL_QUERY_FORMAT:
        return query_format(*((uint32_t*)data));
	case VOCTRL_DRAW_IMAGE:
        return put_image(data);
    case VOCTRL_FULLSCREEN:
		{
			//printf("Display mode switch\n");
	        if(vm)
			{
				mp_msg(MSGT_VO, MSGL_ERR,"<vo_directx><ERROR>currently we do not allow to switch from exclusive to windowed mode\n");
			}
	        else
			{
				if(fs)fs=0;  //set to fullscreen or windowed
		        else fs = 1;
		        if(nooverlay == 0)Directx_DisplayOverlay(); //update window
		        else Directx_DisplayNonOverlay();
			}
		    return VO_TRUE;
		}
    };
    return VO_NOTIMPL;
}
