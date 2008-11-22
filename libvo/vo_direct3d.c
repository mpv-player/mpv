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
#include "libavutil/common.h"

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


/* Global variables "priv" structure. I try to keep their count low.
 */
static struct global_priv {
    int is_paused;              /**< 1 = Movie is paused,
                                0 = Movie is not paused */
    int is_cfg_finished;        /**< Synchronization "semaphore". 1 when
                                instance of reconfigure_d3d is finished */

    RECT fs_movie_rect;         /**< Rect (upscaled) of the movie when displayed
                                in fullscreen */
    RECT fs_panscan_rect;       /**< PanScan source surface cropping in
                                fullscreen */
    int src_width;              /**< Source (movie) width */
    int src_height;             /**< Source (movie) heigth */

    D3DFORMAT movie_src_fmt;        /**< Movie colorspace format (depends on
                                    the movie's codec) */
    D3DFORMAT desktop_fmt;          /**< Desktop (screen) colorspace format.
                                    Usually XRGB */
    LPDIRECT3D9        d3d_handle;  /**< Direct3D Handle */
    LPDIRECT3DDEVICE9  d3d_device;  /**< The Direct3D Adapter */
    IDirect3DSurface9 *d3d_surface; /**< Offscreen Direct3D Surface. MPlayer
                                    renders inside it. Uses colorspace
                                    priv->movie_src_fmt */
    IDirect3DSurface9 *d3d_backbuf; /**< Video card's back buffer (used to
                                    display next frame) */
} *priv;

typedef struct {
    const unsigned int  mplayer_fmt;   /**< Given by MPlayer */
    const D3DFORMAT     fourcc;        /**< Required by D3D's test function */
} struct_fmt_table;

/* Map table from reported MPlayer format to the required
   fourcc. This is needed to perform the format query. */

static const struct_fmt_table fmt_table[] = {
    {IMGFMT_YV12, MAKEFOURCC('Y','V','1','2')},
    {IMGFMT_I420, MAKEFOURCC('I','4','2','0')},
    {IMGFMT_IYUV, MAKEFOURCC('I','Y','U','V')},
    {IMGFMT_YVU9, MAKEFOURCC('Y','V','U','9')},
    {IMGFMT_YUY2, MAKEFOURCC('Y','U','Y','2')},
    {IMGFMT_UYVY, MAKEFOURCC('U','Y','V','Y')},
};

#define DISPLAY_FORMAT_TABLE_ENTRIES \
        (sizeof(fmt_table) / sizeof(fmt_table[0]))

/****************************************************************************
 *                                                                          *
 *                                                                          *
 *                                                                          *
 * Direct3D specific implementation functions                               *
 *                                                                          *
 *                                                                          *
 *                                                                          *
 ****************************************************************************/

/** @brief Calculate scaled fullscreen movie rectangle with
 *  preserved aspect ratio.
 */
static void calc_fs_rect(void)
{
    int scaled_height = 0;
    int scaled_width = 0;

    // set default values
    priv->fs_movie_rect.left     = 0;
    priv->fs_movie_rect.right    = vo_dwidth;
    priv->fs_movie_rect.top      = 0;
    priv->fs_movie_rect.bottom   = vo_dheight;
    priv->fs_panscan_rect.left   = 0;
    priv->fs_panscan_rect.right  = priv->src_width;
    priv->fs_panscan_rect.top    = 0;
    priv->fs_panscan_rect.bottom = priv->src_height;
    if (!vo_fs) return;

    // adjust for fullscreen aspect and panscan
    aspect(&scaled_width, &scaled_height, A_ZOOM);
    panscan_calc();
    scaled_width  += vo_panscan_x;
    scaled_height += vo_panscan_y;

    // note: border is rounded to a multiple of two since at least
    // ATI drivers can not handle odd values with YV12 input
    if (scaled_width > vo_dwidth) {
        int border = priv->src_width * (scaled_width - vo_dwidth) / scaled_width;
        border = (border / 2 + 1) & ~1;
        priv->fs_panscan_rect.left   = border;
        priv->fs_panscan_rect.right  = priv->src_width - border;
    } else {
        priv->fs_movie_rect.left     = (vo_dwidth - scaled_width) / 2;
        priv->fs_movie_rect.right    = priv->fs_movie_rect.left + scaled_width;
    }
    if (scaled_height > vo_dheight) {
        int border = priv->src_height * (scaled_height - vo_dheight) / scaled_height;
        border = (border / 2 + 1) & ~1;
        priv->fs_panscan_rect.top    = border;
        priv->fs_panscan_rect.bottom = priv->src_height - border;
    } else {
        priv->fs_movie_rect.top      = (vo_dheight - scaled_height) / 2;
        priv->fs_movie_rect.bottom   = priv->fs_movie_rect.top + scaled_height;
    }

    mp_msg(MSGT_VO,MSGL_V,
    "<vo_direct3d>Fullscreen Movie Rect: t: %ld, l: %ld, r: %ld, b:%ld\r\n",
        priv->fs_movie_rect.top, priv->fs_movie_rect.left,
        priv->fs_movie_rect.right, priv->fs_movie_rect.bottom);
}

/** @brief Destroy D3D Context related to the current window.
 */
static void destroy_d3d_context(void)
{
    mp_msg(MSGT_VO,MSGL_V,"<vo_direct3d>destroy_d3d_context called\r\n");
    /* Let's destroy the old (if any) D3D Content */

    if (priv->d3d_surface != NULL) {
        IDirect3DSurface9_Release (priv->d3d_surface);
        priv->d3d_surface = NULL;
    }

    if (priv->d3d_device != NULL) {
        IDirect3DDevice9_Release (priv->d3d_device);
        priv->d3d_device = NULL;
    }

    /* The following is not a memory leak. d3d_backbuf is not malloc'ed
     * but just holds a pointer to the back buffer. Nobody gets hurt from
     * setting it to NULL.
     */
    priv->d3d_backbuf = NULL;
}


/** @brief (Re)Initialize Direct3D. Kill and recreate context.
 *  The first function called to initialize D3D context.
 *  @return 1 on success, 0 on failure
 */
static int reconfigure_d3d(void)
{
    D3DPRESENT_PARAMETERS present_params;
    D3DDISPLAYMODE disp_mode;

    mp_msg(MSGT_VO,MSGL_V,"<vo_direct3d><INFO>reconfigure_d3d called \n");

    destroy_d3d_context();

    /* Get the current desktop display mode, so we can set up a back buffer
     * of the same format. */
    if (FAILED(IDirect3D9_GetAdapterDisplayMode(priv->d3d_handle,
                                                D3DADAPTER_DEFAULT,
                                                &disp_mode))) {
        mp_msg(MSGT_VO,MSGL_ERR,
               "<vo_direct3d><INFO>Could not read adapter display mode.\n");
        return 0;
    }

    /* Write current Desktop's colorspace format in the global storage. */
    priv->desktop_fmt = disp_mode.Format;

    /* Prepare Direct3D initialization parameters. */
    memset(&present_params, 0, sizeof(D3DPRESENT_PARAMETERS));
    present_params.Windowed               = TRUE;
    present_params.SwapEffect             = D3DSWAPEFFECT_COPY;
    present_params.Flags                  = D3DPRESENTFLAG_VIDEO;
    present_params.hDeviceWindow          = vo_w32_window; /* w32_common var */
    present_params.BackBufferWidth        = 0; /* Fill up window Width */
    present_params.BackBufferHeight       = 0; /* Fill up window Height */
    present_params.MultiSampleType        = D3DMULTISAMPLE_NONE;
    /* D3DPRESENT_INTERVAL_ONE = vsync */
    present_params.PresentationInterval   = D3DPRESENT_INTERVAL_ONE;
    present_params.BackBufferFormat       = priv->desktop_fmt;
    present_params.BackBufferCount        = 1;
    present_params.EnableAutoDepthStencil = FALSE;

    /* vo_w32_window is w32_common variable. It's a handle to the window. */
    if (FAILED(IDirect3D9_CreateDevice(priv->d3d_handle,
                                     D3DADAPTER_DEFAULT,
                                     D3DDEVTYPE_HAL, vo_w32_window,
                                     D3DCREATE_SOFTWARE_VERTEXPROCESSING,
                                     &present_params, &priv->d3d_device))) {
        mp_msg(MSGT_VO,MSGL_ERR,
               "<vo_direct3d><INFO>Could not create the D3D device\n");
        return 0;
    }

    mp_msg(MSGT_VO,MSGL_V,
      "New BackBuffer: Width: %d, Height:%d. VO Dest Width:%d, Height: %d\n",
          present_params.BackBufferWidth, present_params.BackBufferHeight,
          vo_dwidth, vo_dheight);

    if (FAILED(IDirect3DDevice9_CreateOffscreenPlainSurface(
         priv->d3d_device, priv->src_width, priv->src_height,
         priv->movie_src_fmt, D3DPOOL_DEFAULT, &priv->d3d_surface, NULL))) {
        mp_msg(MSGT_VO,MSGL_ERR,
        "<vo_direct3d><INFO>IDirect3D9_CreateOffscreenPlainSurface Failed.\n");
        return 0;
    }

    if (FAILED(IDirect3DDevice9_GetBackBuffer(priv->d3d_device, 0, 0,
                                              D3DBACKBUFFER_TYPE_MONO,
                                              &(priv->d3d_backbuf)))) {
        mp_msg(MSGT_VO,MSGL_ERR,"<vo_direct3d>Back Buffer address get failed\n");
        return 0;
    }

    /* Fill the Surface with black color. */
    IDirect3DDevice9_ColorFill(priv->d3d_device, priv->d3d_surface, NULL,
                               D3DCOLOR_ARGB(0xFF, 0, 0, 0) );

    calc_fs_rect();

    return 1;
}

/** @brief Uninitialize Direct3D and close the window.
 */
static void uninit_d3d(void)
{
    mp_msg(MSGT_VO,MSGL_V,"<vo_direct3d>uninit_d3d called\r\n");

    /* Block further calls to reconfigure_d3d(). */
    priv->is_cfg_finished = 0;

    /* Destroy D3D Context inside the window. */
    destroy_d3d_context();

    /* Stop the whole D3D. */
    if (NULL != priv->d3d_handle) {
        mp_msg(MSGT_VO,MSGL_V,"<vo_direct3d>Calling IDirect3D9_Release\r\n");
        IDirect3D9_Release(priv->d3d_handle);
    }
}

/** @brief Render a frame on the screen.
 *  @param mpi mpi structure with the decoded frame inside
 *  @return VO_TRUE on success, VO_ERROR on failure
 */
static uint32_t render_d3d_frame(mp_image_t *mpi)
{
    D3DLOCKED_RECT  locked_rect;   /**< Offscreen surface we lock in order
                                         to copy MPlayer's frame inside it.*/

    /* Uncomment when direct rendering is implemented.
     * if (mpi->flags & MP_IMGFLAG_DIRECT) ...
     */

    if (mpi->flags & MP_IMGFLAG_DRAW_CALLBACK)
        return VO_TRUE;

    if (mpi->flags & MP_IMGFLAG_PLANAR) { /* Copy a planar frame. */
        draw_slice(mpi->planes,mpi->stride,mpi->w,mpi->h,0,0);
        return VO_TRUE;
    }

    /* If the previous if failed, we should draw a packed frame */
    if (FAILED(IDirect3DDevice9_BeginScene(priv->d3d_device))) {
       mp_msg(MSGT_VO,MSGL_ERR,"<vo_direct3d>BeginScene failed\n");
       return VO_ERROR;
    }

    if (FAILED(IDirect3DSurface9_LockRect(priv->d3d_surface,
                                           &locked_rect, NULL, 0))) {
       mp_msg(MSGT_VO,MSGL_ERR,"<vo_direct3d>Surface lock failure\n");
       return VO_ERROR;
    }

    memcpy_pic(locked_rect.pBits, mpi->planes[0], mpi->stride[0],
               mpi->height, locked_rect.Pitch, mpi->stride[0]);

    if (FAILED(IDirect3DSurface9_UnlockRect(priv->d3d_surface))) {
        mp_msg(MSGT_VO,MSGL_V,"<vo_direct3d>Surface unlock failure\n");
        return VO_ERROR;
    }

    if (FAILED(IDirect3DDevice9_StretchRect(priv->d3d_device,
                                            priv->d3d_surface,
                                            &priv->fs_panscan_rect,
                                            priv->d3d_backbuf,
                                            &priv->fs_movie_rect,
                                            D3DTEXF_LINEAR))) {
        mp_msg(MSGT_VO,MSGL_ERR,
               "<vo_direct3d>Unable to copy the frame to the back buffer\n");
        return VO_ERROR;
    }

    if (FAILED(IDirect3DDevice9_EndScene(priv->d3d_device))) {
       mp_msg(MSGT_VO,MSGL_ERR,"<vo_direct3d>EndScene failed\n");
       return VO_ERROR;
    }

    return VO_TRUE;
}


/** @brief Query if movie colorspace is supported by the HW.
 *  @return 0 on failure, device capabilities (not probed
 *          currently) on success.
 */
static int query_format(uint32_t movie_fmt)
{
    int i;
    for (i=0; i < DISPLAY_FORMAT_TABLE_ENTRIES; i++) {
        if (fmt_table[i].mplayer_fmt == movie_fmt) {
            /* Test conversion from Movie colorspace to
             * display's target colorspace. */
            if (FAILED(IDirect3D9_CheckDeviceFormatConversion(
                                           priv->d3d_handle,
                                           D3DADAPTER_DEFAULT,
                                           D3DDEVTYPE_HAL,
                                           fmt_table[i].fourcc,
                                           priv->desktop_fmt))) {
                mp_msg(MSGT_VO,MSGL_V,
                "<vo_direct3d>Rejected image format: %s\n",
                vo_format_name(fmt_table[i].mplayer_fmt));
                return 0;
            }

            priv->movie_src_fmt = fmt_table[i].fourcc;
            mp_msg(MSGT_VO,MSGL_V,"<vo_direct3d>Accepted image format: %s\n",
                   vo_format_name(fmt_table[i].mplayer_fmt));
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
    D3DDISPLAYMODE disp_mode;

    /* Set to zero all global variables. */
    priv = calloc(1, sizeof (struct global_priv));
    if (!priv) {
        mp_msg(MSGT_VO,MSGL_ERR,"<vo_direct3d>Not enough memory\r\n");
        return -1;
    }

    /* FIXME
       > Please use subopt-helper.h for this, see vo_gl.c:preinit for
       > an example of how to use it.
    */

    priv->d3d_handle = Direct3DCreate9(D3D_SDK_VERSION);
    if (!priv->d3d_handle) {
        mp_msg(MSGT_VO,MSGL_ERR,"<vo_direct3d>Unable to initialize Direct3D\n");
        return -1;
    }

    if (FAILED(IDirect3D9_GetAdapterDisplayMode(priv->d3d_handle,
                                                D3DADAPTER_DEFAULT,
                                                &disp_mode))) {
        mp_msg(MSGT_VO,MSGL_ERR,"<vo_direct3d>Could not read display mode\n");
        return -1;
    }

    /* Store in priv->desktop_fmt the user desktop's colorspace. Usually XRGB. */
    priv->desktop_fmt = disp_mode.Format;

    mp_msg(MSGT_VO,MSGL_V,"disp_mode.Width %d, disp_mode.Height %d\n",
           disp_mode.Width, disp_mode.Height);

    /* w32_common framework call. Configures window on the screen, gets
     * fullscreen dimensions and does other useful stuff.
     */
    if (!vo_w32_init()) {
        mp_msg(MSGT_VO,MSGL_V,"Unable to configure onscreen window\r\n");
        return -1;
    }

    /* Allow the first call to reconfigure_d3d. */
    priv->is_cfg_finished = 1;

    return 0;
}



/** @brief libvo Callback: Handle control requests.
 *  @return VO_TRUE on success, VO_NOTIMPL when not implemented
 */
static int control(uint32_t request, void *data, ...)
{
    switch (request) {
    case VOCTRL_QUERY_FORMAT:
        return query_format(*(uint32_t*) data);
    case VOCTRL_GET_IMAGE: /* Direct Rendering. Not implemented yet. */
        mp_msg(MSGT_VO,MSGL_V,
               "<vo_direct3d>Direct Rendering request. Not implemented yet\n");
        return VO_NOTIMPL;
    case VOCTRL_DRAW_IMAGE:
        return render_d3d_frame(data);
    case VOCTRL_FULLSCREEN:
        vo_w32_fullscreen();
        reconfigure_d3d();
        return VO_TRUE;
    case VOCTRL_RESET:
        return VO_NOTIMPL;
    case VOCTRL_PAUSE:
        priv->is_paused = 1;
        return VO_TRUE;
    case VOCTRL_RESUME:
        priv->is_paused = 0;
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
        reconfigure_d3d();
        return VO_TRUE;
    case VOCTRL_UPDATE_SCREENINFO:
        w32_update_xinerama_info();
        return VO_TRUE;
    case VOCTRL_SET_PANSCAN:
        calc_fs_rect ();
        return VO_TRUE;
    case VOCTRL_GET_PANSCAN:
        return VO_TRUE;
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

    priv->src_width = width;
    priv->src_height = height;

    /* w32_common framework call. Creates window on the screen with
     * the given coordinates.
     */
    if (!vo_w32_config(d_width, d_height, options)) {
        mp_msg(MSGT_VO,MSGL_V,"Unable to create onscreen window\r\n");
        return VO_ERROR;
    }

    if (priv->is_cfg_finished) {
        priv->is_cfg_finished = 0;
        if (!reconfigure_d3d()) {
            priv->is_cfg_finished = 1;
            return VO_ERROR;
        }
        priv->is_cfg_finished = 1;
    }
    return 0; /* Success */
}

/** @brief libvo Callback: Flip next already drawn frame on the
 *         screen.
 *  @return N/A
 */
static void flip_page(void)
{
    if (FAILED(IDirect3DDevice9_Present(priv->d3d_device, 0, 0, 0, 0))) {
        mp_msg(MSGT_VO,MSGL_V,
               "<vo_direct3d>Video adapter became uncooperative.\n");
        mp_msg(MSGT_VO,MSGL_ERR,"<vo_direct3d>Trying to reinitialize it...\n");
        if (!reconfigure_d3d()) {
            mp_msg(MSGT_VO,MSGL_V,"<vo_direct3d>Reinitialization Failed.\n");
            return;
        }
        if (FAILED(IDirect3DDevice9_Present(priv->d3d_device, 0, 0, 0, 0))) {
            mp_msg(MSGT_VO,MSGL_V,"<vo_direct3d>Reinitialization Failed.\n");
            return;
        }
        else
            mp_msg(MSGT_VO,MSGL_V,"<vo_direct3d>Video adapter reinitialized.\n");

    }
    /*IDirect3DDevice9_Clear (priv->d3d_device, 0, NULL, D3DCLEAR_TARGET, 0, 0, 0);*/
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

    uninit_d3d();
    vo_w32_uninit(); /* w32_common framework call */
    free (priv);
    priv = NULL;
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
        reconfigure_d3d();

    if ((flags & VO_EVENT_EXPOSE) && priv->is_paused)
        flip_page();
}

/** @brief libvo Callback: Draw slice
 *  @return 0 on success
 */
static int draw_slice(uint8_t *src[], int stride[], int w,int h,int x,int y )
{
    D3DLOCKED_RECT  locked_rect;   /**< Offscreen surface we lock in order
                                         to copy MPlayer's frame inside it.*/
    char *Src;      /**< Pointer to the source image */
    char *Dst;      /**< Pointer to the destination image */
    int  UVstride;  /**< Stride of the U/V planes */

    if (FAILED(IDirect3DDevice9_BeginScene(priv->d3d_device))) {
       mp_msg(MSGT_VO,MSGL_ERR,"<vo_direct3d>BeginScene failed\n");
       return VO_ERROR;
    }

    if (FAILED(IDirect3DSurface9_LockRect(priv->d3d_surface,
                                           &locked_rect, NULL, 0))) {
        mp_msg(MSGT_VO,MSGL_V,"<vo_direct3d>Surface lock failure\n");
        return VO_FALSE;
    }

    UVstride = locked_rect.Pitch / 2;

    /* Copy Y */
    Dst = locked_rect.pBits;
    Dst = Dst + locked_rect.Pitch * y + x;
    Src=src[0];
    memcpy_pic(Dst, Src, w, h, locked_rect.Pitch, stride[0]);

    w/=2;h/=2;x/=2;y/=2;

    /* Copy U */
    Dst = locked_rect.pBits;
    Dst = Dst + locked_rect.Pitch * priv->src_height
          + UVstride * y + x;
    if (priv->movie_src_fmt == MAKEFOURCC('Y','V','1','2'))
        Src=src[2];
    else
        Src=src[1];

    memcpy_pic(Dst, Src, w, h, UVstride, stride[1]);

    /* Copy V */
    Dst = locked_rect.pBits;
    Dst = Dst + locked_rect.Pitch * priv->src_height
          + UVstride * (priv->src_height / 2) + UVstride * y + x;
    if (priv->movie_src_fmt == MAKEFOURCC('Y','V','1','2'))
        Src=src[1];
    else
        Src=src[2];

    memcpy_pic(Dst, Src, w, h, UVstride, stride[2]);

    if (FAILED(IDirect3DSurface9_UnlockRect(priv->d3d_surface))) {
        mp_msg(MSGT_VO,MSGL_V,"<vo_direct3d>Surface unlock failure\n");
        return VO_ERROR;
    }

    if (FAILED(IDirect3DDevice9_StretchRect(priv->d3d_device,
                                            priv->d3d_surface,
                                            &priv->fs_panscan_rect,
                                            priv->d3d_backbuf,
                                            &priv->fs_movie_rect,
                                            D3DTEXF_LINEAR))) {
        mp_msg(MSGT_VO,MSGL_V,
               "<vo_direct3d>Unable to copy the frame to the back buffer\n");
        return VO_ERROR;
    }

    if (FAILED(IDirect3DDevice9_EndScene(priv->d3d_device))) {
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
