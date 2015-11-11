/*
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
 *
 * You can alternatively redistribute this file and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include <errno.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/vt.h>
#include <unistd.h>

#include "drm_common.h"

#include "common/common.h"
#include "common/msg.h"
#include "osdep/io.h"

#define EVT_RELEASE 1
#define EVT_ACQUIRE 2
#define EVT_INTERRUPT 255
#define HANDLER_ACQUIRE 0
#define HANDLER_RELEASE 1
#define RELEASE_SIGNAL SIGUSR1
#define ACQUIRE_SIGNAL SIGUSR2

static int vt_switcher_pipe[2];

// KMS ------------------------------------------------------------------------

static bool is_connector_valid(struct kms *kms, int connector_id,
                               drmModeConnector *connector, bool silent)
{
    if (!connector) {
        if (!silent) {
            MP_ERR(kms, "Cannot get connector %d: %s\n", connector_id,
                   mp_strerror(errno));
        }
        return false;
    }

    if (connector->connection != DRM_MODE_CONNECTED) {
        if (!silent) {
            MP_ERR(kms, "Connector %d is disconnected\n", connector_id);
        }
        return false;
    }

    if (connector->count_modes == 0) {
        if (!silent) {
            MP_ERR(kms, "Connector %d has no valid modes\n", connector_id);
        }
        return false;
    }

    return true;
}

static bool setup_connector(
    struct kms *kms, const drmModeRes *res, int connector_id)
{
    drmModeConnector *connector = NULL;
    if (connector_id == -1) {
        // get the first connected connector
        for (int i = 0; i < res->count_connectors; i++) {
            connector = drmModeGetConnector(kms->fd, res->connectors[i]);
            if (is_connector_valid(kms, i, connector, true)) {
                connector_id = i;
                break;
            }
            if (connector) {
                drmModeFreeConnector(connector);
                connector = NULL;
            }
        }
        if (connector_id == -1) {
            MP_ERR(kms, "No connected connectors found\n");
            return false;
        }
    }

    if (connector_id < 0 || connector_id >= res->count_connectors) {
        MP_ERR(kms, "Bad connector ID. Max valid connector ID = %u\n",
               res->count_connectors);
        return false;
    }

    connector = drmModeGetConnector(kms->fd, res->connectors[connector_id]);
    if (!is_connector_valid(kms, connector_id, connector, false))
        return false;

    kms->connector = connector;
    return true;
}

static bool setup_crtc(struct kms *kms, const drmModeRes *res)
{
    for (unsigned int i = 0; i < kms->connector->count_encoders; ++i) {
        drmModeEncoder *encoder
            = drmModeGetEncoder(kms->fd, kms->connector->encoders[i]);
        if (!encoder) {
            MP_WARN(kms, "Cannot retrieve encoder %u:%u: %s\n",
                    i, kms->connector->encoders[i], mp_strerror(errno));
            continue;
        }

        // iterate all global CRTCs
        for (unsigned int j = 0; j < res->count_crtcs; ++j) {
            // check whether this CRTC works with the encoder
            if (!(encoder->possible_crtcs & (1 << j)))
                continue;

            kms->encoder = encoder;
            kms->crtc_id = encoder->crtc_id;
            return true;
        }

        drmModeFreeEncoder(encoder);
    }

    MP_ERR(kms,
           "Connector %u has no suitable CRTC\n",
           kms->connector->connector_id);
    return false;
}

static bool setup_mode(struct kms *kms, int mode_id)
{
    if (mode_id < 0 || mode_id >= kms->connector->count_modes) {
        MP_ERR(
            kms,
            "Bad mode ID (max = %d).\n",
            kms->connector->count_modes - 1);

        MP_INFO(kms, "Available modes:\n");
        for (unsigned int i = 0; i < kms->connector->count_modes; i++) {
            MP_INFO(kms,
                    "Mode %d: %s (%dx%d)\n",
                    i,
                    kms->connector->modes[i].name,
                    kms->connector->modes[i].hdisplay,
                    kms->connector->modes[i].vdisplay);
        }
        return false;
    }

    kms->mode = kms->connector->modes[mode_id];
    return true;
}


struct kms *kms_create(struct mp_log *log)
{
    struct kms *ret = talloc(NULL, struct kms);
    *ret = (struct kms) {
        .log = mp_log_new(ret, log, "kms"),
        .fd = -1,
        .connector = NULL,
        .encoder = NULL,
        .mode = { 0 },
        .crtc_id = -1,
    };
    return ret;
}

bool kms_setup(struct kms *kms, const char *device_path, int connector_id, int mode_id)
{
    kms->fd = open(device_path, O_RDWR | O_CLOEXEC);
    if (kms->fd < 0) {
        MP_ERR(kms, "Cannot open \"%s\": %s.\n", device_path, mp_strerror(errno));
        return false;
    }

    drmModeRes *res = drmModeGetResources(kms->fd);
    if (!res) {
        MP_ERR(kms, "Cannot retrieve DRM resources: %s\n", mp_strerror(errno));
        return false;
    }

    if (!setup_connector(kms, res, connector_id))
        return false;
    if (!setup_crtc(kms, res))
        return false;
    if (!setup_mode(kms, mode_id))
        return false;

    return true;
}

void kms_destroy(struct kms *kms)
{
    if (!kms)
        return;
    if (kms->connector) {
        drmModeFreeConnector(kms->connector);
        kms->connector = NULL;
    }
    if (kms->encoder) {
        drmModeFreeEncoder(kms->encoder);
        kms->encoder = NULL;
    }
    close(kms->fd);
    talloc_free(kms);
}



// VT switcher ----------------------------------------------------------------

static void vt_switcher_sighandler(int sig)
{
    unsigned char event = sig == RELEASE_SIGNAL ? EVT_RELEASE : EVT_ACQUIRE;
    write(vt_switcher_pipe[1], &event, sizeof(event));
}

static bool has_signal_installed(int signo)
{
    struct sigaction act = { 0 };
    sigaction(signo, 0, &act);
    return act.sa_handler != 0;
}

static int install_signal(int signo, void (*handler)(int))
{
    struct sigaction act = { 0 };
    act.sa_handler = handler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_RESTART;
    return sigaction(signo, &act, NULL);
}


bool vt_switcher_init(struct vt_switcher *s, struct mp_log *log)
{
    s->log = log;
    s->tty_fd = -1;
    vt_switcher_pipe[0] = -1;
    vt_switcher_pipe[1] = -1;

    if (mp_make_cloexec_pipe(vt_switcher_pipe)) {
        MP_ERR(s, "Creating pipe failed: %s\n", mp_strerror(errno));
        return false;
    }

    s->tty_fd = open("/dev/tty", O_RDWR | O_CLOEXEC);
    if (s->tty_fd < 0) {
        MP_ERR(s, "Can't open TTY for VT control: %s\n", mp_strerror(errno));
        return false;
    }

    if (has_signal_installed(RELEASE_SIGNAL)) {
        MP_ERR(s, "Can't handle VT release - signal already used\n");
        return false;
    }
    if (has_signal_installed(ACQUIRE_SIGNAL)) {
        MP_ERR(s, "Can't handle VT acquire - signal already used\n");
        return false;
    }

    if (install_signal(RELEASE_SIGNAL, vt_switcher_sighandler)) {
        MP_ERR(s, "Failed to install release signal: %s\n", mp_strerror(errno));
        return false;
    }
    if (install_signal(ACQUIRE_SIGNAL, vt_switcher_sighandler)) {
        MP_ERR(s, "Failed to install acquire signal: %s\n", mp_strerror(errno));
        return false;
    }

    struct vt_mode vt_mode;
    if (ioctl(s->tty_fd, VT_GETMODE, &vt_mode) < 0) {
        MP_ERR(s, "VT_GETMODE failed: %s\n", mp_strerror(errno));
        return false;
    }

    vt_mode.mode = VT_PROCESS;
    vt_mode.relsig = RELEASE_SIGNAL;
    vt_mode.acqsig = ACQUIRE_SIGNAL;
    if (ioctl(s->tty_fd, VT_SETMODE, &vt_mode) < 0) {
        MP_ERR(s, "VT_SETMODE failed: %s\n", mp_strerror(errno));
        return false;
    }

    return true;
}

void vt_switcher_acquire(struct vt_switcher *s,
                         void (*handler)(void*), void *user_data)
{
    s->handlers[HANDLER_ACQUIRE] = handler;
    s->handler_data[HANDLER_ACQUIRE] = user_data;
}

void vt_switcher_release(struct vt_switcher *s,
                         void (*handler)(void*), void *user_data)
{
    s->handlers[HANDLER_RELEASE] = handler;
    s->handler_data[HANDLER_RELEASE] = user_data;
}

void vt_switcher_interrupt_poll(struct vt_switcher *s)
{
    unsigned char event = EVT_INTERRUPT;
    write(vt_switcher_pipe[1], &event, sizeof(event));
}

void vt_switcher_destroy(struct vt_switcher *s)
{
    install_signal(RELEASE_SIGNAL, SIG_DFL);
    install_signal(ACQUIRE_SIGNAL, SIG_DFL);
    close(s->tty_fd);
    close(vt_switcher_pipe[0]);
    close(vt_switcher_pipe[1]);
}

void vt_switcher_poll(struct vt_switcher *s, int timeout_ms)
{
    struct pollfd fds[1] = {
        { .events = POLLIN, .fd = vt_switcher_pipe[0] },
    };
    poll(fds, 1, timeout_ms);
    if (!fds[0].revents)
        return;

    unsigned char event;
    if (read(fds[0].fd, &event, sizeof(event)) != sizeof(event))
        return;

    switch (event) {
    case EVT_RELEASE:
        s->handlers[HANDLER_RELEASE](s->handler_data[HANDLER_RELEASE]);

        if (ioctl(s->tty_fd, VT_RELDISP, 1) < 0) {
            MP_ERR(s, "Failed to release virtual terminal\n");
        }
        break;

    case EVT_ACQUIRE:
        s->handlers[HANDLER_ACQUIRE](s->handler_data[HANDLER_ACQUIRE]);

        if (ioctl(s->tty_fd, VT_RELDISP, VT_ACKACQ) < 0) {
            MP_ERR(s, "Failed to acquire virtual terminal\n");
        }
        break;

    case EVT_INTERRUPT:
        break;
    }
}
