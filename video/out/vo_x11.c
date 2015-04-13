/*
 * Original author: Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include <libswscale/swscale.h>

#include "config.h"
#include "vo.h"
#include "video/csputils.h"
#include "video/mp_image.h"
#include "video/filter/vf.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <errno.h>

#include "x11_common.h"

#if HAVE_SHM
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>
#endif

#include "sub/osd.h"
#include "sub/draw_bmp.h"

#include "video/sws_utils.h"
#include "video/fmt-conversion.h"

#include "common/msg.h"
#include "input/input.h"
#include "options/options.h"
#include "osdep/timer.h"

struct priv {
    struct vo *vo;

    struct mp_image *original_image;

    /* local data */
    unsigned char *ImageData[2];
    //! original unaligned pointer for free
    unsigned char *ImageDataOrig[2];

    /* X11 related variables */
    XImage *myximage[2];
    int depth, bpp;
    XWindowAttributes attribs;

    uint32_t image_width;
    uint32_t image_height;
    struct mp_image_params in_format;

    struct mp_rect src;
    struct mp_rect dst;
    int src_w, src_h;
    int dst_w, dst_h;
    struct mp_osd_res osd;

    struct mp_sws_context *sws;

    XVisualInfo vinfo;
    int ximage_depth;

    int firstTime;

    int current_buf;
    int num_buffers;

    int Shmem_Flag;
#if HAVE_SHM
    int Shm_Warned_Slow;

    XShmSegmentInfo Shminfo[2];
#endif
};

static bool resize(struct vo *vo);

/* Scan the available visuals on this Display/Screen.  Try to find
 * the 'best' available TrueColor visual that has a decent color
 * depth (at least 15bit).  If there are multiple visuals with depth
 * >= 15bit, we prefer visuals with a smaller color depth. */
static int find_depth_from_visuals(struct vo *vo, Visual ** visual_return)
{
    Display *dpy = vo->x11->display;
    int screen = vo->x11->screen;
    XVisualInfo visual_tmpl;
    XVisualInfo *visuals;
    int nvisuals, i;
    int bestvisual = -1;
    int bestvisual_depth = -1;

    visual_tmpl.screen = screen;
    visual_tmpl.class = TrueColor;
    visuals = XGetVisualInfo(dpy,
                             VisualScreenMask | VisualClassMask,
                             &visual_tmpl, &nvisuals);
    if (visuals != NULL)
    {
        for (i = 0; i < nvisuals; i++)
        {
            MP_VERBOSE(vo,
                  "truecolor visual %#lx, depth %d, R:%lX G:%lX B:%lX\n",
                   visuals[i].visualid, visuals[i].depth,
                   visuals[i].red_mask, visuals[i].green_mask,
                   visuals[i].blue_mask);
            /*
             * Save the visual index and its depth, if this is the first
             * truecolor visul, or a visual that is 'preferred' over the
             * previous 'best' visual.
             */
            if (bestvisual_depth == -1
                || (visuals[i].depth >= 15
                    && (visuals[i].depth < bestvisual_depth
                        || bestvisual_depth < 15)))
            {
                bestvisual = i;
                bestvisual_depth = visuals[i].depth;
            }
        }

        if (bestvisual != -1 && visual_return != NULL)
            *visual_return = visuals[bestvisual].visual;

        XFree(visuals);
    }
    return bestvisual_depth;
}

static bool getMyXImage(struct priv *p, int foo)
{
    struct vo *vo = p->vo;
#if HAVE_SHM && HAVE_XEXT
    if (vo->x11->display_is_local && XShmQueryExtension(vo->x11->display)) {
        p->Shmem_Flag = 1;
        vo->x11->ShmCompletionEvent = XShmGetEventBase(vo->x11->display)
                                    + ShmCompletion;
    } else {
        p->Shmem_Flag = 0;
        MP_WARN(vo, "Shared memory not supported\nReverting to normal Xlib\n");
    }

    if (p->Shmem_Flag) {
        p->myximage[foo] =
            XShmCreateImage(vo->x11->display, p->vinfo.visual, p->depth,
                            ZPixmap, NULL, &p->Shminfo[foo], p->image_width,
                            p->image_height);
        if (p->myximage[foo] == NULL) {
            MP_WARN(vo, "Shared memory error,disabling ( Ximage error )\n");
            goto shmemerror;
        }
        p->Shminfo[foo].shmid = shmget(IPC_PRIVATE,
                                       p->myximage[foo]->bytes_per_line *
                                       p->myximage[foo]->height,
                                       IPC_CREAT | 0777);
        if (p->Shminfo[foo].shmid < 0) {
            XDestroyImage(p->myximage[foo]);
            MP_WARN(vo, "Shared memory error,disabling ( seg id error )\n");
            goto shmemerror;
        }
        p->Shminfo[foo].shmaddr = (char *) shmat(p->Shminfo[foo].shmid, 0, 0);

        if (p->Shminfo[foo].shmaddr == ((char *) -1)) {
            XDestroyImage(p->myximage[foo]);
            MP_WARN(vo, "Shared memory error,disabling ( address error )\n");
            goto shmemerror;
        }
        p->myximage[foo]->data = p->Shminfo[foo].shmaddr;
        p->ImageData[foo] = (unsigned char *) p->myximage[foo]->data;
        p->Shminfo[foo].readOnly = False;
        XShmAttach(vo->x11->display, &p->Shminfo[foo]);

        XSync(vo->x11->display, False);

        shmctl(p->Shminfo[foo].shmid, IPC_RMID, 0);

        if (!p->firstTime) {
            MP_VERBOSE(vo, "Sharing memory.\n");
            p->firstTime = 1;
        }
    } else {
shmemerror:
        p->Shmem_Flag = 0;
#endif
    p->myximage[foo] =
        XCreateImage(vo->x11->display, p->vinfo.visual, p->depth, ZPixmap,
                     0, NULL, p->image_width, p->image_height, 8, 0);
    if (!p->myximage[foo]) {
        MP_WARN(vo, "could not allocate image");
        return false;
    }
    size_t sz = p->myximage[foo]->bytes_per_line * p->image_height + 32;
    p->ImageDataOrig[foo] = calloc(1, sz);
    p->myximage[foo]->data = p->ImageDataOrig[foo] + 16
                           - ((long)p->ImageDataOrig & 15);
    p->ImageData[foo] = p->myximage[foo]->data;
#if HAVE_SHM && HAVE_XEXT
}
#endif
    return true;
}

static void freeMyXImage(struct priv *p, int foo)
{
#if HAVE_SHM && HAVE_XEXT
    struct vo *vo = p->vo;
    if (p->Shmem_Flag) {
        XShmDetach(vo->x11->display, &p->Shminfo[foo]);
        XDestroyImage(p->myximage[foo]);
        shmdt(p->Shminfo[foo].shmaddr);
    } else
#endif
    {
        if (p->myximage[foo]) {
            p->myximage[foo]->data = p->ImageDataOrig[foo];
            XDestroyImage(p->myximage[foo]);
            p->ImageDataOrig[foo] = NULL;
        }
    }
    p->myximage[foo] = NULL;
    p->ImageData[foo] = NULL;
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
    {IMGFMT_BGR8,   BO_NATIVE,    0x00000007, 0x00000038, 0x000000C0},
    {IMGFMT_BGR8,   BO_NONNATIVE, 0x00000007, 0x00000038, 0x000000C0},
    {IMGFMT_RGB8,   BO_NATIVE,    0x000000E0, 0x0000001C, 0x00000003},
    {IMGFMT_RGB8,   BO_NONNATIVE, 0x000000E0, 0x0000001C, 0x00000003},
    {IMGFMT_BGR555, BO_NATIVE,    0x0000001F, 0x000003E0, 0x00007C00},
    {IMGFMT_BGR555, BO_NATIVE,    0x00007C00, 0x000003E0, 0x0000001F},
    {IMGFMT_BGR565, BO_NATIVE,    0x0000001F, 0x000007E0, 0x0000F800},
    {IMGFMT_RGB565, BO_NATIVE,    0x0000F800, 0x000007E0, 0x0000001F},
    {IMGFMT_RGB24,  MSBFirst,     0x00FF0000, 0x0000FF00, 0x000000FF},
    {IMGFMT_RGB24,  LSBFirst,     0x000000FF, 0x0000FF00, 0x00FF0000},
    {IMGFMT_BGR24,  MSBFirst,     0x000000FF, 0x0000FF00, 0x00FF0000},
    {IMGFMT_BGR24,  LSBFirst,     0x00FF0000, 0x0000FF00, 0x000000FF},
    {IMGFMT_RGB32,  BO_NATIVE,    0x000000FF, 0x0000FF00, 0x00FF0000},
    {IMGFMT_RGB32,  BO_NONNATIVE, 0xFF000000, 0x00FF0000, 0x0000FF00},
    {IMGFMT_BGR32,  BO_NATIVE,    0x00FF0000, 0x0000FF00, 0x000000FF},
    {IMGFMT_BGR32,  BO_NONNATIVE, 0x0000FF00, 0x00FF0000, 0xFF000000},
    {IMGFMT_ARGB,   MSBFirst,     0x00FF0000, 0x0000FF00, 0x000000FF},
    {IMGFMT_ARGB,   LSBFirst,     0x0000FF00, 0x00FF0000, 0xFF000000},
    {IMGFMT_ABGR,   MSBFirst,     0x000000FF, 0x0000FF00, 0x00FF0000},
    {IMGFMT_ABGR,   LSBFirst,     0xFF000000, 0x00FF0000, 0x0000FF00},
    {IMGFMT_RGBA,   MSBFirst,     0xFF000000, 0x00FF0000, 0x0000FF00},
    {IMGFMT_RGBA,   LSBFirst,     0x000000FF, 0x0000FF00, 0x00FF0000},
    {IMGFMT_BGRA,   MSBFirst,     0x0000FF00, 0x00FF0000, 0xFF000000},
    {IMGFMT_BGRA,   LSBFirst,     0x00FF0000, 0x0000FF00, 0x000000FF},
    {0}
};

static int reconfig(struct vo *vo, struct mp_image_params *fmt, int flags)
{
    struct priv *p = vo->priv;

    mp_image_unrefp(&p->original_image);

    p->in_format = *fmt;

    XGetWindowAttributes(vo->x11->display, vo->x11->rootwin, &p->attribs);
    p->depth = p->attribs.depth;

    if (p->depth != 15 && p->depth != 16 && p->depth != 24 && p->depth != 32) {
        Visual *visual;

        p->depth = find_depth_from_visuals(vo, &visual);
    }
    if (!XMatchVisualInfo(vo->x11->display, vo->x11->screen, p->depth,
                          TrueColor, &p->vinfo))
        return -1;

    vo_x11_config_vo_window(vo, &p->vinfo, flags, "x11");

    if (!resize(vo))
        return -1;

    return 0;
}

static bool resize(struct vo *vo)
{
    struct priv *p = vo->priv;

    for (int i = 0; i < p->num_buffers; i++)
        freeMyXImage(p, i);

    vo_get_src_dst_rects(vo, &p->src, &p->dst, &p->osd);

    p->src_w = p->src.x1 - p->src.x0;
    p->src_h = p->src.y1 - p->src.y0;
    p->dst_w = p->dst.x1 - p->dst.x0;
    p->dst_h = p->dst.y1 - p->dst.y0;

    // p->osd contains the parameters assuming OSD rendering in window
    // coordinates, but OSD can only be rendered in the intersection
    // between window and video rectangle (i.e. not into panscan borders).
    p->osd.w = p->dst_w;
    p->osd.h = p->dst_h;
    p->osd.mt = MPMIN(0, p->osd.mt);
    p->osd.mb = MPMIN(0, p->osd.mb);
    p->osd.mr = MPMIN(0, p->osd.mr);
    p->osd.ml = MPMIN(0, p->osd.ml);

    mp_input_set_mouse_transform(vo->input_ctx, &p->dst, NULL);

    p->image_width = (p->dst_w + 7) & (~7);
    p->image_height = p->dst_h;

    p->num_buffers = 2;
    for (int i = 0; i < p->num_buffers; i++) {
        if (!getMyXImage(p, i))
            return -1;
    }

    const struct fmt2Xfmtentry_s *fmte = fmt2Xfmt;
    while (fmte->mpfmt) {
        int depth = IMGFMT_RGB_DEPTH(fmte->mpfmt);
        /* bits_per_pixel in X seems to be set to 16 for 15 bit formats
           => force depth to 16 so that only the color masks are used for the format check */
        if (depth == 15)
            depth = 16;

        if (depth == p->myximage[0]->bits_per_pixel &&
            fmte->byte_order == p->myximage[0]->byte_order &&
            fmte->red_mask == p->myximage[0]->red_mask &&
            fmte->green_mask == p->myximage[0]->green_mask &&
            fmte->blue_mask == p->myximage[0]->blue_mask)
            break;
        fmte++;
    }
    if (!fmte->mpfmt) {
        MP_ERR(vo, "X server image format not supported,"
                   " please contact the developers\n");
        return -1;
    }
    p->bpp = p->myximage[0]->bits_per_pixel;

    mp_sws_set_from_cmdline(p->sws, vo->opts->sws_opts);
    p->sws->src = p->in_format;
    p->sws->dst = (struct mp_image_params) {
        .imgfmt = fmte->mpfmt,
        .w = p->dst_w,
        .h = p->dst_h,
        .d_w = p->dst_w,
        .d_h = p->dst_h,
    };
    mp_image_params_guess_csp(&p->sws->dst);

    if (mp_sws_reinit(p->sws) < 0)
        return false;

    vo_x11_clear_background(vo, &p->dst);

    vo->want_redraw = true;
    return true;
}

static void Display_Image(struct priv *p, XImage *myximage)
{
    struct vo *vo = p->vo;

    XImage *x_image = p->myximage[p->current_buf];

#if HAVE_SHM && HAVE_XEXT
    if (p->Shmem_Flag) {
        XShmPutImage(vo->x11->display, vo->x11->window, vo->x11->vo_gc, x_image,
                     0, 0, p->dst.x0, p->dst.y0, p->dst_w, p->dst_h,
                     True);
        vo->x11->ShmCompletionWaitCount++;
    } else
#endif
    {
        XPutImage(vo->x11->display, vo->x11->window, vo->x11->vo_gc, x_image,
                  0, 0, p->dst.x0, p->dst.y0, p->dst_w, p->dst_h);
    }
}

static struct mp_image get_x_buffer(struct priv *p, int buf_index)
{
    struct mp_image img = {0};
    mp_image_set_params(&img, &p->sws->dst);

    img.planes[0] = p->ImageData[buf_index];
    img.stride[0] = p->image_width * ((p->bpp + 7) / 8);

    return img;
}

static void wait_for_completion(struct vo *vo, int max_outstanding)
{
#if HAVE_SHM && HAVE_XEXT
    struct priv *ctx = vo->priv;
    struct vo_x11_state *x11 = vo->x11;
    if (ctx->Shmem_Flag) {
        while (x11->ShmCompletionWaitCount > max_outstanding) {
            if (!ctx->Shm_Warned_Slow) {
                MP_WARN(vo, "can't keep up! Waiting"
                            " for XShm completion events...\n");
                ctx->Shm_Warned_Slow = 1;
            }
            mp_sleep_us(1000);
            vo_x11_check_events(vo);
        }
    }
#endif
}

static void flip_page(struct vo *vo)
{
    struct priv *p = vo->priv;
    Display_Image(p, p->myximage[p->current_buf]);
    p->current_buf = (p->current_buf + 1) % p->num_buffers;

    if (!p->Shmem_Flag)
        XSync(vo->x11->display, False);
}

// Note: REDRAW_FRAME can call this with NULL.
static void draw_image(struct vo *vo, mp_image_t *mpi)
{
    struct priv *p = vo->priv;

    wait_for_completion(vo, p->num_buffers - 1);

    struct mp_image img = get_x_buffer(p, p->current_buf);

    if (mpi) {
        struct mp_image src = *mpi;
        struct mp_rect src_rc = p->src;
        src_rc.x0 = MP_ALIGN_DOWN(src_rc.x0, src.fmt.align_x);
        src_rc.y0 = MP_ALIGN_DOWN(src_rc.y0, src.fmt.align_y);
        mp_image_crop_rc(&src, src_rc);

        mp_sws_scale(p->sws, &img, &src);
    } else {
        mp_image_clear(&img, 0, 0, img.w, img.h);
    }

    osd_draw_on_image(vo->osd, p->osd, mpi ? mpi->pts : 0, 0, &img);

    if (mpi != p->original_image) {
        talloc_free(p->original_image);
        p->original_image = mpi;
    }
}

static int query_format(struct vo *vo, int format)
{
    MP_DBG(vo, "query_format was called: %x (%s)\n", format,
            vo_format_name(format));
    if (IMGFMT_IS_RGB(format)) {
        for (int n = 0; fmt2Xfmt[n].mpfmt; n++) {
            if (fmt2Xfmt[n].mpfmt == format)
                return 1;
        }
    }

    if (sws_isSupportedInput(imgfmt2pixfmt(format)))
        return 1;
    return 0;
}

static void find_x11_depth(struct vo *vo)
{
    struct vo_x11_state *x11 = vo->x11;
    struct priv *p = vo->priv;
    XImage *mXImage = NULL;
    int depth, bpp, ximage_depth;
    unsigned int mask;
    XWindowAttributes attribs;

    // get color depth (from root window, or the best visual):
    XGetWindowAttributes(x11->display, x11->rootwin, &attribs);
    depth = attribs.depth;

    if (depth != 15 && depth != 16 && depth != 24 && depth != 32)
    {
        Visual *visual;

        depth = find_depth_from_visuals(vo, &visual);
        if (depth != -1)
            mXImage = XCreateImage(x11->display, visual, depth, ZPixmap,
                                   0, NULL, 1, 1, 8, 1);
    } else
        mXImage =
            XGetImage(x11->display, x11->rootwin, 0, 0, 1, 1, AllPlanes, ZPixmap);

    ximage_depth = depth;   // display depth on screen

    // get bits/pixel from XImage structure:
    if (mXImage == NULL)
    {
        mask = 0;
    } else
    {
        /* for the depth==24 case, the XImage structures might use
         * 24 or 32 bits of data per pixel. */
        bpp = mXImage->bits_per_pixel;
        if ((ximage_depth + 7) / 8 != (bpp + 7) / 8)
            ximage_depth = bpp;     // by A'rpi
        mask =
            mXImage->red_mask | mXImage->green_mask | mXImage->blue_mask;
        MP_VERBOSE(vo, "color mask:  %X  (R:%lX G:%lX B:%lX)\n", mask,
               mXImage->red_mask, mXImage->green_mask, mXImage->blue_mask);
        XDestroyImage(mXImage);
    }
    if (((ximage_depth + 7) / 8) == 2)
    {
        if (mask == 0x7FFF)
            ximage_depth = 15;
        else if (mask == 0xFFFF)
            ximage_depth = 16;
    }

    MP_VERBOSE(vo, "depth %d and %d bpp.\n", depth, ximage_depth);

    p->ximage_depth = ximage_depth;
}

static void uninit(struct vo *vo)
{
    struct priv *p = vo->priv;
    if (p->myximage[0])
        freeMyXImage(p, 0);
    if (p->myximage[1])
        freeMyXImage(p, 1);

    talloc_free(p->original_image);

    vo_x11_uninit(vo);
}

static int preinit(struct vo *vo)
{
    struct priv *p = vo->priv;
    p->vo = vo;

    if (!vo_x11_init(vo))
        return -1;              // Can't open X11
    find_x11_depth(vo);
    p->sws = mp_sws_alloc(vo);
    return 0;
}

static int control(struct vo *vo, uint32_t request, void *data)
{
    struct priv *p = vo->priv;
    switch (request) {
    case VOCTRL_SET_EQUALIZER:
    {
        struct voctrl_set_equalizer_args *args = data;
        struct vf_seteq eq = {args->name, args->value};
        if (mp_sws_set_vf_equalizer(p->sws, &eq) == 0)
            break;
        return true;
    }
    case VOCTRL_GET_EQUALIZER:
    {
        struct voctrl_get_equalizer_args *args = data;
        struct vf_seteq eq = {args->name};
        if (mp_sws_get_vf_equalizer(p->sws, &eq) == 0)
            break;
        *(int *)args->valueptr = eq.value;
        return true;
    }
    case VOCTRL_GET_PANSCAN:
        return VO_TRUE;
    case VOCTRL_SET_PANSCAN:
        if (vo->config_ok)
            resize(vo);
        return VO_TRUE;
    case VOCTRL_REDRAW_FRAME:
        draw_image(vo, p->original_image);
        return true;
    }

    int events = 0;
    int r = vo_x11_control(vo, &events, request, data);
    if (vo->config_ok && (events & (VO_EVENT_EXPOSE | VO_EVENT_RESIZE)))
        resize(vo);
    vo_event(vo, events);
    return r;
}

const struct vo_driver video_out_x11 = {
    .description = "X11 ( XImage/Shm )",
    .name = "x11",
    .priv_size = sizeof(struct priv),
    .options = (const struct m_option []){{0}},
    .preinit = preinit,
    .query_format = query_format,
    .reconfig = reconfig,
    .control = control,
    .draw_image = draw_image,
    .flip_page = flip_page,
    .uninit = uninit,
};
