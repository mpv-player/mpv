/*
 * X11 Xv interface
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
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <libavutil/common.h>

#include "config.h"

#if HAVE_SHM && HAVE_XEXT
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>
#endif

// Note: depends on the inclusion of X11/extensions/XShm.h
#include <X11/extensions/Xv.h>
#include <X11/extensions/Xvlib.h>

#include "options/options.h"
#include "talloc.h"
#include "common/msg.h"
#include "vo.h"
#include "video/mp_image.h"
#include "video/img_fourcc.h"
#include "x11_common.h"
#include "sub/osd.h"
#include "sub/draw_bmp.h"
#include "video/csputils.h"
#include "options/m_option.h"
#include "input/input.h"
#include "osdep/timer.h"

#define CK_METHOD_NONE       0 // no colorkey drawing
#define CK_METHOD_BACKGROUND 1 // set colorkey as window background
#define CK_METHOD_AUTOPAINT  2 // let xv draw the colorkey
#define CK_METHOD_MANUALFILL 3 // manually draw the colorkey
#define CK_SRC_USE           0 // use specified / default colorkey
#define CK_SRC_SET           1 // use and set specified / default colorkey
#define CK_SRC_CUR           2 // use current colorkey (get it from xv)

struct xvctx {
    struct xv_ck_info_s {
        int method; // CK_METHOD_* constants
        int source; // CK_SRC_* constants
    } xv_ck_info;
    int colorkey;
    unsigned long xv_colorkey;
    int xv_port;
    int cfg_xv_adaptor;
    XvAdaptorInfo *ai;
    XvImageFormatValues *fo;
    unsigned int formats, adaptors, xv_format;
    int current_buf;
    int current_ip_buf;
    int num_buffers;
    XvImage *xvimage[2];
    struct mp_image *original_image;
    uint32_t image_width;
    uint32_t image_height;
    uint32_t image_format;
    int cached_csp;
    struct mp_rect src_rect;
    struct mp_rect dst_rect;
    uint32_t max_width, max_height; // zero means: not set
    int Shmem_Flag;
#if HAVE_SHM && HAVE_XEXT
    XShmSegmentInfo Shminfo[2];
    int Shm_Warned_Slow;
#endif
};

struct fmt_entry {
    int imgfmt;
    int fourcc;
};
static const struct fmt_entry fmt_table[] = {
    {IMGFMT_420P,       MP_FOURCC_YV12},
    {IMGFMT_420P,       MP_FOURCC_I420},
    {IMGFMT_YUYV,       MP_FOURCC_YUY2},
    {IMGFMT_UYVY,       MP_FOURCC_UYVY},
    {0}
};

static bool allocate_xvimage(struct vo *, int);
static void deallocate_xvimage(struct vo *vo, int foo);
static struct mp_image get_xv_buffer(struct vo *vo, int buf_index);

static int find_xv_format(int imgfmt)
{
    for (int n = 0; fmt_table[n].imgfmt; n++) {
        if (fmt_table[n].imgfmt == imgfmt)
            return fmt_table[n].fourcc;
    }
    return 0;
}

static int xv_find_atom(struct vo *vo, uint32_t xv_port, const char *name,
                        bool get, int *min, int *max)
{
    Atom atom = None;
    int howmany = 0;
    XvAttribute *attributes = XvQueryPortAttributes(vo->x11->display, xv_port,
                                                    &howmany);
    for (int i = 0; i < howmany && attributes; i++) {
        int flag = get ? XvGettable : XvSettable;
        if (attributes[i].flags & flag) {
            atom = XInternAtom(vo->x11->display, attributes[i].name, True);
            *min = attributes[i].min_value;
            *max = attributes[i].max_value;
/* since we have SET_DEFAULTS first in our list, we can check if it's available
   then trigger it if it's ok so that the other values are at default upon query */
            if (atom != None) {
                if (!strcmp(attributes[i].name, "XV_BRIGHTNESS") &&
                    (!strcmp(name, "brightness")))
                    break;
                else if (!strcmp(attributes[i].name, "XV_CONTRAST") &&
                         (!strcmp(name, "contrast")))
                    break;
                else if (!strcmp(attributes[i].name, "XV_SATURATION") &&
                         (!strcmp(name, "saturation")))
                    break;
                else if (!strcmp(attributes[i].name, "XV_HUE") &&
                         (!strcmp(name, "hue")))
                    break;
                if (!strcmp(attributes[i].name, "XV_RED_INTENSITY") &&
                    (!strcmp(name, "red_intensity")))
                    break;
                else if (!strcmp(attributes[i].name, "XV_GREEN_INTENSITY")
                         && (!strcmp(name, "green_intensity")))
                    break;
                else if (!strcmp(attributes[i].name, "XV_BLUE_INTENSITY")
                         && (!strcmp(name, "blue_intensity")))
                    break;
                else if ((!strcmp(attributes[i].name, "XV_ITURBT_709") //NVIDIA
                          || !strcmp(attributes[i].name, "XV_COLORSPACE")) //ATI
                         && (!strcmp(name, "bt_709")))
                    break;
                atom = None;
                continue;
            }
        }
    }
    XFree(attributes);
    return atom;
}

static int xv_set_eq(struct vo *vo, uint32_t xv_port, const char *name,
                     int value)
{
    MP_VERBOSE(vo, "xv_set_eq called! (%s, %d)\n", name, value);

    int min, max;
    int atom = xv_find_atom(vo, xv_port, name, false, &min, &max);
    if (atom != None) {
        // -100 -> min
        //   0  -> (max+min)/2
        // +100 -> max
        int port_value = (value + 100) * (max - min) / 200 + min;
        XvSetPortAttribute(vo->x11->display, xv_port, atom, port_value);
        return VO_TRUE;
    }
    return VO_FALSE;
}

static int xv_get_eq(struct vo *vo, uint32_t xv_port, const char *name,
                     int *value)
{
    int min, max;
    int atom = xv_find_atom(vo, xv_port, name, true, &min, &max);
    if (atom != None) {
        int port_value = 0;
        XvGetPortAttribute(vo->x11->display, xv_port, atom, &port_value);

        *value = (port_value - min) * 200 / (max - min) - 100;
        MP_VERBOSE(vo, "xv_get_eq called! (%s, %d)\n", name, *value);
        return VO_TRUE;
    }
    return VO_FALSE;
}

static Atom xv_intern_atom_if_exists(struct vo *vo, char const *atom_name)
{
    struct xvctx *ctx = vo->priv;
    XvAttribute *attributes;
    int attrib_count, i;
    Atom xv_atom = None;

    attributes = XvQueryPortAttributes(vo->x11->display, ctx->xv_port,
                                       &attrib_count);
    if (attributes != NULL) {
        for (i = 0; i < attrib_count; ++i) {
            if (strcmp(attributes[i].name, atom_name) == 0) {
                xv_atom = XInternAtom(vo->x11->display, atom_name, False);
                break;
            }
        }
        XFree(attributes);
    }

    return xv_atom;
}

// Try to enable vsync for xv.
// Returns -1 if not available, 0 on failure and 1 on success.
static int xv_enable_vsync(struct vo *vo)
{
    struct xvctx *ctx = vo->priv;
    Atom xv_atom = xv_intern_atom_if_exists(vo, "XV_SYNC_TO_VBLANK");
    if (xv_atom == None)
        return -1;
    return XvSetPortAttribute(vo->x11->display, ctx->xv_port, xv_atom, 1)
           == Success;
}

// Get maximum supported source image dimensions.
// If querying the dimensions fails, don't change *width and *height.
static void xv_get_max_img_dim(struct vo *vo, uint32_t *width, uint32_t *height)
{
    struct xvctx *ctx = vo->priv;
    XvEncodingInfo *encodings;
    unsigned int num_encodings, idx;

    XvQueryEncodings(vo->x11->display, ctx->xv_port, &num_encodings, &encodings);

    if (encodings) {
        for (idx = 0; idx < num_encodings; ++idx) {
            if (strcmp(encodings[idx].name, "XV_IMAGE") == 0) {
                *width  = encodings[idx].width;
                *height = encodings[idx].height;
                break;
            }
        }
    }

    MP_VERBOSE(vo, "Maximum source image dimensions: %ux%u\n", *width, *height);

    XvFreeEncodingInfo(encodings);
}

static void xv_print_ck_info(struct vo *vo)
{
    struct xvctx *xv = vo->priv;

    switch (xv->xv_ck_info.method) {
    case CK_METHOD_NONE:
        MP_VERBOSE(vo, "Drawing no colorkey.\n");
        return;
    case CK_METHOD_AUTOPAINT:
        MP_VERBOSE(vo, "Colorkey is drawn by Xv.\n");
        break;
    case CK_METHOD_MANUALFILL:
        MP_VERBOSE(vo, "Drawing colorkey manually.\n");
        break;
    case CK_METHOD_BACKGROUND:
        MP_VERBOSE(vo, "Colorkey is drawn as window background.\n");
        break;
    }

    switch (xv->xv_ck_info.source) {
    case CK_SRC_CUR:
        MP_VERBOSE(vo, "Using colorkey from Xv (0x%06lx).\n", xv->xv_colorkey);
        break;
    case CK_SRC_USE:
        if (xv->xv_ck_info.method == CK_METHOD_AUTOPAINT) {
            MP_VERBOSE(vo, "Ignoring colorkey from mpv (0x%06lx).\n",
                       xv->xv_colorkey);
        } else {
            MP_VERBOSE(vo, "Using colorkey from mpv (0x%06lx). Use -colorkey to change.\n",
                       xv->xv_colorkey);
        }
        break;
    case CK_SRC_SET:
        MP_VERBOSE(vo, "Setting and using colorkey from mpv (0x%06lx)."
                   " Use -colorkey to change.\n", xv->xv_colorkey);
        break;
    }
}

/* NOTE: If vo.colorkey has bits set after the first 3 low order bytes
 *       we don't draw anything as this means it was forced to off. */
static int xv_init_colorkey(struct vo *vo)
{
    struct xvctx *ctx = vo->priv;
    Display *display = vo->x11->display;
    Atom xv_atom;
    int rez;

    /* check if colorkeying is needed */
    xv_atom = xv_intern_atom_if_exists(vo, "XV_COLORKEY");
    if (xv_atom != None && !(ctx->colorkey & 0xFF000000)) {
        if (ctx->xv_ck_info.source == CK_SRC_CUR) {
            int colorkey_ret;

            rez = XvGetPortAttribute(display, ctx->xv_port, xv_atom,
                                     &colorkey_ret);
            if (rez == Success)
                ctx->xv_colorkey = colorkey_ret;
            else {
                MP_FATAL(vo, "Couldn't get colorkey! "
                         "Maybe the selected Xv port has no overlay.\n");
                return 0; // error getting colorkey
            }
        } else {
            ctx->xv_colorkey = ctx->colorkey;

            /* check if we have to set the colorkey too */
            if (ctx->xv_ck_info.source == CK_SRC_SET) {
                xv_atom = XInternAtom(display, "XV_COLORKEY", False);

                rez = XvSetPortAttribute(display, ctx->xv_port, xv_atom,
                                         ctx->colorkey);
                if (rez != Success) {
                    MP_FATAL(vo, "Couldn't set colorkey!\n");
                    return 0; // error setting colorkey
                }
            }
        }

        xv_atom = xv_intern_atom_if_exists(vo, "XV_AUTOPAINT_COLORKEY");

        /* should we draw the colorkey ourselves or activate autopainting? */
        if (ctx->xv_ck_info.method == CK_METHOD_AUTOPAINT) {
            rez = !Success;

            if (xv_atom != None) // autopaint is supported
                rez = XvSetPortAttribute(display, ctx->xv_port, xv_atom, 1);

            if (rez != Success)
                ctx->xv_ck_info.method = CK_METHOD_MANUALFILL;
        } else {
            // disable colorkey autopainting if supported
            if (xv_atom != None)
                XvSetPortAttribute(display, ctx->xv_port, xv_atom, 0);
        }
    } else // do no colorkey drawing at all
        ctx->xv_ck_info.method = CK_METHOD_NONE;

    xv_print_ck_info(vo);

    return 1;
}

/* Draw the colorkey on the video window.
 *
 * Draws the colorkey depending on the set method ( colorkey_handling ).
 *
 * Also draws the black bars ( when the video doesn't fit the display in
 * fullscreen ) separately, so they don't overlap with the video area. */
static void xv_draw_colorkey(struct vo *vo, const struct mp_rect *rc)
{
    struct xvctx *ctx = vo->priv;
    struct vo_x11_state *x11 = vo->x11;
    if (ctx->xv_ck_info.method == CK_METHOD_MANUALFILL ||
        ctx->xv_ck_info.method == CK_METHOD_BACKGROUND)
    {
        if (!x11->vo_gc)
            return;
        //less tearing than XClearWindow()
        XSetForeground(x11->display, x11->vo_gc, ctx->xv_colorkey);
        XFillRectangle(x11->display, x11->window, x11->vo_gc, rc->x0, rc->y0,
                       rc->x1 - rc->x0, rc->y1 - rc->y0);
    }
}

static void read_xv_csp(struct vo *vo)
{
    struct xvctx *ctx = vo->priv;
    ctx->cached_csp = 0;
    int bt709_enabled;
    if (xv_get_eq(vo, ctx->xv_port, "bt_709", &bt709_enabled))
        ctx->cached_csp = bt709_enabled == 100 ? MP_CSP_BT_709 : MP_CSP_BT_601;
}

static void resize(struct vo *vo)
{
    struct xvctx *ctx = vo->priv;

    // Can't be used, because the function calculates screen-space coordinates,
    // while we need video-space.
    struct mp_osd_res unused;

    vo_get_src_dst_rects(vo, &ctx->src_rect, &ctx->dst_rect, &unused);

    vo_x11_clear_background(vo, &ctx->dst_rect);
    xv_draw_colorkey(vo, &ctx->dst_rect);
    read_xv_csp(vo);

    mp_input_set_mouse_transform(vo->input_ctx, &ctx->dst_rect, &ctx->src_rect);

    vo->want_redraw = true;
}

/*
 * create and map window,
 * allocate colors and (shared) memory
 */
static int reconfig(struct vo *vo, struct mp_image_params *params, int flags)
{
    struct vo_x11_state *x11 = vo->x11;
    struct xvctx *ctx = vo->priv;
    int i;

    mp_image_unrefp(&ctx->original_image);

    ctx->image_height = params->h;
    ctx->image_width  = params->w;
    ctx->image_format = params->imgfmt;

    if ((ctx->max_width != 0 && ctx->max_height != 0)
        && (ctx->image_width > ctx->max_width
            || ctx->image_height > ctx->max_height)) {
        MP_ERR(vo, "Source image dimensions are too high: %ux%u (maximum is %ux%u)\n",
               ctx->image_width, ctx->image_height, ctx->max_width,
               ctx->max_height);
        return -1;
    }

    /* check image formats */
    ctx->xv_format = 0;
    for (i = 0; i < ctx->formats; i++) {
        MP_VERBOSE(vo, "Xvideo image format: 0x%x (%4.4s) %s\n",
                   ctx->fo[i].id, (char *) &ctx->fo[i].id,
                   (ctx->fo[i].format == XvPacked) ? "packed" : "planar");
        if (ctx->fo[i].id == find_xv_format(ctx->image_format))
            ctx->xv_format = ctx->fo[i].id;
    }
    if (!ctx->xv_format)
        return -1;

    vo_x11_config_vo_window(vo, NULL, flags, "xv");

    if (ctx->xv_ck_info.method == CK_METHOD_BACKGROUND)
        XSetWindowBackground(x11->display, x11->window, ctx->xv_colorkey);

    MP_VERBOSE(vo, "using Xvideo port %d for hw scaling\n", ctx->xv_port);

    // In case config has been called before
    for (i = 0; i < ctx->num_buffers; i++)
        deallocate_xvimage(vo, i);

    ctx->num_buffers = 2;

    for (i = 0; i < ctx->num_buffers; i++) {
        if (!allocate_xvimage(vo, i)) {
            MP_FATAL(vo, "could not allocate Xv image data\n");
            return -1;
        }
    }

    ctx->current_buf = 0;
    ctx->current_ip_buf = 0;

    int is_709 = params->colorspace == MP_CSP_BT_709;
    xv_set_eq(vo, ctx->xv_port, "bt_709", is_709 * 200 - 100);
    read_xv_csp(vo);

    resize(vo);

    return 0;
}

static bool allocate_xvimage(struct vo *vo, int foo)
{
    struct xvctx *ctx = vo->priv;
    struct vo_x11_state *x11 = vo->x11;
    // align it for faster OSD rendering (draw_bmp.c swscale usage)
    int aligned_w = FFALIGN(ctx->image_width, 32);
#if HAVE_SHM && HAVE_XEXT
    if (x11->display_is_local && XShmQueryExtension(x11->display)) {
        ctx->Shmem_Flag = 1;
        x11->ShmCompletionEvent = XShmGetEventBase(x11->display)
                                + ShmCompletion;
    } else {
        ctx->Shmem_Flag = 0;
        MP_INFO(vo, "Shared memory not supported\nReverting to normal Xv.\n");
    }
    if (ctx->Shmem_Flag) {
        ctx->xvimage[foo] =
            (XvImage *) XvShmCreateImage(x11->display, ctx->xv_port,
                                         ctx->xv_format, NULL,
                                         aligned_w, ctx->image_height,
                                         &ctx->Shminfo[foo]);
        if (!ctx->xvimage[foo])
            return false;

        ctx->Shminfo[foo].shmid = shmget(IPC_PRIVATE,
                                         ctx->xvimage[foo]->data_size,
                                         IPC_CREAT | 0777);
        ctx->Shminfo[foo].shmaddr = shmat(ctx->Shminfo[foo].shmid, 0, 0);
        if (ctx->Shminfo[foo].shmaddr == (void *)-1)
            return false;
        ctx->Shminfo[foo].readOnly = False;

        ctx->xvimage[foo]->data = ctx->Shminfo[foo].shmaddr;
        XShmAttach(x11->display, &ctx->Shminfo[foo]);
        XSync(x11->display, False);
        shmctl(ctx->Shminfo[foo].shmid, IPC_RMID, 0);
    } else
#endif
    {
        ctx->xvimage[foo] =
            (XvImage *) XvCreateImage(x11->display, ctx->xv_port,
                                      ctx->xv_format, NULL, aligned_w,
                                      ctx->image_height);
        if (!ctx->xvimage[foo])
            return false;
        ctx->xvimage[foo]->data = av_malloc(ctx->xvimage[foo]->data_size);
        if (!ctx->xvimage[foo]->data)
            return false;
        XSync(x11->display, False);
    }
    struct mp_image img = get_xv_buffer(vo, foo);
    img.w = aligned_w;
    mp_image_clear(&img, 0, 0, img.w, img.h);
    return true;
}

static void deallocate_xvimage(struct vo *vo, int foo)
{
    struct xvctx *ctx = vo->priv;
#if HAVE_SHM && HAVE_XEXT
    if (ctx->Shmem_Flag) {
        XShmDetach(vo->x11->display, &ctx->Shminfo[foo]);
        shmdt(ctx->Shminfo[foo].shmaddr);
    } else
#endif
    {
        av_free(ctx->xvimage[foo]->data);
    }
    if (ctx->xvimage[foo])
        XFree(ctx->xvimage[foo]);

    ctx->xvimage[foo] = NULL;
#if HAVE_SHM && HAVE_XEXT
    ctx->Shminfo[foo] = (XShmSegmentInfo){0};
#endif

    XSync(vo->x11->display, False);
    return;
}

static inline void put_xvimage(struct vo *vo, XvImage *xvi)
{
    struct xvctx *ctx = vo->priv;
    struct vo_x11_state *x11 = vo->x11;
    struct mp_rect *src = &ctx->src_rect;
    struct mp_rect *dst = &ctx->dst_rect;
    int dw = dst->x1 - dst->x0, dh = dst->y1 - dst->y0;
    int sw = src->x1 - src->x0, sh = src->y1 - src->y0;
#if HAVE_SHM && HAVE_XEXT
    if (ctx->Shmem_Flag) {
        XvShmPutImage(x11->display, ctx->xv_port, x11->window, x11->vo_gc, xvi,
                      src->x0, src->y0, sw, sh,
                      dst->x0, dst->y0, dw, dh,
                      True);
        x11->ShmCompletionWaitCount++;
    } else
#endif
    {
        XvPutImage(x11->display, ctx->xv_port, x11->window, x11->vo_gc, xvi,
                   src->x0, src->y0, sw, sh,
                   dst->x0, dst->y0, dw, dh);
    }
}

static struct mp_image get_xv_buffer(struct vo *vo, int buf_index)
{
    struct xvctx *ctx = vo->priv;
    XvImage *xv_image = ctx->xvimage[buf_index];

    struct mp_image img = {0};
    mp_image_set_size(&img, ctx->image_width, ctx->image_height);
    mp_image_setfmt(&img, ctx->image_format);

    bool swapuv = ctx->xv_format == MP_FOURCC_YV12;
    for (int n = 0; n < img.num_planes; n++) {
        int sn = n > 0 &&  swapuv ? (n == 1 ? 2 : 1) : n;
        img.planes[n] = xv_image->data + xv_image->offsets[sn];
        img.stride[n] = xv_image->pitches[sn];
    }

    if (vo->params) {
        struct mp_image_params params = *vo->params;
        if (ctx->cached_csp)
            params.colorspace = ctx->cached_csp;
        mp_image_set_attributes(&img, &params);
    }

    return img;
}

static void wait_for_completion(struct vo *vo, int max_outstanding)
{
#if HAVE_SHM && HAVE_XEXT
    struct xvctx *ctx = vo->priv;
    struct vo_x11_state *x11 = vo->x11;
    if (ctx->Shmem_Flag) {
        while (x11->ShmCompletionWaitCount > max_outstanding) {
            if (!ctx->Shm_Warned_Slow) {
                MP_WARN(vo, "X11 can't keep up! Waiting"
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
    struct xvctx *ctx = vo->priv;
    put_xvimage(vo, ctx->xvimage[ctx->current_buf]);

    /* remember the currently visible buffer */
    ctx->current_buf = (ctx->current_buf + 1) % ctx->num_buffers;

    if (!ctx->Shmem_Flag)
        XSync(vo->x11->display, False);
}

// Note: REDRAW_FRAME can call this with NULL.
static void draw_image(struct vo *vo, mp_image_t *mpi)
{
    struct xvctx *ctx = vo->priv;

    wait_for_completion(vo, ctx->num_buffers - 1);

    struct mp_image xv_buffer = get_xv_buffer(vo, ctx->current_buf);
    if (mpi) {
        mp_image_copy(&xv_buffer, mpi);
    } else {
        mp_image_clear(&xv_buffer, 0, 0, xv_buffer.w, xv_buffer.h);
    }

    struct mp_osd_res res = osd_res_from_image_params(vo->params);
    osd_draw_on_image(vo->osd, res, mpi ? mpi->pts : 0, 0, &xv_buffer);

    if (mpi != ctx->original_image) {
        talloc_free(ctx->original_image);
        ctx->original_image = mpi;
    }
}

static int query_format(struct vo *vo, int format)
{
    struct xvctx *ctx = vo->priv;
    uint32_t i;

    int fourcc = find_xv_format(format);
    if (fourcc) {
        for (i = 0; i < ctx->formats; i++) {
            if (ctx->fo[i].id == fourcc)
                return 1;
        }
    }
    return 0;
}

static void uninit(struct vo *vo)
{
    struct xvctx *ctx = vo->priv;
    int i;

    talloc_free(ctx->original_image);

    if (ctx->ai)
        XvFreeAdaptorInfo(ctx->ai);
    ctx->ai = NULL;
    if (ctx->fo) {
        XFree(ctx->fo);
        ctx->fo = NULL;
    }
    for (i = 0; i < ctx->num_buffers; i++)
        deallocate_xvimage(vo, i);
    // uninit() shouldn't get called unless initialization went past vo_init()
    vo_x11_uninit(vo);
}

static int preinit(struct vo *vo)
{
    XvPortID xv_p;
    int busy_ports = 0;
    unsigned int i;
    struct xvctx *ctx = vo->priv;
    int xv_adaptor = ctx->cfg_xv_adaptor;

    if (!vo_x11_init(vo))
        return -1;

    struct vo_x11_state *x11 = vo->x11;

    /* check for Xvideo extension */
    unsigned int ver, rel, req, ev, err;
    if (Success != XvQueryExtension(x11->display, &ver, &rel, &req, &ev, &err)) {
        MP_ERR(vo, "Xv not supported by this X11 version/driver\n");
        goto error;
    }

    /* check for Xvideo support */
    if (Success !=
        XvQueryAdaptors(x11->display, DefaultRootWindow(x11->display),
                        &ctx->adaptors, &ctx->ai)) {
        MP_ERR(vo, "XvQueryAdaptors failed.\n");
        goto error;
    }

    /* check adaptors */
    if (ctx->xv_port) {
        int port_found;

        for (port_found = 0, i = 0; !port_found && i < ctx->adaptors; i++) {
            if ((ctx->ai[i].type & XvInputMask)
                && (ctx->ai[i].type & XvImageMask)) {
                for (xv_p = ctx->ai[i].base_id;
                     xv_p < ctx->ai[i].base_id + ctx->ai[i].num_ports;
                     ++xv_p) {
                    if (xv_p == ctx->xv_port) {
                        port_found = 1;
                        break;
                    }
                }
            }
        }
        if (port_found) {
            if (XvGrabPort(x11->display, ctx->xv_port, CurrentTime))
                ctx->xv_port = 0;
        } else {
            MP_WARN(vo, "Invalid port parameter, overriding with port 0.\n");
            ctx->xv_port = 0;
        }
    }

    for (i = 0; i < ctx->adaptors && ctx->xv_port == 0; i++) {
        /* check if adaptor number has been specified */
        if (xv_adaptor != -1 && xv_adaptor != i)
            continue;

        if ((ctx->ai[i].type & XvInputMask) && (ctx->ai[i].type & XvImageMask)) {
            for (xv_p = ctx->ai[i].base_id;
                 xv_p < ctx->ai[i].base_id + ctx->ai[i].num_ports; ++xv_p)
                if (!XvGrabPort(x11->display, xv_p, CurrentTime)) {
                    ctx->xv_port = xv_p;
                    MP_VERBOSE(vo, "Using Xv Adapter #%d (%s)\n",
                               i, ctx->ai[i].name);
                    break;
                } else {
                    MP_WARN(vo, "Could not grab port %i.\n", (int) xv_p);
                    ++busy_ports;
                }
        }
    }
    if (!ctx->xv_port) {
        if (busy_ports)
            MP_ERR(vo, "Xvideo ports busy.\n");
        else
            MP_ERR(vo, "No Xvideo support found.\n");
        goto error;
    }

    if (!xv_init_colorkey(vo)) {
        goto error;             // bail out, colorkey setup failed
    }
    xv_enable_vsync(vo);
    xv_get_max_img_dim(vo, &ctx->max_width, &ctx->max_height);

    ctx->fo = XvListImageFormats(x11->display, ctx->xv_port,
                                 (int *) &ctx->formats);

    return 0;

  error:
    uninit(vo);                 // free resources
    return -1;
}

static int control(struct vo *vo, uint32_t request, void *data)
{
    struct xvctx *ctx = vo->priv;
    switch (request) {
    case VOCTRL_GET_PANSCAN:
        return VO_TRUE;
    case VOCTRL_SET_PANSCAN:
        resize(vo);
        return VO_TRUE;
    case VOCTRL_SET_EQUALIZER: {
        vo->want_redraw = true;
        struct voctrl_set_equalizer_args *args = data;
        return xv_set_eq(vo, ctx->xv_port, args->name, args->value);
    }
    case VOCTRL_GET_EQUALIZER: {
        struct voctrl_get_equalizer_args *args = data;
        return xv_get_eq(vo, ctx->xv_port, args->name, args->valueptr);
    }
    case VOCTRL_REDRAW_FRAME:
        draw_image(vo, ctx->original_image);
        return true;
    }
    int events = 0;
    int r = vo_x11_control(vo, &events, request, data);
    if (events & (VO_EVENT_EXPOSE | VO_EVENT_RESIZE))
        resize(vo);
    vo_event(vo, events);
    return r;
}

#define OPT_BASE_STRUCT struct xvctx

const struct vo_driver video_out_xv = {
    .description = "X11/Xv",
    .name = "xv",
    .preinit = preinit,
    .query_format = query_format,
    .reconfig = reconfig,
    .control = control,
    .draw_image = draw_image,
    .flip_page = flip_page,
    .uninit = uninit,
    .priv_size = sizeof(struct xvctx),
    .priv_defaults = &(const struct xvctx) {
        .cfg_xv_adaptor = -1,
        .xv_ck_info = {CK_METHOD_MANUALFILL, CK_SRC_CUR},
        .colorkey = 0x0000ff00, // default colorkey is green
                    // (0xff000000 means that colorkey has been disabled)
    },
    .options = (const struct m_option[]) {
        OPT_INT("port", xv_port, M_OPT_MIN, .min = 0),
        OPT_INT("adaptor", cfg_xv_adaptor, M_OPT_MIN, .min = -1),
        OPT_CHOICE("ck", xv_ck_info.source, 0,
                   ({"use", CK_SRC_USE},
                    {"set", CK_SRC_SET},
                    {"cur", CK_SRC_CUR})),
        OPT_CHOICE("ck-method", xv_ck_info.method, 0,
                   ({"bg", CK_METHOD_BACKGROUND},
                    {"man", CK_METHOD_MANUALFILL},
                    {"auto", CK_METHOD_AUTOPAINT})),
        OPT_INT("colorkey", colorkey, 0),
        OPT_FLAG_STORE("no-colorkey", colorkey, 0, 0x1000000),
        {0}
    },
};
