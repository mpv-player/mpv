/*
 * Copyright (c) 2008 Georgi Petrov (gogothebee) <gogothebee@gmail.com>
 *
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <windows.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>
#include <assert.h>
#include <d3d9.h>
#include <inttypes.h>
#include <limits.h>
#include "config.h"
#include "options/options.h"
#include "options/m_option.h"
#include "sub/draw_bmp.h"
#include "mpv_talloc.h"
#include "vo.h"
#include "video/csputils.h"
#include "video/mp_image.h"
#include "video/img_format.h"
#include "common/msg.h"
#include "common/common.h"
#include "w32_common.h"
#include "sub/osd.h"

#include "config.h"
#if !HAVE_GPL
#error GPL only
#endif

#define DEVTYPE D3DDEVTYPE_HAL
//#define DEVTYPE D3DDEVTYPE_REF

#define D3DFVF_OSD_VERTEX (D3DFVF_XYZ | D3DFVF_TEX1)

typedef struct {
    float x, y, z;
    float tu, tv;
} vertex_osd;

struct d3dtex {
    // user-requested size
    int w, h;
    // allocated texture size
    int tex_w, tex_h;
    // D3DPOOL_SYSTEMMEM (or others) texture:
    // - can be locked in order to write (and even read) data
    // - can _not_ (probably) be used as texture for rendering
    // This is always non-NULL if d3dtex_allocate succeeds.
    IDirect3DTexture9 *system;
    // D3DPOOL_DEFAULT texture:
    // - can't be locked (Probably.)
    // - must be used for rendering
    // This can be NULL if the system one can be both locked and mapped.
    IDirect3DTexture9 *device;
};

#define MAX_OSD_RECTS 64

/* Global variables "priv" structure. I try to keep their count low.
 */
typedef struct d3d_priv {
    struct mp_log *log;

    int opt_disable_texture_align;
    // debugging
    int opt_force_power_of_2;
    int opt_texture_memory;
    int opt_swap_discard;
    int opt_exact_backbuffer;

    struct vo *vo;

    bool have_image;
    double osd_pts;

    D3DLOCKED_RECT locked_rect; /**< The locked offscreen surface */
    RECT fs_movie_rect;         /**< Rect (upscaled) of the movie when displayed
                                in fullscreen */
    RECT fs_panscan_rect;       /**< PanScan source surface cropping in
                                fullscreen */
    int src_width;              /**< Source (movie) width */
    int src_height;             /**< Source (movie) heigth */
    struct mp_osd_res osd_res;
    int image_format;           /**< mplayer image format */
    struct mp_image_params params;

    D3DFORMAT movie_src_fmt;        /**< Movie colorspace format (depends on
                                    the movie's codec) */
    D3DFORMAT desktop_fmt;          /**< Desktop (screen) colorspace format.
                                    Usually XRGB */

    HANDLE d3d9_dll;                /**< d3d9 Library HANDLE */
    IDirect3D9 * (WINAPI *pDirect3DCreate9)(UINT); /**< pointer to Direct3DCreate9 function */

    LPDIRECT3D9        d3d_handle;  /**< Direct3D Handle */
    LPDIRECT3DDEVICE9  d3d_device;  /**< The Direct3D Adapter */
    bool d3d_in_scene;              /**< BeginScene was called, EndScene not */
    IDirect3DSurface9 *d3d_surface; /**< Offscreen Direct3D Surface. MPlayer
                                    renders inside it. Uses colorspace
                                    priv->movie_src_fmt */
    IDirect3DSurface9 *d3d_backbuf; /**< Video card's back buffer (used to
                                    display next frame) */
    int cur_backbuf_width;          /**< Current backbuffer width */
    int cur_backbuf_height;         /**< Current backbuffer height */
    int device_caps_power2_only;    /**< 1 = texture sizes have to be power 2
                                    0 = texture sizes can be anything */
    int device_caps_square_only;    /**< 1 = textures have to be square
                                    0 = textures do not have to be square */
    int device_texture_sys;         /**< 1 = device can texture from system memory
                                    0 = device requires shadow */
    int max_texture_width;          /**< from the device capabilities */
    int max_texture_height;         /**< from the device capabilities */

    D3DMATRIX d3d_colormatrix;

    struct mp_draw_sub_cache *osd_cache;
    struct d3dtex osd_texture;
    int osd_num_vertices;
    vertex_osd osd_vertices[MAX_OSD_RECTS * 6];
} d3d_priv;

struct fmt_entry {
    const unsigned int  mplayer_fmt;   /**< Given by MPlayer */
    const D3DFORMAT     fourcc;        /**< Required by D3D's test function */
};

/* Map table from reported MPlayer format to the required
   fourcc. This is needed to perform the format query. */

static const struct fmt_entry fmt_table[] = {
    // planar YUV
    {IMGFMT_420P,  MAKEFOURCC('Y','V','1','2')},
    {IMGFMT_420P,  MAKEFOURCC('I','4','2','0')},
    {IMGFMT_420P,  MAKEFOURCC('I','Y','U','V')},
    {IMGFMT_NV12,  MAKEFOURCC('N','V','1','2')},
    // packed YUV
    {IMGFMT_UYVY,  D3DFMT_UYVY},
    // packed RGB
    {IMGFMT_BGR0, D3DFMT_X8R8G8B8},
    {IMGFMT_RGB0, D3DFMT_X8B8G8R8},
    {IMGFMT_BGR24, D3DFMT_R8G8B8}, //untested
    {IMGFMT_RGB565, D3DFMT_R5G6B5},
    {0},
};


static bool resize_d3d(d3d_priv *priv);
static void uninit(struct vo *vo);
static void flip_page(struct vo *vo);
static mp_image_t *get_window_screenshot(d3d_priv *priv);
static void draw_osd(struct vo *vo);
static bool change_d3d_backbuffer(d3d_priv *priv);

static void d3d_matrix_identity(D3DMATRIX *m)
{
    memset(m, 0, sizeof(D3DMATRIX));
    m->_11 = m->_22 = m->_33 = m->_44 = 1.0f;
}

static void d3d_matrix_ortho(D3DMATRIX *m, float left, float right,
                             float bottom, float top)
{
    d3d_matrix_identity(m);
    m->_11 = 2.0f / (right - left);
    m->_22 = 2.0f / (top - bottom);
    m->_33 = 1.0f;
    m->_41 = -(right + left) / (right - left);
    m->_42 = -(top + bottom) / (top - bottom);
    m->_43 = 0;
    m->_44 = 1.0f;
}

/****************************************************************************
 *                                                                          *
 *                                                                          *
 *                                                                          *
 * Direct3D specific implementation functions                               *
 *                                                                          *
 *                                                                          *
 *                                                                          *
 ****************************************************************************/

static bool d3d_begin_scene(d3d_priv *priv)
{
    if (!priv->d3d_in_scene) {
        if (FAILED(IDirect3DDevice9_BeginScene(priv->d3d_device))) {
            MP_ERR(priv, "BeginScene failed.\n");
            return false;
        }
        priv->d3d_in_scene = true;
    }
    return true;
}

/** @brief Calculate scaled fullscreen movie rectangle with
 *  preserved aspect ratio.
 */
static void calc_fs_rect(d3d_priv *priv)
{
    struct mp_rect src_rect;
    struct mp_rect dst_rect;
    vo_get_src_dst_rects(priv->vo, &src_rect, &dst_rect, &priv->osd_res);

    priv->fs_movie_rect.left     = dst_rect.x0;
    priv->fs_movie_rect.right    = dst_rect.x1;
    priv->fs_movie_rect.top      = dst_rect.y0;
    priv->fs_movie_rect.bottom   = dst_rect.y1;
    priv->fs_panscan_rect.left   = src_rect.x0;
    priv->fs_panscan_rect.right  = src_rect.x1;
    priv->fs_panscan_rect.top    = src_rect.y0;
    priv->fs_panscan_rect.bottom = src_rect.y1;
}

// Adjust the texture size *width/*height to fit the requirements of the D3D
// device. The texture size is only increased.
static void d3d_fix_texture_size(d3d_priv *priv, int *width, int *height)
{
    int tex_width = *width;
    int tex_height = *height;

    // avoid nasty special cases with 0-sized textures and texture sizes
    tex_width = MPMAX(tex_width, 1);
    tex_height = MPMAX(tex_height, 1);

    if (priv->device_caps_power2_only) {
        tex_width  = 1;
        tex_height = 1;
        while (tex_width  < *width) tex_width  <<= 1;
        while (tex_height < *height) tex_height <<= 1;
    }
    if (priv->device_caps_square_only)
        /* device only supports square textures */
        tex_width = tex_height = MPMAX(tex_width, tex_height);
    // better round up to a multiple of 16
    if (!priv->opt_disable_texture_align) {
        tex_width  = (tex_width  + 15) & ~15;
        tex_height = (tex_height + 15) & ~15;
    }

    *width = tex_width;
    *height = tex_height;
}

static void d3dtex_release(d3d_priv *priv, struct d3dtex *tex)
{
    if (tex->system)
        IDirect3DTexture9_Release(tex->system);
    tex->system = NULL;

    if (tex->device)
        IDirect3DTexture9_Release(tex->device);
    tex->device = NULL;

    tex->tex_w = tex->tex_h = 0;
}

static bool d3dtex_allocate(d3d_priv *priv, struct d3dtex *tex, D3DFORMAT fmt,
                            int w, int h)
{
    d3dtex_release(priv, tex);

    tex->w = w;
    tex->h = h;

    int tw = w, th = h;
    d3d_fix_texture_size(priv, &tw, &th);

    bool use_sh = !priv->device_texture_sys;
    int memtype = D3DPOOL_SYSTEMMEM;
    switch (priv->opt_texture_memory) {
    case 1: memtype = D3DPOOL_MANAGED; use_sh = false; break;
    case 2: memtype = D3DPOOL_DEFAULT; use_sh = false; break;
    case 3: memtype = D3DPOOL_DEFAULT; use_sh = true; break;
    case 4: memtype = D3DPOOL_SCRATCH; use_sh = true; break;
    }

    if (FAILED(IDirect3DDevice9_CreateTexture(priv->d3d_device, tw, th, 1,
        D3DUSAGE_DYNAMIC, fmt, memtype, &tex->system, NULL)))
    {
        MP_ERR(priv, "Allocating %dx%d texture in system RAM failed.\n", w, h);
        goto error_exit;
    }

    if (use_sh) {
        if (FAILED(IDirect3DDevice9_CreateTexture(priv->d3d_device, tw, th, 1,
            D3DUSAGE_DYNAMIC, fmt, D3DPOOL_DEFAULT, &tex->device, NULL)))
        {
            MP_ERR(priv, "Allocating %dx%d texture in video RAM failed.\n", w, h);
            goto error_exit;
        }
    }

    tex->tex_w = tw;
    tex->tex_h = th;

    return true;

error_exit:
    d3dtex_release(priv, tex);
    return false;
}

static IDirect3DBaseTexture9 *d3dtex_get_render_texture(d3d_priv *priv,
                                                        struct d3dtex *tex)
{
    return (IDirect3DBaseTexture9 *)(tex->device ? tex->device : tex->system);
}

// Copy system texture contents to device texture.
static bool d3dtex_update(d3d_priv *priv, struct d3dtex *tex)
{
    if (!tex->device)
        return true;
    return !FAILED(IDirect3DDevice9_UpdateTexture(priv->d3d_device,
                   (IDirect3DBaseTexture9 *)tex->system,
                   (IDirect3DBaseTexture9 *)tex->device));
}

static void d3d_unlock_video_objects(d3d_priv *priv)
{
    if (priv->locked_rect.pBits) {
        if (FAILED(IDirect3DSurface9_UnlockRect(priv->d3d_surface)))
            MP_VERBOSE(priv, "Unlocking video objects failed.\n");
    }
    priv->locked_rect.pBits = NULL;
}

// Free video surface/textures,  etc.
static void d3d_destroy_video_objects(d3d_priv *priv)
{
    d3d_unlock_video_objects(priv);

    if (priv->d3d_surface)
        IDirect3DSurface9_Release(priv->d3d_surface);
    priv->d3d_surface = NULL;
}

/** @brief Destroy D3D Offscreen and Backbuffer surfaces.
 */
static void destroy_d3d_surfaces(d3d_priv *priv)
{
    MP_VERBOSE(priv, "destroy_d3d_surfaces called.\n");

    d3d_destroy_video_objects(priv);
    d3dtex_release(priv, &priv->osd_texture);

    if (priv->d3d_backbuf)
        IDirect3DSurface9_Release(priv->d3d_backbuf);
    priv->d3d_backbuf = NULL;

    priv->d3d_in_scene = false;
}

// Allocate video surface.
static bool d3d_configure_video_objects(d3d_priv *priv)
{
    assert(priv->image_format != 0);

    if (!priv->d3d_surface &&
        FAILED(IDirect3DDevice9_CreateOffscreenPlainSurface(
            priv->d3d_device, priv->src_width, priv->src_height,
            priv->movie_src_fmt, D3DPOOL_DEFAULT, &priv->d3d_surface, NULL)))
    {
        MP_ERR(priv, "Allocating offscreen surface failed.\n");
        return false;
    }

    return true;
}

// Recreate and initialize D3D objects if necessary. The amount of work that
// needs to be done can be quite different: it could be that full initialization
// is required, or that some objects need to be created, or that nothing is
// done.
static bool create_d3d_surfaces(d3d_priv *priv)
{
    MP_VERBOSE(priv, "create_d3d_surfaces called.\n");

    if (!priv->d3d_backbuf &&
        FAILED(IDirect3DDevice9_GetBackBuffer(priv->d3d_device, 0, 0,
                                              D3DBACKBUFFER_TYPE_MONO,
                                              &priv->d3d_backbuf))) {
        MP_ERR(priv, "Allocating backbuffer failed.\n");
        return 0;
    }

    if (!d3d_configure_video_objects(priv))
        return 0;

    /* setup default renderstate */
    IDirect3DDevice9_SetRenderState(priv->d3d_device,
                                    D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    IDirect3DDevice9_SetRenderState(priv->d3d_device,
                                    D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    IDirect3DDevice9_SetRenderState(priv->d3d_device,
                                    D3DRS_ALPHAFUNC, D3DCMP_GREATER);
    IDirect3DDevice9_SetRenderState(priv->d3d_device,
                                    D3DRS_ALPHAREF, (DWORD)0x0);
    IDirect3DDevice9_SetRenderState(priv->d3d_device,
                                    D3DRS_LIGHTING, FALSE);

    // we use up to 3 samplers for up to 3 YUV planes
    // TODO
    /*
    for (int n = 0; n < 3; n++) {
        IDirect3DDevice9_SetSamplerState(priv->d3d_device, n, D3DSAMP_MINFILTER,
                                         D3DTEXF_LINEAR);
        IDirect3DDevice9_SetSamplerState(priv->d3d_device, n, D3DSAMP_MAGFILTER,
                                         D3DTEXF_LINEAR);
        IDirect3DDevice9_SetSamplerState(priv->d3d_device, n, D3DSAMP_ADDRESSU,
                                         D3DTADDRESS_CLAMP);
        IDirect3DDevice9_SetSamplerState(priv->d3d_device, n, D3DSAMP_ADDRESSV,
                                         D3DTADDRESS_CLAMP);
    }
    */

    return 1;
}

static bool init_d3d(d3d_priv *priv)
{
    D3DDISPLAYMODE disp_mode;
    D3DCAPS9 disp_caps;
    DWORD texture_caps;
    DWORD dev_caps;

    priv->d3d_handle = priv->pDirect3DCreate9(D3D_SDK_VERSION);
    if (!priv->d3d_handle) {
        MP_ERR(priv, "Initializing Direct3D failed.\n");
        return false;
    }

    if (FAILED(IDirect3D9_GetAdapterDisplayMode(priv->d3d_handle,
                                                D3DADAPTER_DEFAULT,
                                                &disp_mode))) {
        MP_ERR(priv, "Reading display mode failed.\n");
        return false;
    }

    priv->desktop_fmt = disp_mode.Format;
    priv->cur_backbuf_width = disp_mode.Width;
    priv->cur_backbuf_height = disp_mode.Height;

    MP_VERBOSE(priv, "Setting backbuffer dimensions to (%dx%d).\n",
               disp_mode.Width, disp_mode.Height);

    if (FAILED(IDirect3D9_GetDeviceCaps(priv->d3d_handle,
                                        D3DADAPTER_DEFAULT,
                                        DEVTYPE,
                                        &disp_caps)))
    {
        MP_ERR(priv, "Reading display capabilities failed.\n");
        return false;
    }

    /* Store relevant information reguarding caps of device */
    texture_caps                  = disp_caps.TextureCaps;
    dev_caps                      = disp_caps.DevCaps;
    priv->device_caps_power2_only =  (texture_caps & D3DPTEXTURECAPS_POW2) &&
                        !(texture_caps & D3DPTEXTURECAPS_NONPOW2CONDITIONAL);
    priv->device_caps_square_only = texture_caps & D3DPTEXTURECAPS_SQUAREONLY;
    priv->device_texture_sys      = dev_caps & D3DDEVCAPS_TEXTURESYSTEMMEMORY;
    priv->max_texture_width       = disp_caps.MaxTextureWidth;
    priv->max_texture_height      = disp_caps.MaxTextureHeight;

    if (priv->opt_force_power_of_2)
        priv->device_caps_power2_only = 1;

    if (FAILED(IDirect3D9_CheckDeviceFormat(priv->d3d_handle,
                        D3DADAPTER_DEFAULT,
                        DEVTYPE,
                        priv->desktop_fmt,
                        D3DUSAGE_DYNAMIC | D3DUSAGE_QUERY_FILTER,
                        D3DRTYPE_TEXTURE,
                        D3DFMT_A8R8G8B8)))
    {
        MP_ERR(priv, "OSD texture format not supported.\n");
        return false;
    }

    if (!change_d3d_backbuffer(priv))
        return false;

    MP_VERBOSE(priv, "device_caps_power2_only %d, device_caps_square_only %d\n"
               "device_texture_sys %d\n"
               "max_texture_width %d, max_texture_height %d\n",
               priv->device_caps_power2_only, priv->device_caps_square_only,
               priv->device_texture_sys, priv->max_texture_width,
               priv->max_texture_height);

    return true;
}

/** @brief Fill D3D Presentation parameters
 */
static void fill_d3d_presentparams(d3d_priv *priv,
                                   D3DPRESENT_PARAMETERS *present_params)
{
    /* Prepare Direct3D initialization parameters. */
    memset(present_params, 0, sizeof(D3DPRESENT_PARAMETERS));
    present_params->Windowed               = TRUE;
    present_params->SwapEffect             =
        priv->opt_swap_discard ? D3DSWAPEFFECT_DISCARD : D3DSWAPEFFECT_COPY;
    present_params->Flags                  = D3DPRESENTFLAG_VIDEO;
    present_params->hDeviceWindow          = vo_w32_hwnd(priv->vo);
    present_params->BackBufferWidth        = priv->cur_backbuf_width;
    present_params->BackBufferHeight       = priv->cur_backbuf_height;
    present_params->MultiSampleType        = D3DMULTISAMPLE_NONE;
    present_params->PresentationInterval   = D3DPRESENT_INTERVAL_ONE;
    present_params->BackBufferFormat       = priv->desktop_fmt;
    present_params->BackBufferCount        = 1;
    present_params->EnableAutoDepthStencil = FALSE;
}


// Create a new backbuffer. Create or Reset the D3D device.
static bool change_d3d_backbuffer(d3d_priv *priv)
{
    int window_w = priv->vo->dwidth;
    int window_h = priv->vo->dheight;

    /* Grow the backbuffer in the required dimension. */
    if (window_w > priv->cur_backbuf_width)
        priv->cur_backbuf_width = window_w;

    if (window_h > priv->cur_backbuf_height)
        priv->cur_backbuf_height = window_h;

    if (priv->opt_exact_backbuffer) {
        priv->cur_backbuf_width = window_w;
        priv->cur_backbuf_height = window_h;
    }

    /* The grown backbuffer dimensions are ready and fill_d3d_presentparams
     * will use them, so we can reset the device.
     */
    D3DPRESENT_PARAMETERS present_params;
    fill_d3d_presentparams(priv, &present_params);

    if (!priv->d3d_device) {
        if (FAILED(IDirect3D9_CreateDevice(priv->d3d_handle,
                                           D3DADAPTER_DEFAULT,
                                           DEVTYPE, vo_w32_hwnd(priv->vo),
                                           D3DCREATE_SOFTWARE_VERTEXPROCESSING
                                           | D3DCREATE_FPU_PRESERVE
                                           | D3DCREATE_MULTITHREADED,
                                           &present_params, &priv->d3d_device)))
        {
            MP_VERBOSE(priv, "Creating Direct3D device failed.\n");
            return 0;
        }
    } else {
        if (FAILED(IDirect3DDevice9_Reset(priv->d3d_device, &present_params))) {
            MP_ERR(priv, "Reseting Direct3D device failed.\n");
            return 0;
        }
    }

    MP_VERBOSE(priv, "New backbuffer (%dx%d), VO (%dx%d)\n",
               present_params.BackBufferWidth, present_params.BackBufferHeight,
               window_w, window_h);

    return 1;
}

static void destroy_d3d(d3d_priv *priv)
{
    destroy_d3d_surfaces(priv);

    if (priv->d3d_device)
        IDirect3DDevice9_Release(priv->d3d_device);
    priv->d3d_device = NULL;

    if (priv->d3d_handle) {
        MP_VERBOSE(priv, "Stopping Direct3D.\n");
        IDirect3D9_Release(priv->d3d_handle);
    }
    priv->d3d_handle = NULL;
}

/** @brief Reconfigure the whole Direct3D. Called only
 *  when the video adapter becomes uncooperative. ("Lost" devices)
 *  @return 1 on success, 0 on failure
 */
static int reconfigure_d3d(d3d_priv *priv)
{
    MP_VERBOSE(priv, "reconfigure_d3d called.\n");

    // Force complete destruction of the D3D state.
    // Note: this step could be omitted. The resize_d3d call below would detect
    // that d3d_device is NULL, and would properly recreate it. I'm not sure why
    // the following code to release and recreate the d3d_handle exists.
    destroy_d3d(priv);
    if (!init_d3d(priv))
        return 0;

    // Proper re-initialization.
    if (!resize_d3d(priv))
        return 0;

    return 1;
}

// Resize Direct3D context on window resize.
// This function also is called when major initializations need to be done.
static bool resize_d3d(d3d_priv *priv)
{
    D3DVIEWPORT9 vp = {0, 0, priv->vo->dwidth, priv->vo->dheight, 0, 1};

    MP_VERBOSE(priv, "resize_d3d %dx%d called.\n",
               priv->vo->dwidth, priv->vo->dheight);

    /* Make sure that backbuffer is large enough to accommodate the new
       viewport dimensions. Grow it if necessary. */

    bool backbuf_resize = priv->vo->dwidth > priv->cur_backbuf_width ||
                          priv->vo->dheight > priv->cur_backbuf_height;

    if (priv->opt_exact_backbuffer) {
        backbuf_resize = priv->vo->dwidth != priv->cur_backbuf_width ||
                         priv->vo->dheight != priv->cur_backbuf_height;
    }

    if (backbuf_resize || !priv->d3d_device)
    {
        destroy_d3d_surfaces(priv);
        if (!change_d3d_backbuffer(priv))
            return 0;
    }

    if (!priv->d3d_device || !priv->image_format)
        return 1;

    if (!create_d3d_surfaces(priv))
        return 0;

    if (FAILED(IDirect3DDevice9_SetViewport(priv->d3d_device, &vp))) {
        MP_ERR(priv, "Setting viewport failed.\n");
        return 0;
    }

    // so that screen coordinates map to D3D ones
    D3DMATRIX view;
    d3d_matrix_ortho(&view, 0.5f, vp.Width + 0.5f, vp.Height + 0.5f, 0.5f);
    IDirect3DDevice9_SetTransform(priv->d3d_device, D3DTS_VIEW, &view);

    calc_fs_rect(priv);
    priv->vo->want_redraw = true;

    return 1;
}

/** @brief Uninitialize Direct3D and close the window.
 */
static void uninit_d3d(d3d_priv *priv)
{
    MP_VERBOSE(priv, "uninit_d3d called.\n");

    destroy_d3d(priv);
}

static uint32_t d3d_draw_frame(d3d_priv *priv)
{
    if (!priv->d3d_device)
        return VO_TRUE;

    if (!d3d_begin_scene(priv))
        return VO_ERROR;

    IDirect3DDevice9_Clear(priv->d3d_device, 0, NULL, D3DCLEAR_TARGET, 0, 0, 0);

    if (!priv->have_image)
        goto render_osd;

    RECT rm = priv->fs_movie_rect;
    RECT rs = priv->fs_panscan_rect;

    rs.left &= ~(ULONG)1;
    rs.top &= ~(ULONG)1;
    rs.right &= ~(ULONG)1;
    rs.bottom &= ~(ULONG)1;
    if (FAILED(IDirect3DDevice9_StretchRect(priv->d3d_device,
                                            priv->d3d_surface,
                                            &rs,
                                            priv->d3d_backbuf,
                                            &rm,
                                            D3DTEXF_LINEAR))) {
        MP_ERR(priv, "Copying frame to the backbuffer failed.\n");
        return VO_ERROR;
    }

render_osd:

    draw_osd(priv->vo);

    return VO_TRUE;
}

static D3DFORMAT check_format(d3d_priv *priv, uint32_t movie_fmt)
{
    const struct fmt_entry *cur = &fmt_table[0];

    while (cur->mplayer_fmt) {
        if (cur->mplayer_fmt == movie_fmt) {
            HRESULT res;
            /* Test conversion from Movie colorspace to
            * display's target colorspace. */
            res = IDirect3D9_CheckDeviceFormatConversion(priv->d3d_handle,
                        D3DADAPTER_DEFAULT,
                        DEVTYPE,
                        cur->fourcc,
                        priv->desktop_fmt);
            if (FAILED(res)) {
                MP_VERBOSE(priv, "Rejected image format: %s\n",
                           vo_format_name(cur->mplayer_fmt));
                return 0;
            }

            MP_DBG(priv, "Accepted image format: %s\n",
                   vo_format_name(cur->mplayer_fmt));

            return cur->fourcc;
        }
        cur++;
    }

    return 0;
}

// Return if the image format can be used. If it can, decide which rendering
// and conversion mode to use.
// If initialize is true, actually setup all variables to use the picked
// rendering mode.
static bool init_rendering_mode(d3d_priv *priv, uint32_t fmt, bool initialize)
{
    int blit_d3dfmt = check_format(priv, fmt);

    if (!blit_d3dfmt)
        return false;

    MP_VERBOSE(priv, "Accepted rendering methods for "
           "format='%s': StretchRect=%#x.\n",
           vo_format_name(fmt), blit_d3dfmt);

    if (!initialize)
        return true;

    // initialization doesn't fail beyond this point

    priv->movie_src_fmt = 0;
    priv->image_format = fmt;

    priv->movie_src_fmt = blit_d3dfmt;

    return true;
}

/** @brief Query if movie colorspace is supported by the HW.
 *  @return 0 on failure, device capabilities (not probed
 *          currently) on success.
 */
static int query_format(struct vo *vo, int movie_fmt)
{
    d3d_priv *priv = vo->priv;
    if (!init_rendering_mode(priv, movie_fmt, false))
        return 0;

    return 1;
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

static int preinit(struct vo *vo)
{
    d3d_priv *priv = vo->priv;
    priv->vo = vo;
    priv->log = vo->log;

    priv->d3d9_dll = LoadLibraryA("d3d9.dll");
    if (!priv->d3d9_dll) {
        MP_ERR(priv, "Unable to dynamically load d3d9.dll\n");
        goto err_out;
    }

    priv->pDirect3DCreate9 = (void *)GetProcAddress(priv->d3d9_dll,
                                                    "Direct3DCreate9");
    if (!priv->pDirect3DCreate9) {
        MP_ERR(priv, "Unable to find entry point of Direct3DCreate9\n");
        goto err_out;
    }

    /* w32_common framework call. Configures window on the screen, gets
     * fullscreen dimensions and does other useful stuff.
     */
    if (!vo_w32_init(vo)) {
        MP_VERBOSE(priv, "Configuring onscreen window failed.\n");
        goto err_out;
    }

    if (!init_d3d(priv))
        goto err_out;

    return 0;

err_out:
    uninit(vo);
    return -1;
}

/** @brief libvo Callback: Handle control requests.
 *  @return VO_TRUE on success, VO_NOTIMPL when not implemented
 */
static int control(struct vo *vo, uint32_t request, void *data)
{
    d3d_priv *priv = vo->priv;

    switch (request) {
    case VOCTRL_REDRAW_FRAME:
        d3d_draw_frame(priv);
        return VO_TRUE;
    case VOCTRL_SET_PANSCAN:
        calc_fs_rect(priv);
        priv->vo->want_redraw = true;
        return VO_TRUE;
    case VOCTRL_SCREENSHOT_WIN:
        *(struct mp_image **)data = get_window_screenshot(priv);
        return VO_TRUE;
    }

    int events = 0;
    int r = vo_w32_control(vo, &events, request, data);

    if (events & VO_EVENT_RESIZE)
        resize_d3d(priv);

    if (events & VO_EVENT_EXPOSE)
        vo->want_redraw = true;

    vo_event(vo, events);

    return r;
}

static int reconfig(struct vo *vo, struct mp_image_params *params)
{
    d3d_priv *priv = vo->priv;

    priv->have_image = false;

    vo_w32_config(vo);

    if ((priv->image_format != params->imgfmt)
        || (priv->src_width != params->w)
        || (priv->src_height != params->h))
    {
        d3d_destroy_video_objects(priv);

        priv->src_width = params->w;
        priv->src_height = params->h;
        priv->params = *params;
        init_rendering_mode(priv, params->imgfmt, true);
    }

    if (!resize_d3d(priv))
        return VO_ERROR;

    return 0; /* Success */
}

/** @brief libvo Callback: Flip next already drawn frame on the
 *         screen.
 */
static void flip_page(struct vo *vo)
{
    d3d_priv *priv = vo->priv;

    if (priv->d3d_device && priv->d3d_in_scene) {
        if (FAILED(IDirect3DDevice9_EndScene(priv->d3d_device))) {
            MP_ERR(priv, "EndScene failed.\n");
        }
    }
    priv->d3d_in_scene = false;

    RECT rect = {0, 0, vo->dwidth, vo->dheight};
    if (!priv->d3d_device ||
        FAILED(IDirect3DDevice9_Present(priv->d3d_device, &rect, 0, 0, 0))) {
        MP_VERBOSE(priv, "Trying to reinitialize uncooperative video adapter.\n");
        if (!reconfigure_d3d(priv)) {
            MP_VERBOSE(priv, "Reinitialization failed.\n");
            return;
        } else {
            MP_VERBOSE(priv, "Video adapter reinitialized.\n");
        }
    }
}

/** @brief libvo Callback: Uninitializes all pointers and closes
 *         all D3D related stuff,
 */
static void uninit(struct vo *vo)
{
    d3d_priv *priv = vo->priv;

    MP_VERBOSE(priv, "uninit called.\n");

    uninit_d3d(priv);
    vo_w32_uninit(vo);
    if (priv->d3d9_dll)
        FreeLibrary(priv->d3d9_dll);
    priv->d3d9_dll = NULL;
}

// Lock buffers and fill out to point to them.
// Must call d3d_unlock_video_objects() to unlock the buffers again.
static bool get_video_buffer(d3d_priv *priv, struct mp_image *out)
{
    *out = (struct mp_image) {0};
    mp_image_set_size(out, priv->src_width, priv->src_height);
    mp_image_setfmt(out, priv->image_format);

    if (!priv->d3d_device)
        return false;

    if (!priv->locked_rect.pBits) {
        if (FAILED(IDirect3DSurface9_LockRect(priv->d3d_surface,
                                              &priv->locked_rect, NULL, 0)))
        {
            MP_ERR(priv, "Surface lock failed.\n");
            return false;
        }
    }

    uint8_t *base = priv->locked_rect.pBits;
    size_t stride = priv->locked_rect.Pitch;

    out->planes[0] = base;
    out->stride[0] = stride;

    if (out->num_planes == 2) {
        // NV12, NV21
        out->planes[1] = base + stride * out->h;
        out->stride[1] = stride;
    }

    if (out->num_planes == 3) {
        bool swap = priv->movie_src_fmt == MAKEFOURCC('Y','V','1','2');

        size_t uv_stride = stride / 2;
        uint8_t *u = base + out->h * stride;
        uint8_t *v = u + (out->h / 2) * uv_stride;

        out->planes[1] = swap ? v : u;
        out->planes[2] = swap ? u : v;

        out->stride[1] = out->stride[2] = uv_stride;
    }

    return true;
}

static void draw_image(struct vo *vo, mp_image_t *mpi)
{
    d3d_priv *priv = vo->priv;
    if (!priv->d3d_device)
        goto done;

    struct mp_image buffer;
    if (!get_video_buffer(priv, &buffer))
        goto done;

    mp_image_copy(&buffer, mpi);

    d3d_unlock_video_objects(priv);

    priv->have_image = true;
    priv->osd_pts = mpi->pts;

    d3d_draw_frame(priv);

done:
    talloc_free(mpi);
}

static mp_image_t *get_window_screenshot(d3d_priv *priv)
{
    D3DDISPLAYMODE mode;
    mp_image_t *image = NULL;
    RECT window_rc;
    RECT screen_rc;
    RECT visible;
    POINT pt;
    D3DLOCKED_RECT locked_rect;
    int width, height;
    IDirect3DSurface9 *surface = NULL;

    if (FAILED(IDirect3DDevice9_GetDisplayMode(priv->d3d_device, 0, &mode))) {
        MP_ERR(priv, "GetDisplayMode failed.\n");
        goto error_exit;
    }

    if (FAILED(IDirect3DDevice9_CreateOffscreenPlainSurface(priv->d3d_device,
        mode.Width, mode.Height, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &surface,
        NULL)))
    {
        MP_ERR(priv, "Couldn't create surface.\n");
        goto error_exit;
    }

    if (FAILED(IDirect3DDevice9_GetFrontBufferData(priv->d3d_device, 0,
        surface)))
    {
        MP_ERR(priv, "Couldn't copy frontbuffer.\n");
        goto error_exit;
    }

    GetClientRect(vo_w32_hwnd(priv->vo), &window_rc);
    pt = (POINT) { 0, 0 };
    ClientToScreen(vo_w32_hwnd(priv->vo), &pt);
    window_rc.left = pt.x;
    window_rc.top = pt.y;
    window_rc.right += window_rc.left;
    window_rc.bottom += window_rc.top;

    screen_rc = (RECT) { 0, 0, mode.Width, mode.Height };

    if (!IntersectRect(&visible, &screen_rc, &window_rc))
        goto error_exit;
    width = visible.right - visible.left;
    height = visible.bottom - visible.top;
    if (width < 1 || height < 1)
        goto error_exit;

    image = mp_image_alloc(IMGFMT_BGR0, width, height);
    if (!image)
        goto error_exit;

    IDirect3DSurface9_LockRect(surface, &locked_rect, NULL, 0);

    memcpy_pic(image->planes[0], (char*)locked_rect.pBits + visible.top *
               locked_rect.Pitch + visible.left * 4, width * 4, height,
               image->stride[0], locked_rect.Pitch);

    IDirect3DSurface9_UnlockRect(surface);
    IDirect3DSurface9_Release(surface);

    return image;

error_exit:
    if (image)
        talloc_free(image);
    if (surface)
        IDirect3DSurface9_Release(surface);
    return NULL;
}

static void update_osd(d3d_priv *priv)
{
    if (!priv->osd_cache)
        priv->osd_cache = mp_draw_sub_alloc(priv, priv->vo->global);

    struct sub_bitmap_list *sbs = osd_render(priv->vo->osd, priv->osd_res,
                                             priv->osd_pts, 0, mp_draw_sub_formats);

    struct mp_rect act_rc[MAX_OSD_RECTS], mod_rc[64];
    int num_act_rc = 0, num_mod_rc = 0;

    struct mp_image *osd = mp_draw_sub_overlay(priv->osd_cache, sbs,
                    act_rc, MP_ARRAY_SIZE(act_rc), &num_act_rc,
                    mod_rc, MP_ARRAY_SIZE(mod_rc), &num_mod_rc);

    talloc_free(sbs);

    if (!osd) {
        MP_ERR(priv, "Failed to render OSD.\n");
        return;
    }

    if (!num_mod_rc && priv->osd_texture.system)
        return; // nothing changed

    priv->osd_num_vertices = 0;

    if (osd->w > priv->osd_texture.tex_w || osd->h > priv->osd_texture.tex_h) {
        int new_w = osd->w;
        int new_h = osd->h;
        d3d_fix_texture_size(priv, &new_w, &new_h);

        MP_DBG(priv, "reallocate OSD surface to %dx%d.\n", new_w, new_h);

        d3dtex_release(priv, &priv->osd_texture);
        if (!d3dtex_allocate(priv, &priv->osd_texture, D3DFMT_A8R8G8B8,
                             new_w, new_h))
            return;
    }

    // Lazy; could/should use the bounding rect, or perform multiple lock calls.
    // The previous approach (fully packed texture) was more efficient.
    RECT dirty_rc = { 0, 0, priv->osd_texture.w, priv->osd_texture.h };

    D3DLOCKED_RECT locked_rect;

    if (FAILED(IDirect3DTexture9_LockRect(priv->osd_texture.system, 0, &locked_rect,
                                          &dirty_rc, 0)))
    {
        MP_ERR(priv, "OSD texture lock failed.\n");
        return;
    }

    for (int n = 0; n < num_mod_rc; n++) {
        struct mp_rect rc = mod_rc[n];
        int w = mp_rect_w(rc);
        int h = mp_rect_h(rc);
        void *src = mp_image_pixel_ptr(osd, 0, rc.x0, rc.y0);
        void *dst = (char *)locked_rect.pBits + locked_rect.Pitch * rc.y0 +
                    rc.x0 * 4;
        memcpy_pic(dst, src, w * 4, h, locked_rect.Pitch, osd->stride[0]);
    }

    if (FAILED(IDirect3DTexture9_UnlockRect(priv->osd_texture.system, 0))) {
        MP_ERR(priv, "OSD texture unlock failed.\n");
        return;
    }

    if (!d3dtex_update(priv, &priv->osd_texture))
        return;

    // We need 2 primitives per quad which makes 6 vertices.
    priv->osd_num_vertices = num_act_rc * 6;

    float tex_w = priv->osd_texture.tex_w;
    float tex_h = priv->osd_texture.tex_h;

    for (int n = 0; n < num_act_rc; n++) {
        struct mp_rect rc = act_rc[n];

        float tx0 = rc.x0 / tex_w;
        float ty0 = rc.y0 / tex_h;
        float tx1 = rc.x1 / tex_w;
        float ty1 = rc.y1 / tex_h;

        vertex_osd *v = &priv->osd_vertices[n * 6];
        v[0] = (vertex_osd) { rc.x0, rc.y0, 0, tx0, ty0 };
        v[1] = (vertex_osd) { rc.x1, rc.y0, 0, tx1, ty0 };
        v[2] = (vertex_osd) { rc.x0, rc.y1, 0, tx0, ty1 };
        v[3] = (vertex_osd) { rc.x1, rc.y1, 0, tx1, ty1 };
        v[4] = v[2];
        v[5] = v[1];
    }
}

static void draw_osd(struct vo *vo)
{
    d3d_priv *priv = vo->priv;
    if (!priv->d3d_device)
        return;

    update_osd(priv);

    if (!priv->osd_num_vertices)
        return;

    d3d_begin_scene(priv);

    IDirect3DDevice9_SetRenderState(priv->d3d_device,
                                    D3DRS_ALPHABLENDENABLE, TRUE);

    IDirect3DDevice9_SetTexture(priv->d3d_device, 0,
                        d3dtex_get_render_texture(priv, &priv->osd_texture));

    IDirect3DDevice9_SetRenderState(priv->d3d_device, D3DRS_SRCBLEND,
                                    D3DBLEND_ONE);

    IDirect3DDevice9_SetFVF(priv->d3d_device, D3DFVF_OSD_VERTEX);
    IDirect3DDevice9_DrawPrimitiveUP(priv->d3d_device, D3DPT_TRIANGLELIST,
                                     priv->osd_num_vertices / 3,
                                     priv->osd_vertices, sizeof(vertex_osd));

    IDirect3DDevice9_SetRenderState(priv->d3d_device,
                                    D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);

    IDirect3DDevice9_SetTexture(priv->d3d_device, 0, NULL);

    IDirect3DDevice9_SetRenderState(priv->d3d_device,
                                    D3DRS_ALPHABLENDENABLE, FALSE);
}

#define OPT_BASE_STRUCT d3d_priv

static const struct m_option opts[] = {
    {"force-power-of-2", OPT_FLAG(opt_force_power_of_2)},
    {"disable-texture-align", OPT_FLAG(opt_disable_texture_align)},
    {"texture-memory", OPT_CHOICE(opt_texture_memory,
        {"default", 0},
        {"managed", 1},
        {"default-pool", 2},
        {"default-pool-shadow", 3},
        {"scratch", 4})},
    {"swap-discard", OPT_FLAG(opt_swap_discard)},
    {"exact-backbuffer", OPT_FLAG(opt_exact_backbuffer)},
    {0}
};

const struct vo_driver video_out_direct3d = {
    .description = "Direct3D 9 Renderer",
    .name = "direct3d",
    .preinit = preinit,
    .query_format = query_format,
    .reconfig = reconfig,
    .control = control,
    .draw_image = draw_image,
    .flip_page = flip_page,
    .uninit = uninit,
    .priv_size = sizeof(d3d_priv),
    .options = opts,
    .options_prefix = "vo-direct3d",
};
