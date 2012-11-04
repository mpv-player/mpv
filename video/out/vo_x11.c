/*
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
#include <sys/types.h>

#include "config.h"
#include "vo.h"
#include "aspect.h"
#include "video/csputils.h"
#include "video/mp_image.h"
#include "video/vfcap.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <errno.h>

#include "x11_common.h"

#ifdef HAVE_SHM
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>
#endif

#include "sub/sub.h"
#include "sub/draw_bmp.h"

#include "video/sws_utils.h"
#define MODE_RGB  0x1
#define MODE_BGR  0x2

#include "core/mp_msg.h"

extern int sws_flags;

struct priv {
    struct vo *vo;

    struct mp_draw_sub_backup *osd_backup;

    /* local data */
    unsigned char *ImageData;
    //! original unaligned pointer for free
    unsigned char *ImageDataOrig;

    /* X11 related variables */
    XImage *myximage;
    int depth, bpp;
    XWindowAttributes attribs;

    int int_pause;

    int Flip_Flag;
    int zoomFlag;

    uint32_t image_width;
    uint32_t image_height;
    uint32_t in_format;
    uint32_t out_format;
    int out_offset;
    int srcW;
    int srcH;

    int old_vo_dwidth;
    int old_vo_dheight;

    struct SwsContext *swsContext;
    int dst_width;

    XVisualInfo vinfo;

    int firstTime;

#ifdef HAVE_SHM
    int Shmem_Flag;

    XShmSegmentInfo Shminfo[1];
    int gXErrorFlag;
    int CompletionType;
#endif
};

static void flip_page(struct vo *vo);

static void check_events(struct vo *vo)
{
    struct priv *p = vo->priv;

    int ret = vo_x11_check_events(vo);

    if (ret & VO_EVENT_RESIZE)
        vo_x11_clearwindow(vo, vo->x11->window);
    else if (ret & VO_EVENT_EXPOSE)
        vo_x11_clearwindow_part(vo, vo->x11->window, p->myximage->width,
                                p->myximage->height);
    if (ret & VO_EVENT_EXPOSE && p->int_pause)
        flip_page(vo);
}

static void getMyXImage(struct priv *p)
{
    struct vo *vo = p->vo;
#ifdef HAVE_SHM
    if (vo->x11->display_is_local && XShmQueryExtension(vo->x11->display))
        p->Shmem_Flag = 1;
    else {
        p->Shmem_Flag = 0;
        mp_msg(MSGT_VO, MSGL_WARN,
               "Shared memory not supported\nReverting to normal Xlib\n");
    }
    if (p->Shmem_Flag)
        p->CompletionType = XShmGetEventBase(vo->x11->display) + ShmCompletion;

    if (p->Shmem_Flag) {
        p->myximage =
            XShmCreateImage(vo->x11->display, p->vinfo.visual, p->depth,
                            ZPixmap, NULL, &p->Shminfo[0], p->image_width,
                            p->image_height);
        if (p->myximage == NULL) {
            mp_msg(MSGT_VO, MSGL_WARN,
                   "Shared memory error,disabling ( Ximage error )\n");
            goto shmemerror;
        }
        p->Shminfo[0].shmid = shmget(IPC_PRIVATE,
                                     p->myximage->bytes_per_line *
                                        p->myximage->height,
                                     IPC_CREAT | 0777);
        if (p->Shminfo[0].shmid < 0) {
            XDestroyImage(p->myximage);
            mp_msg(MSGT_VO, MSGL_V, "%s\n", strerror(errno));
            //perror( strerror( errno ) );
            mp_msg(MSGT_VO, MSGL_WARN,
                   "Shared memory error,disabling ( seg id error )\n");
            goto shmemerror;
        }
        p->Shminfo[0].shmaddr = (char *) shmat(p->Shminfo[0].shmid, 0, 0);

        if (p->Shminfo[0].shmaddr == ((char *) -1)) {
            XDestroyImage(p->myximage);
            if (p->Shminfo[0].shmaddr != ((char *) -1))
                shmdt(p->Shminfo[0].shmaddr);
            mp_msg(MSGT_VO, MSGL_WARN,
                   "Shared memory error,disabling ( address error )\n");
            goto shmemerror;
        }
        p->myximage->data = p->Shminfo[0].shmaddr;
        p->ImageData = (unsigned char *) p->myximage->data;
        p->Shminfo[0].readOnly = False;
        XShmAttach(vo->x11->display, &p->Shminfo[0]);

        XSync(vo->x11->display, False);

        if (p->gXErrorFlag) {
            XDestroyImage(p->myximage);
            shmdt(p->Shminfo[0].shmaddr);
            mp_msg(MSGT_VO, MSGL_WARN, "Shared memory error,disabling.\n");
            p->gXErrorFlag = 0;
            goto shmemerror;
        } else
            shmctl(p->Shminfo[0].shmid, IPC_RMID, 0);

        if (!p->firstTime) {
            mp_msg(MSGT_VO, MSGL_V, "Sharing memory.\n");
            p->firstTime = 1;
        }
    } else {
shmemerror:
        p->Shmem_Flag = 0;
#endif
    p->myximage =
        XCreateImage(vo->x11->display, p->vinfo.visual, p->depth, ZPixmap,
                     0, NULL, p->image_width, p->image_height, 8, 0);
    p->ImageDataOrig =
        malloc(p->myximage->bytes_per_line * p->image_height + 32);
    p->myximage->data = p->ImageDataOrig + 16 - ((long)p->ImageDataOrig & 15);
    memset(p->myximage->data, 0, p->myximage->bytes_per_line * p->image_height);
    p->ImageData = p->myximage->data;
#ifdef HAVE_SHM
}
#endif
}

static void freeMyXImage(struct priv *p)
{
    struct vo *vo = p->vo;
#ifdef HAVE_SHM
    if (p->Shmem_Flag) {
        XShmDetach(vo->x11->display, &p->Shminfo[0]);
        XDestroyImage(p->myximage);
        shmdt(p->Shminfo[0].shmaddr);
    } else
#endif
    {
        p->myximage->data = p->ImageDataOrig;
        XDestroyImage(p->myximage);
        p->ImageDataOrig = NULL;
    }
    p->myximage = NULL;
    p->ImageData = NULL;
}

#if BYTE_ORDER == BIG_ENDIAN
#define BO_NATIVE    MSBFirst
#define BO_NONNATIVE LSBFirst
#else
#define BO_NATIVE    LSBFirst
#define BO_NONNATIVE MSBFirst
#endif
const struct fmt2Xfmtentry_s {
    uint32_t mpfmt;
    int byte_order;
    unsigned red_mask;
    unsigned green_mask;
    unsigned blue_mask;
} fmt2Xfmt[] = {
    {IMGFMT_RGB8,  BO_NATIVE,    0x00000007, 0x00000038, 0x000000C0},
    {IMGFMT_RGB8,  BO_NONNATIVE, 0x00000007, 0x00000038, 0x000000C0},
    {IMGFMT_BGR8,  BO_NATIVE,    0x000000E0, 0x0000001C, 0x00000003},
    {IMGFMT_BGR8,  BO_NONNATIVE, 0x000000E0, 0x0000001C, 0x00000003},
    {IMGFMT_RGB15, BO_NATIVE,    0x0000001F, 0x000003E0, 0x00007C00},
    {IMGFMT_BGR15, BO_NATIVE,    0x00007C00, 0x000003E0, 0x0000001F},
    {IMGFMT_RGB16, BO_NATIVE,    0x0000001F, 0x000007E0, 0x0000F800},
    {IMGFMT_BGR16, BO_NATIVE,    0x0000F800, 0x000007E0, 0x0000001F},
    {IMGFMT_RGB24, MSBFirst,     0x00FF0000, 0x0000FF00, 0x000000FF},
    {IMGFMT_RGB24, LSBFirst,     0x000000FF, 0x0000FF00, 0x00FF0000},
    {IMGFMT_BGR24, MSBFirst,     0x000000FF, 0x0000FF00, 0x00FF0000},
    {IMGFMT_BGR24, LSBFirst,     0x00FF0000, 0x0000FF00, 0x000000FF},
    {IMGFMT_RGB32, BO_NATIVE,    0x000000FF, 0x0000FF00, 0x00FF0000},
    {IMGFMT_RGB32, BO_NONNATIVE, 0xFF000000, 0x00FF0000, 0x0000FF00},
    {IMGFMT_BGR32, BO_NATIVE,    0x00FF0000, 0x0000FF00, 0x000000FF},
    {IMGFMT_BGR32, BO_NONNATIVE, 0x0000FF00, 0x00FF0000, 0xFF000000},
    {IMGFMT_ARGB,  MSBFirst,     0x00FF0000, 0x0000FF00, 0x000000FF},
    {IMGFMT_ARGB,  LSBFirst,     0x0000FF00, 0x00FF0000, 0xFF000000},
    {IMGFMT_ABGR,  MSBFirst,     0x000000FF, 0x0000FF00, 0x00FF0000},
    {IMGFMT_ABGR,  LSBFirst,     0xFF000000, 0x00FF0000, 0x0000FF00},
    {IMGFMT_RGBA,  MSBFirst,     0xFF000000, 0x00FF0000, 0x0000FF00},
    {IMGFMT_RGBA,  LSBFirst,     0x000000FF, 0x0000FF00, 0x00FF0000},
    {IMGFMT_BGRA,  MSBFirst,     0x0000FF00, 0x00FF0000, 0xFF000000},
    {IMGFMT_BGRA,  LSBFirst,     0x00FF0000, 0x0000FF00, 0x000000FF},
    {0}
};

static int config(struct vo *vo, uint32_t width, uint32_t height,
                  uint32_t d_width, uint32_t d_height, uint32_t flags,
                  uint32_t format)
{
    struct priv *p = vo->priv;

    Colormap theCmap;
    const struct fmt2Xfmtentry_s *fmte = fmt2Xfmt;

#ifdef CONFIG_XF86VM
    int vm = flags & VOFLAG_MODESWITCHING;
#endif
    p->Flip_Flag = flags & VOFLAG_FLIPPING;
    p->zoomFlag = 1;

    p->old_vo_dwidth = -1;
    p->old_vo_dheight = -1;

    p->in_format = format;
    p->srcW = width;
    p->srcH = height;

    XGetWindowAttributes(vo->x11->display, vo->x11->rootwin, &p->attribs);
    p->depth = p->attribs.depth;

    if (p->depth != 15 && p->depth != 16 && p->depth != 24 && p->depth != 32) {
        Visual *visual;

        p->depth = vo_find_depth_from_visuals(vo->x11->display, vo->x11->screen,
                                              &visual);
    }
    if (!XMatchVisualInfo(vo->x11->display, vo->x11->screen, p->depth,
                          DirectColor, &p->vinfo)
         || (WinID > 0
             && p->vinfo.visualid != XVisualIDFromVisual(p->attribs.visual)))
    {
        XMatchVisualInfo(vo->x11->display, vo->x11->screen, p->depth, TrueColor,
                         &p->vinfo);
    }

    /* set image size (which is indeed neither the input nor output size),
       if zoom is on it will be changed during draw_slice anyway so we don't
       duplicate the aspect code here
     */
    p->image_width = (width + 7) & (~7);
    p->image_height = height;

    {
#ifdef CONFIG_XF86VM
        if (vm)
            vo_vm_switch(vo);

#endif
        theCmap = vo_x11_create_colormap(vo, &p->vinfo);

        vo_x11_create_vo_window(vo, &p->vinfo, vo->dx, vo->dy, vo->dwidth,
                                vo->dheight, flags, theCmap, "x11");
        if (WinID > 0)
            p->depth = vo_x11_update_geometry(vo, true);

#ifdef CONFIG_XF86VM
        if (vm) {
            /* Grab the mouse pointer in our window */
            if (vo_grabpointer)
                XGrabPointer(vo->x11->display, vo->x11->window, True, 0,
                             GrabModeAsync, GrabModeAsync,
                             vo->x11->window, None, CurrentTime);
            XSetInputFocus(vo->x11->display, vo->x11->window, RevertToNone,
                           CurrentTime);
        }
#endif
    }

    if (p->myximage) {
        freeMyXImage(p);
        sws_freeContext(p->swsContext);
    }
    getMyXImage(p);

    while (fmte->mpfmt) {
        int depth = IMGFMT_RGB_DEPTH(fmte->mpfmt);
        /* bits_per_pixel in X seems to be set to 16 for 15 bit formats
           => force depth to 16 so that only the color masks are used for the format check */
        if (depth == 15)
            depth = 16;

        if (depth == p->myximage->bits_per_pixel &&
            fmte->byte_order == p->myximage->byte_order &&
            fmte->red_mask == p->myximage->red_mask &&
            fmte->green_mask == p->myximage->green_mask &&
            fmte->blue_mask == p->myximage->blue_mask)
            break;
        fmte++;
    }
    if (!fmte->mpfmt) {
        mp_msg(
            MSGT_VO, MSGL_ERR,
            "X server image format not supported, please contact the developers\n");
        return -1;
    }
    p->out_format = fmte->mpfmt;
    p->bpp = p->myximage->bits_per_pixel;
    p->out_offset = 0;
    // We can easily "emulate" non-native RGB32 and BGR32
    if (p->out_format == (IMGFMT_BGR32 | 128)
        || p->out_format == (IMGFMT_RGB32 | 128))
    {
        p->out_format &= ~128;
#if BYTE_ORDER == BIG_ENDIAN
        p->out_offset = 1;
#else
        p->out_offset = -1;
#endif
    }

    /* always allocate swsContext as size could change between frames */
    p->swsContext = sws_getContextFromCmdLine(width, height, p->in_format,
                                              width, height, p->out_format);
    if (!p->swsContext)
        return -1;

    p->dst_width = width;

    return 0;
}

static void Display_Image(struct priv *p, XImage *myximage, uint8_t *ImageData)
{
    struct vo *vo = p->vo;

    int x = (vo->dwidth - p->dst_width) / 2;
    int y = (vo->dheight - p->myximage->height) / 2;

    // do not draw if the image needs rescaling
    if ((p->old_vo_dwidth != vo->dwidth ||
         p->old_vo_dheight != vo->dheight) && p->zoomFlag)
        return;

    if (WinID == 0) {
        x = vo->dx;
        y = vo->dy;
    }
    p->myximage->data += p->out_offset;
#ifdef HAVE_SHM
    if (p->Shmem_Flag) {
        XShmPutImage(vo->x11->display, vo->x11->window, vo->x11->vo_gc,
                     p->myximage, 0, 0, x, y, p->dst_width, p->myximage->height,
                     True);
    } else
#endif
    {
        XPutImage(vo->x11->display, vo->x11->window, vo->x11->vo_gc,
                  p->myximage, 0, 0, x, y, p->dst_width, p->myximage->height);
    }
    p->myximage->data -= p->out_offset;
}

static struct mp_image get_x_buffer(struct priv *p)
{
    struct mp_image img = {0};
    img.w = img.width = p->image_width;
    img.h = img.height = p->image_height;
    mp_image_setfmt(&img, p->out_format);

    img.planes[0] = p->ImageData;
    img.stride[0] = p->image_width * ((p->bpp + 7) / 8);

    return img;
}

static void draw_osd(struct vo *vo, struct osd_state *osd)
{
    struct priv *p = vo->priv;

    struct mp_image img = get_x_buffer(p);

    struct mp_osd_res res = {
        .w = img.w,
        .h = img.h,
        .display_par = vo->monitor_par,
        .video_par = vo->aspdat.par,
    };

    osd_draw_on_image_bk(osd, res, osd->vo_pts, 0, p->osd_backup, &img);
}

static mp_image_t *get_screenshot(struct vo *vo)
{
    struct priv *p = vo->priv;

    struct mp_image img = get_x_buffer(p);
    struct mp_image *res = alloc_mpi(img.w, img.h, img.imgfmt);
    copy_mpi(res, &img);
    mp_draw_sub_backup_restore(p->osd_backup, res);

    return res;
}

static int redraw_frame(struct vo *vo)
{
    struct priv *p = vo->priv;

    struct mp_image img = get_x_buffer(p);
    mp_draw_sub_backup_restore(p->osd_backup, &img);

    return true;
}

static void flip_page(struct vo *vo)
{
    struct priv *p = vo->priv;
    Display_Image(p, p->myximage, p->ImageData);
    XSync(vo->x11->display, False);
}

static int draw_slice(struct vo *vo, uint8_t *src[], int stride[], int w, int h,
                      int x, int y)
{
    struct priv *p = vo->priv;
    uint8_t *dst[MP_MAX_PLANES] = {NULL};
    int dstStride[MP_MAX_PLANES] = {0};

    if ((p->old_vo_dwidth != vo->dwidth || p->old_vo_dheight != vo->dheight)
        /*&& y==0 */ && p->zoomFlag)
    {
        int newW = vo->dwidth;
        int newH = vo->dheight;
        struct SwsContext *oldContext = p->swsContext;

        p->old_vo_dwidth = vo->dwidth;
        p->old_vo_dheight = vo->dheight;

        if (vo_fs)
            aspect(vo, &newW, &newH, A_ZOOM);
        if (sws_flags == 0)
            newW &= (~31);      // not needed but, if the user wants the FAST_BILINEAR SCALER, then its needed

        p->swsContext
            = sws_getContextFromCmdLine(p->srcW, p->srcH, p->in_format, newW,
                                        newH, p->out_format);
        if (p->swsContext) {
            p->image_width = (newW + 7) & (~7);
            p->image_height = newH;

            freeMyXImage(p);
            getMyXImage(p);
            sws_freeContext(oldContext);
        } else
            p->swsContext = oldContext;
        p->dst_width = newW;
    }

    dstStride[0] = p->image_width * ((p->bpp + 7) / 8);
    dst[0] = p->ImageData;
    if (p->Flip_Flag) {
        dst[0] += dstStride[0] * (p->image_height - 1);
        dstStride[0] = -dstStride[0];
    }
    sws_scale(p->swsContext, (const uint8_t **)src, stride, y, h, dst,
              dstStride);
    mp_draw_sub_backup_reset(p->osd_backup);
    return 0;
}

static void draw_image(struct vo *vo, mp_image_t *mpi)
{
    draw_slice(vo, mpi->planes, mpi->stride, mpi->w, mpi->h, 0, 0);
}

static int query_format(struct vo *vo, uint32_t format)
{
    mp_msg(MSGT_VO, MSGL_DBG2,
           "vo_x11: query_format was called: %x (%s)\n", format,
           vo_format_name(format));
    if (IMGFMT_IS_BGR(format)) {
        if (IMGFMT_BGR_DEPTH(format) <= 8)
            return 0;           // TODO 8bpp not yet fully implemented
        if (IMGFMT_BGR_DEPTH(format) == vo->x11->depthonscreen)
            return VFCAP_CSP_SUPPORTED | VFCAP_CSP_SUPPORTED_BY_HW |
                   VFCAP_OSD | VFCAP_FLIP |
                   VFCAP_ACCEPT_STRIDE;
        else
            return VFCAP_CSP_SUPPORTED | VFCAP_OSD |
                   VFCAP_FLIP |
                   VFCAP_ACCEPT_STRIDE;
    }

    switch (format) {
    case IMGFMT_I420:
    case IMGFMT_IYUV:
    case IMGFMT_YV12:
        return VFCAP_CSP_SUPPORTED | VFCAP_OSD |
               VFCAP_ACCEPT_STRIDE;
    }
    return 0;
}


static void uninit(struct vo *vo)
{
    struct priv *p = vo->priv;
    if (p->myximage)
        freeMyXImage(p);

#ifdef CONFIG_XF86VM
    vo_vm_close(vo);
#endif

    p->zoomFlag = 0;
    vo_x11_uninit(vo);

    sws_freeContext(p->swsContext);
}

static int preinit(struct vo *vo, const char *arg)
{
    struct priv *p = vo->priv;
    p->vo = vo;

    if (arg) {
        mp_msg(MSGT_VO, MSGL_ERR, "vo_x11: Unknown subdevice: %s\n", arg);
        return ENOSYS;
    }

    p->osd_backup = talloc_steal(p, mp_draw_sub_backup_new());

    if (!vo_init(vo))
        return -1;              // Can't open X11
    return 0;
}

static int control(struct vo *vo, uint32_t request, void *data)
{
    struct priv *p = vo->priv;
    switch (request) {
    case VOCTRL_PAUSE:
        return p->int_pause = 1;
    case VOCTRL_RESUME:
        return p->int_pause = 0;
    case VOCTRL_FULLSCREEN:
        vo_x11_fullscreen(vo);
        vo_x11_clearwindow(vo, vo->x11->window);
        return VO_TRUE;
    case VOCTRL_SET_EQUALIZER:
    {
        struct voctrl_set_equalizer_args *args = data;
        return vo_x11_set_equalizer(vo, args->name, args->value);
    }
    case VOCTRL_GET_EQUALIZER:
    {
        struct voctrl_get_equalizer_args *args = data;
        return vo_x11_get_equalizer(args->name, args->valueptr);
    }
    case VOCTRL_ONTOP:
        vo_x11_ontop(vo);
        return VO_TRUE;
    case VOCTRL_UPDATE_SCREENINFO:
        update_xinerama_info(vo);
        return VO_TRUE;
    case VOCTRL_REDRAW_FRAME:
        return redraw_frame(vo);
    case VOCTRL_SCREENSHOT: {
        struct voctrl_screenshot_args *args = data;
        args->out_image = get_screenshot(vo);
        return true;
    }
    }
    return VO_NOTIMPL;
}

const struct vo_driver video_out_x11 = {
    .info = &(const vo_info_t) {
        "X11 ( XImage/Shm )",
        "x11",
        "Aaron Holtzman <aholtzma@ess.engr.uvic.ca>",
        ""
    },
    .priv_size = sizeof(struct priv),
    .priv_defaults = &(const struct priv) {
        .srcW = -1,
        .srcH = -1,
        .old_vo_dwidth = -1,
        .old_vo_dheight = -1,
#ifdef HAVE_SHM
        .CompletionType = -1,
#endif
    },
    .preinit = preinit,
    .query_format = query_format,
    .config = config,
    .control = control,
    .draw_image = draw_image,
    .draw_slice = draw_slice,
    .draw_osd = draw_osd,
    .flip_page = flip_page,
    .check_events = check_events,
    .uninit = uninit,
};
