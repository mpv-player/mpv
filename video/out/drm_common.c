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

#include "config.h"

#if HAVE_CONSIO_H
#include <sys/consio.h>
#else
#include <sys/vt.h>
#endif

#include "drm_common.h"

#include "common/common.h"
#include "common/msg.h"
#include "osdep/io.h"
#include "osdep/timer.h"
#include "misc/ctype.h"
#include "video/out/vo.h"

#define EVT_RELEASE 1
#define EVT_ACQUIRE 2
#define EVT_INTERRUPT 255
#define HANDLER_ACQUIRE 0
#define HANDLER_RELEASE 1
#define RELEASE_SIGNAL SIGUSR1
#define ACQUIRE_SIGNAL SIGUSR2
#define MAX_CONNECTOR_NAME_LEN 20

static int vt_switcher_pipe[2];

static int drm_connector_opt_help(
    struct mp_log *log, const struct m_option *opt, struct bstr name);

static int drm_mode_opt_help(
    struct mp_log *log, const struct m_option *opt, struct bstr name);

static int drm_validate_mode_opt(
    struct mp_log *log, const struct m_option *opt, struct bstr name,
    const char **value);

static void kms_show_available_modes(
    struct mp_log *log, const drmModeConnector *connector);

static void kms_show_available_connectors(struct mp_log *log, int card_no,
                                          const char *card_path);
static double mode_get_Hz(const drmModeModeInfo *mode);

#define OPT_BASE_STRUCT struct drm_opts
const struct m_sub_options drm_conf = {
    .opts = (const struct m_option[]) {
        {"drm-device", OPT_STRING(drm_device_path), .flags = M_OPT_FILE},
        {"drm-connector", OPT_STRING(drm_connector_spec),
            .help = drm_connector_opt_help},
        {"drm-mode", OPT_STRING_VALIDATE(drm_mode_spec, drm_validate_mode_opt),
            .help = drm_mode_opt_help},
        {"drm-atomic", OPT_CHOICE(drm_atomic, {"no", 0}, {"auto", 1})},
        {"drm-draw-plane", OPT_CHOICE(drm_draw_plane,
            {"primary", DRM_OPTS_PRIMARY_PLANE},
            {"overlay", DRM_OPTS_OVERLAY_PLANE}),
            M_RANGE(0, INT_MAX)},
        {"drm-drmprime-video-plane", OPT_CHOICE(drm_drmprime_video_plane,
            {"primary", DRM_OPTS_PRIMARY_PLANE},
            {"overlay", DRM_OPTS_OVERLAY_PLANE}),
            M_RANGE(0, INT_MAX)},
        {"drm-format", OPT_CHOICE(drm_format,
            {"xrgb8888",    DRM_OPTS_FORMAT_XRGB8888},
            {"xrgb2101010", DRM_OPTS_FORMAT_XRGB2101010})},
        {"drm-draw-surface-size", OPT_SIZE_BOX(drm_draw_surface_size)},

        {"drm-osd-plane-id", OPT_REPLACED("drm-draw-plane")},
        {"drm-video-plane-id", OPT_REPLACED("drm-drmprime-video-plane")},
        {"drm-osd-size", OPT_REPLACED("drm-draw-surface-size")},
        {0},
    },
    .defaults = &(const struct drm_opts) {
        .drm_mode_spec = "preferred",
        .drm_atomic = 1,
        .drm_draw_plane = DRM_OPTS_PRIMARY_PLANE,
        .drm_drmprime_video_plane = DRM_OPTS_OVERLAY_PLANE,
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

// KMS ------------------------------------------------------------------------

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
            kms_show_available_connectors(kms->log, kms->card_no,
                                          kms->primary_node_path);
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

static bool setup_mode_by_idx(struct kms *kms, unsigned int mode_idx)
{
    if (mode_idx >= kms->connector->count_modes) {
        MP_ERR(kms, "Bad mode index (max = %d).\n",
               kms->connector->count_modes - 1);
        return false;
    }

    kms->mode.mode = kms->connector->modes[mode_idx];
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

static bool setup_mode_by_numbers(struct kms *kms,
                                  unsigned int width,
                                  unsigned int height,
                                  double refresh,
                                  const char *mode_spec)
{
    for (unsigned int i = 0; i < kms->connector->count_modes; ++i) {
        drmModeModeInfo *current_mode = &kms->connector->modes[i];
        if (mode_match(current_mode, width, height, refresh)) {
            kms->mode.mode = *current_mode;
            return true;
        }
    }

    MP_ERR(kms, "Could not find mode matching %s\n", mode_spec);
    return false;
}

static bool setup_mode_preferred(struct kms *kms)
{
    for (unsigned int i = 0; i < kms->connector->count_modes; ++i) {
        drmModeModeInfo *current_mode = &kms->connector->modes[i];
        if (current_mode->type & DRM_MODE_TYPE_PREFERRED) {
            kms->mode.mode = *current_mode;
            return true;
        }
    }

    // Fall back to first mode
    MP_WARN(kms, "Could not find any preferred mode. Picking the first mode.\n");
    kms->mode.mode = kms->connector->modes[0];
    return true;
}

static bool setup_mode_highest(struct kms *kms)
{
    unsigned int area = 0;
    drmModeModeInfo *highest_resolution_mode = &kms->connector->modes[0];
    for (unsigned int i = 0; i < kms->connector->count_modes; ++i) {
        drmModeModeInfo *current_mode = &kms->connector->modes[i];

        const unsigned int current_area =
            current_mode->hdisplay * current_mode->vdisplay;
        if (current_area > area) {
            highest_resolution_mode = current_mode;
            area = current_area;
        }
    }

    kms->mode.mode = *highest_resolution_mode;
    return true;
}

static bool setup_mode(struct kms *kms, const char *mode_spec)
{
    if (kms->connector->count_modes <= 0) {
        MP_ERR(kms, "No available modes\n");
        return false;
    }

    struct drm_mode_spec parsed;
    if (!parse_mode_spec(mode_spec, &parsed)) {
        MP_ERR(kms, "Parse error\n");
        goto err;
    }

    switch (parsed.type) {
    case DRM_MODE_SPEC_BY_IDX:
        if (!setup_mode_by_idx(kms, parsed.idx))
            goto err;
        break;
    case DRM_MODE_SPEC_BY_NUMBERS:
        if (!setup_mode_by_numbers(kms, parsed.width, parsed.height, parsed.refresh,
                                   mode_spec))
            goto err;
        break;
    case DRM_MODE_SPEC_PREFERRED:
        if (!setup_mode_preferred(kms))
            goto err;
        break;
    case DRM_MODE_SPEC_HIGHEST:
        if (!setup_mode_highest(kms))
            goto err;
        break;
    default:
        MP_ERR(kms, "setup_mode: Internal error\n");
        goto err;
    }

    drmModeModeInfo *mode = &kms->mode.mode;
    MP_VERBOSE(kms, "Selected mode: %s (%dx%d@%.2fHz)\n",
        mode->name, mode->hdisplay, mode->vdisplay, mode_get_Hz(mode));

    return true;

err:
    MP_INFO(kms, "Available modes:\n");
    kms_show_available_modes(kms->log, kms->connector);
    return false;
}

static int open_card_path(const char *path)
{
    return open(path, O_RDWR | O_CLOEXEC);
}

static char *get_primary_device_path(struct mp_log *log, int *card_no)
{
    drmDevice *devices[DRM_MAX_MINOR] = { 0 };
    int card_count = drmGetDevices2(0, devices, MP_ARRAY_SIZE(devices));
    char *device_path = NULL;
    bool card_no_given = (*card_no >= 0);

    if (card_count < 0) {
        mp_err(log, "Listing DRM devices with drmGetDevices failed! (%s)\n",
               mp_strerror(errno));
        goto err;
    }

    if (card_no_given && *card_no > (card_count - 1)) {
        mp_err(log, "Card number %d given too high! %d devices located.\n",
               *card_no, card_count);
        goto err;
    }

    for (int i = card_no_given ? *card_no : 0; i < card_count; i++) {
        drmDevice *dev = devices[i];

        if (!(dev->available_nodes & (1 << DRM_NODE_PRIMARY))) {
            if (card_no_given) {
                mp_err(log,
                       "DRM card number %d given, yet it does not have "
                       "a primary node!\n", i);
                break;
            }

            continue;
        }

        const char *primary_node_path = dev->nodes[DRM_NODE_PRIMARY];

        mp_verbose(log, "Picked DRM card %d, primary node %s%s.\n",
                   i, primary_node_path,
                   card_no_given ? "" : " as the default");

        device_path = talloc_strdup(log, primary_node_path);
        *card_no = i;
        break;
    }

    if (!device_path)
        mp_err(log, "No primary DRM device could be picked!\n");

err:
    drmFreeDevices(devices, card_count);

    return device_path;
}

static void parse_connector_spec(struct mp_log *log,
                                 const char *connector_spec,
                                 int *card_no, char **connector_name)
{
    if (!connector_spec) {
        *card_no = -1;
        *connector_name = NULL;
        return;
    }
    char *dot_ptr = strchr(connector_spec, '.');
    if (dot_ptr) {
        mp_warn(log, "Warning: Selecting a connector by index with drm-connector "
                     "is deprecated. Use the drm-device option instead.\n");
        *card_no = atoi(connector_spec);
        *connector_name = talloc_strdup(log, dot_ptr + 1);
    } else {
        *card_no = -1;
        *connector_name = talloc_strdup(log, connector_spec);
    }
}

struct kms *kms_create(struct mp_log *log,
                       const char *drm_device_path,
                       const char *connector_spec,
                       const char* mode_spec,
                       int draw_plane, int drmprime_video_plane,
                       bool use_atomic)
{
    int card_no = -1;
    char *connector_name = NULL;

    parse_connector_spec(log, connector_spec, &card_no, &connector_name);
    if (drm_device_path && card_no != -1)
        mp_warn(log, "Both DRM device and card number (as part of "
                     "drm-connector) are set! Will prefer given device path "
                     "'%s'!\n",
                drm_device_path);

    char *primary_node_path = drm_device_path ?
                              talloc_strdup(log, drm_device_path) :
                              get_primary_device_path(log, &card_no);

    if (!primary_node_path) {
        mp_err(log,
               "Failed to find a usable DRM primary node!\n");
        return NULL;
    }

    struct kms *kms = talloc(NULL, struct kms);
    *kms = (struct kms) {
        .log = mp_log_new(kms, log, "kms"),
        .primary_node_path = primary_node_path,
        .fd = open_card_path(primary_node_path),
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

    drmVersionPtr ver = drmGetVersion(kms->fd);
    if (ver) {
        mp_verbose(log, "Driver: %s %d.%d.%d (%s)\n", ver->name,
            ver->version_major, ver->version_minor, ver->version_patchlevel,
            ver->date);
        drmFreeVersion(ver);
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
    if (!setup_mode(kms, mode_spec))
        goto err;

    // Universal planes allows accessing all the planes (including primary)
    if (drmSetClientCap(kms->fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1)) {
        mp_err(log, "Failed to set Universal planes capability\n");
    }

    if (!use_atomic) {
        mp_verbose(log, "Using Legacy Modesetting\n");
    } else if (drmSetClientCap(kms->fd, DRM_CLIENT_CAP_ATOMIC, 1)) {
        mp_verbose(log, "No DRM Atomic support found. Falling back to legacy modesetting\n");
    } else {
        mp_verbose(log, "DRM Atomic support found\n");
        kms->atomic_context = drm_atomic_create_context(kms->log, kms->fd, kms->crtc_id,
                                                        kms->connector->connector_id,
                                                        draw_plane, drmprime_video_plane);
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
    double rate = mode->clock * 1000.0 / mode->htotal / mode->vtotal;
    if (mode->flags & DRM_MODE_FLAG_INTERLACE)
        rate *= 2.0;
    return rate;
}

static void kms_show_available_modes(
    struct mp_log *log, const drmModeConnector *connector)
{
    for (unsigned int i = 0; i < connector->count_modes; i++) {
        mp_info(log, "  Mode %d: %s (%dx%d@%.2fHz)\n", i,
                connector->modes[i].name,
                connector->modes[i].hdisplay,
                connector->modes[i].vdisplay,
                mode_get_Hz(&connector->modes[i]));
    }
}

static void kms_show_foreach_connector(struct mp_log *log, int card_no,
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
        drmModeConnector *connector
            = drmModeGetConnector(fd, res->connectors[i]);
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

static void kms_show_connector_name_and_state_callback(
    struct mp_log *log, int card_no, const drmModeConnector *connector)
{
    char other_connector_name[MAX_CONNECTOR_NAME_LEN];
    get_connector_name(connector, other_connector_name);
    const char *connection_str =
        (connector->connection == DRM_MODE_CONNECTED) ? "connected" : "disconnected";
    mp_info(log, "  %s (%s)\n", other_connector_name, connection_str);
}

static void kms_show_available_connectors(struct mp_log *log, int card_no,
                                          const char *card_path)
{
    mp_info(log, "Available connectors for card %d (%s):\n", card_no,
            card_path);
    kms_show_foreach_connector(
        log, card_no, card_path, kms_show_connector_name_and_state_callback);
    mp_info(log, "\n");
}

static void kms_show_connector_modes_callback(struct mp_log *log, int card_no,
                                              const drmModeConnector *connector)
{
    if (connector->connection != DRM_MODE_CONNECTED)
        return;

    char other_connector_name[MAX_CONNECTOR_NAME_LEN];
    get_connector_name(connector, other_connector_name);
    mp_info(log, "Available modes for drm-connector=%d.%s\n",
            card_no, other_connector_name);
    kms_show_available_modes(log, connector);
    mp_info(log, "\n");
}

static void kms_show_available_connectors_and_modes(struct mp_log *log,
                                                    int card_no,
                                                    const char *card_path)
{
    kms_show_foreach_connector(log, card_no, card_path,
                               kms_show_connector_modes_callback);
}

static void kms_show_foreach_card(
    struct mp_log *log, void (*show_fn)(struct mp_log*,int,const char *))
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

        const char *primary_node_path = dev->nodes[DRM_NODE_PRIMARY];

        int fd = open_card_path(primary_node_path);
        if (fd < 0) {
            mp_err(log, "Failed to open primary DRM node path %s!\n",
                   primary_node_path);
            continue;
        }

        close(fd);
        show_fn(log, i, primary_node_path);
    }

    drmFreeDevices(devices, card_count);
}

static void kms_show_available_cards_and_connectors(struct mp_log *log)
{
    kms_show_foreach_card(log, kms_show_available_connectors);
}

static void kms_show_available_cards_connectors_and_modes(struct mp_log *log)
{
    kms_show_foreach_card(log, kms_show_available_connectors_and_modes);
}

double kms_get_display_fps(const struct kms *kms)
{
    return mode_get_Hz(&kms->mode.mode);
}

static int drm_connector_opt_help(struct mp_log *log, const struct m_option *opt,
                                  struct bstr name)
{
    kms_show_available_cards_and_connectors(log);
    return M_OPT_EXIT;
}

static int drm_mode_opt_help(struct mp_log *log, const struct m_option *opt,
                             struct bstr name)
{
    kms_show_available_cards_connectors_and_modes(log);
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

    struct vt_mode vt_mode = { 0 };
    if (ioctl(s->tty_fd, VT_GETMODE, &vt_mode) < 0) {
        MP_ERR(s, "VT_GETMODE failed: %s\n", mp_strerror(errno));
        return false;
    }

    vt_mode.mode = VT_PROCESS;
    vt_mode.relsig = RELEASE_SIGNAL;
    vt_mode.acqsig = ACQUIRE_SIGNAL;
    // frsig is a signal for forced release. Not implemented on Linux,
    // Solaris, BSDs but must be set to a valid signal on some of those.
    vt_mode.frsig = SIGIO; // unused
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

void drm_pflip_cb(int fd, unsigned int msc, unsigned int sec,
                  unsigned int usec, void *data)
{
    struct drm_pflip_cb_closure *closure = data;

    struct drm_vsync_tuple *vsync = closure->vsync;
    // frame_vsync->ust is the timestamp of the pageflip that happened just before this flip was queued
    // frame_vsync->msc is the sequence number of the pageflip that happened just before this flip was queued
    // frame_vsync->sbc is the sequence number for the frame that was just flipped to screen
    struct drm_vsync_tuple *frame_vsync = closure->frame_vsync;
    struct vo_vsync_info *vsync_info = closure->vsync_info;

    const bool ready =
        (vsync->msc != 0) &&
        (frame_vsync->ust != 0) && (frame_vsync->msc != 0);

    const uint64_t ust = (sec * 1000000LL) + usec;

    const unsigned int msc_since_last_flip = msc - vsync->msc;
    if (ready && msc == vsync->msc) {
        // Seems like some drivers only increment msc every other page flip when
        // running in interlaced mode (I'm looking at you nouveau). Obviously we
        // can't work with this, so shame the driver and bail.
        mp_err(closure->log,
               "Got the same msc value twice: (msc: %u, vsync->msc: %u). This shouldn't happen. Possibly broken driver/interlaced mode?\n",
               msc, vsync->msc);
        goto fail;
    }

    vsync->ust = ust;
    vsync->msc = msc;

    if (ready) {
        // Convert to mp_time
        struct timespec ts;
        if (clock_gettime(CLOCK_MONOTONIC, &ts))
            goto fail;
        const uint64_t now_monotonic = ts.tv_sec * 1000000LL + ts.tv_nsec / 1000;
        const uint64_t ust_mp_time = mp_time_us() - (now_monotonic - vsync->ust);

        const uint64_t     ust_since_enqueue = vsync->ust - frame_vsync->ust;
        const unsigned int msc_since_enqueue = vsync->msc - frame_vsync->msc;
        const unsigned int sbc_since_enqueue = vsync->sbc - frame_vsync->sbc;

        vsync_info->vsync_duration = ust_since_enqueue / msc_since_enqueue;
        vsync_info->skipped_vsyncs = msc_since_last_flip - 1; // Valid iff swap_buffers is called every vsync
        vsync_info->last_queue_display_time = ust_mp_time + (sbc_since_enqueue * vsync_info->vsync_duration);
    }

fail:
    *closure->waiting_for_flip = false;
    talloc_free(closure);
}
