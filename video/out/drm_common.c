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

#include <errno.h>
#include <string.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <sys/stat.h>
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
#define MAX_CONNECTOR_NAME_LEN 20

static int vt_switcher_pipe[2];

#define OPT_BASE_STRUCT struct drm_opts
const struct m_sub_options drm_conf = {
    .opts = (const struct m_option[]) {
        OPT_STRING_VALIDATE("drm-connector", drm_connector_spec,
                            0, drm_validate_connector_opt),
        OPT_INT("drm-mode", drm_mode_id, 0),
        OPT_INT("drm-osd-plane-id", drm_osd_plane_id, 0),
        OPT_INT("drm-video-plane-id", drm_video_plane_id, 0),
        OPT_CHOICE("drm-format", drm_format, 0,
                   ({"xrgb8888",    DRM_OPTS_FORMAT_XRGB8888},
                    {"xrgb2101010", DRM_OPTS_FORMAT_XRGB2101010})),
        OPT_SIZE_BOX("drm-osd-size", drm_osd_size, 0),
        {0},
    },
    .defaults = &(const struct drm_opts) {
        .drm_osd_plane_id = -1,
        .drm_video_plane_id = -1,
    },
    .size = sizeof(struct drm_opts),
};

static const char *connector_names[] = {
    "Unknown",   // DRM_MODE_CONNECTOR_Unknown
    "VGA",       // DRM_MODE_CONNECTOR_VGA
    "DVI-I",     // DRM_MODE_CONNECTOR_DVII
    "DVI-D",     // DRM_MODE_CONNECTOR_DVID
    "DVI-A",     // DRM_MODE_CONNECTOR_DVIA
    "Composite", // DRM_MODE_CONNECTOR_Composite
    "SVIDEO",    // DRM_MODE_CONNECTOR_SVIDEO
    "LVDS",      // DRM_MODE_CONNECTOR_LVDS
    "Component", // DRM_MODE_CONNECTOR_Component
    "DIN",       // DRM_MODE_CONNECTOR_9PinDIN
    "DP",        // DRM_MODE_CONNECTOR_DisplayPort
    "HDMI-A",    // DRM_MODE_CONNECTOR_HDMIA
    "HDMI-B",    // DRM_MODE_CONNECTOR_HDMIB
    "TV",        // DRM_MODE_CONNECTOR_TV
    "eDP",       // DRM_MODE_CONNECTOR_eDP
    "Virtual",   // DRM_MODE_CONNECTOR_VIRTUAL
    "DSI",       // DRM_MODE_CONNECTOR_DSI
    "DPI",       // DRM_MODE_CONNECTOR_DPI
};

// KMS ------------------------------------------------------------------------

static void get_connector_name(
    drmModeConnector *connector, char ret[MAX_CONNECTOR_NAME_LEN])
{
    snprintf(ret, MAX_CONNECTOR_NAME_LEN, "%s-%d",
             connector_names[connector->connector_type],
             connector->connector_type_id);
}

// Gets the first connector whose name matches the input parameter.
// The returned connector may be disconnected.
// Result must be freed with drmModeFreeConnector.
static drmModeConnector *get_connector_by_name(const struct kms *kms,
                                               const drmModeRes *res,
                                               const char *connector_name)
{
    for (int i = 0; i < res->count_connectors; i++) {
        drmModeConnector *connector
            = drmModeGetConnector(kms->fd, res->connectors[i]);
        if (!connector)
            continue;
        char other_connector_name[MAX_CONNECTOR_NAME_LEN];
        get_connector_name(connector, other_connector_name);
        if (!strcmp(connector_name, other_connector_name))
            return connector;
        drmModeFreeConnector(connector);
    }
    return NULL;
}

// Gets the first connected connector.
// Result must be freed with drmModeFreeConnector.
static drmModeConnector *get_first_connected_connector(const struct kms *kms,
                                                       const drmModeRes *res)
{
    for (int i = 0; i < res->count_connectors; i++) {
        drmModeConnector *connector
            = drmModeGetConnector(kms->fd, res->connectors[i]);
        if (!connector)
            continue;
        if (connector->connection == DRM_MODE_CONNECTED
        && connector->count_modes > 0) {
            return connector;
        }
        drmModeFreeConnector(connector);
    }
    return NULL;
}

static bool setup_connector(struct kms *kms, const drmModeRes *res,
                            const char *connector_name)
{
    drmModeConnector *connector;

    if (connector_name
    && strcmp(connector_name, "")
    && strcmp(connector_name, "auto")) {
        connector = get_connector_by_name(kms, res, connector_name);
        if (!connector) {
            MP_ERR(kms, "No connector with name %s found\n", connector_name);
            kms_show_available_connectors(kms->log, kms->card_no);
            return false;
        }
    } else {
        connector = get_first_connected_connector(kms, res);
        if (!connector) {
            MP_ERR(kms, "No connected connectors found\n");
            return false;
        }
    }

    if (connector->connection != DRM_MODE_CONNECTED) {
        drmModeFreeConnector(connector);
        MP_ERR(kms, "Chosen connector is disconnected\n");
        return false;
    }

    if (connector->count_modes == 0) {
        drmModeFreeConnector(connector);
        MP_ERR(kms, "Chosen connector has no valid modes\n");
        return false;
    }

    kms->connector = connector;
    return true;
}

static bool setup_crtc(struct kms *kms, const drmModeRes *res)
{
    // First try to find currently connected encoder and its current CRTC
    for (unsigned int i = 0; i < res->count_encoders; i++) {
        drmModeEncoder *encoder = drmModeGetEncoder(kms->fd, res->encoders[i]);
        if (!encoder) {
            MP_WARN(kms, "Cannot retrieve encoder %u:%u: %s\n",
                    i, res->encoders[i], mp_strerror(errno));
            continue;
        }

        if (encoder->encoder_id == kms->connector->encoder_id && encoder->crtc_id != 0) {
            MP_VERBOSE(kms, "Connector %u currently connected to encoder %u\n",
                       kms->connector->connector_id, kms->connector->encoder_id);
            kms->encoder = encoder;
            kms->crtc_id = encoder->crtc_id;
            goto success;
        }

        drmModeFreeEncoder(encoder);
    }

    // Otherwise pick first legal encoder and CRTC combo for the connector
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
            kms->crtc_id = res->crtcs[j];
            goto success;
        }

        drmModeFreeEncoder(encoder);
    }

    MP_ERR(kms, "Connector %u has no suitable CRTC\n",
           kms->connector->connector_id);
    return false;

  success:
    MP_VERBOSE(kms, "Selected Encoder %u with CRTC %u\n",
               kms->encoder->encoder_id, kms->crtc_id);
    return true;
}

static bool setup_mode(struct kms *kms, int mode_id)
{
    if (mode_id < 0 || mode_id >= kms->connector->count_modes) {
        MP_ERR(kms, "Bad mode ID (max = %d).\n",
               kms->connector->count_modes - 1);

        kms_show_available_modes(kms->log, kms->connector);
        return false;
    }

    kms->mode.mode = kms->connector->modes[mode_id];
    return true;
}

static int open_card(int card_no)
{
    char card_path[128];
    snprintf(card_path, sizeof(card_path), DRM_DEV_NAME, DRM_DIR_NAME, card_no);
    return open(card_path, O_RDWR | O_CLOEXEC);
}

static void parse_connector_spec(struct mp_log *log,
                                 const char *connector_spec,
                                 int *card_no, char **connector_name)
{
    if (!connector_spec) {
        *card_no = 0;
        *connector_name = NULL;
        return;
    }
    char *dot_ptr = strchr(connector_spec, '.');
    if (dot_ptr) {
        *card_no = atoi(connector_spec);
        *connector_name = talloc_strdup(log, dot_ptr + 1);
    } else {
        *card_no = 0;
        *connector_name = talloc_strdup(log, connector_spec);
    }
}


struct kms *kms_create(struct mp_log *log, const char *connector_spec,
                       int mode_id, int osd_plane_id, int video_plane_id)
{
    int card_no = -1;
    char *connector_name = NULL;
    parse_connector_spec(log, connector_spec, &card_no, &connector_name);

    struct kms *kms = talloc(NULL, struct kms);
    *kms = (struct kms) {
        .log = mp_log_new(kms, log, "kms"),
        .fd = open_card(card_no),
        .connector = NULL,
        .encoder = NULL,
        .mode = {{0}},
        .crtc_id = -1,
        .card_no = card_no,
    };

    drmModeRes *res = NULL;

    if (kms->fd < 0) {
        mp_err(log, "Cannot open card \"%d\": %s.\n",
               card_no, mp_strerror(errno));
        goto err;
    }

    res = drmModeGetResources(kms->fd);
    if (!res) {
        mp_err(log, "Cannot retrieve DRM resources: %s\n", mp_strerror(errno));
        goto err;
    }

    if (!setup_connector(kms, res, connector_name))
        goto err;
    if (!setup_crtc(kms, res))
        goto err;
    if (!setup_mode(kms, mode_id))
        goto err;

    // Universal planes allows accessing all the planes (including primary)
    if (drmSetClientCap(kms->fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1)) {
        mp_err(log, "Failed to set Universal planes capability\n");
    }

    if (drmSetClientCap(kms->fd, DRM_CLIENT_CAP_ATOMIC, 1)) {
        mp_verbose(log, "No DRM Atomic support found\n");
    } else {
        mp_verbose(log, "DRM Atomic support found\n");
        kms->atomic_context = drm_atomic_create_context(kms->log, kms->fd, kms->crtc_id,
                                                        kms->connector->connector_id, osd_plane_id, video_plane_id);
        if (!kms->atomic_context) {
            mp_err(log, "Failed to create DRM atomic context\n");
            goto err;
        }
    }

    drmModeFreeResources(res);
    return kms;

err:
    if (res)
        drmModeFreeResources(res);
    if (connector_name)
        talloc_free(connector_name);
    kms_destroy(kms);
    return NULL;
}

void kms_destroy(struct kms *kms)
{
    if (!kms)
        return;
    drm_mode_destroy_blob(kms->fd, &kms->mode);
    if (kms->connector) {
        drmModeFreeConnector(kms->connector);
        kms->connector = NULL;
    }
    if (kms->encoder) {
        drmModeFreeEncoder(kms->encoder);
        kms->encoder = NULL;
    }
    if (kms->atomic_context) {
       drm_atomic_destroy_context(kms->atomic_context);
    }

    close(kms->fd);
    talloc_free(kms);
}

static double mode_get_Hz(const drmModeModeInfo *mode)
{
    return mode->clock * 1000.0 / mode->htotal / mode->vtotal;
}

void kms_show_available_modes(
    struct mp_log *log, const drmModeConnector *connector)
{
    mp_info(log, "Available modes:\n");
    for (unsigned int i = 0; i < connector->count_modes; i++) {
        mp_info(log, "Mode %d: %s (%dx%d@%.2fHz)\n", i,
                connector->modes[i].name,
                connector->modes[i].hdisplay,
                connector->modes[i].vdisplay,
                mode_get_Hz(&connector->modes[i]));
    }
}

void kms_show_available_connectors(struct mp_log *log, int card_no)
{
    mp_info(log, "Available connectors for card %d:\n", card_no);

    int fd = open_card(card_no);
    if (fd < 0) {
        mp_err(log, "Failed to open card %d\n", card_no);
        return;
    }

    drmModeRes *res = drmModeGetResources(fd);
    if (!res) {
        mp_err(log, "Cannot retrieve DRM resources: %s\n", mp_strerror(errno));
        goto err;
    }

    for (int i = 0; i < res->count_connectors; i++) {
        drmModeConnector *connector
            = drmModeGetConnector(fd, res->connectors[i]);
        if (!connector)
            continue;
        char other_connector_name[MAX_CONNECTOR_NAME_LEN];
        get_connector_name(connector, other_connector_name);
        mp_info(log, "%s (%s)\n", other_connector_name,
                connector->connection == DRM_MODE_CONNECTED
                    ? "connected"
                    : "disconnected");
        drmModeFreeConnector(connector);
    }

err:
    if (fd >= 0)
        close(fd);
    if (res)
        drmModeFreeResources(res);
}

void kms_show_available_cards_and_connectors(struct mp_log *log)
{
    for (int card_no = 0; card_no < DRM_MAX_MINOR; card_no++) {
        int fd = open_card(card_no);
        if (fd < 0)
            break;
        close(fd);
        kms_show_available_connectors(log, card_no);
    }
}

double kms_get_display_fps(const struct kms *kms)
{
    return mode_get_Hz(&kms->mode.mode);
}

int drm_validate_connector_opt(struct mp_log *log, const struct m_option *opt,
                               struct bstr name, struct bstr param)
{
    if (bstr_equals0(param, "help")) {
        kms_show_available_cards_and_connectors(log);
        return M_OPT_EXIT;
    }
    return 1;
}



// VT switcher ----------------------------------------------------------------

static void vt_switcher_sighandler(int sig)
{
    unsigned char event = sig == RELEASE_SIGNAL ? EVT_RELEASE : EVT_ACQUIRE;
    (void)write(vt_switcher_pipe[1], &event, sizeof(event));
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

    // Block the VT switching signals from interrupting the VO thread (they will
    // still be picked up by other threads, which will fill vt_switcher_pipe for us)
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, RELEASE_SIGNAL);
    sigaddset(&set, ACQUIRE_SIGNAL);
    pthread_sigmask(SIG_BLOCK, &set, NULL);

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
    (void)write(vt_switcher_pipe[1], &event, sizeof(event));
}

void vt_switcher_destroy(struct vt_switcher *s)
{
    struct vt_mode vt_mode = {0};
    vt_mode.mode = VT_AUTO;
    if (ioctl(s->tty_fd, VT_SETMODE, &vt_mode) < 0) {
        MP_ERR(s, "VT_SETMODE failed: %s\n", mp_strerror(errno));
        return;
    }

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
