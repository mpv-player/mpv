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
#include <unistd.h>
#include <limits.h>
#include <math.h>
#include <time.h>
#include <drm_fourcc.h>

#include "config.h"

#if HAVE_CONSIO_H
#include <sys/consio.h>
#elif HAVE_VT_H
#include <sys/vt.h>
#elif HAVE_WSDISPLAY_USL_IO_H
#include <dev/wscons/wsdisplay_usl_io.h>
#endif

#include "drm_atomic.h"
#include "drm_common.h"

#include "common/common.h"
#include "common/msg.h"
#include "misc/ctype.h"
#include "options/m_config.h"
#include "options/path.h"
#include "osdep/io.h"
#include "osdep/poll_wrapper.h"
#include "osdep/timer.h"
#include "present_sync.h"
#include "video/out/vo.h"

#define EVT_RELEASE 1
#define EVT_ACQUIRE 2
#define EVT_INTERRUPT 255
#define HANDLER_ACQUIRE 0
#define HANDLER_RELEASE 1
#define RELEASE_SIGNAL SIGUSR1
#define ACQUIRE_SIGNAL SIGUSR2
#define MAX_CONNECTOR_NAME_LEN 20

#define DRM_PRIM_FACTOR 50000
#define DRM_MIN_LUMA_FACTOR 10000

static int vt_switcher_pipe[2];

static int drm_connector_opt_help(struct mp_log *log, const struct m_option *opt,
                                  struct bstr name);

static int drm_mode_opt_help(struct mp_log *log, const struct m_option *opt,
                             struct bstr name);

static OPT_STRING_VALIDATE_FUNC(drm_validate_mode_opt);

static void drm_show_available_modes(struct mp_log *log, const drmModeConnector *connector);

static void drm_show_available_connectors(struct mp_log *log, int card_no,
                                          const char *card_path);
static double mode_get_Hz(const drmModeModeInfo *mode);

#define OPT_BASE_STRUCT struct drm_opts
const struct m_sub_options drm_conf = {
    .opts = (const struct m_option[]) {
        {"drm-device", OPT_STRING(device_path), .flags = M_OPT_FILE},
        {"drm-connector", OPT_STRING(connector_spec),
            .help = drm_connector_opt_help},
        {"drm-mode", OPT_STRING_VALIDATE(mode_spec, drm_validate_mode_opt),
            .help = drm_mode_opt_help},
        {"drm-draw-plane", OPT_CHOICE(draw_plane,
            {"primary", DRM_OPTS_PRIMARY_PLANE},
            {"overlay", DRM_OPTS_OVERLAY_PLANE}),
            M_RANGE(0, INT_MAX)},
        {"drm-drmprime-video-plane", OPT_CHOICE(drmprime_video_plane,
            {"primary", DRM_OPTS_PRIMARY_PLANE},
            {"overlay", DRM_OPTS_OVERLAY_PLANE}),
            M_RANGE(0, INT_MAX)},
        {"drm-format", OPT_CHOICE(drm_format,
            {"xrgb8888",    DRM_OPTS_FORMAT_XRGB8888},
            {"xrgb2101010", DRM_OPTS_FORMAT_XRGB2101010},
            {"xbgr8888",    DRM_OPTS_FORMAT_XBGR8888},
            {"xbgr2101010", DRM_OPTS_FORMAT_XBGR2101010},
            {"yuyv",        DRM_OPTS_FORMAT_YUYV})},
        {"drm-draw-surface-size", OPT_SIZE_BOX(draw_surface_size)},
        {"drm-vrr-enabled", OPT_CHOICE(vrr_enabled,
            {"no", 0}, {"yes", 1}, {"auto", -1})},
        {0},
    },
    .defaults = &(const struct drm_opts) {
        .mode_spec = "preferred",
        .draw_plane = DRM_OPTS_PRIMARY_PLANE,
        .drmprime_video_plane = DRM_OPTS_OVERLAY_PLANE,
        .drm_format = DRM_OPTS_FORMAT_XRGB8888,
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
    "Writeback", // DRM_MODE_CONNECTOR_WRITEBACK
    "SPI",       // DRM_MODE_CONNECTOR_SPI
    "USB",       // DRM_MODE_CONNECTOR_USB
};

static int eotf_map[PL_COLOR_TRC_COUNT] = {
    [PL_COLOR_TRC_BT_1886] = HDMI_EOTF_TRADITIONAL_GAMMA_SDR,
    [PL_COLOR_TRC_PQ] = HDMI_EOTF_SMPTE_ST2084,
    [PL_COLOR_TRC_HLG] = HDMI_EOTF_BT_2100_HLG,
};

struct drm_mode_spec {
    enum {
        DRM_MODE_SPEC_BY_IDX,     // Specified by idx
        DRM_MODE_SPEC_BY_NUMBERS, // Specified by width, height and opt. refresh
        DRM_MODE_SPEC_PREFERRED,  // Select the preferred mode of the display
        DRM_MODE_SPEC_HIGHEST,    // Select the mode with the highest resolution
    } type;
    unsigned int idx;
    unsigned int width;
    unsigned int height;
    double refresh;
};

/* VT Switcher */
static void vt_switcher_sighandler(int sig)
{
    int saved_errno = errno;
    unsigned char event = sig == RELEASE_SIGNAL ? EVT_RELEASE : EVT_ACQUIRE;
    (void)write(vt_switcher_pipe[1], &event, sizeof(event));
    errno = saved_errno;
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

static void release_vt(void *data)
{
    struct vo_drm_state *drm = data;
    MP_VERBOSE(drm, "Releasing VT\n");
    vo_drm_release_crtc(drm);
}

static void acquire_vt(void *data)
{
    struct vo_drm_state *drm = data;
    MP_VERBOSE(drm, "Acquiring VT\n");
    vo_drm_acquire_crtc(drm);
}

static void vt_switcher_acquire(struct vt_switcher *s,
                         void (*handler)(void*), void *user_data)
{
    s->handlers[HANDLER_ACQUIRE] = handler;
    s->handler_data[HANDLER_ACQUIRE] = user_data;
}

static void vt_switcher_release(struct vt_switcher *s,
                         void (*handler)(void*), void *user_data)
{
    s->handlers[HANDLER_RELEASE] = handler;
    s->handler_data[HANDLER_RELEASE] = user_data;
}

static bool vt_switcher_init(struct vt_switcher *s, struct mp_log *log)
{
    s->tty_fd = -1;
    s->log = log;
    vt_switcher_pipe[0] = -1;
    vt_switcher_pipe[1] = -1;

    if (mp_make_cloexec_pipe(vt_switcher_pipe)) {
        mp_err(log, "Creating pipe failed: %s\n", mp_strerror(errno));
        return false;
    }

    s->tty_fd = open("/dev/tty", O_RDWR | O_CLOEXEC);
    if (s->tty_fd < 0) {
        mp_err(log, "Can't open TTY for VT control: %s\n", mp_strerror(errno));
        return false;
    }

    if (has_signal_installed(RELEASE_SIGNAL)) {
        mp_err(log, "Can't handle VT release - signal already used\n");
        return false;
    }
    if (has_signal_installed(ACQUIRE_SIGNAL)) {
        mp_err(log, "Can't handle VT acquire - signal already used\n");
        return false;
    }

    if (install_signal(RELEASE_SIGNAL, vt_switcher_sighandler)) {
        mp_err(log, "Failed to install release signal: %s\n", mp_strerror(errno));
        return false;
    }
    if (install_signal(ACQUIRE_SIGNAL, vt_switcher_sighandler)) {
        mp_err(log, "Failed to install acquire signal: %s\n", mp_strerror(errno));
        return false;
    }

    struct vt_mode vt_mode = { 0 };
    if (ioctl(s->tty_fd, VT_GETMODE, &vt_mode) < 0) {
        mp_err(log, "VT_GETMODE failed: %s\n", mp_strerror(errno));
        return false;
    }

    vt_mode.mode = VT_PROCESS;
    vt_mode.relsig = RELEASE_SIGNAL;
    vt_mode.acqsig = ACQUIRE_SIGNAL;
    // frsig is a signal for forced release. Not implemented on Linux,
    // Solaris, BSDs but must be set to a valid signal on some of those.
    vt_mode.frsig = SIGIO; // unused
    if (ioctl(s->tty_fd, VT_SETMODE, &vt_mode) < 0) {
        mp_err(log, "VT_SETMODE failed: %s\n", mp_strerror(errno));
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

static void vt_switcher_interrupt_poll(struct vt_switcher *s)
{
    unsigned char event = EVT_INTERRUPT;
    (void)write(vt_switcher_pipe[1], &event, sizeof(event));
}

static void vt_switcher_destroy(struct vt_switcher *s)
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

static void vt_switcher_poll(struct vt_switcher *s, int timeout_ns)
{
    struct pollfd fds[1] = {
        { .events = POLLIN, .fd = vt_switcher_pipe[0] },
    };
    mp_poll(fds, 1, timeout_ns);
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

bool vo_drm_acquire_crtc(struct vo_drm_state *drm)
{
    if (drm->active)
        return true;
    drm->active = true;

    if (drmSetMaster(drm->fd)) {
        MP_WARN(drm, "Failed to acquire DRM master: %s\n",
                mp_strerror(errno));
    }

    struct drm_atomic_context *atomic_ctx = drm->atomic_context;

    if (!drm_atomic_save_old_state(atomic_ctx))
        MP_WARN(drm, "Failed to save old DRM atomic state\n");

    drmModeAtomicReqPtr request = drmModeAtomicAlloc();
    if (!request) {
        MP_ERR(drm, "Failed to allocate drm atomic request\n");
        goto err;
    }

    if (drm_object_set_property(request, atomic_ctx->connector, "CRTC_ID", drm->crtc_id) < 0) {
        MP_ERR(drm, "Could not set CRTC_ID on connector\n");
        goto err;
    }

    if (!drm_mode_ensure_blob(drm->fd, &drm->mode)) {
        MP_ERR(drm, "Failed to create DRM mode blob\n");
        goto err;
    }
    if (drm_object_set_property(request, atomic_ctx->crtc, "MODE_ID", drm->mode.blob_id) < 0) {
        MP_ERR(drm, "Could not set MODE_ID on crtc\n");
        goto err;
    }
    if (drm_object_set_property(request, atomic_ctx->crtc, "ACTIVE", 1) < 0) {
        MP_ERR(drm, "Could not set ACTIVE on crtc\n");
        goto err;
    }

    /*
     * VRR related properties were added in kernel 5.0. We will not fail if we
     * cannot query or set the value, but we will log as appropriate.
     */
    uint64_t vrr_capable = 0;
    drm_object_get_property(atomic_ctx->connector, "VRR_CAPABLE", &vrr_capable);
    MP_VERBOSE(drm, "crtc is%s VRR capable\n", vrr_capable ? "" : " not");

    uint64_t vrr_requested = drm->opts->vrr_enabled;
    if (vrr_requested == 1 || (vrr_capable && vrr_requested == -1)) {
        if (drm_object_set_property(request, atomic_ctx->crtc, "VRR_ENABLED", 1) < 0) {
            MP_WARN(drm, "Could not enable VRR on crtc\n");
        } else {
            MP_VERBOSE(drm, "Enabled VRR on crtc\n");
        }
    }

    drm_object_set_property(request, atomic_ctx->draw_plane, "FB_ID",   drm->fb->id);
    drm_object_set_property(request, atomic_ctx->draw_plane, "CRTC_ID", drm->crtc_id);
    drm_object_set_property(request, atomic_ctx->draw_plane, "SRC_X",   0);
    drm_object_set_property(request, atomic_ctx->draw_plane, "SRC_Y",   0);
    drm_object_set_property(request, atomic_ctx->draw_plane, "SRC_W",   drm->width << 16);
    drm_object_set_property(request, atomic_ctx->draw_plane, "SRC_H",   drm->height << 16);
    drm_object_set_property(request, atomic_ctx->draw_plane, "CRTC_X",  0);
    drm_object_set_property(request, atomic_ctx->draw_plane, "CRTC_Y",  0);
    drm_object_set_property(request, atomic_ctx->draw_plane, "CRTC_W",  drm->mode.mode.hdisplay);
    drm_object_set_property(request, atomic_ctx->draw_plane, "CRTC_H",  drm->mode.mode.vdisplay);

    if (drmModeAtomicCommit(drm->fd, request, DRM_MODE_ATOMIC_ALLOW_MODESET, NULL)) {
        MP_ERR(drm, "Failed to commit ModeSetting atomic request: %s\n", mp_strerror(errno));
        goto err;
    }

    drmModeAtomicFree(request);
    return true;

err:
    drmModeAtomicFree(request);
    return false;
}


void vo_drm_release_crtc(struct vo_drm_state *drm)
{
    if (!drm->active)
        return;
    drm->active = false;

    if (!drm->atomic_context->old_state.saved)
        return;

    bool success = true;
    struct drm_atomic_context *atomic_ctx = drm->atomic_context;
    drmModeAtomicReqPtr request = drmModeAtomicAlloc();
    if (!request) {
        MP_ERR(drm, "Failed to allocate drm atomic request\n");
        success = false;
    }

    if (request && !drm_atomic_restore_old_state(request, atomic_ctx)) {
        MP_WARN(drm, "Got error while restoring old state\n");
        success = false;
    }

    if (request) {
        if (drmModeAtomicCommit(drm->fd, request, DRM_MODE_ATOMIC_ALLOW_MODESET, NULL)) {
            MP_WARN(drm, "Failed to commit ModeSetting atomic request: %s\n",
                    mp_strerror(errno));
            success = false;
        }
    }

    if (request)
        drmModeAtomicFree(request);

    if (!success)
        MP_ERR(drm, "Failed to restore previous mode\n");

    if (drmDropMaster(drm->fd)) {
        MP_WARN(drm, "Failed to drop DRM master: %s\n",
                mp_strerror(errno));
    }
}

/* libdrm */
static void destroy_hdr_blob(struct vo_drm_state *drm)
{
    if (drm->hdr.blob_id) {
        drmModeDestroyPropertyBlob(drm->fd, drm->hdr.blob_id);
        drm->hdr.blob_id = 0;
    }
}

static void get_connector_name(const drmModeConnector *connector,
                               char ret[MAX_CONNECTOR_NAME_LEN])
{
    const char *type_name;

    if (connector->connector_type < MP_ARRAY_SIZE(connector_names)) {
        type_name = connector_names[connector->connector_type];
    } else {
        type_name = "UNKNOWN";
    }

    snprintf(ret, MAX_CONNECTOR_NAME_LEN, "%s-%d", type_name,
             connector->connector_type_id);
}

// Gets the first connector whose name matches the input parameter.
// The returned connector may be disconnected.
// Result must be freed with drmModeFreeConnector.
static drmModeConnector *get_connector_by_name(const drmModeRes *res,
                                               const char *connector_name,
                                               int fd)
{
    for (int i = 0; i < res->count_connectors; i++) {
        drmModeConnector *connector
            = drmModeGetConnector(fd, res->connectors[i]);
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
static drmModeConnector *get_first_connected_connector(const drmModeRes *res,
                                                       int fd)
{
    for (int i = 0; i < res->count_connectors; i++) {
        drmModeConnector *connector = drmModeGetConnector(fd, res->connectors[i]);
        if (!connector)
            continue;
        if (connector->connection == DRM_MODE_CONNECTED && connector->count_modes > 0) {
            return connector;
        }
        drmModeFreeConnector(connector);
    }
    return NULL;
}

static void restore_sdr(struct vo_drm_state *drm)
{
    struct drm_atomic_context *atomic_ctx = drm->atomic_context;
    if (!atomic_ctx)
        return;

    vo_drm_set_hdr_metadata(drm->vo, true);
    int ret = drmModeAtomicCommit(drm->fd, atomic_ctx->request, DRM_MODE_ATOMIC_ALLOW_MODESET, drm);
    if (ret)
        MP_VERBOSE(drm, "Failed to commit atomic request: %s\n", mp_strerror(ret));
}

static bool setup_connector(struct vo_drm_state *drm, const drmModeRes *res,
                            const char *connector_name)
{
    drmModeConnector *connector;

    if (connector_name && strcmp(connector_name, "") && strcmp(connector_name, "auto")) {
        connector = get_connector_by_name(res, connector_name, drm->fd);
        if (!connector) {
            MP_ERR(drm, "No connector with name %s found\n", connector_name);
            drm_show_available_connectors(drm->log, drm->card_no, drm->card_path);
            return false;
        }
    } else {
        connector = get_first_connected_connector(res, drm->fd);
        if (!connector) {
            MP_ERR(drm, "No connected connectors found\n");
            return false;
        }
    }

    if (connector->connection != DRM_MODE_CONNECTED) {
        drmModeFreeConnector(connector);
        MP_ERR(drm, "Chosen connector is disconnected\n");
        return false;
    }

    if (connector->count_modes == 0) {
        drmModeFreeConnector(connector);
        MP_ERR(drm, "Chosen connector has no valid modes\n");
        return false;
    }

    drm->connector = connector;
    return true;
}

static bool setup_crtc(struct vo_drm_state *drm, const drmModeRes *res)
{
    // First try to find currently connected encoder and its current CRTC
    for (unsigned int i = 0; i < res->count_encoders; i++) {
        drmModeEncoder *encoder = drmModeGetEncoder(drm->fd, res->encoders[i]);
        if (!encoder) {
            MP_WARN(drm, "Cannot retrieve encoder %u:%u: %s\n",
                    i, res->encoders[i], mp_strerror(errno));
            continue;
        }

        if (encoder->encoder_id == drm->connector->encoder_id && encoder->crtc_id != 0) {
            MP_VERBOSE(drm, "Connector %u currently connected to encoder %u\n",
                       drm->connector->connector_id, drm->connector->encoder_id);
            drm->encoder = encoder;
            drm->crtc_id = encoder->crtc_id;
            goto success;
        }

        drmModeFreeEncoder(encoder);
    }

    // Otherwise pick first legal encoder and CRTC combo for the connector
    for (unsigned int i = 0; i < drm->connector->count_encoders; ++i) {
        drmModeEncoder *encoder
            = drmModeGetEncoder(drm->fd, drm->connector->encoders[i]);
        if (!encoder) {
            MP_WARN(drm, "Cannot retrieve encoder %u:%u: %s\n",
                    i, drm->connector->encoders[i], mp_strerror(errno));
            continue;
        }

        // iterate all global CRTCs
        for (unsigned int j = 0; j < res->count_crtcs; ++j) {
            // check whether this CRTC works with the encoder
            if (!(encoder->possible_crtcs & (1 << j)))
                continue;

            drm->encoder = encoder;
            drm->crtc_id = res->crtcs[j];
            goto success;
        }

        drmModeFreeEncoder(encoder);
    }

    MP_ERR(drm, "Connector %u has no suitable CRTC\n",
           drm->connector->connector_id);
    return false;

success:
    MP_VERBOSE(drm, "Selected Encoder %u with CRTC %u\n",
               drm->encoder->encoder_id, drm->crtc_id);
    return true;
}

static void setup_edid(struct vo_drm_state *drm)
{
    drmModePropertyBlobRes *blob = NULL;
    for (int i = 0; i < drm->connector->count_props; ++i) {
        drmModePropertyRes *prop = drmModeGetProperty(drm->fd, drm->connector->props[i]);
        if (prop && strcmp(prop->name, "EDID") == 0)
            blob = drmModeGetPropertyBlob(drm->fd, drm->connector->prop_values[i]);
        drmModeFreeProperty(prop);
        if (blob)
            break;
    }
    if (!blob) {
        MP_VERBOSE(drm, "Unable to get EDID blob from connector.\n");
        return;
    }

    drm->info = di_info_parse_edid(blob->data, blob->length);
    if (!drm->info) {
        MP_VERBOSE(drm, "Failed to parse EDID info: %s\n", mp_strerror(errno));
        goto done;
    }

    const struct di_edid *edid = di_info_get_edid(drm->info);
    if (!edid) {
        MP_VERBOSE(drm, "Failed to get EDID info: %s\n", mp_strerror(errno));
        goto done;
    }

    drm->chromaticity = di_edid_get_chromaticity_coords(edid);

    const struct di_edid_cta *cta;
    const struct di_edid_ext *const *exts = di_edid_get_extensions(edid);
    while (*exts && !(cta = di_edid_ext_get_cta(*exts++)));

    if (!cta) {
        MP_VERBOSE(drm, "Unable to find CTA-861 extension block.\n");
        goto done;
    }

    const struct di_cta_data_block *const *blocks = di_edid_cta_get_data_blocks(cta);
    for (int i = 0; *blocks && blocks[i]; ++i) {
        if (!drm->colorimetry)
            drm->colorimetry = di_cta_data_block_get_colorimetry(blocks[i]);
        if (!drm->hdr_static_metadata)
            drm->hdr_static_metadata = di_cta_data_block_get_hdr_static_metadata(blocks[i]);
        if (drm->colorimetry && drm->hdr_static_metadata)
            break;
    }

done:
    if (blob)
        drmModeFreePropertyBlob(blob);
}

static bool target_params_supported_by_display(struct vo_drm_state *drm)
{
    if (!drm->chromaticity || !drm->colorimetry || !drm->hdr_static_metadata)
        return false;

    const struct di_cta_hdr_static_metadata_block *hdr_static_metadata = drm->hdr_static_metadata;
    enum pl_color_transfer trc = drm->target_params.color.transfer;

    if (!pl_color_transfer_is_hdr(trc) && !hdr_static_metadata->eotfs->traditional_sdr)
        return false;

    if (pl_color_transfer_is_hdr(trc) && !drm->colorimetry->bt2020_rgb)
        return false;

    if (trc == PL_COLOR_TRC_PQ && !hdr_static_metadata->eotfs->pq)
        return false;

    if (trc == PL_COLOR_TRC_HLG && !hdr_static_metadata->eotfs->hlg)
        return false;

    return true;
}

static bool all_digits(const char *str)
{
    if (str == NULL || str[0] == '\0') {
        return false;
    }

    for (const char *c = str; *c != '\0'; ++c) {
        if (!mp_isdigit(*c))
            return false;
    }
    return true;
}

static bool parse_mode_spec(const char *spec, struct drm_mode_spec *parse_result)
{
    if (spec == NULL || spec[0] == '\0' || strcmp(spec, "preferred") == 0) {
        if (parse_result) {
            *parse_result =
                (struct drm_mode_spec) { .type = DRM_MODE_SPEC_PREFERRED };
        }
        return true;
    }

    if (strcmp(spec, "highest") == 0) {
        if (parse_result) {
            *parse_result =
                (struct drm_mode_spec) { .type = DRM_MODE_SPEC_HIGHEST };
        }
        return true;
    }

    // If the string is made up of only digits, it means that it is an index number
    if (all_digits(spec)) {
        if (parse_result) {
            *parse_result = (struct drm_mode_spec) {
                .type = DRM_MODE_SPEC_BY_IDX,
                .idx = strtoul(spec, NULL, 10),
            };
        }
        return true;
    }

    if (!mp_isdigit(spec[0]))
        return false;
    char *height_part, *refresh_part;
    const unsigned int width = strtoul(spec, &height_part, 10);
    if (spec == height_part || height_part[0] == '\0' || height_part[0] != 'x')
        return false;

    height_part += 1;
    if (!mp_isdigit(height_part[0]))
        return false;
    const unsigned int height = strtoul(height_part, &refresh_part, 10);
    if (height_part == refresh_part)
        return false;

    char *rest = NULL;
    double refresh;
    switch (refresh_part[0]) {
    case '\0':
        refresh = nan("");
        break;
    case '@':
        refresh_part += 1;
        if (!(mp_isdigit(refresh_part[0]) || refresh_part[0] == '.'))
            return false;
        refresh = strtod(refresh_part, &rest);
        if (refresh_part == rest || rest[0] != '\0' || refresh < 0.0)
            return false;
        break;
    default:
        return false;
    }

    if (parse_result) {
        *parse_result = (struct drm_mode_spec) {
            .type = DRM_MODE_SPEC_BY_NUMBERS,
            .width = width,
            .height = height,
            .refresh = refresh,
        };
    }
    return true;
}

static bool setup_mode_by_idx(struct vo_drm_state *drm, unsigned int mode_idx)
{
    if (mode_idx >= drm->connector->count_modes) {
        MP_ERR(drm, "Bad mode index (max = %d).\n",
               drm->connector->count_modes - 1);
        return false;
    }

    drm->mode.mode = drm->connector->modes[mode_idx];
    return true;
}

static bool mode_match(const drmModeModeInfo *mode,
                       unsigned int width,
                       unsigned int height,
                       double refresh)
{
    if (isnan(refresh)) {
        return
            (mode->hdisplay == width) &&
            (mode->vdisplay == height);
    } else {
        const double mode_refresh = mode_get_Hz(mode);
        return
            (mode->hdisplay == width) &&
            (mode->vdisplay == height) &&
            ((int)round(refresh*100) == (int)round(mode_refresh*100));
    }
}

static bool setup_mode_by_numbers(struct vo_drm_state *drm,
                                  unsigned int width,
                                  unsigned int height,
                                  double refresh)
{
    for (unsigned int i = 0; i < drm->connector->count_modes; ++i) {
        drmModeModeInfo *current_mode = &drm->connector->modes[i];
        if (mode_match(current_mode, width, height, refresh)) {
            drm->mode.mode = *current_mode;
            return true;
        }
    }

    MP_ERR(drm, "Could not find mode matching %s\n", drm->opts->mode_spec);
    return false;
}

static bool setup_mode_preferred(struct vo_drm_state *drm)
{
    for (unsigned int i = 0; i < drm->connector->count_modes; ++i) {
        drmModeModeInfo *current_mode = &drm->connector->modes[i];
        if (current_mode->type & DRM_MODE_TYPE_PREFERRED) {
            drm->mode.mode = *current_mode;
            return true;
        }
    }

    // Fall back to first mode
    MP_WARN(drm, "Could not find any preferred mode. Picking the first mode.\n");
    drm->mode.mode = drm->connector->modes[0];
    return true;
}

static bool setup_mode_highest(struct vo_drm_state *drm)
{
    unsigned int area = 0;
    drmModeModeInfo *highest_resolution_mode = &drm->connector->modes[0];
    for (unsigned int i = 0; i < drm->connector->count_modes; ++i) {
        drmModeModeInfo *current_mode = &drm->connector->modes[i];

        const unsigned int current_area =
            current_mode->hdisplay * current_mode->vdisplay;
        if (current_area > area) {
            highest_resolution_mode = current_mode;
            area = current_area;
        }
    }

    drm->mode.mode = *highest_resolution_mode;
    return true;
}

static bool setup_mode(struct vo_drm_state *drm)
{
    if (drm->connector->count_modes <= 0) {
        MP_ERR(drm, "No available modes\n");
        return false;
    }

    struct drm_mode_spec parsed;
    if (!parse_mode_spec(drm->opts->mode_spec, &parsed)) {
        MP_ERR(drm, "Parse error\n");
        goto err;
    }

    switch (parsed.type) {
    case DRM_MODE_SPEC_BY_IDX:
        if (!setup_mode_by_idx(drm, parsed.idx))
            goto err;
        break;
    case DRM_MODE_SPEC_BY_NUMBERS:
        if (!setup_mode_by_numbers(drm, parsed.width, parsed.height, parsed.refresh))
            goto err;
        break;
    case DRM_MODE_SPEC_PREFERRED:
        if (!setup_mode_preferred(drm))
            goto err;
        break;
    case DRM_MODE_SPEC_HIGHEST:
        if (!setup_mode_highest(drm))
            goto err;
        break;
    default:
        MP_ERR(drm, "setup_mode: Internal error\n");
        goto err;
    }

    drmModeModeInfo *mode = &drm->mode.mode;
    MP_VERBOSE(drm, "Selected mode: %s (%dx%d@%.2fHz)\n",
        mode->name, mode->hdisplay, mode->vdisplay, mode_get_Hz(mode));

    return true;

err:
    MP_INFO(drm, "Available modes:\n");
    drm_show_available_modes(drm->log, drm->connector);
    return false;
}

static int open_card_path(const char *path)
{
    return open(path, O_RDWR | O_CLOEXEC);
}

static bool card_supports_kms(const char *path)
{
    int fd = open_card_path(path);
    bool ret = fd != -1 && drmIsKMS(fd);
    if (fd != -1)
        close(fd);
    return ret;
}

static bool card_has_connection(const char *path)
{
    int fd = open_card_path(path);
    bool ret = false;
    if (fd != -1) {
        drmModeRes *res = drmModeGetResources(fd);
        if (res) {
            drmModeConnector *connector = get_first_connected_connector(res, fd);
            if (connector)
                ret = true;
            drmModeFreeConnector(connector);
            drmModeFreeResources(res);
        }
        close(fd);
    }
    return ret;
}

static void get_primary_device_path(struct vo_drm_state *drm)
{
    if (drm->opts->device_path) {
        drm->card_path = mp_get_user_path(drm, drm->vo->global, drm->opts->device_path);
        return;
    }

    drmDevice *devices[DRM_MAX_MINOR] = { 0 };
    int card_count = drmGetDevices2(0, devices, MP_ARRAY_SIZE(devices));
    bool card_no_given = drm->card_no >= 0;

    if (card_count < 0) {
        MP_ERR(drm, "Listing DRM devices with drmGetDevices failed! (%s)\n",
               mp_strerror(errno));
        goto err;
    }

    if (card_no_given && drm->card_no > (card_count - 1)) {
        MP_ERR(drm, "Card number %d given too high! %d devices located.\n",
               drm->card_no, card_count);
        goto err;
    }

    for (int i = card_no_given ? drm->card_no : 0; i < card_count; i++) {
        drmDevice *dev = devices[i];

        if (!(dev->available_nodes & (1 << DRM_NODE_PRIMARY))) {
            if (card_no_given) {
                MP_ERR(drm, "DRM card number %d given, but it does not have "
                       "a primary node!\n", i);
                break;
            }

            continue;
        }

        const char *card_path = dev->nodes[DRM_NODE_PRIMARY];

        if (!card_supports_kms(card_path)) {
            if (card_no_given) {
                MP_ERR(drm,
                       "DRM card number %d given, but it does not support "
                       "KMS!\n", i);
                break;
            }

            continue;
        }

        if (!card_has_connection(card_path)) {
            if (card_no_given) {
                MP_ERR(drm,
                        "DRM card number %d given, but it does not have any "
                        "connected outputs.\n", i);
                break;
            }

            continue;
        }

        MP_VERBOSE(drm, "Picked DRM card %d, primary node %s%s.\n",
                   i, card_path,
                   card_no_given ? "" : " as the default");

        drm->card_path = talloc_strdup(drm, card_path);
        drm->card_no = i;
        break;
    }

    if (!drm->card_path)
        MP_ERR(drm, "No primary DRM device could be picked!\n");

err:
    drmFreeDevices(devices, card_count);
}

static void drm_pflip_cb(int fd, unsigned int msc, unsigned int sec,
                         unsigned int usec, void *data)
{
    struct vo_drm_state *drm = data;

    int64_t ust = MP_TIME_S_TO_NS(sec) + MP_TIME_US_TO_NS(usec);
    present_sync_update_values(drm->present, ust, msc);
    present_sync_swap(drm->present);
    drm->waiting_for_flip = false;
}

int vo_drm_control(struct vo *vo, int *events, int request, void *arg)
{
    struct vo_drm_state *drm = vo->drm;
    switch (request) {
    case VOCTRL_GET_DISPLAY_FPS: {
        double fps = vo_drm_get_display_fps(drm);
        if (fps <= 0)
            break;
        *(double*)arg = fps;
        return VO_TRUE;
    }
    case VOCTRL_GET_DISPLAY_RES: {
        ((int *)arg)[0] = drm->mode.mode.hdisplay;
        ((int *)arg)[1] = drm->mode.mode.vdisplay;
        return VO_TRUE;
    }
    case VOCTRL_PAUSE:
        vo->want_redraw = true;
        return VO_TRUE;
    case VOCTRL_REDRAW:
        drm->redraw = true;
        return VO_TRUE;
    }
    return VO_NOTIMPL;
}

bool vo_drm_init(struct vo *vo)
{
    vo->drm = talloc_zero(NULL, struct vo_drm_state);
    struct vo_drm_state *drm = vo->drm;

    *drm = (struct vo_drm_state) {
        .vo = vo,
        .log = mp_log_new(drm, vo->log, "drm"),
        .mode = {{0}},
        .crtc_id = -1,
        .card_no = -1,
    };

    drm->vt_switcher_active = vt_switcher_init(&drm->vt_switcher, drm->log);
    if (drm->vt_switcher_active) {
        vt_switcher_acquire(&drm->vt_switcher, acquire_vt, drm);
        vt_switcher_release(&drm->vt_switcher, release_vt, drm);
    } else {
        MP_WARN(drm, "Failed to set up VT switcher. Terminal switching will be unavailable.\n");
    }

    drm->opts = mp_get_config_group(drm, drm->vo->global, &drm_conf);

    drmModeRes *res = NULL;
    get_primary_device_path(drm);

    if (!drm->card_path) {
        MP_ERR(drm, "Failed to find a usable DRM primary node!\n");
        goto err;
    }

    drm->fd = open_card_path(drm->card_path);
    if (drm->fd < 0) {
        MP_ERR(drm, "Cannot open card \"%d\": %s.\n", drm->card_no, mp_strerror(errno));
        goto err;
    }

    drmVersionPtr ver = drmGetVersion(drm->fd);
    if (ver) {
        MP_VERBOSE(drm, "Driver: %s %d.%d.%d (%s)\n", ver->name, ver->version_major,
                   ver->version_minor, ver->version_patchlevel, ver->date);
        drmFreeVersion(ver);
    }

    res = drmModeGetResources(drm->fd);
    if (!res) {
        MP_ERR(drm, "Cannot retrieve DRM resources: %s\n", mp_strerror(errno));
        goto err;
    }

    if (!setup_connector(drm, res, drm->opts->connector_spec))
        goto err;
    if (!setup_crtc(drm, res))
        goto err;
    if (!setup_mode(drm))
        goto err;

    setup_edid(drm);

    // Universal planes allows accessing all the planes (including primary)
    if (drmSetClientCap(drm->fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1)) {
        MP_ERR(drm, "Failed to set Universal planes capability\n");
    }

    if (drmSetClientCap(drm->fd, DRM_CLIENT_CAP_ATOMIC, 1)) {
        MP_ERR(drm, "Failed to create DRM atomic context, no DRM Atomic support\n");
        goto err;
    } else {
        MP_VERBOSE(drm, "DRM Atomic support found\n");
        drm->atomic_context = drm_atomic_create_context(drm->log, drm->fd, drm->crtc_id,
                                                        drm->connector->connector_id,
                                                        drm->opts->draw_plane,
                                                        drm->opts->drmprime_video_plane);
        if (!drm->atomic_context) {
            MP_ERR(drm, "Failed to create DRM atomic context\n");
            goto err;
        }
    }

    drmModeFreeResources(res);

    drm->ev.version = DRM_EVENT_CONTEXT_VERSION;
    drm->ev.page_flip_handler = &drm_pflip_cb;
    drm->present = mp_present_initialize(drm, drm->vo->opts, VO_MAX_SWAPCHAIN_DEPTH);

    return true;

err:
    if (res)
        drmModeFreeResources(res);

    vo_drm_uninit(vo);
    return false;
}

void vo_drm_uninit(struct vo *vo)
{
    struct vo_drm_state *drm = vo->drm;
    if (!drm)
        return;

    restore_sdr(drm);
    destroy_hdr_blob(drm);

    if (drm->info)
        di_info_destroy(drm->info);

    vo_drm_release_crtc(drm);
    if (drm->vt_switcher_active)
        vt_switcher_destroy(&drm->vt_switcher);

    drm_mode_destroy_blob(drm->fd, &drm->mode);

    if (drm->connector) {
        drmModeFreeConnector(drm->connector);
        drm->connector = NULL;
    }
    if (drm->encoder) {
        drmModeFreeEncoder(drm->encoder);
        drm->encoder = NULL;
    }
    if (drm->atomic_context)
        drm_atomic_destroy_context(drm->atomic_context);

    close(drm->fd);
    talloc_free(drm);
    vo->drm = NULL;
}

static double mode_get_Hz(const drmModeModeInfo *mode)
{
    double rate = mode->clock * 1000.0 / mode->htotal / mode->vtotal;
    if (mode->flags & DRM_MODE_FLAG_INTERLACE)
        rate *= 2.0;
    return rate;
}

static void drm_show_available_modes(struct mp_log *log,
                                     const drmModeConnector *connector)
{
    for (unsigned int i = 0; i < connector->count_modes; i++) {
        mp_info(log, "  Mode %d: %s (%dx%d@%.2fHz)\n", i,
                connector->modes[i].name,
                connector->modes[i].hdisplay,
                connector->modes[i].vdisplay,
                mode_get_Hz(&connector->modes[i]));
    }
}

static void drm_show_foreach_connector(struct mp_log *log, int card_no,
                                       const char *card_path,
                                       void (*show_fn)(struct mp_log*, int,
                                                       const drmModeConnector*))
{
    int fd = open_card_path(card_path);
    if (fd < 0) {
        mp_err(log, "Failed to open card %d (%s)\n", card_no, card_path);
        return;
    }

    drmModeRes *res = drmModeGetResources(fd);
    if (!res) {
        mp_err(log, "Cannot retrieve DRM resources: %s\n", mp_strerror(errno));
        goto err;
    }

    for (int i = 0; i < res->count_connectors; i++) {
        drmModeConnector *connector = drmModeGetConnector(fd, res->connectors[i]);
        if (!connector)
            continue;
        show_fn(log, card_no, connector);
        drmModeFreeConnector(connector);
    }

err:
    if (fd >= 0)
        close(fd);
    if (res)
        drmModeFreeResources(res);
}

static void drm_show_connector_name_and_state_callback(struct mp_log *log, int card_no,
                                                       const drmModeConnector *connector)
{
    char other_connector_name[MAX_CONNECTOR_NAME_LEN];
    get_connector_name(connector, other_connector_name);
    const char *connection_str = (connector->connection == DRM_MODE_CONNECTED) ?
                                 "connected" : "disconnected";
    mp_info(log, "  %s (%s)\n", other_connector_name, connection_str);
}

static void drm_show_available_connectors(struct mp_log *log, int card_no,
                                          const char *card_path)
{
    if (card_no)
        mp_info(log, "\n");
    mp_info(log, "Available connectors for card %d (%s):\n", card_no,
            card_path);
    drm_show_foreach_connector(log, card_no, card_path,
                               drm_show_connector_name_and_state_callback);
}

static void drm_show_connector_modes_callback(struct mp_log *log, int card_no,
                                              const drmModeConnector *connector)
{
    if (connector->connection != DRM_MODE_CONNECTED)
        return;

    char other_connector_name[MAX_CONNECTOR_NAME_LEN];
    get_connector_name(connector, other_connector_name);
    if (card_no)
        mp_info(log, "\n");
    mp_info(log, "Available modes for drm-connector=%d.%s\n",
            card_no, other_connector_name);
    drm_show_available_modes(log, connector);
}

static void drm_show_available_connectors_and_modes(struct mp_log *log,
                                                    int card_no,
                                                    const char *card_path)
{
    drm_show_foreach_connector(log, card_no, card_path,
                               drm_show_connector_modes_callback);
}

static void drm_show_foreach_card(struct mp_log *log,
                                  void (*show_fn)(struct mp_log *, int,
                                                  const char *))
{
    drmDevice *devices[DRM_MAX_MINOR] = { 0 };
    int card_count = drmGetDevices2(0, devices, MP_ARRAY_SIZE(devices));
    if (card_count < 0) {
        mp_err(log, "Listing DRM devices with drmGetDevices failed! (%s)\n",
               mp_strerror(errno));
        return;
    }

    for (int i = 0; i < card_count; i++) {
        drmDevice *dev = devices[i];

        if (!(dev->available_nodes & (1 << DRM_NODE_PRIMARY)))
            continue;

        const char *card_path = dev->nodes[DRM_NODE_PRIMARY];

        int fd = open_card_path(card_path);
        if (fd < 0) {
            mp_err(log, "Failed to open primary DRM node path %s!\n",
                   card_path);
            continue;
        }

        close(fd);
        show_fn(log, i, card_path);
    }

    drmFreeDevices(devices, card_count);
}

static void drm_show_available_cards_and_connectors(struct mp_log *log)
{
    drm_show_foreach_card(log, drm_show_available_connectors);
}

static void drm_show_available_cards_connectors_and_modes(struct mp_log *log)
{
    drm_show_foreach_card(log, drm_show_available_connectors_and_modes);
}

static int drm_connector_opt_help(struct mp_log *log, const struct m_option *opt,
                                  struct bstr name)
{
    drm_show_available_cards_and_connectors(log);
    return M_OPT_EXIT;
}

static int drm_mode_opt_help(struct mp_log *log, const struct m_option *opt,
                             struct bstr name)
{
    drm_show_available_cards_connectors_and_modes(log);
    return M_OPT_EXIT;
}

static int drm_validate_mode_opt(struct mp_log *log, const struct m_option *opt,
                                 struct bstr name, const char **value)
{
    const char *param = *value;
    if (!parse_mode_spec(param, NULL)) {
        mp_fatal(log, "Invalid value for option drm-mode. Must be a positive number, a string of the format WxH[@R] or 'help'\n");
        return M_OPT_INVALID;
    }

    return 1;
}

/* Helpers */
double vo_drm_get_display_fps(struct vo_drm_state *drm)
{
    return mode_get_Hz(&drm->mode.mode);
}

bool vo_drm_set_hdr_metadata(struct vo *vo, bool force_sdr)
{
    struct vo_drm_state *drm = vo->drm;
    struct mp_image_params target_params = vo_get_target_params(vo);
    if (!force_sdr && (pl_color_space_equal(&target_params.color, &drm->target_params.color) ||
        !target_params.w || !target_params.h))
        return false;

    destroy_hdr_blob(drm);
    drm->target_params = target_params;
    drm->supported_colorspace = target_params_supported_by_display(drm);
    bool use_sdr = !drm->supported_colorspace || force_sdr;

    // For any HDR, the BT2020 drm colorspace is the only one that works in practice.
    struct drm_atomic_context *atomic_ctx = drm->atomic_context;
    int colorspace = !use_sdr && pl_color_space_is_hdr(&drm->target_params.color) ?
                     DRM_MODE_COLORIMETRY_BT2020_RGB : DRM_MODE_COLORIMETRY_DEFAULT;
    drm_object_set_property(atomic_ctx->request, atomic_ctx->connector, "Colorspace", colorspace);

    const struct pl_hdr_metadata *hdr = &target_params.color.hdr;
    struct hdr_output_metadata metadata = {
        .metadata_type = HDMI_STATIC_METADATA_TYPE1,
        .hdmi_metadata_type1.metadata_type = HDMI_STATIC_METADATA_TYPE1,

        .hdmi_metadata_type1.eotf = use_sdr ? HDMI_EOTF_TRADITIONAL_GAMMA_SDR : eotf_map[target_params.color.transfer],

        .hdmi_metadata_type1.display_primaries[0].x = lrintf(hdr->prim.red.x * DRM_PRIM_FACTOR),
        .hdmi_metadata_type1.display_primaries[0].y = lrintf(hdr->prim.red.y * DRM_PRIM_FACTOR),
        .hdmi_metadata_type1.display_primaries[1].x = lrintf(hdr->prim.green.x * DRM_PRIM_FACTOR),
        .hdmi_metadata_type1.display_primaries[1].y = lrintf(hdr->prim.green.y * DRM_PRIM_FACTOR),
        .hdmi_metadata_type1.display_primaries[2].x = lrintf(hdr->prim.blue.x * DRM_PRIM_FACTOR),
        .hdmi_metadata_type1.display_primaries[2].y = lrintf(hdr->prim.blue.y * DRM_PRIM_FACTOR),

        .hdmi_metadata_type1.white_point.x = lrintf(hdr->prim.white.x * DRM_PRIM_FACTOR),
        .hdmi_metadata_type1.white_point.y = lrintf(hdr->prim.white.y * DRM_PRIM_FACTOR),

        .hdmi_metadata_type1.min_display_mastering_luminance = lrintf(hdr->min_luma * DRM_MIN_LUMA_FACTOR),
        .hdmi_metadata_type1.max_display_mastering_luminance = hdr->max_luma,

        .hdmi_metadata_type1.max_cll = hdr->max_cll,
        .hdmi_metadata_type1.max_fall = hdr->max_fall,
    };
    drmModeCreatePropertyBlob(drm->fd, &metadata, sizeof(metadata), &drm->hdr.blob_id);
    drm_object_set_property(atomic_ctx->request, atomic_ctx->connector, "HDR_OUTPUT_METADATA", drm->hdr.blob_id);
    return true;
}

void vo_drm_set_monitor_par(struct vo *vo)
{
    struct vo_drm_state *drm = vo->drm;
    if (vo->opts->force_monitor_aspect != 0.0) {
        vo->monitor_par = drm->fb->width / (double) drm->fb->height /
                          vo->opts->force_monitor_aspect;
    } else {
        vo->monitor_par = 1 / vo->opts->monitor_pixel_aspect;
    }
    MP_VERBOSE(drm, "Monitor pixel aspect: %g\n", vo->monitor_par);
}

void vo_drm_wait_events(struct vo *vo, int64_t until_time_ns)
{
    struct vo_drm_state *drm = vo->drm;
    if (drm->vt_switcher_active) {
        int64_t wait_ns = until_time_ns - mp_time_ns();
        int64_t timeout_ns = MPCLAMP(wait_ns, 0, MP_TIME_S_TO_NS(10));
        vt_switcher_poll(&drm->vt_switcher, timeout_ns);
    } else {
        vo_wait_default(vo, until_time_ns);
    }
}

void vo_drm_wait_on_flip(struct vo_drm_state *drm)
{
    // poll page flip finish event
    while (drm->waiting_for_flip) {
        struct pollfd fds[1] = { { .events = POLLIN, .fd = drm->fd } };
        mp_poll(fds, 1, MP_TIME_S_TO_NS(3));
        if (fds[0].revents & POLLIN) {
            const int ret = drmHandleEvent(drm->fd, &drm->ev);
            if (ret != 0) {
                MP_ERR(drm, "drmHandleEvent failed: %i\n", ret);
                return;
            }
        }
    }
}

void vo_drm_wakeup(struct vo *vo)
{
    struct vo_drm_state *drm = vo->drm;
    if (drm->vt_switcher_active)
        vt_switcher_interrupt_poll(&drm->vt_switcher);
}
