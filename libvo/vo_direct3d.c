/*
 * Copyright (c) 2008 Georgi Petrov (gogothebee) <gogothebee@gmail.com>
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
#include <errno.h>
#include <stdio.h>
#include <d3d9.h>
#include "config.h"
#include "video_out.h"
#include "video_out_internal.h"
#include "fastmemcpy.h"
#include "mp_msg.h"
#include "aspect.h"
#include "w32_common.h"

static const vo_info_t info =
{
    "Direct3D 9 Renderer",
    "direct3d",
    "Georgi Petrov (gogothebee) <gogothebee@gmail.com>",
    ""
};

/*
 * Link essential libvo functions: preinit, config, control, draw_frame,
 * draw_slice, draw_osd, flip_page, check_events, uninit and
 * the structure info.
 */
const LIBVO_EXTERN(direct3d)


/* Global variables. Each one starts with "g". Pointers include "p".
 * I try to keep their count low.
 */

static int gIsPaused;               /**< 1 = Movie is paused,
                                  0 = Movie is not paused */
static int gIsD3DConfigFinished;    /**< Synchronization "semaphore". 1 when
                                  instance of D3DConfigure is finished */
static int gIsPanscan;              /**< 1= Panscan enabled, 0 = Panscan disabled */
static RECT gFullScrMovieRect;      /**< Rect (upscaled) of the movie when displayed
                                  in fullscreen */
static RECT gPanScanSrcRect;        /**< PanScan source surface cropping in
                                  fullscreen */
static int gSrcWidth;               /**< Source (movie) width */
static int gSrcHeight;              /**< Source (movie) heigth */
static LPDIRECT3D9 gpD3DHandle;         /**< Direct3D Handle */
static LPDIRECT3DDEVICE9  gpD3DDevice;  /**< The Direct3D Adapter */
static IDirect3DSurface9 *gpD3DSurface; /**< Offscreen Direct3D Surface. MPlayer
                                      renders inside it. Uses colorspace
                                      MovieSrcFmt */
static IDirect3DSurface9 *gpD3DBackBuf; /**< Video card's back buffer (used to
                                      display next frame) */
static D3DFORMAT gMovieSrcFmt;          /**< Movie colorspace format (depends on
                                      the movie's codec) */
static D3DFORMAT gDesktopFmt;           /**< Desktop (screen) colorspace format.
                                      Usually XRGB */
typedef struct
{
    const unsigned int  MPlayerFormat; /**< Given by MPlayer */
    const D3DFORMAT     FourCC;        /**< Required by D3D's test function */
} DisplayFormatTable;

/* Map table from reported MPlayer format to the required
   FourCC. This is needed to perform the format query. */

static const DisplayFormatTable gDisplayFormatTable[] =
{
    {IMGFMT_YV12, MAKEFOURCC('Y','V','1','2')},
    {IMGFMT_I420, MAKEFOURCC('I','4','2','0')},
    {IMGFMT_IYUV, MAKEFOURCC('I','Y','U','V')},
    {IMGFMT_YVU9, MAKEFOURCC('Y','V','U','9')},
    {IMGFMT_YUY2, MAKEFOURCC('Y','U','Y','2')},
    {IMGFMT_UYVY, MAKEFOURCC('U','Y','V','Y')}
};

#define DISPLAY_FORMAT_TABLE_ENTRIES \
        (sizeof(gDisplayFormatTable) / sizeof(gDisplayFormatTable[0]))

/****************************************************************************
 *                                                                          *
 *                                                                          *
 *                                                                          *
 * Direct3D specific implementation functions                               *
 *                                                                          *
 *                                                                          *
 *                                                                          *
 ****************************************************************************/

/** @brief Calculate panscan source RECT in fullscreen.
 */
static void CalculatePanscanRect (void)
{
    int scaledHeight = 0;
    int scaledWidth = 0;
    int srcPanx;
    int srcPany;

    mp_msg(MSGT_VO,MSGL_ERR,"<vo_direct3d>CalculatePanscanRect called\r\n");

    aspect(&scaledWidth, &scaledHeight, A_ZOOM);
    panscan_calc();

    if (vo_panscan_x != 0 || vo_panscan_y != 0)
    {
        gIsPanscan = 1;
        mp_msg(MSGT_VO,MSGL_V,
              "<vo_direct3d>Panscan destination correction: x: %d, y: %d\r\n",
              vo_panscan_x, vo_panscan_y);

        mp_msg(MSGT_VO,MSGL_V,
              "<vo_direct3d>vo_panscan_x %d, scaledWidth %d, vo_screenwidth %d\r\n",
              vo_panscan_x, scaledWidth, vo_screenwidth);

        mp_msg(MSGT_VO,MSGL_V,
              "<vo_direct3d>vo_panscan_y %d, scaledHeight %d, vo_screenheight %d\r\n",
              vo_panscan_y, scaledHeight, vo_screenheight);

        srcPanx = vo_panscan_x / (vo_screenwidth / scaledWidth);
        srcPany = vo_panscan_y / (vo_screenheight / scaledHeight);

        mp_msg(MSGT_VO,MSGL_V,
              "<vo_direct3d>Panscan source (needed) correction: x: %d, y: %d\r\n",
              srcPanx, srcPany);

        gPanScanSrcRect.left   = srcPanx / 2;
        if (gPanScanSrcRect.left % 2 != 0) gPanScanSrcRect.left++;
        gPanScanSrcRect.right  = gSrcWidth - (srcPanx / 2);
        if (gPanScanSrcRect.right % 2 != 0) gPanScanSrcRect.right--;
        gPanScanSrcRect.top    = srcPany / 2;
        if (gPanScanSrcRect.top % 2 != 0) gPanScanSrcRect.top++;
        gPanScanSrcRect.bottom = gSrcHeight - (srcPany / 2);
        if (gPanScanSrcRect.bottom % 2 != 0) gPanScanSrcRect.bottom--;

        mp_msg(MSGT_VO,MSGL_V,
        "<vo_direct3d>Panscan Source Rect: t: %ld, l: %ld, r: %ld, b:%ld\r\n",
        gPanScanSrcRect.top, gPanScanSrcRect.left,
        gPanScanSrcRect.right, gPanScanSrcRect.bottom);
    }
    else
        gIsPanscan = 0;
}

/** @brief Calculate scaled fullscreen movie rectangle with
 *  preserved aspect ratio.
 */
static void CalculateFullscreenRect (void)
{
    int scaledHeight = 0;
    int scaledWidth = 0;
    /* If we've created fullscreen context, we should calculate stretched
    * movie RECT, otherwise it will fill the whole fullscreen with
    * wrong aspect ratio */

    aspect(&scaledWidth, &scaledHeight, A_ZOOM);

    gFullScrMovieRect.left   = (vo_screenwidth - scaledWidth) / 2;
    gFullScrMovieRect.right  = gFullScrMovieRect.left + scaledWidth;
    gFullScrMovieRect.top    = (vo_screenheight - scaledHeight) / 2;
    gFullScrMovieRect.bottom = gFullScrMovieRect.top + scaledHeight;

    mp_msg(MSGT_VO,MSGL_V,
    "<vo_direct3d>Fullscreen Movie Rect: t: %ld, l: %ld, r: %ld, b:%ld\r\n",
        gFullScrMovieRect.top, gFullScrMovieRect.left,
        gFullScrMovieRect.right, gFullScrMovieRect.bottom);

    /*CalculatePanscanRect();*/
}

/** @brief Destroy D3D Context related to the current window.
 */
static void D3DDestroyContext (void)
{
    mp_msg(MSGT_VO,MSGL_V,"<vo_direct3d>D3DDestroyContext called\r\n");
    /* Let's destroy the old (if any) D3D Content */

    if (gpD3DSurface != NULL)
    {
        IDirect3DSurface9_Release (gpD3DSurface);
        gpD3DSurface = NULL;
    }

    if (gpD3DDevice != NULL)
    {
        IDirect3DDevice9_Release (gpD3DDevice);
        gpD3DDevice = NULL;
    }

    /* The following is not a memory leak. pD3DBackBuf is not malloc'ed
     * but just holds a pointer to the back buffer. Nobody gets hurt from
     * setting it to NULL.
     */
    gpD3DBackBuf = NULL;
}


/** @brief (Re)Initialize Direct3D. Kill and recreate context.
 *  The first function called to initialize D3D context.
 *  @return 1 on success, 0 on failure
 */
static int D3DConfigure (void)
{
    D3DPRESENT_PARAMETERS PresentParams;
    D3DDISPLAYMODE DisplayMode;

    mp_msg(MSGT_VO,MSGL_V,"<vo_direct3d><INFO>D3DConfigure CALLED\n");

    D3DDestroyContext();

    /* Get the current desktop display mode, so we can set up a back buffer
     * of the same format. */
    if (FAILED (IDirect3D9_GetAdapterDisplayMode (gpD3DHandle,
                                                  D3DADAPTER_DEFAULT,
                                                  &DisplayMode)))
    {
        mp_msg(MSGT_VO,MSGL_ERR,
               "<vo_direct3d><INFO>Could not read adapter display mode.\n");
        return 0;
    }

    /* Write current Desktop's colorspace format in the global storage. */
    gDesktopFmt = DisplayMode.Format;

    /* Prepare Direct3D initialization parameters. */
    memset(&PresentParams, 0, sizeof(D3DPRESENT_PARAMETERS));
    PresentParams.Windowed               = TRUE;
    PresentParams.SwapEffect             = D3DSWAPEFFECT_COPY;
    PresentParams.Flags                  = D3DPRESENTFLAG_VIDEO;
    PresentParams.hDeviceWindow          = vo_w32_window; /* w32_common var */
    PresentParams.BackBufferWidth        = 0; /* Fill up window Width */
    PresentParams.BackBufferHeight       = 0; /* Fill up window Height */
    PresentParams.MultiSampleType        = D3DMULTISAMPLE_NONE;
    /* D3DPRESENT_INTERVAL_ONE = vsync */
    PresentParams.PresentationInterval   = D3DPRESENT_INTERVAL_ONE;
    PresentParams.BackBufferFormat       = gDesktopFmt;
    PresentParams.BackBufferCount        = 1;
    PresentParams.EnableAutoDepthStencil = FALSE;

    /* vo_w32_window is w32_common variable. It's a handle to the window. */
    if (FAILED (IDirect3D9_CreateDevice(gpD3DHandle,
                                     D3DADAPTER_DEFAULT,
                                     D3DDEVTYPE_HAL, vo_w32_window,
                                     D3DCREATE_SOFTWARE_VERTEXPROCESSING,
                                     &PresentParams, &gpD3DDevice)))
    {
        mp_msg(MSGT_VO,MSGL_ERR,
               "<vo_direct3d><INFO>Could not create the D3D device\n");
        return 0;
    }

    mp_msg(MSGT_VO,MSGL_V,
      "New BackBuffer: Width: %d, Height:%d. VO Dest Width:%d, Height: %d\n",
          PresentParams.BackBufferWidth, PresentParams.BackBufferHeight,
          vo_dwidth, vo_dheight);

    if (FAILED (IDirect3DDevice9_CreateOffscreenPlainSurface(
         gpD3DDevice, gSrcWidth, gSrcHeight,
         gMovieSrcFmt, D3DPOOL_DEFAULT, &gpD3DSurface, NULL)))
    {
        mp_msg(MSGT_VO,MSGL_ERR,
        "<vo_direct3d><INFO>IDirect3D9_CreateOffscreenPlainSurface Failed.\n");
        return 0;
    }

    if (FAILED (IDirect3DDevice9_GetBackBuffer (gpD3DDevice, 0, 0,
                                                D3DBACKBUFFER_TYPE_MONO,
                                                &(gpD3DBackBuf))))
    {
        mp_msg(MSGT_VO,MSGL_ERR,"<vo_direct3d>Back Buffer address get failed\n");
        return 0;
    }

    /* Fill the Surface with black color. */
    IDirect3DDevice9_ColorFill(gpD3DDevice, gpD3DSurface, NULL,
                               D3DCOLOR_ARGB(0xFF, 0, 0, 0) );

    if (vo_fs == 1)
        CalculateFullscreenRect ();

    return 1;
}

/** @brief Uninitialize Direct3D and close the window.
 */
static void D3DUninit(void)
{
    mp_msg(MSGT_VO,MSGL_V,"<vo_direct3d>D3DUninit called\r\n");

    /* Block further calls to D3DConfigure(). */
    gIsD3DConfigFinished = 0;

    /* Destroy D3D Context inside the window. */
    D3DDestroyContext();

    /* Stop the whole D3D. */
    if (NULL != gpD3DHandle)
    {
        mp_msg(MSGT_VO,MSGL_V,"<vo_direct3d>Calling IDirect3D9_Release\r\n");
        IDirect3D9_Release (gpD3DHandle);
    }
}

/** @brief Render a frame on the screen.
 *  @param mpi mpi structure with the decoded frame inside
 *  @return VO_TRUE on success, VO_ERROR on failure
 */
static uint32_t D3DRenderFrame (mp_image_t *mpi)
{
    D3DLOCKED_RECT  stLockedRect;   /**< Offscreen surface we lock in order
                                         to copy MPlayer's frame inside it.*/

    /* Uncomment when direct rendering is implemented.
     * if (mpi->flags & MP_IMGFLAG_DIRECT) ...
     */

    if (mpi->flags & MP_IMGFLAG_DRAW_CALLBACK)
        return VO_TRUE;

    if (mpi->flags & MP_IMGFLAG_PLANAR)
    { /* Copy a planar frame. */
        draw_slice(mpi->planes,mpi->stride,mpi->w,mpi->h,0,0);
        return VO_TRUE;
    }

    /* If the previous if failed, we should draw a packed frame */
    if (FAILED (IDirect3DDevice9_BeginScene(gpD3DDevice)))
    {
       mp_msg(MSGT_VO,MSGL_ERR,"<vo_direct3d>BeginScene failed\n");
       return VO_ERROR;
    }

    if (FAILED (IDirect3DSurface9_LockRect(gpD3DSurface,
                                           &stLockedRect, NULL, 0)))
    {
       mp_msg(MSGT_VO,MSGL_ERR,"<vo_direct3d>Surface lock failure\n");
       return VO_ERROR;
    }

    memcpy_pic(stLockedRect.pBits, mpi->planes[0], mpi->stride[0],
               mpi->height, stLockedRect.Pitch, mpi->stride[0]);

    if (FAILED (IDirect3DSurface9_UnlockRect(gpD3DSurface)))
    {
        mp_msg(MSGT_VO,MSGL_V,"<vo_direct3d>Surface unlock failure\n");
        return VO_ERROR;
    }

    if (FAILED (IDirect3DDevice9_StretchRect (gpD3DDevice,
                                              gpD3DSurface,
                                              gIsPanscan == 1 ?
                                              &gPanScanSrcRect : NULL,
                                              gpD3DBackBuf,
                                              vo_fs == 1 ?
                                              &gFullScrMovieRect : NULL,
                                              D3DTEXF_LINEAR)))
    {
        mp_msg(MSGT_VO,MSGL_ERR,
               "<vo_direct3d>Unable to copy the frame to the back buffer\n");
        return VO_ERROR;
    }

    if (FAILED (IDirect3DDevice9_EndScene(gpD3DDevice)))
    {
       mp_msg(MSGT_VO,MSGL_ERR,"<vo_direct3d>EndScene failed\n");
       return VO_ERROR;
    }

    return VO_TRUE;
}


/** @brief Query if movie colorspace is supported by the HW.
 *  @return 0 on failure, device capabilities (not probed
 *          currently) on success.
 */
static int query_format (uint32_t MovieFormat)
{
    int i;
    for (i=0; i < DISPLAY_FORMAT_TABLE_ENTRIES; i++)
    {
        if (gDisplayFormatTable[i].MPlayerFormat == MovieFormat)
        {
            /* Test conversion from Movie colorspace to
             * display's target colorspace. */
            if (FAILED (IDirect3D9_CheckDeviceFormatConversion(
                                           gpD3DHandle,
                                           D3DADAPTER_DEFAULT,
                                           D3DDEVTYPE_HAL,
                                           gDisplayFormatTable[i].FourCC,
                                           gDesktopFmt)))
            return 0;

            gMovieSrcFmt = gDisplayFormatTable[i].FourCC;
            mp_msg(MSGT_VO,MSGL_V,"<vo_direct3d>Accepted Colorspace %s\n",
                   vo_format_name(gDisplayFormatTable[i].MPlayerFormat));
            return (VFCAP_CSP_SUPPORTED | VFCAP_CSP_SUPPORTED_BY_HW
                    /*| VFCAP_OSD*/ | VFCAP_HWSCALE_UP | VFCAP_HWSCALE_DOWN);

        }
    }

    return 0;
}

/****************************************************************************
 *                                                                          *
 *                                                                          *
 *                                                                          *
 * libvo Control / Callback functions                                       *
 *                                                                          *
 *                                                                          *
 *                                                                          *
 ****************************************************************************/




/** @brief libvo Callback: Preinitialize the video card.
 *  Preinit the hardware just enough to be queried about
 *  supported formats.
 *
 *  @return 0 on success, -1 on failure
 */
static int preinit(const char *arg)
{
    D3DDISPLAYMODE DisplayMode;
    /* Set to zero all global variables. */
    gIsPaused = 0;
    gIsD3DConfigFinished = 0;
    gIsPanscan = 0;
    gpD3DHandle = NULL;
    gpD3DDevice = NULL;
    gpD3DSurface = NULL;
    gpD3DBackBuf = NULL;

    /* FIXME
       > Please use subopt-helper.h for this, see vo_gl.c:preinit for
       > an example of how to use it.
    */

    if ((gpD3DHandle = Direct3DCreate9 (D3D_SDK_VERSION)) == NULL)
    {
        mp_msg(MSGT_VO,MSGL_ERR,"<vo_direct3d>Unable to initialize Direct3D\n");
        return -1;
    }

    if (FAILED (IDirect3D9_GetAdapterDisplayMode (gpD3DHandle,
                                                  D3DADAPTER_DEFAULT,
                                                  &DisplayMode)))
    {
        mp_msg(MSGT_VO,MSGL_ERR,"<vo_direct3d>Could not read display mode\n");
        return -1;
    }

    /* Store in gDesktopFmt the user desktop's colorspace. Usually XRGB. */
    gDesktopFmt = DisplayMode.Format;

    mp_msg(MSGT_VO,MSGL_V,"DisplayMode.Width %d, DisplayMode.Height %d\n",
           DisplayMode.Width, DisplayMode.Height);

    /* w32_common framework call. Configures window on the screen, gets
     * fullscreen dimensions and does other useful stuff.
     */
    if (vo_w32_init() == 0)
    {
        mp_msg(MSGT_VO,MSGL_V,"Unable to configure onscreen window\r\n");
        return -1;
    }

    /* Allow the first call to D3DConfigure. */
    gIsD3DConfigFinished = 1;

    return 0;
}



/** @brief libvo Callback: Handle control requests.
 *  @return VO_TRUE on success, VO_NOTIMPL when not implemented
 */
static int control(uint32_t request, void *data, ...)
{
    switch (request)
    {
    case VOCTRL_QUERY_FORMAT:
        return query_format (*(uint32_t*) data);
    case VOCTRL_GET_IMAGE: /* Direct Rendering. Not implemented yet. */
        mp_msg(MSGT_VO,MSGL_V,
               "<vo_direct3d>Direct Rendering request. Not implemented yet\n");
        return VO_NOTIMPL;
    case VOCTRL_DRAW_IMAGE:
        return D3DRenderFrame (data);
    case VOCTRL_FULLSCREEN:
        vo_w32_fullscreen();
        D3DConfigure();
        return VO_TRUE;
    case VOCTRL_RESET:
        return VO_NOTIMPL;
    case VOCTRL_PAUSE:
        gIsPaused = 1;
        return VO_TRUE;
    case VOCTRL_RESUME:
        gIsPaused = 0;
        return VO_TRUE;
    case VOCTRL_GUISUPPORT:
        return VO_NOTIMPL;
    case VOCTRL_SET_EQUALIZER:
        return VO_NOTIMPL;
    case VOCTRL_GET_EQUALIZER:
        return VO_NOTIMPL;
    case VOCTRL_ONTOP:
        vo_w32_ontop();
        return VO_TRUE;
    case VOCTRL_BORDER:
        vo_w32_border();
        D3DConfigure();
        return VO_TRUE;
    case VOCTRL_UPDATE_SCREENINFO:
        w32_update_xinerama_info();
        return VO_TRUE;
/*
    case VOCTRL_SET_PANSCAN:
        CalculatePanscanRect ();
        return VO_TRUE;
    case VOCTRL_GET_PANSCAN:
        return VO_TRUE;
*/
    }
    return VO_FALSE;
}

/** @brief libvo Callback: Configre the Direct3D adapter.
 *  @param width    Movie source width
 *  @param height   Movie source height
 *  @param d_width  Screen (destination) width
 *  @param d_height Screen (destination) height
 *  @param options  Options bitmap
 *  @param title    Window title
 *  @param format   Movie colorspace format (using MPlayer's
 *                  defines, e.g. IMGFMT_YUY2)
 *  @return 0 on success, VO_ERROR on failure
 */
static int config(uint32_t width, uint32_t height, uint32_t d_width,
                  uint32_t d_height, uint32_t options, char *title,
                  uint32_t format)
{
    gSrcWidth = width;
    gSrcHeight = height;

    /* w32_common framework call. Creates window on the screen with
     * the given coordinates.
     */
    if (vo_w32_config(d_width, d_height, options) == 0)
    {
        mp_msg(MSGT_VO,MSGL_V,"Unable to create onscreen window\r\n");
        return VO_ERROR;
    }

    if (gIsD3DConfigFinished == 1)
    {
        gIsD3DConfigFinished = 0;
        if (D3DConfigure () == 0)
        {
            gIsD3DConfigFinished = 1;
            return VO_ERROR;
        }
        gIsD3DConfigFinished = 1;
    }
    return 0; /* Success */
}

/** @brief libvo Callback: Flip next already drawn frame on the
 *         screen.
 *  @return N/A
 */
static void flip_page(void)
{
    if (FAILED (IDirect3DDevice9_Present (gpD3DDevice, 0, 0, 0, 0)))
    {
        mp_msg(MSGT_VO,MSGL_V,
               "<vo_direct3d>Video adapter became uncooperative.\n");
        mp_msg(MSGT_VO,MSGL_ERR,"<vo_direct3d>Trying to reinitialize it...\n");
        if (D3DConfigure() == 0)
        {
            mp_msg(MSGT_VO,MSGL_V,"<vo_direct3d>Reinitialization Failed.\n");
            return;
        }
        if (FAILED (IDirect3DDevice9_Present (gpD3DDevice, 0, 0, 0, 0)))
        {
            mp_msg(MSGT_VO,MSGL_V,"<vo_direct3d>Reinitialization Failed.\n");
            return;
        }
        else
            mp_msg(MSGT_VO,MSGL_V,"<vo_direct3d>Video adapter reinitialized.\n");

    }
    /*IDirect3DDevice9_Clear (gpD3DDevice, 0, NULL, D3DCLEAR_TARGET, 0, 0, 0);*/
}

/** @brief libvo Callback: Draw OSD/Subtitles,
 *  @return N/A
 */
static void draw_osd(void)
{
}

/** @brief libvo Callback: Uninitializes all pointers and closes
 *         all D3D related stuff,
 *  @return N/A
 */
static void uninit(void)
{
    mp_msg(MSGT_VO,MSGL_V,"<vo_direct3d>Uninitialization\r\n");

    D3DUninit();
    vo_w32_uninit(); /* w32_common framework call */
}

/** @brief libvo Callback: Handles video window events.
 *  @return N/A
 */
static void check_events(void)
{
    int flags;
    /* w32_common framework call. Handles video window events.
     * Updates global libvo's vo_dwidth/vo_dheight upon resize
     * with the new window width/height.
     */
    flags = vo_w32_check_events();
    if (flags & VO_EVENT_RESIZE)
        D3DConfigure();

    if ((flags & VO_EVENT_EXPOSE) && gIsPaused == TRUE)
        flip_page();
}

/** @brief libvo Callback: Draw slice
 *  @return 0 on success
 */
static int draw_slice(uint8_t *src[], int stride[], int w,int h,int x,int y )
{
    D3DLOCKED_RECT  stLockedRect;   /**< Offscreen surface we lock in order
                                         to copy MPlayer's frame inside it.*/
    char *Src;      /**< Pointer to the source image */
    char *Dst;      /**< Pointer to the destination image */
    int  UVstride;  /**< Stride of the U/V planes */

    if (FAILED (IDirect3DDevice9_BeginScene(gpD3DDevice)))
    {
       mp_msg(MSGT_VO,MSGL_ERR,"<vo_direct3d>BeginScene failed\n");
       return VO_ERROR;
    }

    if (FAILED (IDirect3DSurface9_LockRect(gpD3DSurface,
                                           &stLockedRect, NULL, 0)))
    {
        mp_msg(MSGT_VO,MSGL_V,"<vo_direct3d>Surface lock failure\n");
        return VO_FALSE;
    }

    UVstride = stLockedRect.Pitch / 2;

    /* Copy Y */
    Dst = stLockedRect.pBits;
    Dst = Dst + stLockedRect.Pitch * y + x;
    Src=src[0];
    memcpy_pic(Dst, Src, w, h, stLockedRect.Pitch, stride[0]);

    w/=2;h/=2;x/=2;y/=2;

    /* Copy U */
    Dst = stLockedRect.pBits;
    Dst = Dst + stLockedRect.Pitch * gSrcHeight
          + UVstride * y + x;
    if (gMovieSrcFmt == MAKEFOURCC('Y','V','1','2'))
        Src=src[2];
    else
        Src=src[1];

    memcpy_pic(Dst, Src, w, h, UVstride, stride[1]);

    /* Copy V */
    Dst = stLockedRect.pBits;
    Dst = Dst + stLockedRect.Pitch * gSrcHeight
          + UVstride * (gSrcHeight / 2) + UVstride * y + x;
    if (gMovieSrcFmt == MAKEFOURCC('Y','V','1','2'))
        Src=src[1];
    else
        Src=src[2];

    memcpy_pic(Dst, Src, w, h, UVstride, stride[2]);

    if (FAILED (IDirect3DSurface9_UnlockRect(gpD3DSurface)))
    {
        mp_msg(MSGT_VO,MSGL_V,"<vo_direct3d>Surface unlock failure\n");
        return VO_ERROR;
    }

    if (FAILED (IDirect3DDevice9_StretchRect (gpD3DDevice,
                                              gpD3DSurface,
                                              gIsPanscan == 1 ?
                                              &gPanScanSrcRect : NULL,
                                              gpD3DBackBuf,
                                              vo_fs == 1 ?
                                              &gFullScrMovieRect : NULL,
                                              D3DTEXF_LINEAR)))
    {
        mp_msg(MSGT_VO,MSGL_V,
               "<vo_direct3d>Unable to copy the frame to the back buffer\n");
        return VO_ERROR;
    }

    if (FAILED (IDirect3DDevice9_EndScene(gpD3DDevice)))
    {
       mp_msg(MSGT_VO,MSGL_ERR,"<vo_direct3d>EndScene failed\n");
       return VO_ERROR;
    }

    return 0; /* Success */
}

/** @brief libvo Callback: Unused function
 *  @return N/A
 */
static int draw_frame(uint8_t *src[])
{
    mp_msg(MSGT_VO,MSGL_V,"<vo_direct3d>draw_frame called\n");
    return VO_FALSE;
}





