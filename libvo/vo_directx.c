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
#include <libavutil/common.h>
#include "config.h"
#include "video_out.h"
#include "video_out_internal.h"
#include "fastmemcpy.h"
#include "mp_msg.h"
#include "aspect.h"
#include "sub/sub.h"
#include "w32_common.h"

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
static HBRUSH               colorbrush = NULL;      // Handle to colorkey brush
static HBRUSH               blackbrush = NULL;      // Handle to black brush
static uint32_t image_width, image_height;          //image width and height
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

/*****************************************************************************
 * DirectDraw GUIDs.
 * Defining them here allows us to get rid of the dxguid library during
 * the linking stage.
 *****************************************************************************/
#define IID_IDirectDraw7 MP_IID_IDirectDraw7
static const GUID MP_IID_IDirectDraw7 =
{
	0x15e65ec0,0x3b9c,0x11d2,{0xb9,0x2f,0x00,0x60,0x97,0x97,0xea,0x5b}
};

#define IID_IDirectDrawColorControl MP_IID_IDirectDrawColorControl
static const GUID MP_IID_IDirectDrawColorControl =
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

static uint32_t Directx_CreatePrimarySurface(void)
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
	while ( i < NUM_FORMATS && imgfmt != g_ddpf[i].img_format)
	{
		i++;
	}
	if (!g_lpdd || !g_lpddsPrimary || i == NUM_FORMATS)
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
             mp_msg(MSGT_VO, MSGL_ERR,"create surface failed with 0x%xu\n",(unsigned)ddrval);
	   }
	   return 1;
	}
    g_lpddsBack = g_lpddsOverlay;
	return 0;
}

static uint32_t Directx_CreateBackpuffer(void)
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
	vo_w32_uninit();
}

static BOOL WINAPI EnumCallbackEx(GUID FAR *lpGUID, LPSTR lpDriverDescription, LPSTR lpDriverName, LPVOID lpContext, HMONITOR  hm)
{
    if (!lpGUID)
        lpDriverDescription = "Primary Display Adapter";
    mp_msg(MSGT_VO, MSGL_INFO ,"<vo_directx> adapter %d: %s", adapter_count, lpDriverDescription);

    if(adapter_count == vo_adapter_num){
        if (!lpGUID)
            selected_guid_ptr = NULL;
        else
        {
            selected_guid = *lpGUID;
            selected_guid_ptr = &selected_guid;
        }

        mp_msg(MSGT_VO, MSGL_INFO ,"\t\t<--");
    }
    mp_msg(MSGT_VO, MSGL_INFO ,"\n");

    adapter_count++;

    return 1; // list all adapters
}

static uint32_t Directx_InitDirectDraw(void)
{
	HRESULT    (WINAPI *OurDirectDrawCreateEx)(GUID *,LPVOID *, REFIID,IUnknown FAR *);
	DDSURFACEDESC2 ddsd;
	LPDIRECTDRAWENUMERATEEX OurDirectDrawEnumerateEx;

	adapter_count = 0;

	mp_msg(MSGT_VO, MSGL_DBG3,"<vo_directx><INFO>Initing DirectDraw\n" );

	//load direct draw DLL: based on videolans code
	hddraw_dll = LoadLibrary("DDRAW.DLL");
	if( hddraw_dll == NULL )
    {
        mp_msg(MSGT_VO, MSGL_FATAL,"<vo_directx><FATAL ERROR>failed loading ddraw.dll\n" );
		return 1;
    }

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
		if (g_lpdd->lpVtbl->SetCooperativeLevel(g_lpdd, vo_w32_window, DDSCL_EXCLUSIVE|DDSCL_FULLSCREEN) != DD_OK)
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
	if (g_lpdd->lpVtbl->SetCooperativeLevel(g_lpdd, vo_w32_window, DDSCL_NORMAL) != DD_OK) // or DDSCL_SETFOCUSWINDOW
     {
        mp_msg(MSGT_VO, MSGL_FATAL,"<vo_directx><FATAL ERROR>could not set cooperativelevel for hardwarecheck\n");
		return 1;
    }
    mp_msg(MSGT_VO, MSGL_DBG3,"<vo_directx><INFO>DirectDraw Initialized\n");
	return 0;
}

static uint32_t Directx_ManageDisplay(void)
{
    HRESULT         ddrval;
    DDCAPS          capsDrv;
    DDOVERLAYFX     ovfx;
    DWORD           dwUpdateFlags=0;
    int width,height;

    rd.left = vo_dx - xinerama_x;
    rd.top  = vo_dy - xinerama_y;
    width  = vo_dwidth;
    height = vo_dheight;

    aspect(&width, &height, A_WINZOOM);
    panscan_calc_windowed();
    width  += vo_panscan_x;
    height += vo_panscan_y;
    width  = FFMIN(width,  vo_screenwidth);
    height = FFMIN(height, vo_screenheight);
    rd.left += (vo_dwidth  - width ) / 2;
    rd.top  += (vo_dheight - height) / 2;

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
        g_lpddclipper->lpVtbl->SetHWnd(g_lpddclipper, 0, vo_w32_window);
    }

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
	  	mp_msg(MSGT_VO, MSGL_ERR ,"<vo_directx><ERROR>Overlay:x1:%li,y1:%li,x2:%li,y2:%li,w:%li,h:%li\n",rd.left,rd.top,rd.right,rd.bottom,rd.right - rd.left,rd.bottom - rd.top );
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
                mp_msg(MSGT_VO, MSGL_ERR ," 0x%xu\n",(unsigned)ddrval);
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

static void check_events(void)
{
    int evt = vo_w32_check_events();
    if (evt & (VO_EVENT_RESIZE | VO_EVENT_MOVE))
        Directx_ManageDisplay();
    if (evt & (VO_EVENT_RESIZE | VO_EVENT_MOVE | VO_EVENT_EXPOSE)) {
        HDC dc = vo_w32_get_dc(vo_w32_window);
        RECT r;
        GetClientRect(vo_w32_window, &r);
        FillRect(dc, &r, vo_fs || vidmode ? blackbrush : colorbrush);
        vo_w32_release_dc(vo_w32_window, dc);
    }
}

//find out supported overlay pixelformats
static uint32_t Directx_CheckOverlayPixelformats(void)
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
static uint32_t Directx_CheckPrimaryPixelformat(void)
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

static int preinit(const char *arg)
{
	if(arg)
	{
		if(strstr(arg,"noaccel"))
		{
			mp_msg(MSGT_VO,MSGL_V,"<vo_directx><INFO>disabled overlay\n");
		    nooverlay = 1;
		}
	}

    windowcolor = vo_colorkey;
    colorbrush = CreateSolidBrush(windowcolor);
    blackbrush = (HBRUSH)GetStockObject(BLACK_BRUSH);
	if (!vo_w32_init())
	    return 1;
	if (!vo_w32_config(100, 100, VOFLAG_HIDDEN))
	    return 1;

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
    uint32_t x = 0;
	uint32_t y = 0;
	uint32_t w = mpi->w;
	uint32_t h = mpi->h;

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
	image_format =  format;
	image_width = width;
	image_height = height;
    if(format != primary_image_format)nooverlay = 0;

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

	if (!vo_w32_config(d_width, d_height, options))
		return 1;

	if (WinID == -1)
	SetWindowText(vo_w32_window,title);


	/*create the surfaces*/
    if(Directx_CreatePrimarySurface())return 1;

	//create palette for 256 color mode
	if(image_format==IMGFMT_BGR8){
		LPDIRECTDRAWPALETTE ddpalette=NULL;
		LPPALETTEENTRY palette=calloc(256, sizeof(*palette));
		int i;
		for(i=0; i<256; i++){
			palette[i].peRed   = ((i >> 5) & 0x07) * 255 / 7;
			palette[i].peGreen = ((i >> 2) & 0x07) * 255 / 7;
			palette[i].peBlue  = ((i >> 0) & 0x03) * 255 / 3;
			palette[i].peFlags = PC_NOCOLLAPSE;
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
        if(g_lpddclipper->lpVtbl->SetHWnd (g_lpddclipper, 0, vo_w32_window)!= DD_OK){mp_msg(MSGT_VO, MSGL_FATAL,"<vo_directx><FATAL ERROR>can't associate clipper with window\n");return 1;}
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
static uint32_t color_ctrl_set(const char *what, int value)
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
static uint32_t color_ctrl_get(const char *what, int *value)
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

static int control(uint32_t request, void *data)
{
    switch (request) {

	case VOCTRL_GET_IMAGE:
      	return get_image(data);
    case VOCTRL_QUERY_FORMAT:
        return query_format(*((uint32_t*)data));
	case VOCTRL_DRAW_IMAGE:
        return put_image(data);
    case VOCTRL_BORDER:
        vo_w32_border();
        Directx_ManageDisplay();
		return VO_TRUE;
    case VOCTRL_ONTOP:
        vo_w32_ontop();
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
				Directx_ManageDisplay();
			}
		return VO_TRUE;
    case VOCTRL_FULLSCREEN:
		{
            vo_w32_fullscreen();
            Directx_ManageDisplay();
		    return VO_TRUE;
		}
	case VOCTRL_SET_EQUALIZER: {
		struct voctrl_set_equalizer_args *args = data;
		return color_ctrl_set(args->name, args->value);
	}
	case VOCTRL_GET_EQUALIZER: {
		struct voctrl_get_equalizer_args *args = data;
		return color_ctrl_get(args->name, args->valueptr);
	}
    case VOCTRL_UPDATE_SCREENINFO:
        w32_update_xinerama_info();
        return VO_TRUE;
    };
    return VO_NOTIMPL;
}
