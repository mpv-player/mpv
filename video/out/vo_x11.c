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

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <errno.h>

#include "x11_common.h"

#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>

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

    XImage *myximage[2];
    int depth;
    GC gc;

    uint32_t image_width;
    uint32_t image_height;

    struct mp_rect src;
    struct mp_rect dst;
    int src_w, src_h;
    int dst_w, dst_h;
    struct mp_osd_res osd;

    struct mp_sws_context *sws;

    XVisualInfo vinfo;

    int current_buf;
    bool reset_view;

    int Shmem_Flag;
    XShmSegmentInfo Shminfo[2];
    int Shm_Warned_Slow;
};

static bool resize(struct vo *vo);

static bool getMyXImage(struct priv *p, int foo)
{
    struct vo *vo = p->vo;
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
        p->Shminfo[foo].readOnly = False;
        XShmAttach(vo->x11->display, &p->Shminfo[foo]);

        XSync(vo->x11->display, False);

        shmctl(p->Shminfo[foo].shmid, IPC_RMID, 0);
    } else {
shmemerror:
        p->Shmem_Flag = 0;

        MP_VERBOSE(vo, "Not using SHM.\n");
        p->myximage[foo] =
            XCreateImage(vo->x11->display, p->vinfo.visual, p->depth, ZPixmap,
                         0, NULL, p->image_width, p->image_height, 8, 0);
        if (!p->myximage[foo]) {
            MP_WARN(vo, "could not allocate image");
            return false;
        }
        p->myximage[foo]->data =
            calloc(1, p->myximage[foo]->bytes_per_line * p->image_height + 32);
    }
    return true;
}

static void freeMyXImage(struct priv *p, int foo)
{
    struct vo *vo = p->vo;
    if (p->Shmem_Flag) {
        XShmDetach(vo->x11->display, &p->Shminfo[foo]);
        XDestroyImage(p->myximage[foo]);
        shmdt(p->Shminfo[foo].shmaddr);
    } else {
        if (p->myximage[foo])
            XDestroyImage(p->myximage[foo]);
    }
    p->myximage[foo] = NULL;
}

const struct fmt_entry {
    uint32_t mpfmt;
    int depth;
    int byte_order;
    unsigned red_mask;
    unsigned green_mask;
    unsigned blue_mask;
} mp_to_x_fmt[] = {
    {IMGFMT_0RGB,   32, MSBFirst,     0x00FF0000, 0x0000FF00, 0x000000FF},
    {IMGFMT_0RGB,   32, LSBFirst,     0x0000FF00, 0x00FF0000, 0xFF000000},
    {IMGFMT_0BGR,   32, MSBFirst,     0x000000FF, 0x0000FF00, 0x00FF0000},
    {IMGFMT_0BGR,   32, LSBFirst,     0xFF000000, 0x00FF0000, 0x0000FF00},
    {IMGFMT_RGB0,   32, MSBFirst,     0xFF000000, 0x00FF0000, 0x0000FF00},
    {IMGFMT_RGB0,   32, LSBFirst,     0x000000FF, 0x0000FF00, 0x00FF0000},
    {IMGFMT_BGR0,   32, MSBFirst,     0x0000FF00, 0x00FF0000, 0xFF000000},
    {IMGFMT_BGR0,   32, LSBFirst,     0x00FF0000, 0x0000FF00, 0x000000FF},
    {IMGFMT_RGB565, 16, LSBFirst,     0x0000F800, 0x000007E0, 0x0000001F},
    {0}
};

static int reconfig(struct vo *vo, struct mp_image_params *fmt)
{
    struct priv *p = vo->priv;

    mp_image_unrefp(&p->original_image);

    p->sws->src = *fmt;

    vo_x11_config_vo_window(vo);

    if (!resize(vo))
        return -1;

    return 0;
}

static bool resize(struct vo *vo)
{
    struct priv *p = vo->priv;

    for (int i = 0; i < 2; i++)
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

    for (int i = 0; i < 2; i++) {
        if (!getMyXImage(p, i))
            return -1;
    }

    const struct fmt_entry *fmte = mp_to_x_fmt;
    while (fmte->mpfmt) {
        if (fmte->depth == p->myximage[0]->bits_per_pixel &&
            fmte->byte_order == p->myximage[0]->byte_order &&
            fmte->red_mask == p->myximage[0]->red_mask &&
            fmte->green_mask == p->myximage[0]->green_mask &&
            fmte->blue_mask == p->myximage[0]->blue_mask)
            break;
        fmte++;
    }
    if (!fmte->mpfmt) {
        MP_ERR(vo, "X server image format not supported, use another VO.\n");
        return -1;
    }

    mp_sws_set_from_cmdline(p->sws, vo->global);
    p->sws->dst = (struct mp_image_params) {
        .imgfmt = fmte->mpfmt,
        .w = p->dst_w,
        .h = p->dst_h,
        .p_w = 1,
        .p_h = 1,
    };
    mp_image_params_guess_csp(&p->sws->dst);

    if (mp_sws_reinit(p->sws) < 0)
        return false;

    p->reset_view = true;
    vo->want_redraw = true;
    return true;
}

static void Display_Image(struct priv *p, XImage *myximage)
{
    struct vo *vo = p->vo;

    XImage *x_image = p->myximage[p->current_buf];

    if (p->reset_view) {
        XFillRectangle(vo->x11->display, vo->x11->window, p->gc, 0, 0, vo->dwidth, vo->dheight);
        p->reset_view = false;
    }

    if (p->Shmem_Flag) {
        XShmPutImage(vo->x11->display, vo->x11->window, p->gc, x_image,
                     0, 0, p->dst.x0, p->dst.y0, p->dst_w, p->dst_h,
                     True);
        vo->x11->ShmCompletionWaitCount++;
    } else {
        XPutImage(vo->x11->display, vo->x11->window, p->gc, x_image,
                  0, 0, p->dst.x0, p->dst.y0, p->dst_w, p->dst_h);
    }
}

static struct mp_image get_x_buffer(struct priv *p, int buf_index)
{
    struct mp_image img = {0};
    mp_image_set_params(&img, &p->sws->dst);

    img.planes[0] = p->myximage[buf_index]->data;
    img.stride[0] =
        p->image_width * ((p->myximage[buf_index]->bits_per_pixel + 7) / 8);

    return img;
}

static void wait_for_completion(struct vo *vo, int max_outstanding)
{
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
}

static void flip_page(struct vo *vo)
{
    struct priv *p = vo->priv;
    Display_Image(p, p->myximage[p->current_buf]);
    p->current_buf = (p->current_buf + 1) % 2;
}

// Note: REDRAW_FRAME can call this with NULL.
static void draw_image(struct vo *vo, mp_image_t *mpi)
{
    struct priv *p = vo->priv;

    wait_for_completion(vo, 1);

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
    if (sws_isSupportedInput(imgfmt2pixfmt(format)))
        return 1;
    return 0;
}

static void uninit(struct vo *vo)
{
    struct priv *p = vo->priv;
    if (p->myximage[0])
        freeMyXImage(p, 0);
    if (p->myximage[1])
        freeMyXImage(p, 1);
    if (p->gc)
        XFreeGC(vo->x11->display, p->gc);

    talloc_free(p->original_image);

    vo_x11_uninit(vo);
}

static int preinit(struct vo *vo)
{
    struct priv *p = vo->priv;
    p->vo = vo;
    p->sws = mp_sws_alloc(vo);

    if (!vo_x11_init(vo))
        goto error;
    struct vo_x11_state *x11 = vo->x11;

    XWindowAttributes attribs;
    XGetWindowAttributes(x11->display, x11->rootwin, &attribs);
    p->depth = attribs.depth;

    if (!XMatchVisualInfo(x11->display, x11->screen, p->depth,
                          TrueColor, &p->vinfo))
        goto error;

    MP_VERBOSE(vo, "selected visual: %d\n", (int)p->vinfo.visualid);

    if (!vo_x11_create_vo_window(vo, &p->vinfo, "x11"))
        goto error;

    p->gc = XCreateGC(x11->display, x11->window, 0, NULL);
    MP_WARN(vo, "Warning: this legacy VO has bad performance. Consider fixing "
                "your graphics drivers, or not forcing the x11 VO.\n");
    return 0;

error:
    uninit(vo);
    return -1;
}

static int control(struct vo *vo, uint32_t request, void *data)
{
    struct priv *p = vo->priv;
    switch (request) {
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
    .description = "X11 (slow, old crap)",
    .name = "x11",
    .priv_size = sizeof(struct priv),
    .preinit = preinit,
    .query_format = query_format,
    .reconfig = reconfig,
    .control = control,
    .draw_image = draw_image,
    .flip_page = flip_page,
    .wakeup = vo_x11_wakeup,
    .wait_events = vo_x11_wait_events,
    .uninit = uninit,
};
