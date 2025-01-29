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

#include <wayland-client.h>

#include "ext-data-control-v1.h"

#include "clipboard.h"
#include "common/common.h"
#include "common/msg.h"
#include "misc/bstr.h"
#include "osdep/io.h"
#include "osdep/poll_wrapper.h"
#include "osdep/threads.h"

struct clipboard_wayland_data_offer {
    struct ext_data_control_offer_v1 *offer;
    char *mime_type;
    int fd;
};

struct clipboard_wayland_priv {
    mp_mutex lock;
    // accessed by both threads
    int death_pipe[2];
    bstr selection_text;
    bstr primary_selection_text;
    bool data_changed;
    // only accessed by the core thread
    mp_thread thread;
    // only accessed by the clipboard thread
    struct mp_log *log;
    int display_fd;
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_list seat_list;
    struct ext_data_control_manager_v1 *dcman;
    struct clipboard_wayland_data_offer *selection_offer;
    struct clipboard_wayland_data_offer *primary_selection_offer;
};

struct clipboard_wayland_seat {
    struct clipboard_wayland_priv *wl;
    struct wl_seat *seat;
    struct wl_list link;
    uint32_t id;
    struct ext_data_control_device_v1 *dcdev;
    bool offered_plain_text;
};

static void remove_seat(struct clipboard_wayland_seat *seat)
{
    if (!seat)
        return;

    MP_VERBOSE(seat->wl, "Deregistering seat 0x%x\n", seat->id);
    wl_list_remove(&seat->link);
    if (seat->dcdev)
        ext_data_control_device_v1_destroy(seat->dcdev);
    wl_seat_destroy(seat->seat);
    talloc_free(seat);
    return;
}

static void destroy_offer(struct clipboard_wayland_data_offer *o)
{
    TA_FREEP(&o->mime_type);
    if (o->fd != -1)
        close(o->fd);
    if (o->offer)
        ext_data_control_offer_v1_destroy(o->offer);
    *o = (struct clipboard_wayland_data_offer){.fd = -1};
}


static void handle_selection(void *data,
                             struct ext_data_control_device_v1 *dcdev,
                             struct ext_data_control_offer_v1 *id,
                             struct clipboard_wayland_data_offer *o)
{
    struct clipboard_wayland_seat *s = data;
    struct clipboard_wayland_priv *wl = s->wl;
    if (o->offer) {
        destroy_offer(o);
        MP_VERBOSE(wl, "Received a new selection offer. Releasing the previous offer.\n");
    }
    o->offer = id;
    if (!id)
        return;

    int pipefd[2];

    if (pipe2(pipefd, O_CLOEXEC) == -1) {
        MP_ERR(wl, "Failed to create selection pipe!\n");
        return;
    }

    // Only receive plain text for now, may expand later.
    if (s->offered_plain_text) {
        ext_data_control_offer_v1_receive(o->offer, "text/plain;charset=utf-8", pipefd[1]);
        s->offered_plain_text = false;
    }
    close(pipefd[1]);
    o->fd = pipefd[0];
}

static void data_offer_handle_offer(void *data,
                                    struct ext_data_control_offer_v1 *ext_data_control_offer_v1,
                                    const char *mime_type)
{
    struct clipboard_wayland_seat *s = data;
    if (!s->offered_plain_text)
        s->offered_plain_text = !strcmp(mime_type, "text/plain;charset=utf-8");
}

static const struct ext_data_control_offer_v1_listener data_offer_listener = {
    data_offer_handle_offer,
};

static void data_device_handle_data_offer(void *data,
                                          struct ext_data_control_device_v1 *dcdev,
                                          struct ext_data_control_offer_v1 *id)
{
    struct clipboard_wayland_seat *s = data;
    ext_data_control_offer_v1_add_listener(id, &data_offer_listener, s);
}

static void data_device_handle_selection(void *data,
                                         struct ext_data_control_device_v1 *dcdev,
                                         struct ext_data_control_offer_v1 *id)
{
    struct clipboard_wayland_seat *s = data;
    struct clipboard_wayland_priv *wl = s->wl;
    handle_selection(data, dcdev, id, wl->selection_offer);
}

static void data_device_handle_finished(void *data,
                                        struct ext_data_control_device_v1 *dcdev)
{
    struct clipboard_wayland_seat *s = data;
    ext_data_control_device_v1_destroy(dcdev);
    s->dcdev = NULL;
}

static void data_device_handle_primary_selection(void *data,
                                                 struct ext_data_control_device_v1 *dcdev,
                                                 struct ext_data_control_offer_v1 *id)
{
    struct clipboard_wayland_seat *s = data;
    struct clipboard_wayland_priv *wl = s->wl;
    handle_selection(data, dcdev, id, wl->primary_selection_offer);
}

static const struct ext_data_control_device_v1_listener data_device_listener = {
    data_device_handle_data_offer,
    data_device_handle_selection,
    data_device_handle_finished,
    data_device_handle_primary_selection,
};

static void registry_handle_add(void *data, struct wl_registry *reg, uint32_t id,
                                const char *interface, uint32_t ver)
{
    int found = 1;
    struct clipboard_wayland_priv *wl = data;

    if (!strcmp(interface, wl_seat_interface.name) && found++) {
        ver = MPMIN(ver, 8);
        struct clipboard_wayland_seat *seat = talloc_zero(wl, struct clipboard_wayland_seat);
        seat->wl   = wl;
        seat->id   = id;
        seat->seat = wl_registry_bind(reg, id, &wl_seat_interface, ver);
        wl_list_insert(&wl->seat_list, &seat->link);
    }

    if (!strcmp(interface, ext_data_control_manager_v1_interface.name) && found++) {
        ver = MPMIN(ver, 2);
        wl->dcman = wl_registry_bind(reg, id, &ext_data_control_manager_v1_interface, ver);
    }

    if (found > 1)
        MP_VERBOSE(wl, "Registered interface %s at version %d\n", interface, ver);
}

static void registry_handle_remove(void *data, struct wl_registry *reg, uint32_t id)
{
    struct clipboard_wayland_priv *wl = data;
    struct clipboard_wayland_seat *seat, *seat_tmp;
    wl_list_for_each_safe(seat, seat_tmp, &wl->seat_list, link) {
        if (seat->id == id) {
            remove_seat(seat);
            return;
        }
    }
}

static const struct wl_registry_listener registry_listener = {
    registry_handle_add,
    registry_handle_remove,
};

static void clipboard_wayland_uninit(struct clipboard_wayland_priv *wl)
{
    if (!wl)
        return;

    destroy_offer(wl->selection_offer);
    destroy_offer(wl->primary_selection_offer);

    struct clipboard_wayland_seat *seat, *seat_tmp;
    wl_list_for_each_safe(seat, seat_tmp, &wl->seat_list, link)
        remove_seat(seat);

    if (wl->dcman)
        ext_data_control_manager_v1_destroy(wl->dcman);
    if (wl->registry)
        wl_registry_destroy(wl->registry);
    if (wl->display)
        wl_display_disconnect(wl->display);
}

static bool clipboard_wayland_init(struct clipboard_wayland_priv *wl)
{
    if (!getenv("WAYLAND_DISPLAY") && !getenv("WAYLAND_SOCKET"))
        goto err;

    wl->display = wl_display_connect(NULL);
    if (!wl->display)
        goto err;

    wl->registry = wl_display_get_registry(wl->display);
    wl_registry_add_listener(wl->registry, &registry_listener, wl);

    /* Do a roundtrip to run the registry */
    wl_display_roundtrip(wl->display);

    if (wl->dcman) {
        struct clipboard_wayland_seat *seat;
        wl_list_for_each(seat, &wl->seat_list, link) {
            seat->dcdev = ext_data_control_manager_v1_get_data_device(wl->dcman, seat->seat);
            ext_data_control_device_v1_add_listener(seat->dcdev, &data_device_listener, seat);
        }
    } else {
        MP_VERBOSE(wl, "Compositor doesn't support the %s protocol!\n",
                   ext_data_control_manager_v1_interface.name);
        goto err;
    }

    wl->display_fd = wl_display_get_fd(wl->display);

    /* Do another roundtrip to ensure all of the above is initialized
     * before mpv does anything else. */
    wl_display_roundtrip(wl->display);

    return true;

err:
    clipboard_wayland_uninit(wl);
    return false;
}

static void get_selection_data(struct clipboard_wayland_priv *wl, struct clipboard_wayland_data_offer *o,
                               bool is_primary)
{
    ssize_t data_read = 0;
    const size_t chunk_size = 256;
    bstr content = {
        .start = talloc_zero_size(wl, chunk_size),
    };

    while (1) {
        data_read = read(o->fd, content.start + content.len, chunk_size);
        if (data_read == -1 && errno == EINTR)
            continue;
        else if (data_read <= 0)
            break;
        content.len += data_read;
        content.start = talloc_realloc_size(wl, content.start, content.len + chunk_size);
        memset(content.start + content.len, 0, chunk_size);
    }

    if (data_read == -1) {
        MP_VERBOSE(wl, "data offer aborted (read error)\n");
    } else {
        MP_VERBOSE(wl, "Read %zu bytes from the data offer fd\n", content.len);
        // Update clipboard text content
        mp_mutex_lock(&wl->lock);
        if (is_primary) {
            talloc_free(wl->primary_selection_text.start);
            wl->primary_selection_text = content;
        } else {
            talloc_free(wl->selection_text.start);
            wl->selection_text = content;
        }
        wl->data_changed = true;
        mp_mutex_unlock(&wl->lock);
        content = (bstr){0};
    }

    talloc_free(content.start);
    destroy_offer(o);
}

static bool clipboard_wayland_dispatch_events(struct clipboard_wayland_priv *wl, int64_t timeout_ns)
{
    if (wl->display_fd == -1)
        return false;

    struct pollfd fds[] = {
        {.fd = wl->display_fd,    .events = POLLIN },
        {.fd = wl->death_pipe[0], .events = POLLIN },
        {.fd = wl->selection_offer->fd, .events = POLLIN },
        {.fd = wl->primary_selection_offer->fd, .events = POLLIN },
    };

    while (wl_display_prepare_read(wl->display) != 0)
        wl_display_dispatch_pending(wl->display);
    wl_display_flush(wl->display);

    mp_poll(fds, MP_ARRAY_SIZE(fds), timeout_ns);

    if (fds[0].revents & POLLIN) {
        wl_display_read_events(wl->display);
    } else {
        wl_display_cancel_read(wl->display);
    }

    if (fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) {
        MP_FATAL(wl, "Error occurred on the display fd\n");
        wl->display_fd = -1;
        return false;
    }

    if (fds[1].revents & POLLIN)
        return false;

    if (fds[2].revents & POLLIN)
        get_selection_data(wl, wl->selection_offer, false);

    if (fds[3].revents & POLLIN)
        get_selection_data(wl, wl->primary_selection_offer, true);

    wl_display_dispatch_pending(wl->display);
    return true;
}

static void clipboard_wayland_run(struct clipboard_wayland_priv *wl)
{
    while (clipboard_wayland_dispatch_events(wl, MP_TIME_S_TO_NS(10)))
        ;

    clipboard_wayland_uninit(wl);
}

static MP_THREAD_VOID clipboard_thread(void *p)
{
    struct clipboard_wayland_priv *priv = p;
    clipboard_wayland_run(priv);
    MP_THREAD_RETURN();
}

static int init(struct clipboard_ctx *cl, struct clipboard_init_params *params)
{
    cl->priv = talloc_zero(cl, struct clipboard_wayland_priv);
    struct clipboard_wayland_priv *priv = cl->priv;
    priv->death_pipe[0] = priv->death_pipe[1] = priv->display_fd = -1;
    priv->log = mp_log_new(priv, cl->log, "wayland");
    priv->selection_offer = talloc_zero(priv, struct clipboard_wayland_data_offer),
    priv->primary_selection_offer = talloc_zero(priv, struct clipboard_wayland_data_offer),
    priv->selection_offer->fd = priv->primary_selection_offer->fd = -1;
    wl_list_init(&priv->seat_list);
    mp_mutex_init(&priv->lock);

    if (mp_make_wakeup_pipe(priv->death_pipe) < 0)
        goto pipe_err;
    if (!clipboard_wayland_init(priv))
        goto init_err;
    if (mp_thread_create(&priv->thread, clipboard_thread, cl->priv))
        goto thread_err;
    return CLIPBOARD_SUCCESS;

thread_err:
    clipboard_wayland_uninit(priv);
init_err:
    close(priv->death_pipe[0]);
    close(priv->death_pipe[1]);
pipe_err:
    mp_mutex_destroy(&priv->lock);
    TA_FREEP(&cl->priv);
    return CLIPBOARD_FAILED;
}

static void uninit(struct clipboard_ctx *cl)
{
    struct clipboard_wayland_priv *priv = cl->priv;
    if (!priv)
        return;
    (void)write(priv->death_pipe[1], &(char){0}, 1);
    mp_thread_join(priv->thread);
    close(priv->death_pipe[0]);
    close(priv->death_pipe[1]);
    mp_mutex_destroy(&priv->lock);
    TA_FREEP(&cl->priv);
}

static bool data_changed(struct clipboard_ctx *cl)
{
    struct clipboard_wayland_priv *priv = cl->priv;
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
    struct clipboard_wayland_priv *priv = cl->priv;
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

const struct clipboard_backend clipboard_backend_wayland = {
    .name = "wayland",
    .desc = "Wayland clipboard (Data control protocol)",
    .init = init,
    .uninit = uninit,
    .data_changed = data_changed,
    .get_data = get_data,
};
