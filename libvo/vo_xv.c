/* vo_xv.c, X11 Xv interface */

// Number of buffers _FOR_DOUBLEBUFFERING_MODE_
// Use option -double to enable double buffering! (default: single buffer)
#define NUM_BUFFERS 3

/*
Buffer allocation:

-nodr:
  1: TEMP
  2: 2*TEMP

-dr:
  1: TEMP
  3: 2*STATIC+TEMP
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"
#include "video_out.h"
#include "libmpcodecs/vfcap.h"
#include "libmpcodecs/mp_image.h"
#include "osd.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <errno.h>

#include "x11_common.h"

#include "fastmemcpy.h"
#include "sub.h"
#include "aspect.h"

#include "subopt-helper.h"

#include "input/input.h"

#ifdef HAVE_NEW_GUI
#include "gui/interface.h"
#endif

#include "libavutil/common.h"

static const vo_info_t info = {
    "X11/Xv",
    "xv",
    "Gerd Knorr <kraxel@goldbach.in-berlin.de> and others",
    ""
};

#ifdef HAVE_SHM
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>
#endif

// Note: depends on the inclusion of X11/extensions/XShm.h
#include <X11/extensions/Xv.h>
#include <X11/extensions/Xvlib.h>

struct xvctx {
    unsigned int xv_port;
    XvAdaptorInfo *ai;
    XvImageFormatValues *fo;
    unsigned int formats, adaptors, xv_format;
    int current_buf;
    int current_ip_buf;
    int num_buffers;
    int visible_buf;
    XvImage *xvimage[NUM_BUFFERS];
    uint32_t image_width;
    uint32_t image_height;
    uint32_t image_format;
    int is_paused;
    uint32_t drwX, drwY;
    uint32_t max_width, max_height; // zero means: not set
    void (*draw_alpha_fnc)(void *ctx, int x0, int y0, int w, int h,
                           unsigned char *src, unsigned char *srca,
                           int stride);
#ifdef HAVE_SHM
    XShmSegmentInfo Shminfo[NUM_BUFFERS];
    int Shmem_Flag;
#endif
};

// FIXME: dynamically allocate this stuff
static void allocate_xvimage(struct vo *, int);



static void draw_alpha_yv12(void *p, int x0, int y0, int w, int h,
                            unsigned char *src, unsigned char *srca,
                            int stride)
{
    struct vo *vo = p;
    struct xvctx *ctx = vo->priv;
    x0 += ctx->image_width * (vo_panscan_x >> 1) / (vo_dwidth + vo_panscan_x);
    vo_draw_alpha_yv12(w, h, src, srca, stride,
                       ctx->xvimage[ctx->current_buf]->data +
                       ctx->xvimage[ctx->current_buf]->offsets[0] +
                       ctx->xvimage[ctx->current_buf]->pitches[0] * y0 + x0,
                       ctx->xvimage[ctx->current_buf]->pitches[0]);
}

static void draw_alpha_yuy2(void *p, int x0, int y0, int w, int h,
                            unsigned char *src, unsigned char *srca,
                            int stride)
{
    struct vo *vo = p;
    struct xvctx *ctx = vo->priv;
    x0 += ctx->image_width * (vo_panscan_x >> 1) / (vo_dwidth + vo_panscan_x);
    vo_draw_alpha_yuy2(w, h, src, srca, stride,
                       ctx->xvimage[ctx->current_buf]->data +
                       ctx->xvimage[ctx->current_buf]->offsets[0] +
                       ctx->xvimage[ctx->current_buf]->pitches[0] * y0 + 2 * x0,
                       ctx->xvimage[ctx->current_buf]->pitches[0]);
}

static void draw_alpha_uyvy(void *p, int x0, int y0, int w, int h,
                            unsigned char *src, unsigned char *srca,
                            int stride)
{
    struct vo *vo = p;
    struct xvctx *ctx = vo->priv;
    x0 += ctx->image_width * (vo_panscan_x >> 1) / (vo_dwidth + vo_panscan_x);
    vo_draw_alpha_yuy2(w, h, src, srca, stride,
                       ctx->xvimage[ctx->current_buf]->data +
                       ctx->xvimage[ctx->current_buf]->offsets[0] +
                       ctx->xvimage[ctx->current_buf]->pitches[0] * y0 + 2 * x0 + 1,
                       ctx->xvimage[ctx->current_buf]->pitches[0]);
}

static void draw_alpha_null(void *p, int x0, int y0, int w, int h,
                            unsigned char *src, unsigned char *srca,
                            int stride)
{
}


static void deallocate_xvimage(struct vo *vo, int foo);

static void calc_drwXY(uint32_t *drwX, uint32_t *drwY) {
  *drwX = *drwY = 0;
  if (vo_fs) {
    aspect(&vo_dwidth, &vo_dheight, A_ZOOM);
    vo_dwidth = FFMIN(vo_dwidth, vo_screenwidth);
    vo_dheight = FFMIN(vo_dheight, vo_screenheight);
    *drwX = (vo_screenwidth - vo_dwidth) / 2;
    *drwY = (vo_screenheight - vo_dheight) / 2;
    mp_msg(MSGT_VO, MSGL_V, "[xv-fs] dx: %d dy: %d dw: %d dh: %d\n",
           *drwX, *drwY, vo_dwidth, vo_dheight);
  } else if (WinID == 0) {
    *drwX = vo_dx;
    *drwY = vo_dy;
  }
}

/*
 * connect to server, create and map window,
 * allocate colors and (shared) memory
 */
static int config(struct vo *vo, uint32_t width, uint32_t height,
                  uint32_t d_width, uint32_t d_height, uint32_t flags,
                  char *title, uint32_t format)
{
    XSizeHints hint;
    XVisualInfo vinfo;
    XGCValues xgcv;
    XSetWindowAttributes xswa;
    XWindowAttributes attribs;
    unsigned long xswamask;
    int depth;
    struct xvctx *ctx = vo->priv;
    int i;

#ifdef HAVE_XF86VM
    int vm = 0;
    unsigned int modeline_width, modeline_height;
    static uint32_t vm_width;
    static uint32_t vm_height;
#endif

    ctx->image_height = height;
    ctx->image_width = width;
    ctx->image_format = format;

    if ((ctx->max_width != 0 && ctx->max_height != 0) &&
        (ctx->image_width > ctx->max_width || ctx->image_height > ctx->max_height))
    {
        mp_msg( MSGT_VO, MSGL_ERR, MSGTR_VO_XV_ImagedimTooHigh,
                ctx->image_width, ctx->image_height, ctx->max_width, ctx->max_height);
        return -1;
    }

    vo_mouse_autohide = 1;

    ctx->is_paused = 0;
    ctx->visible_buf = -1;

#ifdef HAVE_XF86VM
    if (flags & VOFLAG_MODESWITCHING)
        vm = 1;
#endif

    ctx->num_buffers =
        vo_doublebuffering ? (vo_directrendering ? NUM_BUFFERS : 2) : 1;

    /* check image formats */
    ctx->xv_format = 0;
    for (i = 0; i < ctx->formats; i++) {
        mp_msg(MSGT_VO, MSGL_V,
               "Xvideo image format: 0x%x (%4.4s) %s\n", ctx->fo[i].id,
               (char *) &ctx->fo[i].id,
               (ctx->fo[i].format == XvPacked) ? "packed" : "planar");
        if (ctx->fo[i].id == format)
            ctx->xv_format = ctx->fo[i].id;
    }
    if (!ctx->xv_format)
        return -1;

#ifdef HAVE_NEW_GUI
    if (use_gui)
        guiGetEvent(guiSetShVideo, 0);  // let the GUI to setup/resize our window
    else
#endif
    {
        hint.x = vo_dx;
        hint.y = vo_dy;
        hint.width = d_width;
        hint.height = d_height;
#ifdef HAVE_XF86VM
        if (vm)
        {
            if ((d_width == 0) && (d_height == 0))
            {
                vm_width = ctx->image_width;
                vm_height = ctx->image_height;
            } else
            {
                vm_width = d_width;
                vm_height = d_height;
            }
            vo_vm_switch(vm_width, vm_height, &modeline_width,
                         &modeline_height);
            hint.x = (vo_screenwidth - modeline_width) / 2;
            hint.y = (vo_screenheight - modeline_height) / 2;
            hint.width = modeline_width;
            hint.height = modeline_height;
            aspect_save_screenres(modeline_width, modeline_height);
        } else
#warning This "else" makes no sense
#endif
        hint.flags = PPosition | PSize /* | PBaseSize */ ;
        hint.base_width = hint.width;
        hint.base_height = hint.height;
        XGetWindowAttributes(mDisplay, DefaultRootWindow(mDisplay),
                             &attribs);
        depth = attribs.depth;
        if (depth != 15 && depth != 16 && depth != 24 && depth != 32)
            depth = 24;
        XMatchVisualInfo(mDisplay, mScreen, depth, TrueColor, &vinfo);

        xswa.background_pixel = 0;
        if (xv_ck_info.method == CK_METHOD_BACKGROUND)
        {
          xswa.background_pixel = xv_colorkey;
        }
        xswa.border_pixel = 0;
        xswamask = CWBackPixel | CWBorderPixel;

        if (WinID >= 0)
        {
            vo_window = WinID ? ((Window) WinID) : mRootWin;
            if (WinID)
            {
                XUnmapWindow(mDisplay, vo_window);
                XChangeWindowAttributes(mDisplay, vo_window, xswamask,
                                        &xswa);
                vo_x11_selectinput_witherr(mDisplay, vo_window,
                                           StructureNotifyMask |
                                           KeyPressMask |
                                           PropertyChangeMask |
                                           PointerMotionMask |
                                           ButtonPressMask |
                                           ButtonReleaseMask |
                                           ExposureMask);
                XMapWindow(mDisplay, vo_window);
                Window mRoot;
                uint32_t drwBorderWidth, drwDepth;
                XGetGeometry(mDisplay, vo_window, &mRoot,
                             &ctx->drwX, &ctx->drwY, &vo_dwidth, &vo_dheight,
                             &drwBorderWidth, &drwDepth);
                if (vo_dwidth <= 0) vo_dwidth = d_width;
                if (vo_dheight <= 0) vo_dheight = d_height;
                aspect_save_prescale(vo_dwidth, vo_dheight);
            }
        } else
        {
            vo_x11_create_vo_window(&vinfo, vo_dx, vo_dy, d_width, d_height,
                   flags, CopyFromParent, "xv", title);
            XChangeWindowAttributes(mDisplay, vo_window, xswamask, &xswa);
        }

        if (vo_gc != None)
            XFreeGC(mDisplay, vo_gc);
        vo_gc = XCreateGC(mDisplay, vo_window, 0L, &xgcv);
        XSync(mDisplay, False);
#ifdef HAVE_XF86VM
        if (vm)
        {
            /* Grab the mouse pointer in our window */
            if (vo_grabpointer)
                XGrabPointer(mDisplay, vo_window, True, 0,
                             GrabModeAsync, GrabModeAsync,
                             vo_window, None, CurrentTime);
            XSetInputFocus(mDisplay, vo_window, RevertToNone, CurrentTime);
        }
#endif
    }

    mp_msg(MSGT_VO, MSGL_V, "using Xvideo port %d for hw scaling\n",
           xv_port);

    switch (ctx->xv_format)
    {
        case IMGFMT_YV12:
        case IMGFMT_I420:
        case IMGFMT_IYUV:
            ctx->draw_alpha_fnc = draw_alpha_yv12;
            break;
        case IMGFMT_YUY2:
        case IMGFMT_YVYU:
            ctx->draw_alpha_fnc = draw_alpha_yuy2;
            break;
        case IMGFMT_UYVY:
            ctx->draw_alpha_fnc = draw_alpha_uyvy;
            break;
        default:
            ctx->draw_alpha_fnc = draw_alpha_null;
    }

    if (vo_config_count)
        for (i = 0; i < ctx->num_buffers; i++)
            deallocate_xvimage(vo, i);

    for (i = 0; i < ctx->num_buffers; i++)
        allocate_xvimage(vo, i);

    ctx->current_buf = 0;
    ctx->current_ip_buf = 0;

#if 0
    set_gamma_correction();
#endif

    aspect(&vo_dwidth, &vo_dheight, A_NOZOOM);
    if ((flags & VOFLAG_FULLSCREEN) && WinID <= 0) vo_fs = 1;
    calc_drwXY(&ctx->drwX, &ctx->drwY);

    panscan_calc();
    
    vo_xv_draw_colorkey(ctx->drwX - (vo_panscan_x >> 1),
                        ctx->drwY - (vo_panscan_y >> 1),
                        vo_dwidth + vo_panscan_x - 1,
                        vo_dheight + vo_panscan_y - 1);

    mp_msg(MSGT_VO, MSGL_V, "[xv] dx: %d dy: %d dw: %d dh: %d\n", ctx->drwX,
           ctx->drwY, vo_dwidth, vo_dheight);

    if (vo_ontop)
        vo_x11_setlayer(mDisplay, vo_window, vo_ontop);

    return 0;
}

static void allocate_xvimage(struct vo *vo, int foo)
{
    struct xvctx *ctx = vo->priv;
    /*
     * allocate XvImages.  FIXME: no error checking, without
     * mit-shm this will bomb... trzing to fix ::atmos
     */
#ifdef HAVE_SHM
    if (mLocalDisplay && XShmQueryExtension(mDisplay))
        ctx->Shmem_Flag = 1;
    else
    {
        ctx->Shmem_Flag = 0;
        mp_msg(MSGT_VO, MSGL_INFO,
               MSGTR_LIBVO_XV_SharedMemoryNotSupported);
    }
    if (ctx->Shmem_Flag)
    {
        ctx->xvimage[foo] =
            (XvImage *) XvShmCreateImage(mDisplay, xv_port, ctx->xv_format,
                                         NULL, ctx->image_width, ctx->image_height,
                                         &ctx->Shminfo[foo]);

        ctx->Shminfo[foo].shmid =
            shmget(IPC_PRIVATE, ctx->xvimage[foo]->data_size, IPC_CREAT | 0777);
        ctx->Shminfo[foo].shmaddr = (char *) shmat(ctx->Shminfo[foo].shmid, 0, 0);
        ctx->Shminfo[foo].readOnly = False;

        ctx->xvimage[foo]->data = ctx->Shminfo[foo].shmaddr;
        XShmAttach(mDisplay, &ctx->Shminfo[foo]);
        XSync(mDisplay, False);
        shmctl(ctx->Shminfo[foo].shmid, IPC_RMID, 0);
    } else
#endif
    {
        ctx->xvimage[foo] =
            (XvImage *) XvCreateImage(mDisplay, xv_port, ctx->xv_format, NULL,
                                      ctx->image_width, ctx->image_height);
        ctx->xvimage[foo]->data = malloc(ctx->xvimage[foo]->data_size);
        XSync(mDisplay, False);
    }
    memset(ctx->xvimage[foo]->data, 128, ctx->xvimage[foo]->data_size);
    return;
}

static void deallocate_xvimage(struct vo *vo, int foo)
{
    struct xvctx *ctx = vo->priv;
#ifdef HAVE_SHM
    if (ctx->Shmem_Flag)
    {
        XShmDetach(mDisplay, &ctx->Shminfo[foo]);
        shmdt(ctx->Shminfo[foo].shmaddr);
    } else
#endif
    {
        free(ctx->xvimage[foo]->data);
    }
    XFree(ctx->xvimage[foo]);

    XSync(mDisplay, False);
    return;
}

static inline void put_xvimage(struct vo *vo, XvImage *xvi)
{
    struct xvctx *ctx = vo->priv;
#ifdef HAVE_SHM
    if (ctx->Shmem_Flag)
    {
        XvShmPutImage(mDisplay, xv_port, vo_window, vo_gc,
                      xvi, 0, 0, ctx->image_width,
                      ctx->image_height, ctx->drwX - (vo_panscan_x >> 1),
                      ctx->drwY - (vo_panscan_y >> 1), vo_dwidth + vo_panscan_x,
                      vo_dheight + vo_panscan_y,
                      False);
    } else
#endif
    {
        XvPutImage(mDisplay, xv_port, vo_window, vo_gc,
                   xvi, 0, 0, ctx->image_width, ctx->image_height,
                   ctx->drwX - (vo_panscan_x >> 1), ctx->drwY - (vo_panscan_y >> 1),
                   vo_dwidth + vo_panscan_x,
                   vo_dheight + vo_panscan_y);
    }
}

static void check_events(struct vo *vo)
{
    struct xvctx *ctx = vo->priv;
    int e = vo_x11_check_events(mDisplay);

    if (e & VO_EVENT_RESIZE)
    {
        Window mRoot;
        uint32_t drwBorderWidth, drwDepth;
        XGetGeometry(mDisplay, vo_window, &mRoot, &ctx->drwX, &ctx->drwY,
                     &vo_dwidth, &vo_dheight, &drwBorderWidth, &drwDepth);
        mp_msg(MSGT_VO, MSGL_V, "[xv] dx: %d dy: %d dw: %d dh: %d\n", ctx->drwX,
               ctx->drwY, vo_dwidth, vo_dheight);

        calc_drwXY(&ctx->drwX, &ctx->drwY);
    }

    if (e & VO_EVENT_EXPOSE || e & VO_EVENT_RESIZE)
    {
	vo_xv_draw_colorkey(ctx->drwX - (vo_panscan_x >> 1),
			    ctx->drwY - (vo_panscan_y >> 1),
			    vo_dwidth + vo_panscan_x - 1,
			    vo_dheight + vo_panscan_y - 1);
    }

    if ((e & VO_EVENT_EXPOSE || e & VO_EVENT_RESIZE) && ctx->is_paused)
    {
        /* did we already draw a buffer */
        if ( ctx->visible_buf != -1 )
        {
          /* redraw the last visible buffer */
          put_xvimage(vo, ctx->xvimage[ctx->visible_buf]);
        }
    }
}

static void draw_osd(struct vo *vo)
{
    struct xvctx *ctx = vo->priv;

    osd_draw_text(ctx->image_width -
                  ctx->image_width * vo_panscan_x / (vo_dwidth + vo_panscan_x),
                  ctx->image_height, ctx->draw_alpha_fnc, vo);
}

static void flip_page(struct vo *vo)
{
    struct xvctx *ctx = vo->priv;
    put_xvimage(vo, ctx->xvimage[ctx->current_buf]);

    /* remember the currently visible buffer */
    ctx->visible_buf = ctx->current_buf;

    if (ctx->num_buffers > 1)
    {
        ctx->current_buf =
            vo_directrendering ? 0 : ((ctx->current_buf + 1) % ctx->num_buffers);
        XFlush(mDisplay);
    } else
        XSync(mDisplay, False);
    return;
}

static int draw_slice(struct vo *vo, uint8_t * image[], int stride[], int w,
                      int h, int x, int y)
{
    struct xvctx *ctx = vo->priv;
    uint8_t *dst;
    XvImage *current_image = ctx->xvimage[ctx->current_buf];

    dst = current_image->data + current_image->offsets[0] +
        current_image->pitches[0] * y + x;
    memcpy_pic(dst, image[0], w, h, current_image->pitches[0],
               stride[0]);

    x /= 2;
    y /= 2;
    w /= 2;
    h /= 2;

    dst = current_image->data + current_image->offsets[1] +
        current_image->pitches[1] * y + x;
    if (ctx->image_format != IMGFMT_YV12)
        memcpy_pic(dst, image[1], w, h, current_image->pitches[1],
                   stride[1]);
    else
        memcpy_pic(dst, image[2], w, h, current_image->pitches[1],
                   stride[2]);

    dst = current_image->data + current_image->offsets[2] +
        current_image->pitches[2] * y + x;
    if (ctx->image_format == IMGFMT_YV12)
        memcpy_pic(dst, image[1], w, h, current_image->pitches[1],
                   stride[1]);
    else
        memcpy_pic(dst, image[2], w, h, current_image->pitches[1],
                   stride[2]);

    return 0;
}

static int draw_frame(struct vo *vo, uint8_t * src[])
{
    mp_msg(MSGT_VO,MSGL_INFO, MSGTR_LIBVO_XV_DrawFrameCalled);
    return -1;
}

static uint32_t draw_image(struct vo *vo, mp_image_t * mpi)
{
    struct xvctx *ctx = vo->priv;
    if (mpi->flags & MP_IMGFLAG_DIRECT)
    {
        // direct rendering:
        ctx->current_buf = (int) (mpi->priv);        // hack!
        return VO_TRUE;
    }
    if (mpi->flags & MP_IMGFLAG_DRAW_CALLBACK)
        return VO_TRUE;         // done
    if (mpi->flags & MP_IMGFLAG_PLANAR)
    {
        draw_slice(vo, mpi->planes, mpi->stride, mpi->w, mpi->h, 0, 0);
        return VO_TRUE;
    }
    if (mpi->flags & MP_IMGFLAG_YUV)
    {
        // packed YUV:
        memcpy_pic(ctx->xvimage[ctx->current_buf]->data +
                   ctx->xvimage[ctx->current_buf]->offsets[0], mpi->planes[0],
                   mpi->w * (mpi->bpp / 8), mpi->h,
                   ctx->xvimage[ctx->current_buf]->pitches[0], mpi->stride[0]);
        return VO_TRUE;
    }
    return VO_FALSE;            // not (yet) supported
}

static uint32_t get_image(struct xvctx *ctx, mp_image_t * mpi)
{
    int buf = ctx->current_buf;      // we shouldn't change current_buf unless we do DR!

    if (mpi->type == MP_IMGTYPE_STATIC && ctx->num_buffers > 1)
        return VO_FALSE;        // it is not static
    if (mpi->imgfmt != ctx->image_format)
        return VO_FALSE;        // needs conversion :(
//    if(mpi->flags&MP_IMGFLAG_READABLE) return VO_FALSE; // slow video ram
    if (mpi->flags & MP_IMGFLAG_READABLE &&
        (mpi->type == MP_IMGTYPE_IPB || mpi->type == MP_IMGTYPE_IP))
    {
        // reference (I/P) frame of IP or IPB:
        if (ctx->num_buffers < 2)
            return VO_FALSE;    // not enough
        ctx->current_ip_buf ^= 1;
        // for IPB with 2 buffers we can DR only one of the 2 P frames:
        if (mpi->type == MP_IMGTYPE_IPB && ctx->num_buffers < 3
            && ctx->current_ip_buf)
            return VO_FALSE;
        buf = ctx->current_ip_buf;
        if (mpi->type == MP_IMGTYPE_IPB)
            ++buf;              // preserve space for B
    }
    if (mpi->height > ctx->xvimage[buf]->height)
        return VO_FALSE;        //buffer to small
    if (mpi->width * (mpi->bpp / 8) > ctx->xvimage[buf]->pitches[0])
        return VO_FALSE;        //buffer to small
    if ((mpi->flags & (MP_IMGFLAG_ACCEPT_STRIDE | MP_IMGFLAG_ACCEPT_WIDTH))
        || (mpi->width * (mpi->bpp / 8) == ctx->xvimage[buf]->pitches[0]))
    {
        ctx->current_buf = buf;
        XvImage *current_image = ctx->xvimage[ctx->current_buf];
        mpi->planes[0] = current_image->data + current_image->offsets[0];
        mpi->stride[0] = current_image->pitches[0];
        mpi->width = mpi->stride[0] / (mpi->bpp / 8);
        if (mpi->flags & MP_IMGFLAG_PLANAR)
        {
            if (mpi->flags & MP_IMGFLAG_SWAPPED)
            {
                // I420
                mpi->planes[1] =
                    current_image->data +
                    current_image->offsets[1];
                mpi->planes[2] =
                    current_image->data +
                    current_image->offsets[2];
                mpi->stride[1] = current_image->pitches[1];
                mpi->stride[2] = current_image->pitches[2];
            } else
            {
                // YV12
                mpi->planes[1] =
                    current_image->data +
                    current_image->offsets[2];
                mpi->planes[2] =
                    current_image->data +
                    current_image->offsets[1];
                mpi->stride[1] = current_image->pitches[2];
                mpi->stride[2] = current_image->pitches[1];
            }
        }
        mpi->flags |= MP_IMGFLAG_DIRECT;
        mpi->priv = (void *) ctx->current_buf;
//      printf("mga: get_image() SUCCESS -> Direct Rendering ENABLED\n");
        return VO_TRUE;
    }
    return VO_FALSE;
}

static int query_format(struct xvctx *ctx, uint32_t format)
{
    uint32_t i;
    int flag = VFCAP_CSP_SUPPORTED | VFCAP_CSP_SUPPORTED_BY_HW | VFCAP_HWSCALE_UP | VFCAP_HWSCALE_DOWN | VFCAP_OSD | VFCAP_ACCEPT_STRIDE;       // FIXME! check for DOWN

    /* check image formats */
    for (i = 0; i < ctx->formats; i++)
    {
        if (ctx->fo[i].id == format)
            return flag;        //xv_format = fo[i].id;
    }
    return 0;
}

static void uninit(struct vo *vo)
{
    struct xvctx *ctx = vo->priv;
    int i;

    if (!vo_config_count)
        return;
    ctx->visible_buf = -1;
    XvFreeAdaptorInfo(ctx->ai);
    ctx->ai = NULL;
    if (ctx->fo) {
        XFree(ctx->fo);
        ctx->fo = NULL;
    }
    for (i = 0; i < ctx->num_buffers; i++)
        deallocate_xvimage(vo, i);
#ifdef HAVE_XF86VM
    vo_vm_close(mDisplay);
#endif
    mp_input_rm_event_fd(ConnectionNumber(mDisplay));
    vo_x11_uninit();
    free(ctx);
    vo->priv = NULL;
}

static void x11_fd_callback(void *ctx)
{
    return check_events(ctx);
}

static int preinit(struct vo *vo, const char *arg)
{
    XvPortID xv_p;
    int busy_ports = 0;
    unsigned int i;
    strarg_t ck_src_arg = { 0, NULL };
    strarg_t ck_method_arg = { 0, NULL };
    struct xvctx *ctx = calloc(1, sizeof *ctx);
    vo->priv = ctx;

    opt_t subopts[] =
    {  
      /* name         arg type     arg var         test */
      {  "port",      OPT_ARG_INT, &ctx->xv_port,       (opt_test_f)int_pos },
      {  "ck",        OPT_ARG_STR, &ck_src_arg,    xv_test_ck },
      {  "ck-method", OPT_ARG_STR, &ck_method_arg, xv_test_ckm },
      {  NULL }
    };

    xv_port = 0;

    /* parse suboptions */
    if ( subopt_parse( arg, subopts ) != 0 )
    {
      return -1;
    }

    /* modify colorkey settings according to the given options */
    xv_setup_colorkeyhandling( ck_method_arg.str, ck_src_arg.str );

    if (!vo_init())
        return -1;

    /* check for Xvideo extension */
    unsigned int ver, rel, req, ev, err;
    if (Success != XvQueryExtension(mDisplay, &ver, &rel, &req, &ev, &err))
    {
        mp_msg(MSGT_VO, MSGL_ERR,
               MSGTR_LIBVO_XV_XvNotSupportedByX11);
        return -1;
    }

    /* check for Xvideo support */
    if (Success !=
        XvQueryAdaptors(mDisplay, DefaultRootWindow(mDisplay), &ctx->adaptors,
                        &ctx->ai))
    {
        mp_msg(MSGT_VO, MSGL_ERR, MSGTR_LIBVO_XV_XvQueryAdaptorsFailed);
        return -1;
    }

    /* check adaptors */
    if (xv_port)
    {
        int port_found;

        for (port_found = 0, i = 0; !port_found && i < ctx->adaptors; i++)
        {
            if ((ctx->ai[i].type & XvInputMask) && (ctx->ai[i].type & XvImageMask))
            {
                for (xv_p = ctx->ai[i].base_id;
                     xv_p < ctx->ai[i].base_id + ctx->ai[i].num_ports; ++xv_p)
                {
                    if (xv_p == xv_port)
                    {
                        port_found = 1;
                        break;
                    }
                }
            }
        }
        if (port_found)
        {
            if (XvGrabPort(mDisplay, xv_port, CurrentTime))
                xv_port = 0;
        } else
        {
            mp_msg(MSGT_VO, MSGL_WARN,
                   MSGTR_LIBVO_XV_InvalidPortParameter);
            xv_port = 0;
        }
    }

    for (i = 0; i < ctx->adaptors && xv_port == 0; i++)
    {
        if ((ctx->ai[i].type & XvInputMask) && (ctx->ai[i].type & XvImageMask))
        {
            for (xv_p = ctx->ai[i].base_id;
                 xv_p < ctx->ai[i].base_id + ctx->ai[i].num_ports; ++xv_p)
                if (!XvGrabPort(mDisplay, xv_p, CurrentTime))
                {
                    xv_port = xv_p;
                    break;
                } else
                {
                    mp_msg(MSGT_VO, MSGL_WARN,
                           MSGTR_LIBVO_XV_CouldNotGrabPort, (int) xv_p);
                    ++busy_ports;
                }
        }
    }
    if (!xv_port)
    {
        if (busy_ports)
            mp_msg(MSGT_VO, MSGL_ERR,
                   MSGTR_LIBVO_XV_CouldNotFindFreePort);
        else
            mp_msg(MSGT_VO, MSGL_ERR,
                   MSGTR_LIBVO_XV_NoXvideoSupport);
        return -1;
    }

    if ( !vo_xv_init_colorkey() )
    {
      return -1; // bail out, colorkey setup failed
    }
    vo_xv_enable_vsync();
    vo_xv_get_max_img_dim(&ctx->max_width, &ctx->max_height);

    ctx->fo = XvListImageFormats(mDisplay, xv_port, (int *) &ctx->formats);

    mp_input_add_event_fd(ConnectionNumber(mDisplay), x11_fd_callback, vo);
    return 0;
}

static int control(struct vo *vo, uint32_t request, void *data)
{
    struct xvctx *ctx = vo->priv;
    switch (request)
    {
        case VOCTRL_PAUSE:
            return (ctx->is_paused = 1);
        case VOCTRL_RESUME:
            return (ctx->is_paused = 0);
        case VOCTRL_QUERY_FORMAT:
            return query_format(ctx, *((uint32_t *) data));
        case VOCTRL_GET_IMAGE:
            return get_image(ctx, data);
        case VOCTRL_DRAW_IMAGE:
            return draw_image(vo, data);
        case VOCTRL_GUISUPPORT:
            return VO_TRUE;
        case VOCTRL_GET_PANSCAN:
            if (!vo_config_count || !vo_fs)
                return VO_FALSE;
            return VO_TRUE;
        case VOCTRL_FULLSCREEN:
            vo_x11_fullscreen();
            /* indended, fallthrough to update panscan on fullscreen/windowed switch */
        case VOCTRL_SET_PANSCAN:
            if ((vo_fs && (vo_panscan != vo_panscan_amount))
                || (!vo_fs && vo_panscan_amount))
            {
                int old_y = vo_panscan_y;

                panscan_calc();

                if (old_y != vo_panscan_y)
                {
                    vo_x11_clearwindow_part(mDisplay, vo_window,
                                            vo_dwidth + vo_panscan_x - 1,
                                            vo_dheight + vo_panscan_y - 1,
                                            1);
		    vo_xv_draw_colorkey(ctx->drwX - (vo_panscan_x >> 1),
					ctx->drwY - (vo_panscan_y >> 1),
					vo_dwidth + vo_panscan_x - 1,
					vo_dheight + vo_panscan_y - 1);
                    flip_page(vo);
                }
            }
            return VO_TRUE;
        case VOCTRL_SET_EQUALIZER:
            {
                struct voctrl_set_equalizer_args *args = data;
                return vo_xv_set_eq(xv_port, args->name, args->value);
            }
        case VOCTRL_GET_EQUALIZER:
            {
                struct voctrl_get_equalizer_args *args = data;
                return vo_xv_get_eq(xv_port, args->name, args->valueptr);
            }
        case VOCTRL_ONTOP:
            vo_x11_ontop();
            return VO_TRUE;
        case VOCTRL_UPDATE_SCREENINFO:
            update_xinerama_info();
            return VO_TRUE;
    }
    return VO_NOTIMPL;
}

const struct vo_driver video_out_xv = {
    .is_new = 1,
    .info = &info,
    .preinit = preinit,
    .config = config,
    .control = control,
    .draw_frame = draw_frame,
    .draw_slice = draw_slice,
    .draw_osd = draw_osd,
    .flip_page = flip_page,
    .check_events = check_events,
    .uninit = uninit
};
