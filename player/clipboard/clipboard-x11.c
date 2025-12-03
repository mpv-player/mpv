/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xfixes.h>

#include "clipboard.h"
#include "common/common.h"
#include "common/msg.h"
#include "misc/bstr.h"
#include "osdep/io.h"
#include "osdep/poll_wrapper.h"
#include "osdep/threads.h"

static const uint8_t MESSAGE_DEATH = 0;
static const uint8_t MESSAGE_SET_OWNER = 1;

struct clipboard_x11_priv {
    mp_mutex lock;
    // accessed by both threads
    int message_pipe[2];
    bstr selection_text;
    bstr primary_selection_text;
    bool data_changed;
    // only accessed by the core thread
    mp_thread thread;
    // only accessed by the clipboard thread
    struct mp_log *log;
    Window window;
    Display *display;
    int XFixesSelectionNotifyEvent;
};

#define XA(x11, s) (XInternAtom((x11)->display, # s, False))


static void clipboard_x11_uninit(struct clipboard_x11_priv *x11)
{
    if (!x11)
        return;

    if (x11->window)
        XDestroyWindow(x11->display, x11->window);
    if (x11->display)
        XCloseDisplay(x11->display);
}

static bool clipboard_x11_init(struct clipboard_x11_priv *x11, bool xwayland)
{
    if (!xwayland && (getenv("WAYLAND_DISPLAY") || getenv("WAYLAND_SOCKET"))) {
        MP_VERBOSE(x11, "Stopping init due to suspected wayland environment\n");
        goto err;
    }

    x11->display = XOpenDisplay(NULL);
    if (!x11->display)
        goto err;

    XSetWindowAttributes dummy = {0};
    x11->window = XCreateWindow(x11->display, DefaultRootWindow(x11->display),
                                0, 0, 1, 1, 0, CopyFromParent, InputOnly, CopyFromParent, 0, &dummy);
    if (!x11->window)
        goto err;
    x11->XFixesSelectionNotifyEvent = -1;
    int opcode, event, error;
    if (XQueryExtension(x11->display, "XFIXES", &opcode, &event, &error)) {
        x11->XFixesSelectionNotifyEvent = event + XFixesSelectionNotify;
        XFixesSelectSelectionInput(x11->display, DefaultRootWindow(x11->display),
                                   XA_PRIMARY, XFixesSetSelectionOwnerNotifyMask);
        XFixesSelectSelectionInput(x11->display, DefaultRootWindow(x11->display),
                                   XA(x11, CLIPBOARD), XFixesSetSelectionOwnerNotifyMask);
    } else {
        MP_FATAL(x11, "Xfixes init failed\n");
        goto err;
    }
    return true;

err:
    clipboard_x11_uninit(x11);
    return false;
}

static void clipboard_x11_handle_selection_request(struct clipboard_x11_priv *x11, XEvent *event)
{
    XEvent reply = { .type = SelectionNotify };
    XSelectionRequestEvent request = event->xselectionrequest;
    reply.xselection.property = request.property;
    reply.xselection.display = request.display;
    reply.xselection.requestor = request.requestor;
    reply.xselection.selection = request.selection;
    reply.xselection.target = request.target;
    reply.xselection.time = request.time;

    if (request.target == XA(x11, TARGETS)) {
        Atom targets[] = { XA(x11, TARGETS), XA(x11, UTF8_STRING) };
        XChangeProperty(x11->display, request.requestor, request.property,
                        XA_ATOM, 32, PropModeReplace,
                        (unsigned char*)targets, MP_ARRAY_SIZE(targets));
    } else if (request.target == XA(x11, UTF8_STRING) && request.selection == XA(x11, CLIPBOARD)) {
        mp_mutex_lock(&x11->lock);
        XChangeProperty(x11->display, request.requestor, request.property,
                        request.target, 8, PropModeReplace,
                        (unsigned char*)x11->selection_text.start, x11->selection_text.len);
        mp_mutex_unlock(&x11->lock);
    } else if (request.target == XA(x11, UTF8_STRING) && request.selection == XA_PRIMARY) {
        mp_mutex_lock(&x11->lock);
        XChangeProperty(x11->display, request.requestor, request.property,
                        request.target, 8, PropModeReplace,
                        (unsigned char*)x11->primary_selection_text.start, x11->primary_selection_text.len);
        mp_mutex_unlock(&x11->lock);
    } else {
        reply.xselection.property = None;
    }
    XSendEvent(x11->display, request.requestor, False, 0, &reply);
}

static void clipboard_x11_handle_selection_notify(struct clipboard_x11_priv *x11, XEvent *event)
{
    unsigned char *data = NULL;
    Atom actual_type;
    int actual_format;
    unsigned long nitems;
    unsigned long bytes_after;
    Atom property = event->xselection.property;
    Atom selection = event->xselection.selection;
    if (property == None)
        return;
    XGetWindowProperty(x11->display, event->xselection.requestor, property,
                       0, LONG_MAX, True, XA(x11, UTF8_STRING), &actual_type, &actual_format,
                       &nitems, &bytes_after, &data);
    if (actual_type == XA(x11, UTF8_STRING) && selection == XA(x11, CLIPBOARD)) {
        mp_mutex_lock(&x11->lock);
        talloc_free(x11->selection_text.start);
        x11->selection_text = bstrdup(x11, bstr0(data));
        x11->data_changed = true;
        mp_mutex_unlock(&x11->lock);
    } else if (actual_type == XA(x11, UTF8_STRING) && selection == XA_PRIMARY) {
        mp_mutex_lock(&x11->lock);
        talloc_free(x11->primary_selection_text.start);
        x11->primary_selection_text = bstrdup(x11, bstr0(data));
        x11->data_changed = true;
        mp_mutex_unlock(&x11->lock);
    }
    XFree(data);
}

static void clipboard_x11_set_owner(struct clipboard_x11_priv *x11)
{
    XSetSelectionOwner(x11->display, XA_PRIMARY, x11->window, CurrentTime);
    XSetSelectionOwner(x11->display, XA(x11, CLIPBOARD), x11->window, CurrentTime);
}

static bool clipboard_x11_dispatch_events(struct clipboard_x11_priv *x11, int64_t timeout_ns)
{
    struct pollfd fds[] = {
        {.fd = ConnectionNumber(x11->display), .events = POLLIN},
        {.fd = x11->message_pipe[0], .events = POLLIN},
    };

    mp_poll(fds, MP_ARRAY_SIZE(fds), timeout_ns);

    if (fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) {
        MP_FATAL(x11, "Error occurred on the display fd\n");
        return false;
    }

    if (fds[0].revents & POLLIN) {
        while (XPending(x11->display)) {
            XEvent event;
            XNextEvent(x11->display, &event);
            MP_TRACE(x11, "XEvent: %d\n", event.type);
            if (event.type == x11->XFixesSelectionNotifyEvent) {
                XFixesSelectionNotifyEvent *ev = (XFixesSelectionNotifyEvent *)&event;
                if (ev->owner != x11->window && ev->selection == XA(x11, CLIPBOARD)) {
                    XConvertSelection(x11->display, ev->selection, XA(x11, UTF8_STRING),
                                      XA(x11, MPV_CLIPBOARD), x11->window, CurrentTime);
                } else if (ev->owner != x11->window && ev->selection == XA_PRIMARY) {
                    XConvertSelection(x11->display, ev->selection, XA(x11, UTF8_STRING),
                                      XA(x11, MPV_PRIMARY), x11->window, CurrentTime);
                }
            } else if (event.type == SelectionRequest) {
                clipboard_x11_handle_selection_request(x11, &event);
            } else if (event.type == SelectionNotify) {
                clipboard_x11_handle_selection_notify(x11, &event);
            }
        }
    }

    if (fds[1].revents & POLLIN) {
        uint8_t msg = 0;
        if (read(x11->message_pipe[0], &msg, sizeof(msg)) == sizeof(msg) && msg == MESSAGE_SET_OWNER) {
            clipboard_x11_set_owner(x11);
            XFlush(x11->display);
        } else {
            return false;
        }
    }

    return true;
}

static void clipboard_x11_run(struct clipboard_x11_priv *x11)
{
    while (clipboard_x11_dispatch_events(x11, MP_TIME_S_TO_NS(10)))
        ;

    clipboard_x11_uninit(x11);
}

static MP_THREAD_VOID clipboard_thread(void *p)
{
    struct clipboard_x11_priv *priv = p;
    clipboard_x11_run(priv);
    MP_THREAD_RETURN();
}

static int init(struct clipboard_ctx *cl, struct clipboard_init_params *params)
{
    cl->priv = talloc_zero(cl, struct clipboard_x11_priv);
    struct clipboard_x11_priv *priv = cl->priv;
    priv->message_pipe[0] = priv->message_pipe[1] = -1;
    priv->log = mp_log_new(priv, cl->log, "x11");
    mp_mutex_init(&priv->lock);

    if (mp_make_wakeup_pipe(priv->message_pipe) < 0)
        goto pipe_err;
    if (!clipboard_x11_init(priv, params->flags & CLIPBOARD_INIT_ENABLE_XWAYLAND))
        goto init_err;
    if (mp_thread_create(&priv->thread, clipboard_thread, cl->priv))
        goto thread_err;
    return CLIPBOARD_SUCCESS;

thread_err:
    clipboard_x11_uninit(priv);
init_err:
    close(priv->message_pipe[0]);
    close(priv->message_pipe[1]);
pipe_err:
    mp_mutex_destroy(&priv->lock);
    TA_FREEP(&cl->priv);
    return CLIPBOARD_FAILED;
}

static void uninit(struct clipboard_ctx *cl)
{
    struct clipboard_x11_priv *priv = cl->priv;
    if (!priv)
        return;
    (void)write(priv->message_pipe[1], &MESSAGE_DEATH, sizeof(MESSAGE_DEATH));
    mp_thread_join(priv->thread);
    close(priv->message_pipe[0]);
    close(priv->message_pipe[1]);
    mp_mutex_destroy(&priv->lock);
    TA_FREEP(&cl->priv);
}

static bool data_changed(struct clipboard_ctx *cl)
{
    struct clipboard_x11_priv *priv = cl->priv;
    bool res;
    mp_mutex_lock(&priv->lock);
    res = priv->data_changed;
    if (res)
        priv->data_changed = false;
    mp_mutex_unlock(&priv->lock);
    return res;
}

static int get_data(struct clipboard_ctx *cl, struct clipboard_access_params *params,
                    struct clipboard_data *out, void *talloc_ctx)
{
    if (params->type != CLIPBOARD_DATA_TEXT)
        return CLIPBOARD_FAILED;

    int ret = CLIPBOARD_FAILED;
    struct clipboard_x11_priv *priv = cl->priv;
    mp_mutex_lock(&priv->lock);
    switch (params->target) {
    case CLIPBOARD_TARGET_CLIPBOARD:
        out->type = CLIPBOARD_DATA_TEXT;
        out->u.text = bstrto0(talloc_ctx, priv->selection_text);
        ret = CLIPBOARD_SUCCESS;
        break;
    case CLIPBOARD_TARGET_PRIMARY_SELECTION:
        out->type = CLIPBOARD_DATA_TEXT;
        out->u.text = bstrto0(talloc_ctx, priv->primary_selection_text);
        ret = CLIPBOARD_SUCCESS;
        break;
    }
    mp_mutex_unlock(&priv->lock);
    return ret;
}

static int set_data(struct clipboard_ctx *cl, struct clipboard_access_params *params,
                    struct clipboard_data *data)
{
    if (params->type != CLIPBOARD_DATA_TEXT || data->type != CLIPBOARD_DATA_TEXT)
        return CLIPBOARD_FAILED;

    struct clipboard_x11_priv *priv = cl->priv;
    bstr text = bstrdup(priv, bstr0(data->u.text));
    mp_mutex_lock(&priv->lock);
    switch (params->target) {
    case CLIPBOARD_TARGET_CLIPBOARD:
        talloc_free(priv->selection_text.start);
        priv->selection_text = text;
        break;
    case CLIPBOARD_TARGET_PRIMARY_SELECTION:
        talloc_free(priv->primary_selection_text.start);
        priv->primary_selection_text = text;
        break;
    }
    mp_mutex_unlock(&priv->lock);
    (void)write(priv->message_pipe[1], &MESSAGE_SET_OWNER, sizeof(MESSAGE_SET_OWNER));
    return CLIPBOARD_SUCCESS;
}

const struct clipboard_backend clipboard_backend_x11 = {
    .name = "x11",
    .desc = "x11 clipboard",
    .init = init,
    .uninit = uninit,
    .data_changed = data_changed,
    .get_data = get_data,
    .set_data = set_data,
};
