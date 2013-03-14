/*
 * X11 Xv interface
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

#ifdef HAVE_SHM
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>
#endif

// Note: depends on the inclusion of X11/extensions/XShm.h
#include <X11/extensions/Xv.h>
#include <X11/extensions/Xvlib.h>

#include "core/options.h"
#include "talloc.h"
#include "core/mp_msg.h"
#include "vo.h"
#include "video/vfcap.h"
#include "video/mp_image.h"
#include "video/img_fourcc.h"
#include "x11_common.h"
#include "video/memcpy_pic.h"
#include "sub/sub.h"
#include "sub/draw_bmp.h"
#include "video/csputils.h"
#include "core/subopt-helper.h"
#include "osdep/timer.h"

static const vo_info_t info = {
    "X11/Xv",
    "xv",
    "Gerd Knorr <kraxel@goldbach.in-berlin.de> and others",
    ""
};

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
    unsigned long xv_colorkey;
    unsigned int xv_port;
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
    struct mp_csp_details cached_csp;
    struct mp_rect src_rect;
    struct mp_rect dst_rect;
    uint32_t max_width, max_height; // zero means: not set
    int Shmem_Flag;
#ifdef HAVE_SHM
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

static void allocate_xvimage(struct vo *, int);
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
                    (!strcasecmp(name, "brightness")))
                    break;
                else if (!strcmp(attributes[i].name, "XV_CONTRAST") &&
                         (!strcasecmp(name, "contrast")))
                    break;
                else if (!strcmp(attributes[i].name, "XV_SATURATION") &&
                         (!strcasecmp(name, "saturation")))
                    break;
                else if (!strcmp(attributes[i].name, "XV_HUE") &&
                         (!strcasecmp(name, "hue")))
                    break;
                if (!strcmp(attributes[i].name, "XV_RED_INTENSITY") &&
                    (!strcasecmp(name, "red_intensity")))
                    break;
                else if (!strcmp(attributes[i].name, "XV_GREEN_INTENSITY")
                         && (!strcasecmp(name, "green_intensity")))
                    break;
                else if (!strcmp(attributes[i].name, "XV_BLUE_INTENSITY")
                         && (!strcasecmp(name, "blue_intensity")))
                    break;
                else if ((!strcmp(attributes[i].name, "XV_ITURBT_709") //NVIDIA
                          || !strcmp(attributes[i].name, "XV_COLORSPACE")) //ATI
                         && (!strcasecmp(name, "bt_709")))
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
    mp_dbg(MSGT_VO, MSGL_V, "xv_set_eq called! (%s, %d)\n", name, value);

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
        mp_dbg(MSGT_VO, MSGL_V, "xv_get_eq called! (%s, %d)\n",
               name, *value);
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

    mp_msg(MSGT_VO, MSGL_V,
           "[xv] Maximum source image dimensions: %ux%u\n",
           *width, *height);

    XvFreeEncodingInfo(encodings);
}

static void xv_print_ck_info(struct xvctx *xv)
{
    mp_msg(MSGT_VO, MSGL_V, "[xv] ");

    switch (xv->xv_ck_info.method) {
    case CK_METHOD_NONE:
        mp_msg(MSGT_VO, MSGL_V, "Drawing no colorkey.\n");
        return;
    case CK_METHOD_AUTOPAINT:
        mp_msg(MSGT_VO, MSGL_V, "Colorkey is drawn by Xv.");
        break;
    case CK_METHOD_MANUALFILL:
        mp_msg(MSGT_VO, MSGL_V, "Drawing colorkey manually.");
        break;
    case CK_METHOD_BACKGROUND:
        mp_msg(MSGT_VO, MSGL_V, "Colorkey is drawn as window background.");
        break;
    }

    mp_msg(MSGT_VO, MSGL_V, "\n[xv] ");

    switch (xv->xv_ck_info.source) {
    case CK_SRC_CUR:
        mp_msg(MSGT_VO, MSGL_V, "Using colorkey from Xv (0x%06lx).\n",
               xv->xv_colorkey);
        break;
    case CK_SRC_USE:
        if (xv->xv_ck_info.method == CK_METHOD_AUTOPAINT) {
            mp_msg(MSGT_VO, MSGL_V, "Ignoring colorkey from mpv (0x%06lx).\n",
                   xv->xv_colorkey);
        } else {
            mp_msg(MSGT_VO, MSGL_V,
                   "Using colorkey from mpv (0x%06lx). Use -colorkey to change.\n",
                   xv->xv_colorkey);
        }
        break;
    case CK_SRC_SET:
        mp_msg(MSGT_VO, MSGL_V, "Setting and using colorkey from mpv (0x%06lx)."
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
    if (xv_atom != None && !(vo->opts->colorkey & 0xFF000000)) {
        if (ctx->xv_ck_info.source == CK_SRC_CUR) {
            int colorkey_ret;

            rez = XvGetPortAttribute(display, ctx->xv_port, xv_atom,
                                     &colorkey_ret);
            if (rez == Success)
                ctx->xv_colorkey = colorkey_ret;
            else {
                mp_msg(MSGT_VO, MSGL_FATAL, "[xv] Couldn't get colorkey!"
                       "Maybe the selected Xv port has no overlay.\n");
                return 0; // error getting colorkey
            }
        } else {
            ctx->xv_colorkey = vo->opts->colorkey;

            /* check if we have to set the colorkey too */
            if (ctx->xv_ck_info.source == CK_SRC_SET) {
                xv_atom = XInternAtom(display, "XV_COLORKEY", False);

                rez = XvSetPortAttribute(display, ctx->xv_port, xv_atom,
                                         vo->opts->colorkey);
                if (rez != Success) {
                    mp_msg(MSGT_VO, MSGL_FATAL, "[xv] Couldn't set colorkey!\n");
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

    xv_print_ck_info(ctx);

    return 1;
}

/* Draw the colorkey on the video window.
 *
 * Draws the colorkey depending on the set method ( colorkey_handling ).
 *
 * Also draws the black bars ( when the video doesn't fit the display in
 * fullscreen ) separately, so they don't overlap with the video area. */
static void xv_draw_colorkey(struct vo *vo, int32_t x, int32_t y,
                             int32_t w, int32_t h)
{
    struct xvctx *ctx = vo->priv;
    struct vo_x11_state *x11 = vo->x11;
    if (ctx->xv_ck_info.method == CK_METHOD_MANUALFILL ||
        ctx->xv_ck_info.method == CK_METHOD_BACKGROUND)
    {
        //less tearing than XClearWindow()
        XSetForeground(x11->display, x11->vo_gc, ctx->xv_colorkey);
        XFillRectangle(x11->display, x11->window, x11->vo_gc, x, y, w, h);
    }
}

// Tests if a valid argument for the ck suboption was given.
static int xv_test_ck(void *arg)
{
    strarg_t *strarg = (strarg_t *)arg;

    if (strargcmp(strarg, "use") == 0 ||
        strargcmp(strarg, "set") == 0 ||
        strargcmp(strarg, "cur") == 0)
        return 1;

    return 0;
}

// Tests if a valid arguments for the ck-method suboption was given.
static int xv_test_ckm(void *arg)
{
    strarg_t *strarg = (strarg_t *)arg;

    if (strargcmp(strarg, "bg") == 0 ||
        strargcmp(strarg, "man") == 0 ||
        strargcmp(strarg, "auto") == 0)
        return 1;

    return 0;
}

/* Modify the colorkey_handling var according to str
 *
 * Checks if a valid pointer ( not NULL ) to the string
 * was given. And in that case modifies the colorkey_handling
 * var to reflect the requested behaviour.
 * If nothing happens the content of colorkey_handling stays
 * the same.
 */
static void xv_setup_colorkeyhandling(struct xvctx *ctx,
                                      const char *ck_method_str,
                                      const char *ck_str)
{
    /* check if a valid pointer to the string was passed */
    if (ck_str) {
        if (strncmp(ck_str, "use", 3) == 0)
            ctx->xv_ck_info.source = CK_SRC_USE;
        else if (strncmp(ck_str, "set", 3) == 0)
            ctx->xv_ck_info.source = CK_SRC_SET;
    }
    /* check if a valid pointer to the string was passed */
    if (ck_method_str) {
        if (strncmp(ck_method_str, "bg", 2) == 0)
            ctx->xv_ck_info.method = CK_METHOD_BACKGROUND;
        else if (strncmp(ck_method_str, "man", 3) == 0)
            ctx->xv_ck_info.method = CK_METHOD_MANUALFILL;
        else if (strncmp(ck_method_str, "auto", 4) == 0)
            ctx->xv_ck_info.method = CK_METHOD_AUTOPAINT;
    }
}

static void read_xv_csp(struct vo *vo)
{
    struct xvctx *ctx = vo->priv;
    struct mp_csp_details *cspc = &ctx->cached_csp;
    *cspc = (struct mp_csp_details) MP_CSP_DETAILS_DEFAULTS;
    int bt709_enabled;
    if (xv_get_eq(vo, ctx->xv_port, "bt_709", &bt709_enabled))
        cspc->format = bt709_enabled == 100 ? MP_CSP_BT_709 : MP_CSP_BT_601;
}

static void resize(struct vo *vo)
{
    struct xvctx *ctx = vo->priv;

    // Can't be used, because the function calculates screen-space coordinates,
    // while we need video-space.
    struct mp_osd_res unused;

    vo_get_src_dst_rects(vo, &ctx->src_rect, &ctx->dst_rect, &unused);

    struct mp_rect *dst = &ctx->dst_rect;
    int dw = dst->x1 - dst->x0, dh = dst->y1 - dst->y0;
    vo_x11_clearwindow_part(vo, vo->x11->window, dw, dh);
    xv_draw_colorkey(vo, dst->x0, dst->y0, dw, dh);
    read_xv_csp(vo);
}

/*
 * connect to server, create and map window,
 * allocate colors and (shared) memory
 */
static int config(struct vo *vo, uint32_t width, uint32_t height,
                  uint32_t d_width, uint32_t d_height, uint32_t flags,
                  uint32_t format)
{
    struct vo_x11_state *x11 = vo->x11;
    struct xvctx *ctx = vo->priv;
    int i;

    mp_image_unrefp(&ctx->original_image);

    ctx->image_height = height;
    ctx->image_width = width;
    ctx->image_format = format;

    if ((ctx->max_width != 0 && ctx->max_height != 0)
        && (ctx->image_width > ctx->max_width
            || ctx->image_height > ctx->max_height)) {
        mp_tmsg(MSGT_VO, MSGL_ERR, "Source image dimensions are too high: %ux%u (maximum is %ux%u)\n",
               ctx->image_width, ctx->image_height, ctx->max_width,
               ctx->max_height);
        return -1;
    }

    /* check image formats */
    ctx->xv_format = 0;
    for (i = 0; i < ctx->formats; i++) {
        mp_msg(MSGT_VO, MSGL_V, "Xvideo image format: 0x%x (%4.4s) %s\n",
               ctx->fo[i].id, (char *) &ctx->fo[i].id,
               (ctx->fo[i].format == XvPacked) ? "packed" : "planar");
        if (ctx->fo[i].id == find_xv_format(format))
            ctx->xv_format = ctx->fo[i].id;
    }
    if (!ctx->xv_format)
        return -1;

    vo_x11_config_vo_window(vo, NULL, vo->dx, vo->dy, vo->dwidth,
                            vo->dheight, flags, "xv");

    if (ctx->xv_ck_info.method == CK_METHOD_BACKGROUND)
        XSetWindowBackground(x11->display, x11->window, ctx->xv_colorkey);

    mp_msg(MSGT_VO, MSGL_V, "using Xvideo port %d for hw scaling\n",
           ctx->xv_port);

    // In case config has been called before
    for (i = 0; i < ctx->num_buffers; i++)
        deallocate_xvimage(vo, i);

    ctx->num_buffers = 2;

    for (i = 0; i < ctx->num_buffers; i++)
        allocate_xvimage(vo, i);

    ctx->current_buf = 0;
    ctx->current_ip_buf = 0;


    resize(vo);

    return 0;
}

static void allocate_xvimage(struct vo *vo, int foo)
{
    struct xvctx *ctx = vo->priv;
    struct vo_x11_state *x11 = vo->x11;
    // align it for faster OSD rendering (draw_bmp.c swscale usage)
    int aligned_w = FFALIGN(ctx->image_width, 32);
#ifdef HAVE_SHM
    if (x11->display_is_local && XShmQueryExtension(x11->display)) {
        ctx->Shmem_Flag = 1;
        x11->ShmCompletionEvent = XShmGetEventBase(x11->display)
                                + ShmCompletion;
    } else {
        ctx->Shmem_Flag = 0;
        mp_tmsg(MSGT_VO, MSGL_INFO, "[VO_XV] Shared memory not supported\nReverting to normal Xv.\n");
    }
    if (ctx->Shmem_Flag) {
        ctx->xvimage[foo] =
            (XvImage *) XvShmCreateImage(x11->display, ctx->xv_port,
                                         ctx->xv_format, NULL,
                                         aligned_w, ctx->image_height,
                                         &ctx->Shminfo[foo]);

        ctx->Shminfo[foo].shmid = shmget(IPC_PRIVATE,
                                         ctx->xvimage[foo]->data_size,
                                         IPC_CREAT | 0777);
        ctx->Shminfo[foo].shmaddr = (char *) shmat(ctx->Shminfo[foo].shmid, 0,
                                                   0);
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
        ctx->xvimage[foo]->data = av_malloc(ctx->xvimage[foo]->data_size);
        XSync(x11->display, False);
    }
    struct mp_image img = get_xv_buffer(vo, foo);
    img.w = aligned_w;
    mp_image_clear(&img, 0, 0, img.w, img.h);
    return;
}

static void deallocate_xvimage(struct vo *vo, int foo)
{
    struct xvctx *ctx = vo->priv;
#ifdef HAVE_SHM
    if (ctx->Shmem_Flag) {
        XShmDetach(vo->x11->display, &ctx->Shminfo[foo]);
        shmdt(ctx->Shminfo[foo].shmaddr);
    } else
#endif
    {
        av_free(ctx->xvimage[foo]->data);
    }
    XFree(ctx->xvimage[foo]);

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
#ifdef HAVE_SHM
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

    mp_image_set_colorspace_details(&img, &ctx->cached_csp);

    return img;
}

static void check_events(struct vo *vo)
{
    int e = vo_x11_check_events(vo);

    if (e & VO_EVENT_EXPOSE || e & VO_EVENT_RESIZE) {
        resize(vo);
        vo->want_redraw = true;
    }
}

static void draw_osd(struct vo *vo, struct osd_state *osd)
{
    struct xvctx *ctx = vo->priv;

    struct mp_image img = get_xv_buffer(vo, ctx->current_buf);

    struct mp_osd_res res = {
        .w = ctx->image_width,
        .h = ctx->image_height,
        .display_par = 1.0 / vo->aspdat.par,
        .video_par = vo->aspdat.par,
    };

    osd_draw_on_image(osd, res, osd->vo_pts, 0, &img);
}

static void wait_for_completion(struct vo *vo, int max_outstanding)
{
#ifdef HAVE_SHM
    struct xvctx *ctx = vo->priv;
    struct vo_x11_state *x11 = vo->x11;
    if (ctx->Shmem_Flag) {
        while (x11->ShmCompletionWaitCount > max_outstanding) {
            if (!ctx->Shm_Warned_Slow) {
                mp_msg(MSGT_VO, MSGL_WARN, "[VO_XV] X11 can't keep up! Waiting"
                                           " for XShm completion events...\n");
                ctx->Shm_Warned_Slow = 1;
            }
            usec_sleep(1000);
            check_events(vo);
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

static mp_image_t *get_screenshot(struct vo *vo)
{
    struct xvctx *ctx = vo->priv;
    if (!ctx->original_image)
        return NULL;

    struct mp_image *res = mp_image_new_ref(ctx->original_image);
    mp_image_set_display_size(res, vo->aspdat.prew, vo->aspdat.preh);
    return res;
}

static void draw_image(struct vo *vo, mp_image_t *mpi)
{
    struct xvctx *ctx = vo->priv;

    wait_for_completion(vo, ctx->num_buffers - 1);

    struct mp_image xv_buffer = get_xv_buffer(vo, ctx->current_buf);
    mp_image_copy(&xv_buffer, mpi);

    mp_image_setrefp(&ctx->original_image, mpi);
}

static int redraw_frame(struct vo *vo)
{
    struct xvctx *ctx = vo->priv;

    if (!ctx->original_image)
        return false;

    draw_image(vo, ctx->original_image);
    return true;
}

static int query_format(struct vo *vo, uint32_t format)
{
    struct xvctx *ctx = vo->priv;
    uint32_t i;
    int flag = VFCAP_CSP_SUPPORTED | VFCAP_CSP_SUPPORTED_BY_HW;

    int fourcc = find_xv_format(format);
    if (fourcc) {
        for (i = 0; i < ctx->formats; i++) {
            if (ctx->fo[i].id == fourcc)
                return flag;
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

static int preinit(struct vo *vo, const char *arg)
{
    XvPortID xv_p;
    int busy_ports = 0;
    unsigned int i;
    strarg_t ck_src_arg = { 0, NULL };
    strarg_t ck_method_arg = { 0, NULL };
    struct xvctx *ctx = talloc_ptrtype(vo, ctx);
    *ctx = (struct xvctx) {
        .xv_ck_info = { CK_METHOD_MANUALFILL, CK_SRC_CUR },
    };
    vo->priv = ctx;
    int xv_adaptor = -1;

    if (!vo_x11_init(vo))
        return -1;

    struct vo_x11_state *x11 = vo->x11;

    const opt_t subopts[] =
    {
      /* name         arg type     arg var         test */
      {  "port",      OPT_ARG_INT, &ctx->xv_port,  int_pos },
      {  "adaptor",   OPT_ARG_INT, &xv_adaptor,    int_non_neg },
      {  "ck",        OPT_ARG_STR, &ck_src_arg,    xv_test_ck },
      {  "ck-method", OPT_ARG_STR, &ck_method_arg, xv_test_ckm },
      {  NULL }
    };

    /* parse suboptions */
    if (subopt_parse(arg, subopts) != 0) {
        return -1;
    }

    /* modify colorkey settings according to the given options */
    xv_setup_colorkeyhandling(ctx, ck_method_arg.str, ck_src_arg.str);

    /* check for Xvideo extension */
    unsigned int ver, rel, req, ev, err;
    if (Success != XvQueryExtension(x11->display, &ver, &rel, &req, &ev, &err)) {
        mp_tmsg(MSGT_VO, MSGL_ERR, "[VO_XV] Sorry, Xv not supported by this X11 version/driver\n[VO_XV] ******** Try with  -vo x11 *********\n");
        goto error;
    }

    /* check for Xvideo support */
    if (Success !=
        XvQueryAdaptors(x11->display, DefaultRootWindow(x11->display),
                        &ctx->adaptors, &ctx->ai)) {
        mp_tmsg(MSGT_VO, MSGL_ERR, "[VO_XV] XvQueryAdaptors failed.\n");
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
            mp_tmsg(MSGT_VO, MSGL_WARN, "[VO_XV] Invalid port parameter, overriding with port 0.\n");
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
                    mp_msg(MSGT_VO, MSGL_V,
                           "[VO_XV] Using Xv Adapter #%d (%s)\n",
                           i, ctx->ai[i].name);
                    break;
                } else {
                    mp_tmsg(MSGT_VO, MSGL_WARN, "[VO_XV] Could not grab port %i.\n",
                           (int) xv_p);
                    ++busy_ports;
                }
        }
    }
    if (!ctx->xv_port) {
        if (busy_ports)
            mp_tmsg(MSGT_VO, MSGL_ERR,
                "[VO_XV] Could not find free Xvideo port - maybe another process is already\n"\
                "[VO_XV] using it. Close all video applications, and try again. If that does\n"\
                "[VO_XV] not help, see 'mpv -vo help' for other (non-xv) video out drivers.\n");
        else
            mp_tmsg(MSGT_VO, MSGL_ERR,
                "[VO_XV] It seems there is no Xvideo support for your video card available.\n"\
                "[VO_XV] Run 'xvinfo' to verify its Xv support and read\n"\
                "[VO_XV] DOCS/HTML/en/video.html#xv!\n"\
                "[VO_XV] See 'mpv -vo help' for other (non-xv) video out drivers.\n"\
                "[VO_XV] Try -vo x11.\n");
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
    case VOCTRL_FULLSCREEN:
        vo_x11_fullscreen(vo);
        /* indended, fallthrough to update panscan on fullscreen/windowed switch */
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
    case VOCTRL_SET_YUV_COLORSPACE:;
        struct mp_csp_details* given_cspc = data;
        int is_709 = given_cspc->format == MP_CSP_BT_709;
        xv_set_eq(vo, ctx->xv_port, "bt_709", is_709 * 200 - 100);
        read_xv_csp(vo);
        vo->want_redraw = true;
        return true;
    case VOCTRL_GET_YUV_COLORSPACE:;
        struct mp_csp_details* cspc = data;
        read_xv_csp(vo);
        *cspc = ctx->cached_csp;
        return true;
    case VOCTRL_ONTOP:
        vo_x11_ontop(vo);
        return VO_TRUE;
    case VOCTRL_UPDATE_SCREENINFO:
        vo_x11_update_screeninfo(vo);
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

const struct vo_driver video_out_xv = {
    .info = &info,
    .preinit = preinit,
    .query_format = query_format,
    .config = config,
    .control = control,
    .draw_image = draw_image,
    .draw_osd = draw_osd,
    .flip_page = flip_page,
    .check_events = check_events,
    .uninit = uninit
};
